#include "xblob/cpu/cpu.hpp"

#include "xblob/cpu/decoder.hpp"
#include "xblob/cpu/executor.hpp"

namespace xblob::cpu {

void Cpu::Reset() noexcept {
    context_.Reset();
    lifecycle_ = CpuLifecycle::Running;
    last_fault_ = std::nullopt;
    last_exception_ = std::nullopt;
    last_unsupported_ = std::nullopt;
}

template <typename MemoryType>
StepOutcome Cpu::StepGeneric(MemoryType& memory) {
    if (lifecycle_ == CpuLifecycle::Halted) {
        return StepOutcome{StepResult::Halted, 0, std::nullopt, last_exception_, last_unsupported_};
    }
    if (lifecycle_ == CpuLifecycle::Faulted) {
        return StepOutcome{StepResult::Faulted, 0, last_fault_, last_exception_, last_unsupported_};
    }

    // 1. Check interrupt boundary
    if (interrupt_source_ != nullptr && interrupt_source_->HasPendingInterrupt() &&
        context_.GetFlag(kFlagIF)) {
        auto vec_opt = interrupt_source_->AcknowledgeInterrupt();
        if (vec_opt.has_value()) {
            auto del_res = Executor::DeliverGate(*vec_opt, context_, memory, std::nullopt, false,
                                                 context_.eip);
            if (!del_res) {
                lifecycle_ = CpuLifecycle::Faulted;
                last_fault_ = del_res.error();
                return StepOutcome{StepResult::Faulted, cycles::kInt, last_fault_, last_exception_,
                                   last_unsupported_};
            }
            return StepOutcome{StepResult::Ok, cycles::kInt, std::nullopt, std::nullopt,
                               std::nullopt};
        }
    }

    // 2. Decode instruction
    const u32 start_eip = context_.eip;
    std::optional<UnsupportedFormInfo> unsupported_info{std::nullopt};
    auto decode_res = Decoder::Decode(context_, memory, start_eip, &unsupported_info);
    if (!decode_res) {
        if (unsupported_info.has_value()) {
            last_unsupported_ = unsupported_info;
        }
        if (decode_res.error().code == ErrorCode::UnsupportedFeature) {
            lifecycle_ = CpuLifecycle::Faulted;
            last_fault_ = decode_res.error();
            return StepOutcome{StepResult::Faulted, 0, last_fault_, std::nullopt,
                               last_unsupported_};
        }
        if constexpr (requires { memory.last_page_fault(); }) {
            if (const auto& pf = memory.last_page_fault(); pf.has_value()) {
                last_exception_ = CpuException{ExceptionVector::PageFault, pf->error_code,
                                               start_eip, pf->fault_address};
            }
        }
        if (!last_exception_.has_value()) {
            if (decode_res.error().code == ErrorCode::InvalidOpcode) {
                last_exception_ =
                    CpuException{ExceptionVector::InvalidOpcode, std::nullopt, start_eip, 0};
            } else {
                last_exception_ =
                    CpuException{ExceptionVector::GeneralProtection, std::nullopt, start_eip, 0};
            }
        }
        lifecycle_ = CpuLifecycle::Faulted;
        last_fault_ = decode_res.error();
        return StepOutcome{StepResult::Faulted, 0, last_fault_, last_exception_, last_unsupported_};
    }

    // 3. Execute instruction
    auto exec_res = Executor::Execute(*decode_res, context_, memory, start_eip, trap_handler_);
    if (!exec_res) {
        if (exec_res.error().code == ErrorCode::UnsupportedFeature) {
            last_unsupported_ = UnsupportedFormInfo{start_eip, {}, exec_res.error().message};
            lifecycle_ = CpuLifecycle::Faulted;
            last_fault_ = exec_res.error();
            return StepOutcome{StepResult::Faulted, decode_res->cycles, last_fault_, std::nullopt,
                               last_unsupported_};
        }
        if constexpr (requires { memory.last_page_fault(); }) {
            if (const auto& pf = memory.last_page_fault(); pf.has_value()) {
                last_exception_ = CpuException{ExceptionVector::PageFault, pf->error_code,
                                               start_eip, pf->fault_address};
            }
        }
        if (!last_exception_.has_value()) {
            if (exec_res.error().code == ErrorCode::InvalidOpcode) {
                last_exception_ =
                    CpuException{ExceptionVector::InvalidOpcode, std::nullopt, start_eip, 0};
            } else {
                last_exception_ =
                    CpuException{ExceptionVector::GeneralProtection,
                                 static_cast<u32>(exec_res.error().offset), start_eip, 0};
            }
        }
        lifecycle_ = CpuLifecycle::Faulted;
        last_fault_ = exec_res.error();
        return StepOutcome{StepResult::Faulted, decode_res->cycles, last_fault_, last_exception_,
                           last_unsupported_};
    }

    if (exec_res->exception.has_value()) {
        last_exception_ = exec_res->exception;
        if (context_.idtr.limit > 0) {
            auto res = Executor::DeliverGate(static_cast<u8>(exec_res->exception->vector), context_,
                                             memory, exec_res->exception->error_code, false,
                                             exec_res->exception->fault_eip);
            if (res) {
                return StepOutcome{StepResult::Ok, decode_res->cycles, std::nullopt, std::nullopt,
                                   std::nullopt};
            }
        }
        lifecycle_ = CpuLifecycle::Faulted;
        last_fault_ = Error{ErrorCode::ExecutionFault,
                            (exec_res->exception->vector == ExceptionVector::DivideError)
                                ? "Erro de divisão (#DE)"
                                : "Exceção de CPU",
                            exec_res->exception->fault_eip};
        return StepOutcome{StepResult::Faulted, decode_res->cycles, last_fault_, last_exception_,
                           std::nullopt};
    }

    // 4. Commit
    if (exec_res->halted) {
        context_.eip = start_eip + decode_res->length;
        lifecycle_ = CpuLifecycle::Halted;
        return StepOutcome{StepResult::Halted, decode_res->cycles, std::nullopt, std::nullopt,
                           std::nullopt};
    }

    if (exec_res->branched) {
        context_.eip = exec_res->next_eip;
    } else {
        context_.eip = start_eip + decode_res->length;
    }

    return StepOutcome{StepResult::Ok, decode_res->cycles, std::nullopt, std::nullopt,
                       std::nullopt};
}

template <typename MemoryType>
Result<void> Cpu::RaiseException(CpuException ex, MemoryType& mem) {
    last_exception_ = ex;
    if (context_.idtr.limit > 0) {
        auto res = Executor::DeliverGate(static_cast<u8>(ex.vector), context_, mem, ex.error_code,
                                         false, ex.fault_eip);
        if (res) {
            return {};
        }
    }

    lifecycle_ = CpuLifecycle::Faulted;
    if (ex.vector == ExceptionVector::DivideError) {
        last_fault_ = Error{ErrorCode::ExecutionFault, "Erro de divisão (#DE)", ex.fault_eip};
    } else if (ex.vector == ExceptionVector::InvalidOpcode) {
        last_fault_ = Error{ErrorCode::InvalidOpcode, "Opcode inválido (#UD)", ex.fault_eip};
    } else if (ex.vector == ExceptionVector::PageFault) {
        last_fault_ = Error{ErrorCode::AccessViolation, "Falha de página (#PF)", ex.cr2};
    } else {
        last_fault_ =
            Error{ErrorCode::GeneralProtection, "Falha geral de proteção (#GP)", ex.fault_eip};
    }
    return last_fault_.value();
}

template <typename MemoryType>
RunOutcome Cpu::RunWithBudgetGeneric(MemoryType& memory, u64 max_instructions, Cycle max_cycles) {
    RunOutcome outcome{};
    outcome.status = RunStatus::BudgetExhausted;

    while (outcome.instructions_executed < max_instructions &&
           outcome.cycles_consumed < max_cycles) {
        StepOutcome step = StepGeneric(memory);
        outcome.cycles_consumed += step.cycles_consumed;

        if (step.result == StepResult::Halted) {
            outcome.instructions_executed++;
            outcome.status = RunStatus::Halted;
            return outcome;
        }

        if (step.result == StepResult::Faulted) {
            outcome.status = RunStatus::Faulted;
            outcome.fault = step.fault;
            outcome.exception = step.exception;
            outcome.unsupported_form = step.unsupported_form;
            return outcome;
        }

        outcome.instructions_executed++;
    }

    if (lifecycle_ == CpuLifecycle::Halted) {
        outcome.status = RunStatus::Halted;
    }

    return outcome;
}

StepOutcome Cpu::Step(const memory::AddressSpace& memory) {
    return Step(const_cast<memory::AddressSpace&>(memory));
}

StepOutcome Cpu::Step(memory::AddressSpace& memory) {
    return StepGeneric(memory);
}

StepOutcome Cpu::Step(memory::VirtualMemory& memory) {
    return StepGeneric(memory);
}

RunOutcome Cpu::RunWithBudget(const memory::AddressSpace& memory, u64 max_instructions,
                              Cycle max_cycles) {
    return RunWithBudget(const_cast<memory::AddressSpace&>(memory), max_instructions, max_cycles);
}

RunOutcome Cpu::RunWithBudget(memory::AddressSpace& memory, u64 max_instructions,
                              Cycle max_cycles) {
    return RunWithBudgetGeneric(memory, max_instructions, max_cycles);
}

RunOutcome Cpu::RunWithBudget(memory::VirtualMemory& memory, u64 max_instructions,
                              Cycle max_cycles) {
    return RunWithBudgetGeneric(memory, max_instructions, max_cycles);
}

template Result<void> Cpu::RaiseException<memory::AddressSpace>(CpuException ex,
                                                                memory::AddressSpace& mem);
template Result<void> Cpu::RaiseException<memory::VirtualMemory>(CpuException ex,
                                                                 memory::VirtualMemory& mem);

} // namespace xblob::cpu
