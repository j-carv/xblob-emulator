#include "c_api_internal.hpp"
#include "xblob/c_api.h"
#include "xblob/formats/inspector.hpp"
#include "xblob/io/file_source.hpp"
#include "xblob/machine/machine_session.hpp"
#include "xblob/machine/trace_buffer.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <memory>
#include <new>
#include <string>

extern "C" {

xblob_status_t xblob_machine_create(xblob_machine_t* out_machine) {
    if (!out_machine) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    auto session_res = xblob::machine::MachineSession::Create(xblob::memory::kRamSizeRetail);
    if (!session_res) {
        return XBLOB_STATUS_ERROR_INTERNAL;
    }
    auto* machine = new (std::nothrow) xblob_machine_s();
    if (!machine) {
        return XBLOB_STATUS_ERROR_INTERNAL;
    }
    machine->session = std::move(*session_res);
    machine->last_diagnostic.struct_size = sizeof(xblob_prepare_diagnostic_t);
    machine->last_diagnostic.state = XBLOB_MACHINE_STATE_CREATED;
    machine->last_diagnostic.ram_size_bytes = xblob::memory::kRamSizeRetail;
    *out_machine = machine;
    return XBLOB_STATUS_OK;
}

void xblob_machine_destroy(xblob_machine_t machine) {
    delete machine;
}

xblob_status_t xblob_machine_prepare_xbe(xblob_machine_t machine, const char* path_utf8,
                                         size_t path_length,
                                         xblob_prepare_diagnostic_t* out_diagnostic) {
    if (!machine || !path_utf8) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    if (path_length == 0 || !xblob::c_api_internal::is_valid_utf8(path_utf8, path_length)) {
        return XBLOB_STATUS_ERROR_INVALID_ARGUMENT;
    }
    std::filesystem::path fs_path(reinterpret_cast<const char8_t*>(path_utf8),
                                  reinterpret_cast<const char8_t*>(path_utf8 + path_length));
    auto file_res = xblob::FileSource::Open(fs_path);
    if (!file_res) {
        return xblob::c_api_internal::map_error_to_status(file_res.error().code);
    }

    auto insp_res = xblob::MediaInspector::InspectFile(fs_path);
    if (insp_res && insp_res->xbe && insp_res->xbe->certificate) {
        std::strncpy(machine->last_diagnostic.title_name,
                     insp_res->xbe->certificate->title_name.c_str(),
                     sizeof(machine->last_diagnostic.title_name) - 1);
        machine->last_diagnostic.title_id = insp_res->xbe->certificate->title_id;
    }

    auto prep_res = machine->session->Prepare(*file_res.value());
    machine->last_diagnostic.state = static_cast<xblob_machine_state_t>(machine->session->state());
    if (prep_res) {
        machine->last_diagnostic.is_prepared = 1;
        if (machine->session->load_plan()) {
            const auto& plan = *machine->session->load_plan();
            machine->last_diagnostic.entry_point = plan.entry_point;
            machine->last_diagnostic.section_count = static_cast<uint32_t>(plan.sections.size());
            machine->last_diagnostic.headers_size = plan.headers_size;
            machine->last_diagnostic.image_size = plan.image_size;
        }
        machine->last_diagnostic.error_message[0] = '\0';
    } else {
        machine->last_diagnostic.is_prepared = 0;
        std::strncpy(machine->last_diagnostic.error_message, prep_res.error().message.c_str(),
                     sizeof(machine->last_diagnostic.error_message) - 1);
    }

    if (out_diagnostic) {
        if (out_diagnostic->struct_size < sizeof(uint32_t)) {
            return XBLOB_STATUS_ERROR_INCOMPATIBLE_VERSION;
        }
        const size_t copy_size = std::min(static_cast<size_t>(out_diagnostic->struct_size),
                                          sizeof(xblob_prepare_diagnostic_t));
        std::memcpy(out_diagnostic, &machine->last_diagnostic, copy_size);
    }

    if (!prep_res) {
        return xblob::c_api_internal::map_error_to_status(prep_res.error().code);
    }
    return XBLOB_STATUS_OK;
}

xblob_status_t xblob_machine_get_state(const struct xblob_machine_s* machine,
                                       xblob_machine_state_t* out_state) {
    if (!machine || !out_state) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    *out_state = static_cast<xblob_machine_state_t>(machine->session->state());
    return XBLOB_STATUS_OK;
}

xblob_status_t xblob_machine_get_diagnostic(const struct xblob_machine_s* machine,
                                            xblob_prepare_diagnostic_t* out_diagnostic) {
    if (!machine || !out_diagnostic) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    if (out_diagnostic->struct_size < sizeof(uint32_t)) {
        return XBLOB_STATUS_ERROR_INCOMPATIBLE_VERSION;
    }
    const size_t copy_size = std::min(static_cast<size_t>(out_diagnostic->struct_size),
                                      sizeof(xblob_prepare_diagnostic_t));
    std::memcpy(out_diagnostic, &machine->last_diagnostic, copy_size);
    return XBLOB_STATUS_OK;
}

xblob_status_t xblob_machine_step(xblob_machine_t machine, uint64_t instruction_budget,
                                  xblob_machine_execution_result_t* out_result) {
    if (!machine) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    if (out_result && out_result->struct_size < sizeof(uint32_t)) {
        return XBLOB_STATUS_ERROR_INCOMPATIBLE_VERSION;
    }
    auto step_res = machine->session->Step(instruction_budget);
    if (!step_res) {
        return xblob::c_api_internal::map_error_to_status(step_res.error().code);
    }
    machine->trace_buffer.RecordInstruction(machine->session->scheduler().current_cycle(), 1,
                                            machine->session->cpu().context().eip, 0, "");

    if (out_result) {
        xblob_machine_execution_result_t res{};
        res.struct_size = sizeof(xblob_machine_execution_result_t);
        res.state = static_cast<xblob_machine_state_t>(step_res->state);
        res.instructions_executed = step_res->instructions_executed;
        res.cycles_consumed = step_res->cycles_consumed;
        const size_t copy_size =
            std::min(static_cast<size_t>(out_result->struct_size), sizeof(res));
        std::memcpy(out_result, &res, copy_size);
    }
    return XBLOB_STATUS_OK;
}

xblob_status_t xblob_machine_run_diagnostic(xblob_machine_t machine, uint64_t max_instructions,
                                            uint64_t max_cycles,
                                            xblob_machine_execution_result_t* out_result) {
    if (!machine) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    if (out_result && out_result->struct_size < sizeof(uint32_t)) {
        return XBLOB_STATUS_ERROR_INCOMPATIBLE_VERSION;
    }
    auto run_res = machine->session->RunWithBudget(max_instructions, max_cycles);
    if (!run_res) {
        return xblob::c_api_internal::map_error_to_status(run_res.error().code);
    }
    machine->trace_buffer.RecordInstruction(machine->session->scheduler().current_cycle(), 1,
                                            machine->session->cpu().context().eip, 0, "");

    if (out_result) {
        xblob_machine_execution_result_t res{};
        res.struct_size = sizeof(xblob_machine_execution_result_t);
        res.state = static_cast<xblob_machine_state_t>(run_res->state);
        res.instructions_executed = run_res->instructions_executed;
        res.cycles_consumed = run_res->cycles_consumed;
        const size_t copy_size =
            std::min(static_cast<size_t>(out_result->struct_size), sizeof(res));
        std::memcpy(out_result, &res, copy_size);
    }
    return XBLOB_STATUS_OK;
}

xblob_status_t xblob_machine_pause(xblob_machine_t machine) {
    if (!machine) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    auto pause_res = machine->session->Pause();
    if (!pause_res) {
        return xblob::c_api_internal::map_error_to_status(pause_res.error().code);
    }
    return XBLOB_STATUS_OK;
}

xblob_status_t xblob_machine_stop(xblob_machine_t machine) {
    if (!machine) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    auto stop_res = machine->session->Stop();
    if (!stop_res) {
        return xblob::c_api_internal::map_error_to_status(stop_res.error().code);
    }
    return XBLOB_STATUS_OK;
}

xblob_status_t xblob_machine_get_trace_summary(const struct xblob_machine_s* machine,
                                               xblob_trace_summary_t* out_summary) {
    if (!machine || !out_summary) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    if (out_summary->struct_size < sizeof(uint32_t)) {
        return XBLOB_STATUS_ERROR_INCOMPATIBLE_VERSION;
    }
    xblob_trace_summary_t sum{};
    sum.struct_size = sizeof(xblob_trace_summary_t);
    sum.event_count = static_cast<uint64_t>(machine->trace_buffer.size());
    sum.dropped_count = machine->trace_buffer.dropped_count();
    sum.total_recorded = machine->trace_buffer.total_recorded();
    const size_t copy_size = std::min(static_cast<size_t>(out_summary->struct_size), sizeof(sum));
    std::memcpy(out_summary, &sum, copy_size);
    return XBLOB_STATUS_OK;
}

xblob_status_t xblob_machine_clear_trace(xblob_machine_t machine) {
    if (!machine) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    machine->trace_buffer.Clear();
    return XBLOB_STATUS_OK;
}

xblob_status_t xblob_machine_get_frame_metadata(const struct xblob_machine_s* machine,
                                                xblob_frame_metadata_t* out_metadata) {
    if (!machine || !out_metadata) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    if (out_metadata->struct_size < sizeof(uint32_t)) {
        return XBLOB_STATUS_ERROR_INCOMPATIBLE_VERSION;
    }
    const auto meta = machine->session->GetLatestFrameMetadata();
    xblob_frame_metadata_t full{};
    full.struct_size = sizeof(xblob_frame_metadata_t);
    full.is_valid =
        (meta.sequence_number > 0 && meta.buffer_size > 0 && meta.width > 0 && meta.height > 0) ? 1
                                                                                                : 0;
    full.width = full.is_valid ? meta.width : 0;
    full.height = full.is_valid ? meta.height : 0;
    full.pitch = full.is_valid ? meta.pitch : 0;
    full.pixel_format = static_cast<uint32_t>(meta.pixel_format);
    full.sequence_number = meta.sequence_number;
    full.frame_cycle = meta.frame_cycle;
    full.buffer_size = full.is_valid ? meta.buffer_size : 0;

    const size_t copy_size =
        std::min(static_cast<size_t>(out_metadata->struct_size), sizeof(xblob_frame_metadata_t));
    std::memcpy(out_metadata, &full, copy_size);
    return XBLOB_STATUS_OK;
}

xblob_status_t xblob_machine_copy_frame_pixels(const struct xblob_machine_s* machine,
                                               uint8_t* buffer, size_t* inout_buffer_size) {
    if (!machine || !inout_buffer_size) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    const auto meta = machine->session->GetLatestFrameMetadata();
    if (meta.sequence_number == 0 || meta.width == 0 || meta.height == 0) {
        *inout_buffer_size = 0;
        return XBLOB_STATUS_OK;
    }

    const size_t required_size = static_cast<size_t>(meta.buffer_size);

    if (!buffer) {
        *inout_buffer_size = required_size;
        return XBLOB_STATUS_OK;
    }

    if (*inout_buffer_size < required_size) {
        *inout_buffer_size = required_size;
        return XBLOB_STATUS_ERROR_BUFFER_TOO_SMALL;
    }

    if (required_size == 0) {
        *inout_buffer_size = 0;
        return XBLOB_STATUS_OK;
    }

    auto copy_res =
        machine->session->CopyLatestFrame(std::span<xblob::u8>(buffer, *inout_buffer_size));
    if (!copy_res) {
        return xblob::c_api_internal::map_error_to_status(copy_res.error().code);
    }

    *inout_buffer_size = *copy_res;
    return XBLOB_STATUS_OK;
}

static xblob_stop_reason_code_t map_stop_reason_code(xblob::machine::StopReasonCode c) {
    switch (c) {
    case xblob::machine::StopReasonCode::None:
        return XBLOB_STOP_REASON_NONE;
    case xblob::machine::StopReasonCode::Paused:
        return XBLOB_STOP_REASON_PAUSED;
    case xblob::machine::StopReasonCode::StepCompleted:
        return XBLOB_STOP_REASON_STEP_COMPLETED;
    case xblob::machine::StopReasonCode::Halted:
        return XBLOB_STOP_REASON_HALTED;
    case xblob::machine::StopReasonCode::BudgetInstructionsExhausted:
        return XBLOB_STOP_REASON_BUDGET_INSTRUCTIONS;
    case xblob::machine::StopReasonCode::BudgetCyclesExhausted:
        return XBLOB_STOP_REASON_BUDGET_CYCLES;
    case xblob::machine::StopReasonCode::BudgetWallTimeExhausted:
        return XBLOB_STOP_REASON_BUDGET_WALL_TIME;
    case xblob::machine::StopReasonCode::BudgetEventsExhausted:
        return XBLOB_STOP_REASON_BUDGET_EVENTS;
    case xblob::machine::StopReasonCode::WatchdogTimeout:
        return XBLOB_STOP_REASON_WATCHDOG_TIMEOUT;
    case xblob::machine::StopReasonCode::UnsupportedOpcode:
        return XBLOB_STOP_REASON_UNSUPPORTED_OPCODE;
    case xblob::machine::StopReasonCode::UnsupportedExport:
        return XBLOB_STOP_REASON_UNSUPPORTED_EXPORT;
    case xblob::machine::StopReasonCode::UnsupportedGpuMethod:
        return XBLOB_STOP_REASON_UNSUPPORTED_GPU_METHOD;
    case xblob::machine::StopReasonCode::UnsupportedFileService:
        return XBLOB_STOP_REASON_UNSUPPORTED_FILE_SERVICE;
    case xblob::machine::StopReasonCode::CpuException:
        return XBLOB_STOP_REASON_CPU_EXCEPTION;
    case xblob::machine::StopReasonCode::MemoryFault:
        return XBLOB_STOP_REASON_MEMORY_FAULT;
    case xblob::machine::StopReasonCode::InternalError:
    default:
        return XBLOB_STOP_REASON_INTERNAL_ERROR;
    }
}

xblob_status_t xblob_machine_start_execution(xblob_machine_t machine,
                                             const xblob_execution_budgets_t* budgets) {
    if (!machine) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    xblob::machine::ExecutionBudgets b{};
    if (budgets) {
        if (budgets->struct_size < sizeof(uint32_t)) {
            return XBLOB_STATUS_ERROR_INCOMPATIBLE_VERSION;
        }
        b.max_instructions = budgets->max_instructions;
        b.max_cycles = budgets->max_cycles;
        b.max_wall_time_ms = budgets->max_wall_time_ms;
        b.max_events = budgets->max_events;
        b.chunk_instructions = budgets->chunk_instructions;
    }
    auto start_res = machine->session->Start(b);
    if (!start_res) {
        return xblob::c_api_internal::map_error_to_status(start_res.error().code);
    }
    return XBLOB_STATUS_OK;
}

xblob_status_t xblob_machine_resume_execution(xblob_machine_t machine,
                                              const xblob_execution_budgets_t* budgets) {
    if (!machine) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    xblob::machine::ExecutionBudgets b{};
    if (budgets) {
        if (budgets->struct_size < sizeof(uint32_t)) {
            return XBLOB_STATUS_ERROR_INCOMPATIBLE_VERSION;
        }
        b.max_instructions = budgets->max_instructions;
        b.max_cycles = budgets->max_cycles;
        b.max_wall_time_ms = budgets->max_wall_time_ms;
        b.max_events = budgets->max_events;
        b.chunk_instructions = budgets->chunk_instructions;
    }
    auto resume_res = machine->session->Resume(b);
    if (!resume_res) {
        return xblob::c_api_internal::map_error_to_status(resume_res.error().code);
    }
    return XBLOB_STATUS_OK;
}

xblob_status_t xblob_machine_wait_completion(xblob_machine_t machine, uint32_t timeout_ms,
                                             int* out_completed) {
    if (!machine || !out_completed) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    auto wait_res = machine->session->WaitCompletion(timeout_ms);
    if (!wait_res) {
        return xblob::c_api_internal::map_error_to_status(wait_res.error().code);
    }
    *out_completed = *wait_res ? 1 : 0;
    return XBLOB_STATUS_OK;
}

xblob_status_t xblob_machine_get_snapshot(const struct xblob_machine_s* machine,
                                          xblob_machine_snapshot_t* out_snapshot) {
    if (!machine || !out_snapshot) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    if (out_snapshot->struct_size < sizeof(uint32_t)) {
        return XBLOB_STATUS_ERROR_INCOMPATIBLE_VERSION;
    }

    const auto snap = machine->session->GetSnapshot();
    xblob_machine_snapshot_t full{};
    full.struct_size = sizeof(xblob_machine_snapshot_t);
    full.state = static_cast<xblob_machine_state_t>(snap.state_val);
    full.stop_reason_code = map_stop_reason_code(snap.last_stop_reason.code);
    full.fault_eip = snap.last_stop_reason.fault_eip;
    full.active_thread_id = snap.active_thread_id;
    full.thread_count = snap.thread_count;
    full.current_cycle = snap.current_cycle;
    full.instructions_executed = snap.instructions_executed;
    full.events_fired = snap.events_fired;

    full.registers.struct_size = sizeof(xblob_cpu_registers_snapshot_t);
    full.registers.eax = snap.cpu_context.GetGpr(xblob::cpu::Reg32::EAX);
    full.registers.ecx = snap.cpu_context.GetGpr(xblob::cpu::Reg32::ECX);
    full.registers.edx = snap.cpu_context.GetGpr(xblob::cpu::Reg32::EDX);
    full.registers.ebx = snap.cpu_context.GetGpr(xblob::cpu::Reg32::EBX);
    full.registers.esp = snap.cpu_context.GetGpr(xblob::cpu::Reg32::ESP);
    full.registers.ebp = snap.cpu_context.GetGpr(xblob::cpu::Reg32::EBP);
    full.registers.esi = snap.cpu_context.GetGpr(xblob::cpu::Reg32::ESI);
    full.registers.edi = snap.cpu_context.GetGpr(xblob::cpu::Reg32::EDI);
    full.registers.eip = snap.cpu_context.eip;
    full.registers.eflags = snap.eflags_raw;

    full.stack_valid = snap.stack_valid ? 1 : 0;
    for (std::size_t i = 0; i < snap.stack_words.size() && i < 8; ++i) {
        full.stack_words[i] = snap.stack_words[i];
    }

    std::strncpy(full.stop_reason_category, snap.last_stop_reason.category.c_str(),
                 sizeof(full.stop_reason_category) - 1);
    std::strncpy(full.stop_reason_symbol, snap.last_stop_reason.symbol_or_mnemonic.c_str(),
                 sizeof(full.stop_reason_symbol) - 1);
    std::strncpy(full.stop_reason_detail, snap.last_stop_reason.detail.c_str(),
                 sizeof(full.stop_reason_detail) - 1);
    std::strncpy(full.error_message, snap.error_message.c_str(), sizeof(full.error_message) - 1);

    const size_t copy_size =
        std::min(static_cast<size_t>(out_snapshot->struct_size), sizeof(xblob_machine_snapshot_t));
    std::memcpy(out_snapshot, &full, copy_size);
    return XBLOB_STATUS_OK;
}

xblob_status_t
xblob_machine_get_compatibility_diagnostic(const struct xblob_machine_s* machine,
                                           xblob_compatibility_diagnostic_t* out_diagnostic) {
    if (!machine || !out_diagnostic) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    if (out_diagnostic->struct_size < sizeof(uint32_t)) {
        return XBLOB_STATUS_ERROR_INCOMPATIBLE_VERSION;
    }

    const auto diag = machine->session->GetCompatibilityDiagnostic();
    xblob_compatibility_diagnostic_t full{};
    full.struct_size = sizeof(xblob_compatibility_diagnostic_t);
    full.first_blocker_code = map_stop_reason_code(diag.first_blocker.code);
    full.blocker_ordinal_or_opcode = diag.first_blocker.ordinal_or_opcode;
    full.blocker_thread_id = diag.first_blocker.thread_id;
    full.blocker_eip = diag.first_blocker.fault_eip;
    full.blocker_count = diag.first_blocker.count;
    std::strncpy(full.blocker_category, diag.first_blocker.category.c_str(),
                 sizeof(full.blocker_category) - 1);
    std::strncpy(full.blocker_symbol_or_mnemonic, diag.first_blocker.symbol_or_mnemonic.c_str(),
                 sizeof(full.blocker_symbol_or_mnemonic) - 1);
    std::strncpy(full.blocker_detail, diag.first_blocker.detail.c_str(),
                 sizeof(full.blocker_detail) - 1);
    full.total_instructions = diag.total_instructions;
    full.total_cycles = diag.total_cycles;

    const size_t copy_size = std::min(static_cast<size_t>(out_diagnostic->struct_size),
                                      sizeof(xblob_compatibility_diagnostic_t));
    std::memcpy(out_diagnostic, &full, copy_size);
    return XBLOB_STATUS_OK;
}

xblob_status_t xblob_machine_get_trace_text(const struct xblob_machine_s* machine, char* buffer,
                                            size_t* inout_buffer_size) {
    if (!machine || !inout_buffer_size) {
        return XBLOB_STATUS_ERROR_NULL_POINTER;
    }
    const std::string text = machine->session->trace_buffer().FormatText();
    return xblob::c_api_internal::copy_string_to_two_call_buffer(text, buffer, inout_buffer_size);
}

} // extern "C"
