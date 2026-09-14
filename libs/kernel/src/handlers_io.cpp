#include "xblob/kernel/kernel_hle.hpp"

namespace xblob::kernel {

void RegisterIoExports(ExportRegistry& reg) {
    // Ordinal 18: NtClose (1 parameter: Handle)
    (void)reg.RegisterExport(18, "NtClose", 1, [](GuestContext& ctx) -> Result<u32> {
        auto h = ctx.ReadArg(0);
        if (!h)
            return h.error();

        // Check HandleTable first
        if (ctx.kernel.handles().IsValidHandle(*h)) {
            auto close_res = ctx.kernel.handles().CloseHandle(*h);
            return close_res.has_value() ? kStatusSuccess : kNtStatusInvalidHandle;
        }

        // Otherwise close via file services
        return ctx.kernel.file_services().CloseHandle(*h);
    });

    // Ordinal 190: NtCreateFile (4 parameters: OutHandlePtr, DesiredAccess, PathPtr, Disposition)
    (void)reg.RegisterExport(190, "NtCreateFile", 4, [](GuestContext& ctx) -> Result<u32> {
        auto out_h = ctx.ReadArg(0);
        auto access = ctx.ReadArg(1);
        auto path = ctx.ReadArg(2);
        auto disp = ctx.ReadArg(3);
        if (!out_h || !access || !path || !disp)
            return Error{ErrorCode::InvalidArgument, "Missing NtCreateFile arguments"};
        return ctx.kernel.file_services().CreateFile(*path, *access, *disp, *out_h, ctx.mem);
    });

    // Ordinal 219: NtReadFile (5 parameters: Handle, Buffer, Length, BytesReadPtr, OverlappedPtr)
    (void)reg.RegisterExport(219, "NtReadFile", 5, [](GuestContext& ctx) -> Result<u32> {
        auto h = ctx.ReadArg(0);
        auto buf = ctx.ReadArg(1);
        auto len = ctx.ReadArg(2);
        auto read_ptr = ctx.ReadArg(3);
        auto ovl = ctx.ReadArg(4);
        if (!h || !buf || !len || !read_ptr || !ovl)
            return Error{ErrorCode::InvalidArgument, "Missing NtReadFile arguments"};
        return ctx.kernel.file_services().ReadFile(*h, *buf, *len, *read_ptr, *ovl, ctx.mem);
    });

    // Ordinal 256: NtWriteFile (5 parameters: Handle, Buffer, Length, BytesWrittenPtr,
    // OverlappedPtr)
    (void)reg.RegisterExport(256, "NtWriteFile", 5, [](GuestContext& ctx) -> Result<u32> {
        auto h = ctx.ReadArg(0);
        auto buf = ctx.ReadArg(1);
        auto len = ctx.ReadArg(2);
        auto written_ptr = ctx.ReadArg(3);
        auto ovl = ctx.ReadArg(4);
        if (!h || !buf || !len || !written_ptr || !ovl)
            return Error{ErrorCode::InvalidArgument, "Missing NtWriteFile arguments"};
        return ctx.kernel.file_services().WriteFile(*h, *buf, *len, *written_ptr, *ovl, ctx.mem);
    });

    // Ordinal 224: SetFilePointer (4 parameters: Handle, Distance, HighDistPtr, MoveMethod)
    (void)reg.RegisterExport(224, "SetFilePointer", 4, [](GuestContext& ctx) -> Result<u32> {
        auto h = ctx.ReadArg(0);
        auto dist = ctx.ReadArg(1);
        auto high = ctx.ReadArg(2);
        auto method = ctx.ReadArg(3);
        if (!h || !dist || !high || !method)
            return Error{ErrorCode::InvalidArgument, "Missing SetFilePointer arguments"};
        return ctx.kernel.file_services().SetFilePointer(*h, static_cast<i32>(*dist), *high,
                                                         *method, ctx.mem);
    });

    // Ordinal 196: NtDeviceIoControlFile (8 parameters)
    (void)reg.RegisterExport(196, "NtDeviceIoControlFile", 8, [](GuestContext& ctx) -> Result<u32> {
        auto h = ctx.ReadArg(0);
        auto code = ctx.ReadArg(1);
        auto in_buf = ctx.ReadArg(2);
        auto in_len = ctx.ReadArg(3);
        auto out_buf = ctx.ReadArg(4);
        auto out_len = ctx.ReadArg(5);
        auto ret_ptr = ctx.ReadArg(6);
        auto ovl = ctx.ReadArg(7);
        if (!h || !code || !in_buf || !in_len || !out_buf || !out_len || !ret_ptr || !ovl)
            return Error{ErrorCode::InvalidArgument, "Missing NtDeviceIoControlFile arguments"};
        return ctx.kernel.file_services().DeviceIoControl(*h, *code, *in_buf, *in_len, *out_buf,
                                                          *out_len, *ret_ptr, *ovl, ctx.mem);
    });

    // Ordinal 217: NtQueryInformationFile (5 parameters)
    (void)reg.RegisterExport(217, "NtQueryInformationFile", 5,
                             [](GuestContext& ctx) -> Result<u32> {
                                 auto h = ctx.ReadArg(0);
                                 auto info_ptr = ctx.ReadArg(2);
                                 auto len = ctx.ReadArg(3);
                                 auto info_cls = ctx.ReadArg(4);
                                 if (!h || !info_ptr || !len || !info_cls)
                                     return Error{ErrorCode::InvalidArgument,
                                                  "Missing NtQueryInformationFile arguments"};
                                 return ctx.kernel.file_services().QueryInformationFile(
                                     *h, *info_ptr, *len, *info_cls, ctx.mem);
                             });

    // Ordinal 216: NtQueryDirectoryFile (6 parameters)
    (void)reg.RegisterExport(216, "NtQueryDirectoryFile", 6, [](GuestContext& ctx) -> Result<u32> {
        auto h = ctx.ReadArg(0);
        auto out_ptr = ctx.ReadArg(5);
        auto len = ctx.ReadArg(6);
        if (!h || !out_ptr || !len)
            return Error{ErrorCode::InvalidArgument, "Missing NtQueryDirectoryFile arguments"};
        return ctx.kernel.file_services().QueryDirectoryFile(*h, *out_ptr, *len, ctx.mem);
    });
}

} // namespace xblob::kernel
