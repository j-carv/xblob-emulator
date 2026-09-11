#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/common/types.hpp"
#include "xblob/cpu/registers.hpp"
#include "xblob/kernel/debug_sink.hpp"
#include "xblob/kernel/file_services.hpp"
#include "xblob/kernel/guest_heap.hpp"
#include "xblob/kernel/registry.hpp"
#include "xblob/kernel/sync_objects.hpp"
#include "xblob/kernel/thread_scheduler.hpp"
#include "xblob/memory/address_space.hpp"

#include <memory>

namespace xblob::kernel {

class KernelHle {
public:
    explicit KernelHle(GuestAddr heap_base = 0x10000000, std::size_t heap_size = 16 * 1024 * 1024,
                       std::shared_ptr<IDebugSink> debug_sink = nullptr);

    Result<u32> DispatchThunk(u32 ordinal, cpu::CpuContext& ctx, memory::AddressSpace& mem);

    [[nodiscard]] ExportRegistry& registry() noexcept { return registry_; }
    [[nodiscard]] const ExportRegistry& registry() const noexcept { return registry_; }

    [[nodiscard]] GuestHeap& heap() noexcept { return heap_; }
    [[nodiscard]] const GuestHeap& heap() const noexcept { return heap_; }

    [[nodiscard]] HandleTable& handles() noexcept { return handles_; }
    [[nodiscard]] const HandleTable& handles() const noexcept { return handles_; }

    [[nodiscard]] ThreadScheduler& threads() noexcept { return threads_; }
    [[nodiscard]] const ThreadScheduler& threads() const noexcept { return threads_; }

    [[nodiscard]] IDebugSink& debug_sink() noexcept { return *debug_sink_; }
    [[nodiscard]] const IDebugSink& debug_sink() const noexcept { return *debug_sink_; }

    [[nodiscard]] KernelFileServices& file_services() noexcept { return file_services_; }
    [[nodiscard]] const KernelFileServices& file_services() const noexcept {
        return file_services_;
    }
    void SetVfs(std::shared_ptr<Vfs> vfs) noexcept { file_services_.SetVfs(std::move(vfs)); }

    void Reset();

private:
    ExportRegistry registry_;
    GuestHeap heap_;
    HandleTable handles_;
    ThreadScheduler threads_;
    std::shared_ptr<IDebugSink> debug_sink_;
    KernelFileServices file_services_{nullptr};

    void RegisterStandardExports();
};

} // namespace xblob::kernel
