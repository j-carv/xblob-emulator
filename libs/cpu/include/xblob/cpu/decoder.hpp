#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/cpu/instruction.hpp"
#include "xblob/cpu/registers.hpp"

namespace xblob::cpu {

class Decoder {
public:
    template <typename MemoryType>
    static Result<DecodedInstruction> Decode(const CpuContext& ctx, MemoryType& mem, GuestAddr eip);
};

} // namespace xblob::cpu
