#include "xblob/kernel/kernel_hle.hpp"

namespace xblob::kernel {

void RegisterMemoryExports(ExportRegistry& reg) {
    // Ordinal 184: RtlAllocateHeap (3 parameters: HeapHandle, Flags, Size)
    (void)reg.RegisterExport(184, "RtlAllocateHeap", 3, [](GuestContext& ctx) -> Result<u32> {
        auto heap_h = ctx.ReadArg(0);
        auto size = ctx.ReadArg(2);
        if (!heap_h || !size)
            return 0;

        auto heap_res = ctx.kernel.heap_manager().GetHeap(*heap_h);
        if (!heap_res) {
            return 0;
        }

        auto alloc_res = (*heap_res)->Allocate(*size);
        if (!alloc_res) {
            return 0;
        }
        return *alloc_res;
    });

    // Ordinal 185: RtlFreeHeap (3 parameters: HeapHandle, Flags, BaseAddress)
    (void)reg.RegisterExport(185, "RtlFreeHeap", 3, [](GuestContext& ctx) -> Result<u32> {
        auto heap_h = ctx.ReadArg(0);
        auto ptr = ctx.ReadArg(2);
        if (!heap_h || !ptr)
            return 0;

        auto heap_res = ctx.kernel.heap_manager().GetHeap(*heap_h);
        if (!heap_res) {
            return 0;
        }

        auto free_res = (*heap_res)->Free(*ptr);
        return free_res.has_value() ? 1 : 0;
    });

    // Ordinal 186: RtlReAllocateHeap (4 parameters: HeapHandle, Flags, BaseAddress, NewSize)
    (void)reg.RegisterExport(186, "RtlReAllocateHeap", 4, [](GuestContext& ctx) -> Result<u32> {
        auto heap_h = ctx.ReadArg(0);
        auto ptr = ctx.ReadArg(2);
        auto new_size = ctx.ReadArg(3);
        if (!heap_h || !ptr || !new_size)
            return 0;

        auto heap_res = ctx.kernel.heap_manager().GetHeap(*heap_h);
        if (!heap_res) {
            return 0;
        }

        auto realloc_res = (*heap_res)->ReAllocate(*ptr, *new_size, &ctx.mem);
        if (!realloc_res) {
            return 0;
        }
        return *realloc_res;
    });

    // Ordinal 187: RtlSizeHeap (3 parameters: HeapHandle, Flags, BaseAddress)
    (void)reg.RegisterExport(187, "RtlSizeHeap", 3, [](GuestContext& ctx) -> Result<u32> {
        auto heap_h = ctx.ReadArg(0);
        auto ptr = ctx.ReadArg(2);
        if (!heap_h || !ptr)
            return 0xFFFFFFFFU;

        auto heap_res = ctx.kernel.heap_manager().GetHeap(*heap_h);
        if (!heap_res) {
            return 0xFFFFFFFFU;
        }

        auto size_res = (*heap_res)->GetBlockSize(*ptr);
        if (!size_res) {
            return 0xFFFFFFFFU;
        }
        return static_cast<u32>(*size_res);
    });

    // Ordinal 181: RtlCreateHeap (6 parameters)
    (void)reg.RegisterExport(181, "RtlCreateHeap", 6, [](GuestContext& ctx) -> Result<u32> {
        auto base = ctx.ReadArg(1);
        auto reserve = ctx.ReadArg(2);
        GuestAddr b = base ? *base : 0;
        std::size_t sz = reserve ? *reserve : 1024 * 1024;
        if (sz == 0)
            sz = 1024 * 1024;

        auto h_res = ctx.kernel.heap_manager().CreateHeap(b, sz);
        if (!h_res) {
            return 0;
        }
        return *h_res;
    });

    // Ordinal 182: RtlDestroyHeap (1 parameter: HeapHandle)
    (void)reg.RegisterExport(182, "RtlDestroyHeap", 1, [](GuestContext& ctx) -> Result<u32> {
        auto heap_h = ctx.ReadArg(0);
        if (!heap_h)
            return 0;

        auto res = ctx.kernel.heap_manager().DestroyHeap(*heap_h);
        return res.has_value() ? 1 : 0;
    });

    // Ordinal 323: MmAllocateContiguousMemory (2 parameters: NumberOfBytes, HighestAddress)
    (void)reg.RegisterExport(
        323, "MmAllocateContiguousMemory", 2, [](GuestContext& ctx) -> Result<u32> {
            auto num_bytes = ctx.ReadArg(0);
            if (!num_bytes || *num_bytes == 0) {
                return 0;
            }
            auto alloc_res = ctx.kernel.vm().Allocate(0, *num_bytes, kMemCommit | kMemReserve,
                                                      kPageReadWrite, ctx.mem);
            if (!alloc_res) {
                return 0;
            }
            return *alloc_res;
        });

    // Ordinal 324: MmFreeContiguousMemory (1 parameter: BaseAddress)
    (void)reg.RegisterExport(
        324, "MmFreeContiguousMemory", 1, [](GuestContext& ctx) -> Result<u32> {
            auto base = ctx.ReadArg(0);
            if (!base || *base == 0) {
                return 0;
            }
            auto free_res = ctx.kernel.vm().Free(*base, 0, kMemRelease, ctx.mem);
            return free_res.has_value() ? 1 : 0;
        });

    // Ordinal 326: MmQueryAddressProtect (1 parameter: BaseAddress)
    (void)reg.RegisterExport(326, "MmQueryAddressProtect", 1, [](GuestContext& ctx) -> Result<u32> {
        auto base = ctx.ReadArg(0);
        if (!base) {
            return 0;
        }
        auto q_res = ctx.kernel.vm().Query(*base);
        if (!q_res) {
            return 0;
        }
        return q_res->protect;
    });

    // Ordinal 188: NtAllocateVirtualMemory (5 parameters: BasePtr, ZeroBits, SizePtr, Type,
    // Protect)
    (void)reg.RegisterExport(
        188, "NtAllocateVirtualMemory", 5, [](GuestContext& ctx) -> Result<u32> {
            auto base_ptr = ctx.ReadArg(0);
            auto size_ptr = ctx.ReadArg(2);
            auto alloc_type = ctx.ReadArg(3);
            auto protect = ctx.ReadArg(4);
            if (!base_ptr || !size_ptr || !alloc_type || !protect) {
                return kStatusInvalidParameter;
            }

            auto read_base = ctx.Read32(*base_ptr);
            auto read_size = ctx.Read32(*size_ptr);
            if (!read_base || !read_size) {
                return kStatusAccessViolation;
            }

            GuestAddr req_base = *read_base;
            std::size_t req_size = *read_size;

            auto alloc_res =
                ctx.kernel.vm().Allocate(req_base, req_size, *alloc_type, *protect, ctx.mem);
            if (!alloc_res) {
                return kStatusConflict;
            }

            // Write back updated base and size atomically
            (void)ctx.Write32(*base_ptr, *alloc_res);
            (void)ctx.Write32(*size_ptr,
                              static_cast<u32>((req_size + VirtualMemoryManager::kPageSize - 1) &
                                               ~(VirtualMemoryManager::kPageSize - 1)));
            return kStatusSuccess;
        });

    // Ordinal 199: NtFreeVirtualMemory (3 parameters: BasePtr, SizePtr, FreeType)
    (void)reg.RegisterExport(199, "NtFreeVirtualMemory", 3, [](GuestContext& ctx) -> Result<u32> {
        auto base_ptr = ctx.ReadArg(0);
        auto size_ptr = ctx.ReadArg(1);
        auto free_type = ctx.ReadArg(2);
        if (!base_ptr || !size_ptr || !free_type) {
            return kStatusInvalidParameter;
        }

        auto read_base = ctx.Read32(*base_ptr);
        auto read_size = ctx.Read32(*size_ptr);
        if (!read_base || !read_size) {
            return kStatusAccessViolation;
        }

        auto free_res = ctx.kernel.vm().Free(*read_base, *read_size, *free_type, ctx.mem);
        if (!free_res) {
            return kStatusUnsuccessful;
        }
        return kStatusSuccess;
    });

    // Ordinal 220: NtQueryVirtualMemory (5 parameters: Base, Class, InfoPtr, Length, RetLengthPtr)
    (void)reg.RegisterExport(220, "NtQueryVirtualMemory", 5, [](GuestContext& ctx) -> Result<u32> {
        auto base = ctx.ReadArg(0);
        auto info_ptr = ctx.ReadArg(2);
        auto len = ctx.ReadArg(3);
        auto ret_len_ptr = ctx.ReadArg(4);
        if (!base || !info_ptr || !len) {
            return kStatusInvalidParameter;
        }

        if (*len < sizeof(MemoryBasicInformation32)) {
            return kStatusBufferTooSmall;
        }

        auto q_res = ctx.kernel.vm().Query(*base);
        if (!q_res) {
            return kStatusUnsuccessful;
        }

        // Write MemoryBasicInformation32
        (void)ctx.Write32(*info_ptr + 0, q_res->base_address);
        (void)ctx.Write32(*info_ptr + 4, q_res->allocation_base);
        (void)ctx.Write32(*info_ptr + 8, q_res->allocation_protect);
        (void)ctx.Write32(*info_ptr + 12, q_res->region_size);
        (void)ctx.Write32(*info_ptr + 16, q_res->state);
        (void)ctx.Write32(*info_ptr + 20, q_res->protect);
        (void)ctx.Write32(*info_ptr + 24, q_res->type);

        if (ret_len_ptr && *ret_len_ptr != 0) {
            (void)ctx.Write32(*ret_len_ptr, static_cast<u32>(sizeof(MemoryBasicInformation32)));
        }
        return kStatusSuccess;
    });

    // Ordinal 218: NtProtectVirtualMemory (4 parameters: BasePtr, SizePtr, NewProtect,
    // OldProtectPtr)
    (void)reg.RegisterExport(
        218, "NtProtectVirtualMemory", 4, [](GuestContext& ctx) -> Result<u32> {
            auto base_ptr = ctx.ReadArg(0);
            auto size_ptr = ctx.ReadArg(1);
            auto new_prot = ctx.ReadArg(2);
            auto old_prot_ptr = ctx.ReadArg(3);
            if (!base_ptr || !size_ptr || !new_prot || !old_prot_ptr) {
                return kStatusInvalidParameter;
            }

            auto read_base = ctx.Read32(*base_ptr);
            auto read_size = ctx.Read32(*size_ptr);
            if (!read_base || !read_size) {
                return kStatusAccessViolation;
            }

            auto prot_res = ctx.kernel.vm().Protect(*read_base, *read_size, *new_prot);
            if (!prot_res) {
                return kStatusUnsuccessful;
            }

            (void)ctx.Write32(*old_prot_ptr, *prot_res);
            return kStatusSuccess;
        });
}

} // namespace xblob::kernel
