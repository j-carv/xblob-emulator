#pragma once

#include "xblob/c_api.h"
#include "xblob/common/error.hpp"
#include "xblob/machine/machine_session.hpp"
#include "xblob/machine/trace_buffer.hpp"

#include <cstddef>
#include <memory>
#include <string_view>

struct xblob_machine_s {
    std::unique_ptr<xblob::machine::MachineSession> session;
    xblob_prepare_diagnostic_t last_diagnostic{};
    xblob::machine::TraceRingBuffer trace_buffer{1024};
};

namespace xblob::c_api_internal {

bool is_valid_utf8(const char* data, size_t length) noexcept;
xblob_status_t map_error_to_status(xblob::ErrorCode code) noexcept;
xblob_status_t copy_string_to_two_call_buffer(std::string_view str, char* buffer,
                                              size_t* inout_buffer_size) noexcept;

} // namespace xblob::c_api_internal
