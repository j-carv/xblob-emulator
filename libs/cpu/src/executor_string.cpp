#include "xblob/cpu/executor.hpp"
#include "xblob/cpu/flags.hpp"
#include "xblob/memory/address_space.hpp"
#include "xblob/memory/virtual_memory.hpp"

namespace xblob::cpu::detail {

template <typename MemoryType>
Result<ExecutionResult> ExecuteString(const DecodedInstruction& inst, CpuContext& ctx,
                                      MemoryType& mem, u32 current_eip) {
    if (inst.id == InstructionId::Cld) {
        ctx.SetFlag(kFlagDF, false);
        return ExecutionResult{};
    }
    if (inst.id == InstructionId::Std) {
        ctx.SetFlag(kFlagDF, true);
        return ExecutionResult{};
    }

    const bool has_rep = inst.rep || inst.repne;
    if (has_rep) {
        const u32 ecx = ctx.GetGpr(Reg32::ECX);
        if (ecx == 0) {
            // Count already zero: REP completes immediately without memory operation
            return ExecutionResult{};
        }
    }

    const bool df = ctx.GetFlag(kFlagDF);
    const u8 size = inst.data_size;
    const int32_t step = df ? -static_cast<int32_t>(size) : static_cast<int32_t>(size);

    bool should_terminate_cc = false;

    switch (inst.id) {
    case InstructionId::Movs: {
        const GuestAddr src_addr = ctx.GetGpr(Reg32::ESI);
        const GuestAddr dst_addr = ctx.GetGpr(Reg32::EDI);

        if (size == 1) {
            auto r_res = mem.Read8(src_addr);
            if (!r_res)
                return r_res.error();
            auto w_res = mem.Write8(dst_addr, *r_res);
            if (!w_res)
                return w_res.error();
        } else {
            auto r_res = mem.Read32(src_addr);
            if (!r_res)
                return r_res.error();
            auto w_res = mem.Write32(dst_addr, *r_res);
            if (!w_res)
                return w_res.error();
        }

        ctx.SetGpr(Reg32::ESI, src_addr + static_cast<u32>(step));
        ctx.SetGpr(Reg32::EDI, dst_addr + static_cast<u32>(step));
        break;
    }
    case InstructionId::Stos: {
        const GuestAddr dst_addr = ctx.GetGpr(Reg32::EDI);

        if (size == 1) {
            const u8 al = ctx.GetGpr8(0);
            auto w_res = mem.Write8(dst_addr, al);
            if (!w_res)
                return w_res.error();
        } else {
            const u32 eax = ctx.GetGpr(Reg32::EAX);
            auto w_res = mem.Write32(dst_addr, eax);
            if (!w_res)
                return w_res.error();
        }

        ctx.SetGpr(Reg32::EDI, dst_addr + static_cast<u32>(step));
        break;
    }
    case InstructionId::Lods: {
        const GuestAddr src_addr = ctx.GetGpr(Reg32::ESI);

        if (size == 1) {
            auto r_res = mem.Read8(src_addr);
            if (!r_res)
                return r_res.error();
            ctx.SetGpr8(0, *r_res);
        } else {
            auto r_res = mem.Read32(src_addr);
            if (!r_res)
                return r_res.error();
            ctx.SetGpr(Reg32::EAX, *r_res);
        }

        ctx.SetGpr(Reg32::ESI, src_addr + static_cast<u32>(step));
        break;
    }
    case InstructionId::Cmps: {
        const GuestAddr s1_addr = ctx.GetGpr(Reg32::ESI);
        const GuestAddr s2_addr = ctx.GetGpr(Reg32::EDI);

        if (size == 1) {
            auto r1 = mem.Read8(s1_addr);
            if (!r1)
                return r1.error();
            auto r2 = mem.Read8(s2_addr);
            if (!r2)
                return r2.error();
            ctx.eflags = CalculateSubFlags8(static_cast<u8>(*r1), static_cast<u8>(*r2), ctx.eflags);
        } else {
            auto r1 = mem.Read32(s1_addr);
            if (!r1)
                return r1.error();
            auto r2 = mem.Read32(s2_addr);
            if (!r2)
                return r2.error();
            ctx.eflags = CalculateSubFlags(*r1, *r2, ctx.eflags);
        }

        ctx.SetGpr(Reg32::ESI, s1_addr + static_cast<u32>(step));
        ctx.SetGpr(Reg32::EDI, s2_addr + static_cast<u32>(step));

        const bool zf = ctx.GetFlag(kFlagZF);
        if (inst.rep && !zf) {
            should_terminate_cc = true;
        } else if (inst.repne && zf) {
            should_terminate_cc = true;
        }
        break;
    }
    case InstructionId::Scas: {
        const GuestAddr dst_addr = ctx.GetGpr(Reg32::EDI);

        if (size == 1) {
            const u8 al = ctx.GetGpr8(0);
            auto r = mem.Read8(dst_addr);
            if (!r)
                return r.error();
            ctx.eflags = CalculateSubFlags8(al, static_cast<u8>(*r), ctx.eflags);
        } else {
            const u32 eax = ctx.GetGpr(Reg32::EAX);
            auto r = mem.Read32(dst_addr);
            if (!r)
                return r.error();
            ctx.eflags = CalculateSubFlags(eax, *r, ctx.eflags);
        }

        ctx.SetGpr(Reg32::EDI, dst_addr + static_cast<u32>(step));

        const bool zf = ctx.GetFlag(kFlagZF);
        if (inst.rep && !zf) {
            should_terminate_cc = true;
        } else if (inst.repne && zf) {
            should_terminate_cc = true;
        }
        break;
    }
    default:
        return Error{ErrorCode::InvalidOpcode, "Operação de string desconhecida"};
    }

    if (has_rep) {
        const u32 remaining_ecx = ctx.GetGpr(Reg32::ECX) - 1U;
        ctx.SetGpr(Reg32::ECX, remaining_ecx);

        if (remaining_ecx > 0 && !should_terminate_cc) {
            // Keep EIP pointing at current instruction so it resumes deterministically
            ExecutionResult res;
            res.branched = true;
            res.next_eip = current_eip;
            return res;
        }
    }

    return ExecutionResult{};
}

template Result<ExecutionResult> ExecuteString<memory::AddressSpace>(const DecodedInstruction& inst,
                                                                     CpuContext& ctx,
                                                                     memory::AddressSpace& mem,
                                                                     u32 current_eip);
template Result<ExecutionResult>
ExecuteString<memory::VirtualMemory>(const DecodedInstruction& inst, CpuContext& ctx,
                                     memory::VirtualMemory& mem, u32 current_eip);

} // namespace xblob::cpu::detail
