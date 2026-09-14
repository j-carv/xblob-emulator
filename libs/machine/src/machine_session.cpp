#include "xblob/machine/machine_session.hpp"

#include "xblob/machine/media_boot_pipeline.hpp"
#include "xblob/memory/bus_adapter.hpp"

#include <algorithm>
#include <chrono>

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
      ohci_controller_(std::make_unique<usb::OhciController>()),
      gamepad_(std::make_shared<input::XidController>()),
      usb_guest_memory_(std::make_unique<AddressSpaceUsbGuestMemory>(space_)),
      ohci_bus_device_(std::make_shared<OhciBusDevice>(*ohci_controller_)),
      interrupt_controller_(gpu_device_, ohci_controller_.get(), 0x23, 0x24) {
    (void)pci_registry_.RegisterDevice(gpu_device_);
    // Program BAR0 (16MB MMIO at 0xFD000000)
    (void)pci_bridge_.ProgramBar(pci::PciBdf{0, 0, 0}, 0, 0xFD000000u);
    // Program BAR1 (128MB VRAM at 0xF0000000)
    (void)pci_bridge_.ProgramBar(pci::PciBdf{0, 0, 0}, 1, 0xF0000000u);
    // Enable memory space
    (void)pci_bridge_.SetMemorySpaceEnabled(pci::PciBdf{0, 0, 0}, true);
    // Connect bus to address space for MMIO (0xFD000000..0xFE000000)
    (void)memory::MapBusMmio(space_, 0xFD000000u, 0x01000000u, bus_);
    // Attach XID gamepad to OHCI port 0
    (void)ohci_controller_->AttachDevice(0, gamepad_);
    // Map OHCI USB MMIO (0xFED00000..0xFED01000)
    (void)bus_.MapDevice(0xFED00000u, 0x1000u, ohci_bus_device_);
    (void)memory::MapBusMmio(space_, 0xFED00000u, 0x1000u, bus_);
    // Map entire physical RAM into address space
    (void)space_.MapRam(0x00000000, static_cast<GuestSize>(ram_.size()), ram_, 0,
                        memory::MemoryPermission::All);
    // Connect CPU interrupt source
    cpu_.SetInterruptSource(&interrupt_controller_);

    // Connect CPU trap handler for synthetic kernel HLE interrupts (INT 0x2D)
    cpu_.SetTrapHandler([this](u8 vector, cpu::CpuContext& ctx) -> Result<bool> {
        if (vector == 0x2D) {
            const u32 ordinal = ctx.GetGpr(cpu::Reg32::EAX);
            const u32 thread_id = kernel_.threads().current_thread_id();
            const GuestAddr caller_eip = ctx.eip;

            // Advance EIP past the 2-byte INT 0x2D instruction so return lands on RET
            ctx.eip += 2;

            auto disp_res = kernel_.DispatchThunk(ordinal, ctx, space_);
            if (!disp_res) {
                trace_buffer_.RecordKernelHle(scheduler_.current_cycle(), thread_id, caller_eip,
                                              ordinal, 0, "UNSUPPORTED");
                return disp_res.error();
            }

            trace_buffer_.RecordKernelHle(scheduler_.current_cycle(), thread_id, caller_eip,
                                          ordinal, *disp_res, "");
            return true;
        }
        return false;
    });

    last_snapshot_ = TakeSnapshotLocked();

    // Start background execution worker thread
    worker_thread_ = std::thread(&MachineSession::WorkerLoop, this);
}

MachineSession::~MachineSession() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        worker_exit_ = true;
        stop_requested_ = true;
        state_ = MachineState::Stopped;
    }
    cv_cmd_.notify_all();
    cv_done_.notify_all();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

MachineState MachineSession::state() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

const std::optional<Error>& MachineSession::last_error() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_error_;
}

StopReason MachineSession::last_stop_reason() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_stop_reason_;
}

Result<void> MachineSession::Prepare(const ByteSource& xbe_source) {
    std::lock_guard<std::mutex> lock(mutex_);
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
    last_snapshot_ = TakeSnapshotLocked();

    return {};
}

Result<void> MachineSession::PrepareMedia(std::shared_ptr<const ByteSource> media_source) {
    std::lock_guard<std::mutex> lock(mutex_);
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
    last_snapshot_ = TakeSnapshotLocked();

    return {};
}

Result<void> MachineSession::Start(ExecutionBudgets budgets) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (state_ != MachineState::Prepared && state_ != MachineState::Paused) {
        return Error{ErrorCode::InvalidState,
                     "Sessão não está em estado executável (Prepared ou Paused)"};
    }

    state_ = MachineState::Running;
    active_budgets_ = budgets;
    session_instructions_executed_ = 0;
    session_cycles_consumed_ = 0;
    session_start_time_ = std::chrono::steady_clock::now();
    pause_requested_ = false;
    stop_requested_ = false;
    MachineCommand cmd;
    cmd.type = CommandType::Start;
    cmd.budgets = budgets;
    command_queue_.push_back(std::move(cmd));
    cv_cmd_.notify_one();
    return {};
}

Result<void> MachineSession::Resume(ExecutionBudgets budgets) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (state_ != MachineState::Paused) {
        return Error{ErrorCode::InvalidState, "Sessão só pode ser resumida a partir de Paused"};
    }

    state_ = MachineState::Running;
    active_budgets_ = budgets;
    session_start_time_ = std::chrono::steady_clock::now();
    pause_requested_ = false;
    stop_requested_ = false;
    MachineCommand cmd;
    cmd.type = CommandType::Resume;
    cmd.budgets = budgets;
    command_queue_.push_back(std::move(cmd));
    cv_cmd_.notify_one();
    return {};
}

Result<MachineStepResult> MachineSession::Step(u64 instruction_budget) {
    std::promise<Result<MachineStepResult>> promise;
    auto future = promise.get_future();

    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (state_ != MachineState::Prepared && state_ != MachineState::Paused) {
            return Error{ErrorCode::InvalidState,
                         "Sessão não está em estado executável (Prepared ou Paused)"};
        }

        pause_requested_ = false;
        stop_requested_ = false;
        MachineCommand cmd;
        cmd.type = CommandType::Step;
        cmd.step_instruction_budget = instruction_budget;
        cmd.promise = &promise;
        command_queue_.push_back(std::move(cmd));
        cv_cmd_.notify_one();
    }

    return future.get();
}

Result<MachineStepResult> MachineSession::RunWithBudget(u64 max_instructions, u64 max_cycles) {
    std::promise<Result<MachineStepResult>> promise;
    auto future = promise.get_future();

    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (state_ != MachineState::Prepared && state_ != MachineState::Paused) {
            return Error{ErrorCode::InvalidState,
                         "Sessão não está em estado executável (Prepared ou Paused)"};
        }

        pause_requested_ = false;
        stop_requested_ = false;
        MachineCommand cmd;
        cmd.type = CommandType::RunWithBudget;
        cmd.step_instruction_budget = max_instructions;
        cmd.run_max_cycles = max_cycles;
        cmd.promise = &promise;
        command_queue_.push_back(std::move(cmd));
        cv_cmd_.notify_one();
    }

    return future.get();
}

Result<void> MachineSession::Pause() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (state_ == MachineState::Stopped || state_ == MachineState::Faulted) {
        return Error{ErrorCode::InvalidState,
                     "Não é possível pausar uma sessão Faulted ou Stopped"};
    }
    if (state_ == MachineState::Paused) {
        return {}; // Idempotente
    }
    if (state_ == MachineState::Prepared) {
        state_ = MachineState::Paused;
        last_stop_reason_.code = StopReasonCode::Paused;
        last_snapshot_ = TakeSnapshotLocked();
        return {};
    }

    pause_requested_ = true;
    MachineCommand cmd;
    cmd.type = CommandType::Pause;
    command_queue_.push_back(std::move(cmd));
    cv_cmd_.notify_one();

    // Aguarda pausa com timeout razoável
    cv_done_.wait_for(lock, std::chrono::milliseconds(500),
                      [this] { return state_ != MachineState::Running; });
    return {};
}

Result<void> MachineSession::Stop() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (state_ == MachineState::Stopped) {
        return {}; // Idempotente
    }

    stop_requested_ = true;
    MachineCommand cmd;
    cmd.type = CommandType::Stop;
    command_queue_.push_back(std::move(cmd));
    cv_cmd_.notify_one();

    cv_done_.wait_for(lock, std::chrono::milliseconds(500),
                      [this] { return state_ == MachineState::Stopped; });
    return {};
}

Result<bool> MachineSession::WaitCompletion(u32 timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!command_queue_.empty() || state_ == MachineState::Running) {
        if (timeout_ms == 0) {
            cv_done_.wait(
                lock, [this] { return command_queue_.empty() && state_ != MachineState::Running; });
            return true;
        }
        return cv_done_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] {
            return command_queue_.empty() && state_ != MachineState::Running;
        });
    }
    return true;
}

MachineSnapshot MachineSession::GetSnapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return TakeSnapshotLocked();
}

CompatibilityDiagnostic MachineSession::GetCompatibilityDiagnostic() const {
    std::lock_guard<std::mutex> lock(mutex_);
    CompatibilityDiagnostic diag;
    if (first_blocker_.has_value()) {
        diag.first_blocker = *first_blocker_;
        diag.total_instructions = first_blocker_instructions_;
        diag.total_cycles = first_blocker_cycles_;
    } else {
        diag.first_blocker = last_stop_reason_;
        diag.total_instructions = total_instructions_executed_;
        diag.total_cycles = scheduler_.current_cycle();
    }
    diag.recent_trace = trace_buffer_.Snapshot();

    // Aggregate GPU 3D unsupported methods
    if (pushbuffer_processor_) {
        const auto& gpu_unsupported = pushbuffer_processor_->ctx_3d().unsupported_methods();
        for (const auto& entry : gpu_unsupported) {
            char cap_buf[64];
            std::snprintf(cap_buf, sizeof(cap_buf), "NV2A Method 0x%04X", entry.method);
            char ctx_buf[128];
            std::snprintf(ctx_buf, sizeof(ctx_buf), "Subchannel %u, Class 0x%04X, Param 0x%08X",
                          entry.subchannel, entry.class_id, entry.parameter);
            diag.unsupported_features.push_back(UnsupportedFeatureEntry{
                .subsystem = "GPU",
                .capability = cap_buf,
                .identifier = entry.method,
                .count = entry.count,
                .first_context = ctx_buf,
            });
        }
    }

    // Aggregate Kernel unsupported exports if any recorded
    if (kernel_.registry().last_unsupported_export().has_value()) {
        const auto& ue = *kernel_.registry().last_unsupported_export();
        char cap_buf[64];
        std::snprintf(cap_buf, sizeof(cap_buf), "Ordinal %u", ue.ordinal);
        diag.unsupported_features.push_back(UnsupportedFeatureEntry{
            .subsystem = "Kernel",
            .capability = ue.name.empty() ? cap_buf : ue.name,
            .identifier = ue.ordinal,
            .count = ue.call_count,
            .first_context = "Synthetic Thunk Caller",
        });
    }

    // Aggregate USB unsupported requests if any recorded
    if (gamepad_) {
        const auto usb_unsupported = gamepad_->unsupported_requests();
        for (const auto& entry : usb_unsupported) {
            char cap_buf[64];
            std::snprintf(cap_buf, sizeof(cap_buf), "USB Request 0x%02X (Type 0x%02X)",
                          entry.request, entry.request_type);
            char ctx_buf[128];
            std::snprintf(ctx_buf, sizeof(ctx_buf), "Value 0x%04X, Index 0x%04X", entry.value,
                          entry.index);
            diag.unsupported_features.push_back(UnsupportedFeatureEntry{
                .subsystem = "USB",
                .capability = cap_buf,
                .identifier = (static_cast<u32>(entry.request_type) << 8) | entry.request,
                .count = entry.count,
                .first_context = ctx_buf,
            });
        }
    }

    return diag;
}

void MachineSession::WorkerLoop() {
    while (true) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_cmd_.wait(lock, [this] {
            return worker_exit_ || !command_queue_.empty() ||
                   (state_ == MachineState::Running && !pause_requested_ && !stop_requested_);
        });

        if (worker_exit_) {
            break;
        }

        if (!command_queue_.empty()) {
            auto cmd = std::move(command_queue_.front());
            command_queue_.pop_front();
            ExecuteCommandLocked(cmd, lock);
            continue;
        }

        if (state_ == MachineState::Running && !pause_requested_ && !stop_requested_) {
            ExecuteRunningSliceLocked(lock);
        }
    }
}

void MachineSession::ExecuteCommandLocked(MachineCommand& cmd,
                                          std::unique_lock<std::mutex>& /*lock*/) {
    switch (cmd.type) {
    case CommandType::Start: {
        active_budgets_ = cmd.budgets;
        session_instructions_executed_ = 0;
        session_cycles_consumed_ = 0;
        session_start_time_ = std::chrono::steady_clock::now();
        last_stop_reason_ = {};
        last_snapshot_ = TakeSnapshotLocked();
        break;
    }
    case CommandType::Resume: {
        active_budgets_ = cmd.budgets;
        session_start_time_ = std::chrono::steady_clock::now();
        last_snapshot_ = TakeSnapshotLocked();
        break;
    }
    case CommandType::Pause: {
        if (state_ == MachineState::Running) {
            state_ = MachineState::Paused;
            last_stop_reason_.code = StopReasonCode::Paused;
            last_stop_reason_.detail = "Execução pausada pelo usuário";
            last_snapshot_ = TakeSnapshotLocked();
            cv_done_.notify_all();
        }
        pause_requested_ = false;
        break;
    }
    case CommandType::Stop: {
        state_ = MachineState::Stopped;
        last_stop_reason_.code = StopReasonCode::None;
        last_stop_reason_.detail = "Sessão parada";
        last_snapshot_ = TakeSnapshotLocked();
        stop_requested_ = false;
        cv_done_.notify_all();
        break;
    }
    case CommandType::Step: {
        MachineStepResult res{};
        res.state = state_;

        if (state_ != MachineState::Prepared && state_ != MachineState::Paused) {
            if (cmd.promise) {
                cmd.promise->set_value(
                    Error{ErrorCode::InvalidState, "Sessão não está em estado executável"});
            }
            return;
        }

        const u64 budget = cmd.step_instruction_budget > 0 ? cmd.step_instruction_budget : 1;
        for (u64 i = 0; i < budget; ++i) {
            const GuestAddr pre_eip = cpu_.context().eip;
            auto outcome = cpu_.Step(space_);
            res.instructions_executed++;
            total_instructions_executed_++;
            res.cycles_consumed += outcome.cycles_consumed;
            res.cpu_result = outcome.result;

            if (outcome.cycles_consumed > 0) {
                (void)scheduler_.StepCycles(outcome.cycles_consumed);
            }

            trace_buffer_.RecordInstruction(scheduler_.current_cycle(),
                                            kernel_.threads().current_thread_id(), pre_eip, 0, "");

            if (outcome.result == cpu::StepResult::Halted) {
                state_ = MachineState::Paused;
                last_stop_reason_ = StopReason{
                    .code = StopReasonCode::Halted,
                    .fault_eip = cpu_.context().eip,
                    .thread_id = kernel_.threads().current_thread_id(),
                    .ordinal_or_opcode = 0xF4,
                    .count = 1,
                    .category = "CPU",
                    .symbol_or_mnemonic = "HLT",
                    .detail = "Instrução HLT executada",
                };
                break;
            }

            if (outcome.result == cpu::StepResult::Faulted) {
                state_ = MachineState::Faulted;
                last_error_ = Error{ErrorCode::ExecutionFault, "Falha de CPU durante o step"};
                PopulateFaultStopReasonLocked(outcome);
                break;
            }
        }

        if (state_ == MachineState::Prepared) {
            state_ = MachineState::Paused;
        }
        if (last_stop_reason_.code == StopReasonCode::None) {
            last_stop_reason_.code = StopReasonCode::StepCompleted;
            last_stop_reason_.detail = "Step de instrução concluído";
        }
        res.state = state_;
        last_snapshot_ = TakeSnapshotLocked();
        if (cmd.promise) {
            cmd.promise->set_value(res);
        }
        cv_done_.notify_all();
        break;
    }
    case CommandType::RunWithBudget: {
        MachineStepResult res{};
        res.state = state_;

        if (state_ != MachineState::Prepared && state_ != MachineState::Paused) {
            if (cmd.promise) {
                cmd.promise->set_value(
                    Error{ErrorCode::InvalidState, "Sessão não está em estado executável"});
            }
            return;
        }

        const u64 max_inst = cmd.step_instruction_budget;
        const u64 max_cyc = cmd.run_max_cycles;

        while (res.instructions_executed < max_inst && res.cycles_consumed < max_cyc) {
            const GuestAddr pre_eip = cpu_.context().eip;
            auto outcome = cpu_.Step(space_);
            res.instructions_executed++;
            total_instructions_executed_++;
            res.cycles_consumed += outcome.cycles_consumed;
            res.cpu_result = outcome.result;

            if (outcome.cycles_consumed > 0) {
                (void)scheduler_.StepCycles(outcome.cycles_consumed);
            }

            trace_buffer_.RecordInstruction(scheduler_.current_cycle(),
                                            kernel_.threads().current_thread_id(), pre_eip, 0, "");

            if (outcome.result == cpu::StepResult::Halted) {
                state_ = MachineState::Paused;
                last_stop_reason_ = StopReason{
                    .code = StopReasonCode::Halted,
                    .fault_eip = cpu_.context().eip,
                    .thread_id = kernel_.threads().current_thread_id(),
                    .ordinal_or_opcode = 0xF4,
                    .count = 1,
                    .category = "CPU",
                    .symbol_or_mnemonic = "HLT",
                    .detail = "Instrução HLT executada",
                };
                break;
            }

            if (outcome.result == cpu::StepResult::Faulted) {
                state_ = MachineState::Faulted;
                last_error_ = Error{ErrorCode::ExecutionFault, "Falha de CPU durante a execução"};
                PopulateFaultStopReasonLocked(outcome);
                break;
            }
        }

        if (state_ == MachineState::Prepared) {
            state_ = MachineState::Paused;
        }
        if (last_stop_reason_.code == StopReasonCode::None) {
            if (res.instructions_executed >= max_inst) {
                last_stop_reason_.code = StopReasonCode::BudgetInstructionsExhausted;
                last_stop_reason_.detail = "Limite de instruções atingido";
            } else if (res.cycles_consumed >= max_cyc) {
                last_stop_reason_.code = StopReasonCode::BudgetCyclesExhausted;
                last_stop_reason_.detail = "Limite de ciclos atingido";
            }
        }
        res.state = state_;
        last_snapshot_ = TakeSnapshotLocked();
        if (cmd.promise) {
            cmd.promise->set_value(res);
        }
        cv_done_.notify_all();
        break;
    }
    case CommandType::None:
        break;
    }
}

void MachineSession::ExecuteRunningSliceLocked(std::unique_lock<std::mutex>& /*lock*/) {
    // 1. Safe-point: check pause and stop requests
    if (pause_requested_) {
        pause_requested_ = false;
        state_ = MachineState::Paused;
        last_stop_reason_.code = StopReasonCode::Paused;
        last_stop_reason_.detail = "Execução pausada pelo usuário";
        last_snapshot_ = TakeSnapshotLocked();
        cv_done_.notify_all();
        return;
    }

    if (stop_requested_) {
        stop_requested_ = false;
        state_ = MachineState::Stopped;
        last_stop_reason_.code = StopReasonCode::None;
        last_stop_reason_.detail = "Sessão finalizada";
        last_snapshot_ = TakeSnapshotLocked();
        cv_done_.notify_all();
        return;
    }

    // 2. Budget checks: wall-time watchdog
    if (active_budgets_.max_wall_time_ms > 0) {
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - session_start_time_)
                .count();
        if (static_cast<u64>(elapsed_ms) >= active_budgets_.max_wall_time_ms) {
            state_ = MachineState::Paused;
            last_stop_reason_ = StopReason{
                .code = StopReasonCode::BudgetWallTimeExhausted,
                .fault_eip = cpu_.context().eip,
                .thread_id = kernel_.threads().current_thread_id(),
                .ordinal_or_opcode = 0,
                .count = 1,
                .category = "Watchdog",
                .symbol_or_mnemonic = "WallTime",
                .detail = "Watchdog: limite de tempo wall-clock esgotado",
            };
            last_snapshot_ = TakeSnapshotLocked();
            cv_done_.notify_all();
            return;
        }
    }

    // Budget checks: instructions
    if (session_instructions_executed_ >= active_budgets_.max_instructions) {
        state_ = MachineState::Paused;
        last_stop_reason_ = StopReason{
            .code = StopReasonCode::BudgetInstructionsExhausted,
            .fault_eip = cpu_.context().eip,
            .thread_id = kernel_.threads().current_thread_id(),
            .ordinal_or_opcode = 0,
            .count = 1,
            .category = "Budget",
            .symbol_or_mnemonic = "Instructions",
            .detail = "Limite de instruções esgotado",
        };
        last_snapshot_ = TakeSnapshotLocked();
        cv_done_.notify_all();
        return;
    }

    // Budget checks: cycles
    if (session_cycles_consumed_ >= active_budgets_.max_cycles) {
        state_ = MachineState::Paused;
        last_stop_reason_ = StopReason{
            .code = StopReasonCode::BudgetCyclesExhausted,
            .fault_eip = cpu_.context().eip,
            .thread_id = kernel_.threads().current_thread_id(),
            .ordinal_or_opcode = 0,
            .count = 1,
            .category = "Budget",
            .symbol_or_mnemonic = "Cycles",
            .detail = "Limite de ciclos esgotado",
        };
        last_snapshot_ = TakeSnapshotLocked();
        cv_done_.notify_all();
        return;
    }

    // 3. Execute bounded instruction slice
    const u32 chunk = active_budgets_.chunk_instructions > 0
                          ? std::min(active_budgets_.chunk_instructions, 1000u)
                          : 1000u;
    const u64 remaining_inst = active_budgets_.max_instructions - session_instructions_executed_;
    const u32 slice_limit = static_cast<u32>(std::min(static_cast<u64>(chunk), remaining_inst));

    for (u32 i = 0; i < slice_limit; ++i) {
        if (pause_requested_ || stop_requested_) {
            break;
        }

        const GuestAddr pre_eip = cpu_.context().eip;
        auto outcome = cpu_.Step(space_);
        session_instructions_executed_++;
        total_instructions_executed_++;

        if (outcome.cycles_consumed > 0) {
            (void)scheduler_.StepCycles(outcome.cycles_consumed);
            session_cycles_consumed_ += outcome.cycles_consumed;
        }

        trace_buffer_.RecordInstruction(scheduler_.current_cycle(),
                                        kernel_.threads().current_thread_id(), pre_eip, 0, "");

        if (outcome.result == cpu::StepResult::Halted) {
            state_ = MachineState::Paused;
            last_stop_reason_ = StopReason{
                .code = StopReasonCode::Halted,
                .fault_eip = cpu_.context().eip,
                .thread_id = kernel_.threads().current_thread_id(),
                .ordinal_or_opcode = 0xF4,
                .count = 1,
                .category = "CPU",
                .symbol_or_mnemonic = "HLT",
                .detail = "Instrução HLT executada",
            };
            break;
        }

        if (outcome.result == cpu::StepResult::Faulted) {
            state_ = MachineState::Faulted;
            last_error_ = Error{ErrorCode::ExecutionFault, "Falha de CPU durante execução"};
            PopulateFaultStopReasonLocked(outcome);
            break;
        }
    }

    // Deterministic USB OHCI frame processing (~1ms / 50000 cycles)
    constexpr Cycle kUsbFrameCycleInterval = 50000;
    if (scheduler_.current_cycle() >= last_usb_frame_cycle_ + kUsbFrameCycleInterval) {
        last_usb_frame_cycle_ = scheduler_.current_cycle();
        if (ohci_controller_ && usb_guest_memory_) {
            (void)ohci_controller_->ProcessFrame(*usb_guest_memory_);
        }
    }

    // Frame pacing / backpressure: sync frame sequence
    if (gpu_device_ && gpu_device_->frame_counter() > frame_sequence_) {
        frame_sequence_ = gpu_device_->frame_counter();
    }

    last_snapshot_ = TakeSnapshotLocked();

    if (state_ != MachineState::Running) {
        cv_done_.notify_all();
    }
}

void MachineSession::PopulateFaultStopReasonLocked(const cpu::StepOutcome& outcome) {
    if (kernel_.registry().last_unsupported_export().has_value()) {
        const auto& ue = *kernel_.registry().last_unsupported_export();
        last_stop_reason_ = StopReason{
            .code = StopReasonCode::UnsupportedExport,
            .fault_eip = ue.eip,
            .thread_id = ue.thread_id,
            .ordinal_or_opcode = ue.ordinal,
            .count = ue.call_count,
            .category = "Kernel",
            .symbol_or_mnemonic =
                ue.name.empty() ? ("Ordinal " + std::to_string(ue.ordinal)) : ue.name,
            .detail =
                "Export do Kernel HLE não implementado: ordinal " + std::to_string(ue.ordinal),
        };
    } else if (outcome.unsupported_form.has_value()) {
        const auto& uf = *outcome.unsupported_form;
        u32 opc = 0;
        if (!uf.opcode_bytes.empty()) {
            for (std::size_t b = 0; b < std::min(std::size_t(4), uf.opcode_bytes.size()); ++b) {
                opc |= static_cast<u32>(uf.opcode_bytes[b]) << (b * 8);
            }
        }
        last_stop_reason_ = StopReason{
            .code = StopReasonCode::UnsupportedOpcode,
            .fault_eip = uf.fault_eip,
            .thread_id = kernel_.threads().current_thread_id(),
            .ordinal_or_opcode = opc,
            .count = 1,
            .category = "CPU",
            .symbol_or_mnemonic = uf.reason.empty() ? "UnsupportedOpcode" : uf.reason,
            .detail = uf.reason.empty() ? "Instrução IA-32 não suportada" : uf.reason,
        };
    } else if (outcome.exception.has_value()) {
        last_stop_reason_ = StopReason{
            .code = StopReasonCode::CpuException,
            .fault_eip = outcome.exception->fault_eip,
            .thread_id = kernel_.threads().current_thread_id(),
            .ordinal_or_opcode = static_cast<u32>(outcome.exception->vector),
            .count = 1,
            .category = "CPU Exception",
            .symbol_or_mnemonic = cpu::ToString(outcome.exception->vector),
            .detail = "Exceção arquitetural de CPU",
        };
    } else {
        last_stop_reason_ = StopReason{
            .code = StopReasonCode::MemoryFault,
            .fault_eip = cpu_.context().eip,
            .thread_id = kernel_.threads().current_thread_id(),
            .ordinal_or_opcode = 0,
            .count = 1,
            .category = "Memory",
            .symbol_or_mnemonic = "Fault",
            .detail = outcome.fault ? outcome.fault->message : "Falha de memória",
        };
    }

    RecordFirstBlockerLocked(last_stop_reason_);
}

void MachineSession::RecordFirstBlockerLocked(const StopReason& reason) {
    if (!first_blocker_.has_value() && reason.code != StopReasonCode::None &&
        reason.code != StopReasonCode::Paused && reason.code != StopReasonCode::StepCompleted) {
        first_blocker_ = reason;
        first_blocker_instructions_ = total_instructions_executed_;
        first_blocker_cycles_ = scheduler_.current_cycle();
    }
}

MachineSnapshot MachineSession::TakeSnapshotLocked() const {
    MachineSnapshot snap;
    snap.state_val = static_cast<u8>(state_);
    snap.last_stop_reason = last_stop_reason_;
    snap.cpu_context = cpu_.context();
    snap.eflags_raw = cpu_.context().eflags;
    snap.current_cycle = scheduler_.current_cycle();
    snap.instructions_executed = total_instructions_executed_;
    snap.events_fired = total_events_fired_;
    snap.active_thread_id = kernel_.threads().current_thread_id();
    snap.thread_count = static_cast<u32>(kernel_.threads().active_thread_count());
    if (snap.thread_count == 0 && state_ != MachineState::Created) {
        snap.thread_count = 1;
    }
    if (last_error_) {
        snap.error_message = last_error_->message;
    }

    // Stack inspection: safely check if ESP is mapped and readable
    const GuestAddr esp = cpu_.context().GetGpr(cpu::Reg32::ESP);
    auto test_word = space_.Read32(esp);
    if (test_word.has_value()) {
        snap.stack_valid = true;
        snap.stack_words.reserve(8);
        for (std::size_t i = 0; i < 8; ++i) {
            auto word = space_.Read32(esp + static_cast<GuestAddr>(i * 4));
            if (word.has_value()) {
                snap.stack_words.push_back(*word);
            } else {
                break;
            }
        }
    } else {
        snap.stack_valid = false;
    }

    return snap;
}

Result<gpu::PushbufferStats> MachineSession::ExecutePushbuffer(GuestAddr addr, u32 word_count,
                                                               gpu::PushbufferBudgets budgets) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto reader = [this](GuestAddr a) -> Result<u32> { return space_.Read32(a); };
    return pushbuffer_processor_->ExecuteFromMemory(addr, reader, word_count, budgets);
}

Result<EventId> MachineSession::SchedulePushbuffer(GuestAddr addr, u32 word_count, Cycle delay) {
    std::lock_guard<std::mutex> lock(mutex_);
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
    std::lock_guard<std::mutex> lock(mutex_);
    return gpu_device_->front_surface().metadata(scheduler_.current_cycle());
}

Result<std::size_t> MachineSession::CopyLatestFrame(std::span<u8> destination) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return gpu_device_->front_surface().CopyRawPixels(destination);
}

Result<bool> MachineSession::SubmitHostInput(const input::HostInputSnapshot& snapshot) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!gamepad_) {
        return Error{ErrorCode::InvalidState, "Gamepad não inicializado"};
    }
    bool accepted = gamepad_->SubmitSnapshot(snapshot);
    if (accepted) {
        input_sequence_ = snapshot.sequence;
    }
    return accepted;
}

InteractiveMetrics MachineSession::GetInteractiveMetrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    InteractiveMetrics m{};
    m.frame_sequence = frame_sequence_;
    m.input_sequence = input_sequence_;
    m.instructions_executed = total_instructions_executed_;
    m.cycles_consumed = scheduler_.current_cycle();
    if (pushbuffer_processor_) {
        m.unsupported_gpu_count = pushbuffer_processor_->ctx_3d().total_unsupported_methods_count();
    }
    if (gamepad_) {
        m.unsupported_usb_count = gamepad_->unsupported_requests_count();
    }
    m.state_val = static_cast<u8>(state_);
    m.stop_reason = last_stop_reason_.code;
    return m;
}

u64 MachineSession::frame_sequence() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return frame_sequence_;
}

u64 MachineSession::input_sequence() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return input_sequence_;
}

} // namespace xblob::machine
