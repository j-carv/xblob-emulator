#include "xblob/gpu/pushbuffer_decoder.hpp"

#include "xblob/common/safe_math.hpp"

namespace xblob::gpu {

Result<PushbufferPacket> PushbufferDecoder::DecodeFromSpan(std::span<const u32> words,
                                                           std::size_t& inout_offset) {
    if (inout_offset >= words.size()) {
        return Error{ErrorCode::UnexpectedEof, "Pushbuffer stream ended before next command"};
    }

    const u32 header = words[inout_offset++];

    // 1. Jump command (bit 0 set)
    if ((header & 0x00000001u) != 0) {
        PushbufferPacket pkt;
        pkt.opcode = PacketOpcode::Jump;
        pkt.jump_target = header & ~0x00000003u;
        return pkt;
    }

    // 2. Method call command
    const u32 prefix = (header >> 30) & 0x03;
    if (prefix != 0b00 && prefix != 0b10) {
        return Error{ErrorCode::InvalidField, "Unknown pushbuffer packet prefix opcode"};
    }

    PushbufferPacket pkt;
    pkt.opcode = PacketOpcode::Method;
    pkt.method = header & 0x00001FFCu;
    pkt.count = (header >> 18) & 0x000007FFu;
    pkt.non_incrementing = (prefix == 0b10) || ((header & (1u << 29)) != 0);

    // Verify all payload words are present (prevent partial writes)
    if (inout_offset + pkt.count > words.size()) {
        return Error{ErrorCode::TruncatedData,
                     "Pushbuffer packet payload ends before declared count"};
    }

    pkt.parameters.reserve(pkt.count);
    for (u32 i = 0; i < pkt.count; ++i) {
        pkt.parameters.push_back(words[inout_offset++]);
    }

    return pkt;
}

Result<PushbufferPacket> PushbufferDecoder::DecodeFromMemory(GuestAddr current_addr,
                                                             const ReadWordFn& reader,
                                                             u32 words_available) {
    if (words_available == 0) {
        return Error{ErrorCode::UnexpectedEof, "No words available in pushbuffer stream"};
    }
    if ((current_addr % 4) != 0) {
        return Error{ErrorCode::InvalidArgument, "Pushbuffer address misaligned", current_addr};
    }

    auto hdr_res = reader(current_addr);
    if (!hdr_res.has_value()) {
        return hdr_res.error();
    }
    const u32 header = *hdr_res;

    // Jump command
    if ((header & 0x00000001u) != 0) {
        PushbufferPacket pkt;
        pkt.opcode = PacketOpcode::Jump;
        pkt.jump_target = header & ~0x00000003u;
        return pkt;
    }

    const u32 prefix = (header >> 30) & 0x03;
    if (prefix != 0b00 && prefix != 0b10) {
        return Error{ErrorCode::InvalidField, "Unknown pushbuffer packet prefix opcode"};
    }

    PushbufferPacket pkt;
    pkt.opcode = PacketOpcode::Method;
    pkt.method = header & 0x00001FFCu;
    pkt.count = (header >> 18) & 0x000007FFu;
    pkt.non_incrementing = (prefix == 0b10) || ((header & (1u << 29)) != 0);

    // Bounds checking against available words
    if (pkt.count + 1 > words_available) {
        return Error{ErrorCode::TruncatedData,
                     "Pushbuffer packet payload ends before declared count"};
    }

    pkt.parameters.reserve(pkt.count);
    for (u32 i = 0; i < pkt.count; ++i) {
        GuestAddr param_addr = 0;
        if (!CheckedAdd(current_addr, static_cast<GuestAddr>((i + 1) * 4), param_addr)) {
            return Error{ErrorCode::IntegerOverflow, "Pushbuffer address wrapped around",
                         current_addr};
        }
        auto param_res = reader(param_addr);
        if (!param_res.has_value()) {
            return Error{ErrorCode::TruncatedData,
                         "Failed to fetch parameter word before declared count"};
        }
        pkt.parameters.push_back(*param_res);
    }

    return pkt;
}

} // namespace xblob::gpu
