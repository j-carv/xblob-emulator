#include "xblob/gpu/pushbuffer_processor.hpp"

#include "pushbuffer_3d_methods.hpp"

namespace xblob::gpu {

namespace {

bool CheckBudgetExhausted(const PushbufferStats& stats, const PushbufferBudgets& b) noexcept {
    return stats.words_processed >= b.max_words || stats.packets_processed >= b.max_packets ||
           stats.methods_executed >= b.max_methods || stats.jumps_taken >= b.max_jumps ||
           stats.vertices_processed >= b.max_vertices ||
           stats.triangles_rasterized >= b.max_triangles;
}

} // namespace

PushbufferProcessor::PushbufferProcessor(Nv2aDevice& device) : device_(device) {
    subchannel_classes_.fill(0);
    subchannel_classes_[0] = kClassNv097Kelvin3D;
}

Result<void> PushbufferProcessor::ValidatePacket(const PushbufferPacket& packet) {
    if (packet.opcode == PacketOpcode::Jump) {
        if ((packet.jump_target % 4) != 0) {
            return Error{ErrorCode::InvalidArgument, "Misaligned pushbuffer jump target",
                         packet.jump_target};
        }
        return {};
    }

    if (packet.opcode != PacketOpcode::Method) {
        return Error{ErrorCode::InvalidField, "Unknown pushbuffer packet opcode"};
    }

    if (packet.subchannel >= kMaxSubchannels) {
        return Error{ErrorCode::OutOfBounds, "Invalid subchannel index"};
    }

    u32 temp_rx = rect_x_;
    u32 temp_ry = rect_y_;
    u32 temp_rw = rect_w_;
    u32 temp_rh = rect_h_;

    for (u32 i = 0; i < packet.count; ++i) {
        const u32 method = packet.non_incrementing ? packet.method : (packet.method + i * 4);
        const u32 val = packet.parameters[i];

        switch (method) {
        case kMethodSetObject:
            if (val != kClassNv097Kelvin3D && val != kClassNv096Kelvin3D &&
                val != kClassNv062Surface && val != kClassNv01fBlit && val != kClassNv044Video &&
                val != kClassSynthetic2D) {
                ctx_3d_.RecordUnsupportedMethod(subchannel_classes_[packet.subchannel], method,
                                                packet.subchannel, packet.count, val);
                return Error{ErrorCode::UnsupportedFeature, "Unsupported object class ID", val};
            }
            break;

        // 2D Synthetic Legacy Methods
        case kMethodSurfaceWidth:
        case kMethodSurfaceHeight:
        case kMethodSurfacePitch:
        case kMethodClearColor:
        case kMethodClearSurface:
        case kMethodRectColor:
        case kMethodFlip:
            break;
        case kMethodRectX:
            temp_rx = val;
            break;
        case kMethodRectY:
            temp_ry = val;
            break;
        case kMethodRectW:
            temp_rw = val;
            break;
        case kMethodRectH:
            temp_rh = val;
            break;
        case kMethodRectDraw: {
            const auto& surf = device_.back_surface();
            if (static_cast<u64>(temp_rx) + temp_rw > surf.width() ||
                static_cast<u64>(temp_ry) + temp_rh > surf.height()) {
                return Error{ErrorCode::OutOfBounds, "FillRect bounds exceed surface dimensions"};
            }
            break;
        }

        default: {
            auto res_3d = detail::Validate3dMethod(method, val, packet.count, packet.subchannel,
                                                   subchannel_classes_[packet.subchannel], ctx_3d_,
                                                   memory_reader_);
            if (!res_3d.has_value()) {
                return res_3d;
            }
            break;
        }
        }
    }

    return {};
}

Result<void> PushbufferProcessor::ApplyPacket(const PushbufferPacket& packet) {
    if (packet.opcode == PacketOpcode::Jump) {
        return {};
    }

    for (u32 i = 0; i < packet.count; ++i) {
        const u32 method = packet.non_incrementing ? packet.method : (packet.method + i * 4);
        const u32 val = packet.parameters[i];

        switch (method) {
        case kMethodSetObject:
            subchannel_classes_[packet.subchannel] = val;
            break;

        // 2D Legacy
        case kMethodSurfaceWidth:
            surface_w_ = val;
            break;
        case kMethodSurfaceHeight:
            surface_h_ = val;
            break;
        case kMethodSurfacePitch:
            surface_pitch_ = val;
            break;
        case kMethodClearColor:
            clear_color_ = val;
            break;
        case kMethodClearSurface:
            device_.back_surface().Clear(clear_color_ & 0xFF, (clear_color_ >> 8) & 0xFF,
                                         (clear_color_ >> 16) & 0xFF, (clear_color_ >> 24) & 0xFF);
            break;
        case kMethodRectX:
            rect_x_ = val;
            break;
        case kMethodRectY:
            rect_y_ = val;
            break;
        case kMethodRectW:
            rect_w_ = val;
            break;
        case kMethodRectH:
            rect_h_ = val;
            break;
        case kMethodRectColor:
            rect_color_ = val;
            break;
        case kMethodRectDraw:
            (void)device_.back_surface().FillRect(
                rect_x_, rect_y_, rect_w_, rect_h_, rect_color_ & 0xFF, (rect_color_ >> 8) & 0xFF,
                (rect_color_ >> 16) & 0xFF, (rect_color_ >> 24) & 0xFF);
            break;
        case kMethodFlip:
            device_.Flip();
            break;

        default:
            (void)detail::Apply3dMethod(method, val, ctx_3d_, device_.back_surface(), stats_,
                                        memory_reader_);
            break;
        }

        ++stats_.methods_executed;
    }

    return {};
}

Result<void> PushbufferProcessor::ExecutePacket(const PushbufferPacket& packet) {
    auto val_res = ValidatePacket(packet);
    if (!val_res.has_value()) {
        const auto code = val_res.error().code;
        if (code == ErrorCode::OutOfBounds) {
            device_.SetFault(GpuFault::InvalidCoordinates);
        } else if (code == ErrorCode::UnsupportedFeature) {
            const bool is_tex =
                val_res.error().message.find("texture format") != std::string_view::npos;
            device_.SetFault(is_tex ? GpuFault::UnsupportedTextureFormat : GpuFault::UnknownMethod);
        } else if (code == ErrorCode::LimitReached) {
            device_.SetFault(GpuFault::BudgetExhausted);
        } else if (code == ErrorCode::InvalidState) {
            device_.SetFault(GpuFault::InvalidState);
        } else {
            device_.SetFault(GpuFault::UnknownOpcode);
        }
        return val_res;
    }

    return ApplyPacket(packet);
}

Result<PushbufferStats> PushbufferProcessor::ExecuteBuffer(std::span<const u32> words,
                                                           PushbufferBudgets budgets) {
    std::size_t offset = 0;
    std::unordered_set<std::size_t> visited_offsets;

    while (offset < words.size()) {
        if (CheckBudgetExhausted(stats_, budgets)) {
            device_.SetFault(GpuFault::BudgetExhausted);
            return Error{ErrorCode::LimitReached, "Pushbuffer budget exhausted"};
        }

        const std::size_t prev_offset = offset;
        auto pkt_res = PushbufferDecoder::DecodeFromSpan(words, offset);
        if (!pkt_res.has_value()) {
            if (pkt_res.error().code == ErrorCode::TruncatedData) {
                device_.SetFault(GpuFault::PushbufferTruncated);
            }
            return pkt_res.error();
        }

        const auto& pkt = *pkt_res;
        stats_.words_processed += static_cast<u32>(offset - prev_offset);
        ++stats_.packets_processed;

        if (pkt.opcode == PacketOpcode::Jump) {
            ++stats_.jumps_taken;
            if (stats_.jumps_taken > budgets.max_jumps) {
                device_.SetFault(GpuFault::BudgetExhausted);
                return Error{ErrorCode::LimitReached, "Pushbuffer jump loop budget exceeded"};
            }

            const std::size_t target_word_idx = pkt.jump_target / 4;
            if (target_word_idx >= words.size()) {
                device_.SetFault(GpuFault::BudgetExhausted);
                return Error{ErrorCode::OutOfBounds, "Jump target out of pushbuffer bounds"};
            }

            if (visited_offsets.contains(target_word_idx)) {
                device_.SetFault(GpuFault::BudgetExhausted);
                return Error{ErrorCode::LimitReached, "Cyclic pushbuffer loop detected"};
            }

            visited_offsets.insert(target_word_idx);
            offset = target_word_idx;
            continue;
        }

        auto exec_res = ExecutePacket(pkt);
        if (!exec_res.has_value()) {
            return exec_res.error();
        }
    }

    return stats_;
}

Result<PushbufferStats> PushbufferProcessor::ExecuteFromMemory(GuestAddr start_addr,
                                                               PushbufferDecoder::ReadWordFn reader,
                                                               u32 max_words,
                                                               PushbufferBudgets budgets) {
    GuestAddr current_addr = start_addr;
    u32 words_remaining = max_words;
    std::unordered_set<GuestAddr> visited_addrs;
    memory_reader_ = reader;

    while (words_remaining > 0) {
        if (CheckBudgetExhausted(stats_, budgets)) {
            device_.SetFault(GpuFault::BudgetExhausted);
            return Error{ErrorCode::LimitReached, "Pushbuffer budget exhausted"};
        }

        auto pkt_res = PushbufferDecoder::DecodeFromMemory(current_addr, reader, words_remaining);
        if (!pkt_res.has_value()) {
            if (pkt_res.error().code == ErrorCode::TruncatedData) {
                device_.SetFault(GpuFault::PushbufferTruncated);
            }
            return pkt_res.error();
        }

        const auto& pkt = *pkt_res;
        const u32 words_consumed = (pkt.opcode == PacketOpcode::Jump) ? 1 : (1 + pkt.count);
        stats_.words_processed += words_consumed;
        words_remaining =
            (words_remaining > words_consumed) ? (words_remaining - words_consumed) : 0;
        current_addr += words_consumed * 4;
        ++stats_.packets_processed;

        if (pkt.opcode == PacketOpcode::Jump) {
            ++stats_.jumps_taken;
            if (stats_.jumps_taken > budgets.max_jumps) {
                device_.SetFault(GpuFault::BudgetExhausted);
                return Error{ErrorCode::LimitReached, "Pushbuffer jump loop budget exceeded"};
            }

            if (visited_addrs.contains(pkt.jump_target)) {
                device_.SetFault(GpuFault::BudgetExhausted);
                return Error{ErrorCode::LimitReached, "Pushbuffer loop detected"};
            }

            visited_addrs.insert(pkt.jump_target);
            current_addr = pkt.jump_target;
            continue;
        }

        auto exec_res = ExecutePacket(pkt);
        if (!exec_res.has_value()) {
            return exec_res.error();
        }
    }

    return stats_;
}

} // namespace xblob::gpu
