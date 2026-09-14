#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/cpu/registers.hpp"

namespace xblob::cpu {

enum class OperandKind : u8 {
    None = 0,
    Register = 1,
    Immediate = 2,
    Memory = 3,
};

struct Operand {
    OperandKind kind{OperandKind::None};
    Reg32 reg{Reg32::EAX};
    u8 reg8{0};
    u32 immediate{0};
    GuestAddr mem_addr{0};
    u8 size_bytes{4};

    [[nodiscard]] static constexpr Operand MakeReg(Reg32 r, u8 size = 4) noexcept {
        Operand op;
        op.kind = OperandKind::Register;
        op.reg = r;
        op.reg8 = static_cast<u8>(r);
        op.size_bytes = size;
        return op;
    }

    [[nodiscard]] static constexpr Operand MakeReg8(u8 r8) noexcept {
        Operand op;
        op.kind = OperandKind::Register;
        op.reg = static_cast<Reg32>(r8 & 3);
        op.reg8 = r8;
        op.size_bytes = 1;
        return op;
    }

    [[nodiscard]] static constexpr Operand MakeReg16(Reg32 r) noexcept {
        Operand op;
        op.kind = OperandKind::Register;
        op.reg = r;
        op.reg8 = static_cast<u8>(r);
        op.size_bytes = 2;
        return op;
    }

    [[nodiscard]] static constexpr Operand MakeImm(u32 val, u8 size = 4) noexcept {
        Operand op;
        op.kind = OperandKind::Immediate;
        op.immediate = val;
        op.size_bytes = size;
        return op;
    }

    [[nodiscard]] static constexpr Operand MakeMem(GuestAddr addr, u8 size = 4) noexcept {
        Operand op;
        op.kind = OperandKind::Memory;
        op.mem_addr = addr;
        op.size_bytes = size;
        return op;
    }

    template <typename MemoryType>
    [[nodiscard]] Result<u32> Read(const CpuContext& ctx, MemoryType& mem) const {
        switch (kind) {
        case OperandKind::Register:
            if (size_bytes == 1) {
                return static_cast<u32>(ctx.GetGpr8(reg8));
            }
            if (size_bytes == 2) {
                return static_cast<u32>(ctx.GetGpr16(reg));
            }
            return ctx.GetGpr(reg);
        case OperandKind::Immediate:
            if (size_bytes == 1) {
                return immediate & 0xFFU;
            }
            if (size_bytes == 2) {
                return immediate & 0xFFFFU;
            }
            return immediate;
        case OperandKind::Memory: {
            if (size_bytes == 1) {
                auto res = mem.Read8(mem_addr);
                if (!res) {
                    return res.error();
                }
                return static_cast<u32>(*res);
            }
            if (size_bytes == 2) {
                auto res = mem.Read16(mem_addr);
                if (!res) {
                    return res.error();
                }
                return static_cast<u32>(*res);
            }
            auto res = mem.Read32(mem_addr);
            if (!res) {
                return res.error();
            }
            return *res;
        }
        case OperandKind::None:
            return Error{ErrorCode::InvalidArgument, "Tentativa de ler operando nulo"};
        }
        return Error{ErrorCode::InvalidArgument, "Tipo de operando desconhecido"};
    }

    template <typename MemoryType>
    [[nodiscard]] Result<void> Write(CpuContext& ctx, MemoryType& mem, u32 value) const {
        switch (kind) {
        case OperandKind::Register:
            if (size_bytes == 1) {
                ctx.SetGpr8(reg8, static_cast<u8>(value & 0xFF));
                return {};
            }
            if (size_bytes == 2) {
                ctx.SetGpr16(reg, static_cast<u16>(value & 0xFFFF));
                return {};
            }
            ctx.SetGpr(reg, value);
            return {};
        case OperandKind::Memory: {
            if (size_bytes == 1) {
                return mem.Write8(mem_addr, static_cast<u8>(value & 0xFF));
            }
            if (size_bytes == 2) {
                return mem.Write16(mem_addr, static_cast<u16>(value & 0xFFFF));
            }
            return mem.Write32(mem_addr, value);
        }
        case OperandKind::Immediate:
        case OperandKind::None:
            return Error{ErrorCode::InvalidArgument, "Operando não gravável"};
        }
        return Error{ErrorCode::InvalidArgument, "Tipo de operando desconhecido"};
    }
};

} // namespace xblob::cpu
