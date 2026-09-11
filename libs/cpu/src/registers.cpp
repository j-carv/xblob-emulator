#include "xblob/cpu/registers.hpp"

namespace xblob::cpu {

void CpuContext::Reset() noexcept {
    gpr.fill(0);
    eip = 0;
    eflags = kFlagReserved1;
    segments.fill(0);
    cr0 = 0;
    cr2 = 0;
    cr3 = 0;
    idtr = Idtr{0, 0};
}

} // namespace xblob::cpu
