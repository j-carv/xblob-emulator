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

} // extern "C"
