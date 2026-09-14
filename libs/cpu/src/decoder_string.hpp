#pragma once

#include "instruction_stream.hpp"
#include "xblob/cpu/instructions.hpp"

#include <optional>

namespace xblob::cpu::detail {

template <typename MemoryType>
Result<std::optional<DecodedInstruction>>
TryDecodeString(u8 opcode, InstructionStream<MemoryType>& stream, const CpuContext& ctx,
                std::optional<UnsupportedFormInfo>* out_unsupported) {
    (void)ctx;
    (void)out_unsupported;

    if (opcode == opcodes::kCld) {
        DecodedInstruction inst;
        inst.id = InstructionId::Cld;
        inst.cycles = 1;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }
    if (opcode == opcodes::kStd) {
        DecodedInstruction inst;
        inst.id = InstructionId::Std;
        inst.cycles = 1;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    if (opcode == opcodes::kMovsb || opcode == opcodes::kMovsd) {
        DecodedInstruction inst;
        inst.id = InstructionId::Movs;
        inst.data_size = (opcode == opcodes::kMovsb) ? 1 : 4;
        inst.cycles = cycles::kStringOp;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    if (opcode == opcodes::kCmpsb || opcode == opcodes::kCmpsd) {
        DecodedInstruction inst;
        inst.id = InstructionId::Cmps;
        inst.data_size = (opcode == opcodes::kCmpsb) ? 1 : 4;
        inst.cycles = cycles::kStringOp;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    if (opcode == opcodes::kStosb || opcode == opcodes::kStosd) {
        DecodedInstruction inst;
        inst.id = InstructionId::Stos;
        inst.data_size = (opcode == opcodes::kStosb) ? 1 : 4;
        inst.cycles = cycles::kStringOp;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    if (opcode == opcodes::kLodsb || opcode == opcodes::kLodsd) {
        DecodedInstruction inst;
        inst.id = InstructionId::Lods;
        inst.data_size = (opcode == opcodes::kLodsb) ? 1 : 4;
        inst.cycles = cycles::kStringOp;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    if (opcode == opcodes::kScasb || opcode == opcodes::kScasd) {
        DecodedInstruction inst;
        inst.id = InstructionId::Scas;
        inst.data_size = (opcode == opcodes::kScasb) ? 1 : 4;
        inst.cycles = cycles::kStringOp;
        inst.length = stream.length();
        return std::optional<DecodedInstruction>(inst);
    }

    return std::optional<DecodedInstruction>(std::nullopt);
}

} // namespace xblob::cpu::detail
