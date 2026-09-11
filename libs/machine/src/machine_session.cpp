#include "xblob/machine/machine_session.hpp"

#include "xblob/machine/media_boot_pipeline.hpp"
#include "xblob/memory/bus_adapter.hpp"

namespace xblob::machine {

Result<std::unique_ptr<MachineSession>> MachineSession::Create(std::size_t ram_size) {
    auto ram_res = memory::Ram::Create(ram_size);
    if (!ram_res) {
        return ram_res.error();
    }
    return std::unique_ptr<MachineSession>(new MachineSession(std::move(*ram_res)));
}

MachineSession::MachineSession(memory::Ram ram)
    : ram_(std::move(ram)), scheduler_(0), pci_bridge_(bus_, pci_registry_),
      gpu_device_(gpu::Nv2aDevice::Create(pci::PciBdf{0, 0, 0})),
      pushbuffer_processor_(std::make_unique<gpu::PushbufferProcessor>(*gpu_device_)),
      gpu_interrupt_source_(gpu_device_, 0x23) {
    (void)pci_registry_.RegisterDevice(gpu_device_);
    // Program BAR0 (16MB MMIO at 0xFD000000)
    (void)pci_bridge_.ProgramBar(pci::PciBdf{0, 0, 0}, 0, 0xFD000000u);
    // Program BAR1 (128MB VRAM at 0xF0000000)
    (void)pci_bridge_.ProgramBar(pci::PciBdf{0, 0, 0}, 1, 0xF0000000u);
    // Enable memory space
    (void)pci_bridge_.SetMemorySpaceEnabled(pci::PciBdf{0, 0, 0}, true);
    // Connect bus to address space for MMIO (0xFD000000..0xFE000000)
    (void)memory::MapBusMmio(space_, 0xFD000000u, 0x01000000u, bus_);
    // Map entire physical RAM into address space
    (void)space_.MapRam(0x00000000, static_cast<GuestSize>(ram_.size()), ram_, 0,
                        memory::MemoryPermission::All);
    // Connect CPU interrupt source
    cpu_.SetInterruptSource(&gpu_interrupt_source_);
}

Result<void> MachineSession::Prepare(const ByteSource& xbe_source) {
    if (state_ != MachineState::Created) {
        return Error{ErrorCode::InvalidState,
                     "Sessão só pode ser preparada a partir do estado Created"};
    }

    // Ensure physical RAM is mapped into address space
    bool ram_mapped = false;
    for (const auto& reg : space_.regions()) {
        if (reg.base == 0x00000000 && reg.type == memory::RegionType::Ram) {
            ram_mapped = true;
            break;
        }
    }
    if (!ram_mapped) {
        auto map_ram = space_.MapRam(0x00000000, static_cast<GuestSize>(ram_.size()), ram_, 0,
                                     memory::MemoryPermission::All);
        if (!map_ram) {
            last_error_ = map_ram.error();
            return map_ram.error();
        }
    }

    // Validate and plan XBE
    auto plan_res = loader::XbeLoader::Plan(xbe_source);
    if (!plan_res) {
        last_error_ = plan_res.error();
        return plan_res.error();
    }

    // Apply plan transactionally
    auto apply_res = loader::XbeLoader::Apply(*plan_res, xbe_source, space_);
    if (!apply_res) {
        last_error_ = apply_res.error();
        return apply_res.error();
    }

    // Generate initial CPU execution context
    auto init_ctx = loader::XbeLoader::CreateInitialContext(*plan_res);
    cpu_.context() = init_ctx.cpu_context;

    vfs_ = std::make_shared<Vfs>();
    kernel_.SetVfs(vfs_);

    load_plan_ = std::move(*plan_res);
    initial_context_ = std::move(init_ctx);
    state_ = MachineState::Prepared;
    last_error_ = std::nullopt;

    return {};
}

Result<void> MachineSession::PrepareMedia(std::shared_ptr<const ByteSource> media_source) {
    if (state_ != MachineState::Created) {
        return Error{ErrorCode::InvalidState,
                     "Sessão só pode ser preparada a partir do estado Created"};
    }

    auto plan_res = MediaBootPipeline::Plan(media_source);
    if (!plan_res) {
        last_error_ = plan_res.error();
        return plan_res.error();
    }

    // Ensure physical RAM is mapped into address space
    bool ram_mapped = false;
    for (const auto& reg : space_.regions()) {
        if (reg.base == 0x00000000 && reg.type == memory::RegionType::Ram) {
            ram_mapped = true;
            break;
        }
    }
    if (!ram_mapped) {
        auto map_ram = space_.MapRam(0x00000000, static_cast<GuestSize>(ram_.size()), ram_, 0,
                                     memory::MemoryPermission::All);
        if (!map_ram) {
            last_error_ = map_ram.error();
            return map_ram.error();
        }
    }

    // Apply plan transactionally
    auto apply_res =
        loader::XbeLoader::Apply(plan_res->load_plan, *plan_res->executable_source, space_);
    if (!apply_res) {
        last_error_ = apply_res.error();
        return apply_res.error();
    }

    cpu_.context() = plan_res->initial_context.cpu_context;
    media_source_ = std::move(media_source);
    vfs_ = plan_res->vfs;
    kernel_.SetVfs(vfs_);

    load_plan_ = std::move(plan_res->load_plan);
    initial_context_ = std::move(plan_res->initial_context);
    state_ = MachineState::Prepared;
    last_error_ = std::nullopt;

    return {};
}

Result<MachineStepResult> MachineSession::Step(u64 instruction_budget) {
    if (state_ != MachineState::Prepared && state_ != MachineState::Paused) {
        return Error{ErrorCode::InvalidState,
                     "Sessão não está em estado executável (Prepared ou Paused)"};
    }

    MachineStepResult res;
    res.state = state_;

    for (u64 i = 0; i < instruction_budget; ++i) {
        auto outcome = cpu_.Step(space_);
        res.instructions_executed++;
        res.cycles_consumed += outcome.cycles_consumed;
        res.cpu_result = outcome.result;

        if (outcome.cycles_consumed > 0) {
            (void)scheduler_.StepCycles(outcome.cycles_consumed);
        }

        if (outcome.result == cpu::StepResult::Halted) {
            state_ = MachineState::Paused;
            break;
        }

        if (outcome.result == cpu::StepResult::Faulted) {
            state_ = MachineState::Faulted;
            last_error_ = Error{ErrorCode::ExecutionFault, "Falha de CPU durante o step"};
            break;
        }
    }

    res.state = state_;
    return res;
}

Result<MachineStepResult> MachineSession::RunWithBudget(u64 max_instructions, u64 max_cycles) {
    if (state_ != MachineState::Prepared && state_ != MachineState::Paused) {
        return Error{ErrorCode::InvalidState,
                     "Sessão não está em estado executável (Prepared ou Paused)"};
    }

    MachineStepResult res;
    res.state = state_;

    while (res.instructions_executed < max_instructions && res.cycles_consumed < max_cycles) {
        auto outcome = cpu_.Step(space_);
        res.instructions_executed++;
        res.cycles_consumed += outcome.cycles_consumed;
        res.cpu_result = outcome.result;

        if (outcome.cycles_consumed > 0) {
            (void)scheduler_.StepCycles(outcome.cycles_consumed);
        }

        if (outcome.result == cpu::StepResult::Halted) {
            state_ = MachineState::Paused;
            break;
        }

        if (outcome.result == cpu::StepResult::Faulted) {
            state_ = MachineState::Faulted;
            last_error_ = Error{ErrorCode::ExecutionFault, "Falha de CPU durante a execução"};
            break;
        }
    }

    res.state = state_;
    return res;
}

Result<void> MachineSession::Pause() {
    if (state_ == MachineState::Stopped || state_ == MachineState::Faulted) {
        return Error{ErrorCode::InvalidState,
                     "Não é possível pausar uma sessão Faulted ou Stopped"};
    }
    if (state_ == MachineState::Prepared) {
        state_ = MachineState::Paused;
    }
    return {};
}

Result<void> MachineSession::Stop() {
    state_ = MachineState::Stopped;
    return {};
}

Result<gpu::PushbufferStats> MachineSession::ExecutePushbuffer(GuestAddr addr, u32 word_count,
                                                               gpu::PushbufferBudgets budgets) {
    auto reader = [this](GuestAddr a) -> Result<u32> { return space_.Read32(a); };
    return pushbuffer_processor_->ExecuteFromMemory(addr, reader, word_count, budgets);
}

Result<EventId> MachineSession::SchedulePushbuffer(GuestAddr addr, u32 word_count, Cycle delay) {
    const Cycle target_cycle = scheduler_.current_cycle() + delay;
    auto event_res = scheduler_.ScheduleEvent(target_cycle, [this, addr, word_count](Cycle) {
        auto stats_res = ExecutePushbuffer(addr, word_count);
        if (stats_res.has_value()) {
            const Cycle flip_cycle = scheduler_.current_cycle() + 200;
            (void)scheduler_.ScheduleEvent(flip_cycle, [this](Cycle) { gpu_device_->Flip(); });
        }
    });
    if (event_res.has_value()) {
        gpu_device_->RegisterPendingEvent(*event_res);
    }
    return event_res;
}

gpu::GpuFrameMetadata MachineSession::GetLatestFrameMetadata() const noexcept {
    return gpu_device_->front_surface().metadata(scheduler_.current_cycle());
}

Result<std::size_t> MachineSession::CopyLatestFrame(std::span<u8> destination) const {
    return gpu_device_->front_surface().CopyRawPixels(destination);
}

} // namespace xblob::machine
