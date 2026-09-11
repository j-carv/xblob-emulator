#pragma once

#include "xblob/common/error.hpp"
#include "xblob/common/result.hpp"
#include "xblob/formats/xdvdfs.hpp"
#include "xblob/io/byte_source.hpp"
#include "xblob/loader/loader_types.hpp"
#include "xblob/loader/xbe_loader.hpp"
#include "xblob/vfs/vfs.hpp"

#include <memory>
#include <string>

namespace xblob::machine {

class MachineSession;

enum class BootStage : u8 {
    Detect,
    Mount,
    Lookup,
    Parse,
    Plan,
    Apply,
};

[[nodiscard]] constexpr std::string_view ToString(BootStage stage) noexcept {
    switch (stage) {
    case BootStage::Detect:
        return "Detect";
    case BootStage::Mount:
        return "Mount";
    case BootStage::Lookup:
        return "Lookup";
    case BootStage::Parse:
        return "Parse";
    case BootStage::Plan:
        return "Plan";
    case BootStage::Apply:
        return "Apply";
    }
    return "Unknown";
}

enum class DetectedMediaType : u8 {
    DirectXbe,
    XdvdfsIso,
};

struct BootPipelineResult {
    DetectedMediaType media_type{DetectedMediaType::DirectXbe};
    std::string default_xbe_path;
    std::shared_ptr<const ByteSource> media_source;
    std::shared_ptr<const ByteSource> executable_source;
    std::shared_ptr<Vfs> vfs;
    loader::XbeLoadPlan load_plan;
    loader::InitialContext initial_context;
};

struct BootStageError {
    BootStage stage{BootStage::Detect};
    Error error;
};

class MediaBootPipeline {
public:
    [[nodiscard]] static Result<BootPipelineResult> Plan(std::shared_ptr<const ByteSource> source);

    [[nodiscard]] static Result<BootPipelineResult>
    Execute(std::shared_ptr<const ByteSource> source, MachineSession& session);
};

} // namespace xblob::machine
