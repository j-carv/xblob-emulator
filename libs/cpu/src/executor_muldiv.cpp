#include "xblob/cpu/executor.hpp"
#include "xblob/cpu/flags.hpp"
#include "xblob/memory/address_space.hpp"
#include "xblob/memory/virtual_memory.hpp"

#include <cstdint>
#include <limits>

namespace xblob::cpu::detail {

template <typename MemoryType>
Result<ExecutionResult> ExecuteMulDiv(const DecodedInstruction& inst, CpuContext& ctx,
                                      MemoryType& mem, u32 current_eip) {
    if (inst.id == InstructionId::Mul) {
        auto src_res = inst.op1.Read(ctx, mem);
        if (!src_res) {
            return src_res.error();
        }
        if (inst.data_size == 1) {
            const u16 product =
                static_cast<u16>(ctx.GetGpr8(0)) * static_cast<u16>(*src_res & 0xFFU);
            ctx.SetGpr16(Reg32::EAX, product);
            const bool overflow = (product >> 8) != 0;
            ctx.SetFlag(kFlagCF, overflow);
            ctx.SetFlag(kFlagOF, overflow);
        } else {
            const u64 product =
                static_cast<u64>(ctx.GetGpr(Reg32::EAX)) * static_cast<u64>(*src_res);
            ctx.SetGpr(Reg32::EAX, static_cast<u32>(product & 0xFFFFFFFFULL));
            ctx.SetGpr(Reg32::EDX, static_cast<u32>(product >> 32));
            const bool overflow = (ctx.GetGpr(Reg32::EDX) != 0);
            ctx.SetFlag(kFlagCF, overflow);
            ctx.SetFlag(kFlagOF, overflow);
        }
        return ExecutionResult{};
    }

    if (inst.id == InstructionId::Imul) {
        // Form 1: 3-operand IMUL r32, r/m32, imm
        if (inst.op3.kind != OperandKind::None) {
            auto src_res = inst.op2.Read(ctx, mem);
            if (!src_res) {
                return src_res.error();
            }
            auto imm_res = inst.op3.Read(ctx, mem);
            if (!imm_res) {
                return imm_res.error();
            }
            const auto s_src = static_cast<int64_t>(static_cast<int32_t>(*src_res));
            const auto s_imm = static_cast<int64_t>(static_cast<int32_t>(*imm_res));
            const int64_t product = s_src * s_imm;
            const auto s_res32 = static_cast<int32_t>(product);

            auto write_res = inst.op1.Write(ctx, mem, static_cast<u32>(s_res32));
            if (!write_res) {
                return write_res.error();
            }
            const bool overflow = (product != static_cast<int64_t>(s_res32));
            ctx.SetFlag(kFlagCF, overflow);
            ctx.SetFlag(kFlagOF, overflow);
            return ExecutionResult{};
        }

        // Form 2: 2-operand IMUL r32, r/m32
        if (inst.op2.kind != OperandKind::None) {
            auto dst_res = inst.op1.Read(ctx, mem);
            if (!dst_res) {
                return dst_res.error();
            }
            auto src_res = inst.op2.Read(ctx, mem);
            if (!src_res) {
                return src_res.error();
            }
            const auto s_dst = static_cast<int64_t>(static_cast<int32_t>(*dst_res));
            const auto s_src = static_cast<int64_t>(static_cast<int32_t>(*src_res));
            const int64_t product = s_dst * s_src;
            const auto s_res32 = static_cast<int32_t>(product);

            auto write_res = inst.op1.Write(ctx, mem, static_cast<u32>(s_res32));
            if (!write_res) {
                return write_res.error();
            }
            const bool overflow = (product != static_cast<int64_t>(s_res32));
            ctx.SetFlag(kFlagCF, overflow);
            ctx.SetFlag(kFlagOF, overflow);
            return ExecutionResult{};
        }

        // Form 3: 1-operand IMUL r/m32
        auto src_res = inst.op1.Read(ctx, mem);
        if (!src_res) {
            return src_res.error();
        }
        const auto s_eax = static_cast<int64_t>(static_cast<int32_t>(ctx.GetGpr(Reg32::EAX)));
        const auto s_src = static_cast<int64_t>(static_cast<int32_t>(*src_res));
        const int64_t product = s_eax * s_src;
        const auto s_res32 = static_cast<int32_t>(product);

        ctx.SetGpr(Reg32::EAX, static_cast<u32>(static_cast<uint64_t>(product) & 0xFFFFFFFFULL));
        ctx.SetGpr(Reg32::EDX, static_cast<u32>(static_cast<uint64_t>(product) >> 32));

        const bool overflow = (product != static_cast<int64_t>(s_res32));
        ctx.SetFlag(kFlagCF, overflow);
        ctx.SetFlag(kFlagOF, overflow);
        return ExecutionResult{};
    }

    if (inst.id == InstructionId::Div) {
        auto src_res = inst.op1.Read(ctx, mem);
        if (!src_res) {
            return src_res.error();
        }
        const u32 divisor = *src_res;
        if (divisor == 0) {
            // Divide by zero: #DE exception, no mutation to registers
            ExecutionResult res;
            res.exception =
                CpuException{ExceptionVector::DivideError, std::nullopt, current_eip, 0};
            return res;
        }

        const u64 dividend = (static_cast<u64>(ctx.GetGpr(Reg32::EDX)) << 32) |
                             static_cast<u64>(ctx.GetGpr(Reg32::EAX));
        const u64 quotient = dividend / divisor;
        const u64 remainder = dividend % divisor;

        if (quotient > 0xFFFFFFFFULL) {
            // Quotient overflow: #DE exception, atomic context preservation
            ExecutionResult res;
            res.exception =
                CpuException{ExceptionVector::DivideError, std::nullopt, current_eip, 0};
            return res;
        }

        ctx.SetGpr(Reg32::EAX, static_cast<u32>(quotient));
        ctx.SetGpr(Reg32::EDX, static_cast<u32>(remainder));
        return ExecutionResult{};
    }

    if (inst.id == InstructionId::Idiv) {
        auto src_res = inst.op1.Read(ctx, mem);
        if (!src_res) {
            return src_res.error();
        }
        const auto divisor = static_cast<int32_t>(*src_res);
        if (divisor == 0) {
            // Divide by zero: #DE
            ExecutionResult res;
            res.exception =
                CpuException{ExceptionVector::DivideError, std::nullopt, current_eip, 0};
            return res;
        }

        const auto dividend =
            static_cast<int64_t>((static_cast<u64>(ctx.GetGpr(Reg32::EDX)) << 32) |
                                 static_cast<u64>(ctx.GetGpr(Reg32::EAX)));

        // Guard against host UB: INT64_MIN / -1
        if (dividend == std::numeric_limits<int64_t>::min() && divisor == -1) {
            ExecutionResult res;
            res.exception =
                CpuException{ExceptionVector::DivideError, std::nullopt, current_eip, 0};
            return res;
        }

        const int64_t quotient = dividend / divisor;
        const int64_t remainder = dividend % divisor;

        if (quotient < std::numeric_limits<int32_t>::min() ||
            quotient > std::numeric_limits<int32_t>::max()) {
            // Signed divide overflow: #DE
            ExecutionResult res;
            res.exception =
                CpuException{ExceptionVector::DivideError, std::nullopt, current_eip, 0};
            return res;
        }

        ctx.SetGpr(Reg32::EAX, static_cast<u32>(static_cast<int32_t>(quotient)));
        ctx.SetGpr(Reg32::EDX, static_cast<u32>(static_cast<int32_t>(remainder)));
        return ExecutionResult{};
    }

    return Error{ErrorCode::InvalidOpcode, "Operação de multiplicação/divisão inválida"};
}

template Result<ExecutionResult> ExecuteMulDiv<memory::AddressSpace>(const DecodedInstruction& inst,
                                                                     CpuContext& ctx,
                                                                     memory::AddressSpace& mem,
                                                                     u32 current_eip);
template Result<ExecutionResult>
ExecuteMulDiv<memory::VirtualMemory>(const DecodedInstruction& inst, CpuContext& ctx,
                                     memory::VirtualMemory& mem, u32 current_eip);

} // namespace xblob::cpu::detail
