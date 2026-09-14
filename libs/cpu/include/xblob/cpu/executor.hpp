#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/cpu/exceptions.hpp"
#include "xblob/cpu/instruction.hpp"
#include "xblob/cpu/registers.hpp"

namespace xblob::cpu {

struct ExecutionResult {
    bool halted{false};
    bool branched{false};
    u32 next_eip{0};
    std::optional<CpuException> exception{std::nullopt};
};

using TrapHandler = std::function<Result<bool>(u8 vector, CpuContext& ctx)>;

namespace detail {

template <typename MemoryType>
Result<ExecutionResult> ExecuteData(const DecodedInstruction& inst, CpuContext& ctx,
                                    MemoryType& mem);

template <typename MemoryType>
Result<ExecutionResult> ExecuteAlu(const DecodedInstruction& inst, CpuContext& ctx,
                                   MemoryType& mem);

template <typename MemoryType>
Result<ExecutionResult> ExecuteShift(const DecodedInstruction& inst, CpuContext& ctx,
                                     MemoryType& mem);

template <typename MemoryType>
Result<ExecutionResult> ExecuteMulDiv(const DecodedInstruction& inst, CpuContext& ctx,
                                      MemoryType& mem, u32 current_eip);

template <typename MemoryType>
Result<ExecutionResult> ExecuteBitExt(const DecodedInstruction& inst, CpuContext& ctx,
                                      MemoryType& mem);

template <typename MemoryType>
Result<ExecutionResult> ExecuteString(const DecodedInstruction& inst, CpuContext& ctx,
                                      MemoryType& mem, u32 current_eip);

template <typename MemoryType>
Result<ExecutionResult> ExecuteAtomic(const DecodedInstruction& inst, CpuContext& ctx,
                                      MemoryType& mem);

template <typename MemoryType>
Result<ExecutionResult> ExecuteStack(const DecodedInstruction& inst, CpuContext& ctx,
                                     MemoryType& mem, u32 current_eip);

template <typename MemoryType>
Result<ExecutionResult> ExecuteBranch(const DecodedInstruction& inst, CpuContext& ctx,
                                      MemoryType& mem, u32 current_eip);

template <typename MemoryType>
Result<ExecutionResult> ExecuteInterruptInstruction(const DecodedInstruction& inst, CpuContext& ctx,
                                                    MemoryType& mem, u32 current_eip,
                                                    const TrapHandler& trap = nullptr);

} // namespace detail

class Executor {
public:
    template <typename MemoryType>
    static Result<ExecutionResult> Execute(const DecodedInstruction& inst, CpuContext& ctx,
                                           MemoryType& mem, u32 current_eip,
                                           const TrapHandler& trap = nullptr);

    template <typename MemoryType>
    static Result<void> DeliverGate(u8 vector, CpuContext& ctx, MemoryType& mem,
                                    std::optional<u32> error_code, bool is_software_int,
                                    u32 return_eip);

    template <typename MemoryType>
    static Result<void> ExecuteIret(CpuContext& ctx, MemoryType& mem);
};

} // namespace xblob::cpu
