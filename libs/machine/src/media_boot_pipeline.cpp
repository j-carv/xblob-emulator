#include "xblob/machine/media_boot_pipeline.hpp"

#include "xblob/machine/machine_session.hpp"
#include "xblob/vfs/xdvdfs_vfs_volume.hpp"

namespace xblob::machine {

Result<BootPipelineResult> MediaBootPipeline::Plan(std::shared_ptr<const ByteSource> source) {
    if (!source) {
        return Error{ErrorCode::InvalidArgument, "Media source cannot be null"};
    }

    BootPipelineResult result;
    result.media_source = source;

    // Stage 1: Detect
    bool is_xbe = false;
    if (source->size() >= 4) {
        u8 magic[4] = {0};
        auto r = source->ReadAt(0, magic);
        if (r.has_value() && magic[0] == 'X' && magic[1] == 'B' && magic[2] == 'E' &&
            magic[3] == 'H') {
            is_xbe = true;
        }
    }

    if (is_xbe) {
        result.media_type = DetectedMediaType::DirectXbe;
        result.default_xbe_path = "DEFAULT.XBE";
        result.executable_source = source;
        result.vfs = std::make_shared<Vfs>();
    } else {
        // Try opening as XDVDFS
        auto vol_res = XdvdfsVolume::Open(source);
        if (!vol_res) {
            return Error{ErrorCode::UnknownFormat,
                         "Unsupported media format: neither direct XBE nor valid XDVDFS image"};
        }

        result.media_type = DetectedMediaType::XdvdfsIso;

        // Stage 2: Mount
        auto vfs = std::make_shared<Vfs>();
        auto vfs_vol = std::make_shared<XdvdfsVfsVolume>(*vol_res);
        auto m1 = vfs->Mount("D", vfs_vol);
        if (!m1) {
            return m1.error();
        }
        (void)vfs->Mount("DEVICE\\CDROM0", vfs_vol);
        result.vfs = std::move(vfs);

        // Stage 3: Lookup
        auto find_res = (*vol_res)->FindEntry("default.xbe");
        if (!find_res) {
            return Error{ErrorCode::FileNotFound, "Could not locate default.xbe on disc image"};
        }

        auto file_res = (*vol_res)->OpenFile("default.xbe");
        if (!file_res) {
            return file_res.error();
        }

        result.default_xbe_path = "D:\\DEFAULT.XBE";
        result.executable_source = *file_res;
    }

    // Stage 4 & 5: Parse and Plan
    auto plan_res = loader::XbeLoader::Plan(*result.executable_source);
    if (!plan_res) {
        return plan_res.error();
    }
    result.load_plan = *plan_res;
    result.initial_context = loader::XbeLoader::CreateInitialContext(*plan_res);

    return result;
}

Result<BootPipelineResult> MediaBootPipeline::Execute(std::shared_ptr<const ByteSource> source,
                                                      MachineSession& session) {
    auto plan_res = Plan(source);
    if (!plan_res) {
        return plan_res.error();
    }

    auto prep_res = session.PrepareMedia(source);
    if (!prep_res) {
        return prep_res.error();
    }

    return plan_res;
}

} // namespace xblob::machine
