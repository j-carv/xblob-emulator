#pragma once

#include "xblob/common/types.hpp"

#include <optional>

namespace xblob::cpu {

class IInterruptSource {
public:
    virtual ~IInterruptSource() = default;

    [[nodiscard]] virtual bool HasPendingInterrupt() const noexcept = 0;
    [[nodiscard]] virtual std::optional<u8> AcknowledgeInterrupt() noexcept = 0;
};

} // namespace xblob::cpu
