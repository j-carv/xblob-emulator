#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/gpu/pushbuffer_types.hpp"

#include <functional>
#include <span>

namespace xblob::gpu {

class PushbufferDecoder {
public:
    using ReadWordFn = std::function<Result<u32>(GuestAddr addr)>;

    // Decode from memory word-fetch function
    static Result<PushbufferPacket> DecodeFromMemory(GuestAddr current_addr,
                                                     const ReadWordFn& reader, u32 words_available);

    // Decode sequentially from contiguous word span
    static Result<PushbufferPacket> DecodeFromSpan(std::span<const u32> words,
                                                   std::size_t& inout_offset);
};

} // namespace xblob::gpu
