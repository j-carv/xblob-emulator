#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/cpu/operand.hpp"
#include "xblob/cpu/registers.hpp"

#include <utility>

namespace xblob::cpu::detail {

template <typename MemoryType>
class InstructionStream {
public:
    InstructionStream(MemoryType& mem, GuestAddr eip) : mem_(mem), eip_(eip) {}

    Result<u8> NextByte() {
        if (length_ >= 15) {
            return Error{ErrorCode::InvalidOpcode,
                         "Instrução excede limite arquitetural de 15 bytes"};
        }
        auto res = mem_.Fetch8(eip_ + length_);
        if (!res) {
            return res.error();
        }
        length_++;
        return *res;
    }

    Result<u32> ReadImm32() {
        auto r0 = NextByte();
        if (!r0)
            return r0.error();
        auto r1 = NextByte();
        if (!r1)
            return r1.error();
        auto r2 = NextByte();
        if (!r2)
            return r2.error();
        auto r3 = NextByte();
        if (!r3)
            return r3.error();
        return static_cast<u32>(*r0) | (static_cast<u32>(*r1) << 8) |
               (static_cast<u32>(*r2) << 16) | (static_cast<u32>(*r3) << 24);
    }

    Result<u16> ReadImm16() {
        auto r0 = NextByte();
        if (!r0)
            return r0.error();
        auto r1 = NextByte();
        if (!r1)
            return r1.error();
        return static_cast<u16>(static_cast<u32>(*r0) | (static_cast<u32>(*r1) << 8));
    }

    Result<int8_t> ReadImm8() {
        auto r = NextByte();
        if (!r)
            return r.error();
        return static_cast<int8_t>(*r);
    }

    [[nodiscard]] u8 length() const noexcept { return length_; }
    [[nodiscard]] GuestAddr start_eip() const noexcept { return eip_; }

private:
    MemoryType& mem_;
    GuestAddr eip_;
    u8 length_{0};
};

template <typename MemoryType>
Result<std::pair<Operand, u8>> ParseModRm(InstructionStream<MemoryType>& stream,
                                          const CpuContext& ctx) {
    auto b_res = stream.NextByte();
    if (!b_res)
        return b_res.error();
    u8 modrm = *b_res;
    u8 mod = (modrm >> 6) & 3;
    u8 reg = (modrm >> 3) & 7;
    u8 rm = modrm & 7;

    if (mod == 3) {
        return std::make_pair(Operand::MakeReg(static_cast<Reg32>(rm)), reg);
    }

    GuestAddr ea = 0;
    if (rm != 4) { // Direct register base, no SIB
        if (mod == 0 && rm == 5) {
            auto disp_res = stream.ReadImm32();
            if (!disp_res)
                return disp_res.error();
            ea = *disp_res;
        } else {
            ea = ctx.GetGpr(static_cast<Reg32>(rm));
            if (mod == 1) {
                auto disp_res = stream.ReadImm8();
                if (!disp_res)
                    return disp_res.error();
                ea += static_cast<u32>(static_cast<int32_t>(*disp_res));
            } else if (mod == 2) {
                auto disp_res = stream.ReadImm32();
                if (!disp_res)
                    return disp_res.error();
                ea += *disp_res;
            }
        }
    } else { // rm == 4: SIB byte follows
        auto sib_res = stream.NextByte();
        if (!sib_res)
            return sib_res.error();
        u8 sib = *sib_res;
        u8 scale = (sib >> 6) & 3;
        u8 index = (sib >> 3) & 7;
        u8 base = sib & 7;

        u32 scaled_index = 0;
        if (index != 4) { // ESP cannot be an index register
            scaled_index = ctx.GetGpr(static_cast<Reg32>(index)) * (1u << scale);
        }

        u32 base_val = 0;
        if (base == 5 && mod == 0) {
            auto disp_res = stream.ReadImm32();
            if (!disp_res)
                return disp_res.error();
            base_val = *disp_res;
        } else {
            base_val = ctx.GetGpr(static_cast<Reg32>(base));
            if (mod == 1) {
                auto disp_res = stream.ReadImm8();
                if (!disp_res)
                    return disp_res.error();
                base_val += static_cast<u32>(static_cast<int32_t>(*disp_res));
            } else if (mod == 2) {
                auto disp_res = stream.ReadImm32();
                if (!disp_res)
                    return disp_res.error();
                base_val += *disp_res;
            }
        }
        ea = base_val + scaled_index;
    }

    return std::make_pair(Operand::MakeMem(ea, 4), reg);
}

} // namespace xblob::cpu::detail
