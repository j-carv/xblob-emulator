#include "xblob/cpu/executor.hpp"
#include "xblob/memory/address_space.hpp"
#include "xblob/memory/virtual_memory.hpp"

namespace xblob::cpu {

namespace detail {

template <typename MemoryType>
Result<ExecutionResult> ExecuteData(const DecodedInstruction& inst, CpuContext& ctx,
                                    MemoryType& mem) {
    ExecutionResult res;
    if (inst.id == InstructionId::Mov) {
        auto val_res = inst.op2.Read(ctx, mem);
        if (!val_res) {
            return val_res.error();
        }
        auto write_res = inst.op1.Write(ctx, mem, *val_res);
        if (!write_res) {
            return write_res.error();
        }
        return res;
    }

    if (inst.id == InstructionId::Lea) {
        auto write_res = inst.op1.Write(ctx, mem, inst.op2.immediate);
        if (!write_res) {
            return write_res.error();
        }
        return res;
    }

    return Error{ErrorCode::InvalidArgument, "Instrução de dados não reconhecida"};
}

} // namespace detail

// Template instantiations
template Result<ExecutionResult>
detail::ExecuteData<memory::AddressSpace>(const DecodedInstruction& inst, CpuContext& ctx,
                                          memory::AddressSpace& mem);

template Result<ExecutionResult>
detail::ExecuteData<memory::VirtualMemory>(const DecodedInstruction& inst, CpuContext& ctx,
                                           memory::VirtualMemory& mem);

} // namespace xblob::cpu
