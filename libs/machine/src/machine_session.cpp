#include "xblob/machine/machine_session.hpp"

namespace xblob::machine {

Result<std::unique_ptr<MachineSession>> MachineSession::Create(std::size_t ram_size) {
    auto ram_res = memory::Ram::Create(ram_size);
    if (!ram_res) {
        return ram_res.error();
    }
    return std::unique_ptr<MachineSession>(new MachineSession(std::move(*ram_res)));
}

MachineSession::MachineSession(memory::Ram ram) : ram_(std::move(ram)), scheduler_(0) {}

Result<void> MachineSession::Prepare(const ByteSource& xbe_source) {
    if (state_ != MachineState::Created) {
        return Error{ErrorCode::InvalidState,
                     "Sessão só pode ser preparada a partir do estado Created"};
    }

    // Map entire physical RAM into address space
    auto map_ram = space_.MapRam(0x00000000, static_cast<GuestSize>(ram_.size()), ram_, 0,
                                 memory::MemoryPermission::All);
    if (!map_ram) {
        last_error_ = map_ram.error();
        return map_ram.error();
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

    load_plan_ = std::move(*plan_res);
    initial_context_ = std::move(init_ctx);
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

} // namespace xblob::machine
