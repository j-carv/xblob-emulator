#pragma once

#include "xblob/common/types.hpp"

namespace xblob::usb::ohci {

// OHCI MMIO Register Offsets
inline constexpr u32 kRegHcRevision = 0x00;
inline constexpr u32 kRegHcControl = 0x04;
inline constexpr u32 kRegHcCommandStatus = 0x08;
inline constexpr u32 kRegHcInterruptStatus = 0x0C;
inline constexpr u32 kRegHcInterruptEnable = 0x10;
inline constexpr u32 kRegHcInterruptDisable = 0x14;
inline constexpr u32 kRegHcHCCA = 0x18;
inline constexpr u32 kRegHcPeriodCurrentED = 0x1C;
inline constexpr u32 kRegHcControlHeadED = 0x20;
inline constexpr u32 kRegHcControlCurrentED = 0x24;
inline constexpr u32 kRegHcBulkHeadED = 0x28;
inline constexpr u32 kRegHcBulkCurrentED = 0x2C;
inline constexpr u32 kRegHcDoneHead = 0x30;
inline constexpr u32 kRegHcFmInterval = 0x34;
inline constexpr u32 kRegHcFmRemaining = 0x38;
inline constexpr u32 kRegHcFmNumber = 0x3C;
inline constexpr u32 kRegHcPeriodicStart = 0x40;
inline constexpr u32 kRegHcLSThreshold = 0x44;
inline constexpr u32 kRegHcRhDescriptorA = 0x48;
inline constexpr u32 kRegHcRhDescriptorB = 0x4C;
inline constexpr u32 kRegHcRhStatus = 0x50;
inline constexpr u32 kRegHcRhPortStatusBase = 0x54;

inline constexpr u32 kPortRegisterStride = 4;
inline constexpr u32 kMaxRootHubPorts = 4;

// Default values on reset
inline constexpr u32 kDefaultRevision = 0x00000110; // OHCI 1.0/1.1
inline constexpr u32 kDefaultFmInterval = 0x2EDF;   // 11,999 bits per frame
inline constexpr u32 kDefaultFmIntervalFull =
    0x27782EDF; // FSLargestDataPacket(0x2778) | FIT(0) | FrameInterval(0x2EDF)
inline constexpr u32 kDefaultPeriodicStart = 0x2A2F; // 90% of frame interval
inline constexpr u32 kDefaultLSThreshold = 0x0628;

// HcControl bitmasks
namespace control {
inline constexpr u32 kControlBulkServiceRatioMask = 0x00000003;
inline constexpr u32 kPeriodicListEnable = 1U << 2;
inline constexpr u32 kIsochronousEnable = 1U << 3;
inline constexpr u32 kControlListEnable = 1U << 4;
inline constexpr u32 kBulkListEnable = 1U << 5;
inline constexpr u32 kHostControllerFunctionalStateMask = 0x000000C0;
inline constexpr u32 kStateUsbReset = 0x00000000;
inline constexpr u32 kStateUsbResume = 0x00000040;
inline constexpr u32 kStateUsbOperational = 0x00000080;
inline constexpr u32 kStateUsbSuspend = 0x000000C0;
inline constexpr u32 kInterruptRouting = 1U << 8;
inline constexpr u32 kRemoteWakeupConnected = 1U << 9;
inline constexpr u32 kRemoteWakeupEnable = 1U << 10;
} // namespace control

// HcCommandStatus bitmasks
namespace command_status {
inline constexpr u32 kHostControllerReset = 1U << 0;
inline constexpr u32 kControlListFilled = 1U << 1;
inline constexpr u32 kBulkListFilled = 1U << 2;
inline constexpr u32 kOwnershipChangeRequest = 1U << 3;
inline constexpr u32 kSchedulingOverrunCountMask = 0x00030000;
} // namespace command_status

// HcInterruptStatus / Enable / Disable bitmasks
namespace interrupt {
inline constexpr u32 kSchedulingOverrun = 1U << 0;
inline constexpr u32 kWritebackDoneHead = 1U << 1;
inline constexpr u32 kStartOfFrame = 1U << 2;
inline constexpr u32 kResumeDetected = 1U << 3;
inline constexpr u32 kUnrecoverableError = 1U << 4;
inline constexpr u32 kFrameNumberOverflow = 1U << 5;
inline constexpr u32 kRootHubStatusChange = 1U << 6;
inline constexpr u32 kOwnershipChange = 1U << 30;
inline constexpr u32 kMasterInterruptEnable = 1U << 31;

inline constexpr u32 kValidInterruptMask =
    kSchedulingOverrun | kWritebackDoneHead | kStartOfFrame | kResumeDetected |
    kUnrecoverableError | kFrameNumberOverflow | kRootHubStatusChange | kOwnershipChange;
} // namespace interrupt

// HcRhPortStatus bitmasks
namespace port_status {
inline constexpr u32 kCurrentConnectStatus = 1U << 0;
inline constexpr u32 kPortEnableStatus = 1U << 1;
inline constexpr u32 kPortSuspendStatus = 1U << 2;
inline constexpr u32 kPortOverCurrentIndicator = 1U << 3;
inline constexpr u32 kPortResetStatus = 1U << 4;
inline constexpr u32 kPortPowerStatus = 1U << 8;
inline constexpr u32 kLowSpeedDeviceAttached = 1U << 9;

// W1C (Write-1-to-Clear) status change bits
inline constexpr u32 kConnectStatusChange = 1U << 16;
inline constexpr u32 kPortEnableStatusChange = 1U << 17;
inline constexpr u32 kPortSuspendStatusChange = 1U << 18;
inline constexpr u32 kPortOverCurrentIndicatorChange = 1U << 19;
inline constexpr u32 kPortResetStatusChange = 1U << 20;

inline constexpr u32 kStatusChangeMask = kConnectStatusChange | kPortEnableStatusChange |
                                         kPortSuspendStatusChange |
                                         kPortOverCurrentIndicatorChange | kPortResetStatusChange;
} // namespace port_status

} // namespace xblob::usb::ohci
