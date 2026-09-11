#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"

#include <functional>

namespace xblob::memory {

enum class AccessWidth : u8 { Byte = 1, Word = 2, Dword = 4 };

using MmioReadCallback = std::function<Result<u32>(u32 offset, AccessWidth width)>;
using MmioWriteCallback = std::function<Result<void>(u32 offset, AccessWidth width, u32 value)>;

struct MmioHandler {
    MmioReadCallback read;
    MmioWriteCallback write;
};

} // namespace xblob::memory
