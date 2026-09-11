#pragma once

#include "xblob/bus/device.hpp"

#include <vector>

namespace xblob::bus {

struct SyntheticRegisterLogEntry {
    bool is_write{false};
    u32 offset{0};
    BusAccessWidth width{BusAccessWidth::Byte};
    u32 value{0};

    [[nodiscard]] constexpr bool operator==(const SyntheticRegisterLogEntry& other) const noexcept {
        return is_write == other.is_write && offset == other.offset && width == other.width &&
               value == other.value;
    }
};

class SyntheticRegisterBank : public BusDevice {
public:
    explicit SyntheticRegisterBank(std::size_t size_bytes = 64);

    [[nodiscard]] std::string_view name() const noexcept override {
        return "synthetic_register_bank";
    }

    [[nodiscard]] Result<u32> Read(u32 offset, BusAccessWidth width) override;
    [[nodiscard]] Result<void> Write(u32 offset, BusAccessWidth width, u32 value) override;

    [[nodiscard]] std::size_t size() const noexcept { return storage_.size(); }
    [[nodiscard]] const std::vector<SyntheticRegisterLogEntry>& access_log() const noexcept {
        return log_;
    }

    void ClearLog() noexcept { log_.clear(); }

    [[nodiscard]] u8 ReadRawByte(std::size_t offset) const { return storage_.at(offset); }

private:
    std::vector<u8> storage_;
    std::vector<SyntheticRegisterLogEntry> log_;
};

} // namespace xblob::bus
