#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/io/byte_source.hpp"
#include "xblob/loader/loader_types.hpp"
#include "xblob/memory/address_space.hpp"

namespace xblob::loader {

class XbeLoader {
public:
    // Phase 1: validate and construct immutable load plan without modifying memory
    [[nodiscard]] static Result<XbeLoadPlan> Plan(const ByteSource& source);

    // Phase 2: transactionally apply plan to target address space with total rollback on failure
    [[nodiscard]] static Result<void> Apply(const XbeLoadPlan& plan, const ByteSource& source,
                                            memory::AddressSpace& space);

    // Convenience one-shot transactional load
    [[nodiscard]] static Result<XbeLoadPlan> Load(const ByteSource& source,
                                                  memory::AddressSpace& space);

    // Context generation without CPU side effects
    [[nodiscard]] static InitialContext CreateInitialContext(const XbeLoadPlan& plan,
                                                             GuestAddr stack_top = 0x00070000,
                                                             GuestSize stack_size = 0x00010000);
};

} // namespace xblob::loader
