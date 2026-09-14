#include "libs/c_api/src/c_api_internal.hpp"
#include "tests/fixtures/synthetic_media.hpp"
#include "tests/fixtures/synthetic_xdvdfs.hpp"
#include "tests/test_framework.hpp"
#include "xblob/c_api.h"
#include "xblob/gpu/pushbuffer_types.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

TEST_CASE(TestCApiVersionAndCapabilities) {
    EXPECT_EQ(xblob_get_abi_version_major(), 1u);
    EXPECT_EQ(xblob_get_abi_version_minor(), 5u);
    EXPECT_EQ(xblob_get_abi_version_patch(), 0u);

    uint64_t caps = xblob_get_capabilities();
    EXPECT_TRUE((caps & XBLOB_CAPABILITY_MEDIA_INSPECTION) != 0);
    EXPECT_TRUE((caps & XBLOB_CAPABILITY_DETERMINISTIC_SCHEDULER) != 0);
    EXPECT_TRUE((caps & XBLOB_CAPABILITY_GUEST_MEMORY) != 0);
    EXPECT_TRUE((caps & XBLOB_CAPABILITY_SYNTHETIC_CPU) != 0);
    EXPECT_TRUE((caps & XBLOB_CAPABILITY_GUEST_BUS) != 0);
    EXPECT_TRUE((caps & XBLOB_CAPABILITY_VIRTUAL_MEMORY) != 0);
    EXPECT_TRUE((caps & XBLOB_CAPABILITY_XBE_LOADER) != 0);
    EXPECT_TRUE((caps & XBLOB_CAPABILITY_MACHINE_SESSION) != 0);
    EXPECT_TRUE((caps & XBLOB_CAPABILITY_DIAGNOSTIC_EXECUTION) != 0);
    EXPECT_TRUE((caps & XBLOB_CAPABILITY_FRAMEBUFFER_PRESENTATION) != 0);
    EXPECT_TRUE((caps & XBLOB_CAPABILITY_NV2A_GPU) != 0);
    EXPECT_TRUE((caps & XBLOB_CAPABILITY_XDVDFS_VFS) != 0);
    EXPECT_TRUE((caps & XBLOB_CAPABILITY_MEDIA_BOOT) != 0);
    EXPECT_TRUE((caps & XBLOB_CAPABILITY_EXPERIMENTAL_TITLE_EXECUTION) != 0);
    EXPECT_TRUE((caps & XBLOB_CAPABILITY_COMPATIBILITY_DIAGNOSTICS) != 0);

    EXPECT_EQ(std::string(xblob_get_product_version()), "0.1.0");
    EXPECT_EQ(std::string(xblob_get_product_name()), "xblob");

    EXPECT_EQ(std::string(xblob_status_to_string(XBLOB_STATUS_OK)), "OK");
    EXPECT_EQ(std::string(xblob_status_to_string(XBLOB_STATUS_ERROR_INVALID_ARGUMENT)),
              "Argumento invalido");
    EXPECT_EQ(std::string(xblob_status_to_string(XBLOB_STATUS_ERROR_NOT_FOUND)),
              "Arquivo ou recurso nao encontrado");
    EXPECT_EQ(std::string(xblob_status_to_string(XBLOB_STATUS_ERROR_UNSUPPORTED_FORMAT)),
              "Formato nao suportado");
    EXPECT_EQ(std::string(xblob_status_to_string(XBLOB_STATUS_ERROR_BUFFER_TOO_SMALL)),
              "Buffer insuficiente");
    EXPECT_EQ(std::string(xblob_status_to_string(XBLOB_STATUS_ERROR_INVALID_STATE)),
              "Estado de operacao invalido");
}

TEST_CASE(TestCApiCoreInfoStructuralCompatibility) {
    // Null pointer
    EXPECT_EQ(xblob_get_core_info(nullptr), XBLOB_STATUS_ERROR_NULL_POINTER);

    // Incompatible zero or too small struct_size
    xblob_core_info_t info{};
    info.struct_size = 0;
    EXPECT_EQ(xblob_get_core_info(&info), XBLOB_STATUS_ERROR_INCOMPATIBLE_VERSION);

    info.struct_size = sizeof(uint16_t);
    EXPECT_EQ(xblob_get_core_info(&info), XBLOB_STATUS_ERROR_INCOMPATIBLE_VERSION);

    // Smaller compatible size (e.g. struct_size + abi_version_major)
    struct SmallerInfo {
        uint32_t struct_size;
        uint32_t abi_version_major;
    } smaller{};
    smaller.struct_size = sizeof(SmallerInfo);
    EXPECT_EQ(xblob_get_core_info(reinterpret_cast<xblob_core_info_t*>(&smaller)), XBLOB_STATUS_OK);
    EXPECT_EQ(smaller.abi_version_major, 1u);

    // Full exact struct size
    std::memset(&info, 0, sizeof(info));
    info.struct_size = sizeof(xblob_core_info_t);
    EXPECT_EQ(xblob_get_core_info(&info), XBLOB_STATUS_OK);
    EXPECT_EQ(info.abi_version_major, 1u);
    EXPECT_EQ(info.abi_version_minor, 5u);
    EXPECT_EQ(info.abi_version_patch, 0u);
    EXPECT_EQ(info.capabilities, xblob_get_capabilities());
    EXPECT_EQ(std::string(info.product_name), "xblob");
    EXPECT_EQ(std::string(info.product_version), "0.1.0");
}

TEST_CASE(TestCApiNullSafetyAndDestroy) {
    // Destroying null pointer must be completely safe
    xblob_media_report_destroy(nullptr);

    // Getters on null report must return NULL_POINTER
    xblob_media_type_t type = XBLOB_MEDIA_TYPE_UNKNOWN;
    EXPECT_EQ(xblob_media_report_get_type(nullptr, &type), XBLOB_STATUS_ERROR_NULL_POINTER);

    uint64_t size = 0;
    EXPECT_EQ(xblob_media_report_get_file_size(nullptr, &size), XBLOB_STATUS_ERROR_NULL_POINTER);

    size_t buf_sz = 128;
    char buf[128];
    EXPECT_EQ(xblob_media_report_get_file_path(nullptr, buf, &buf_sz),
              XBLOB_STATUS_ERROR_NULL_POINTER);
    EXPECT_EQ(xblob_media_report_get_title(nullptr, buf, &buf_sz), XBLOB_STATUS_ERROR_NULL_POINTER);
    EXPECT_EQ(xblob_media_report_get_human_summary(nullptr, buf, &buf_sz),
              XBLOB_STATUS_ERROR_NULL_POINTER);
    EXPECT_EQ(xblob_media_report_get_error_message(nullptr, buf, &buf_sz),
              XBLOB_STATUS_ERROR_NULL_POINTER);

    uint32_t u32_val = 0;
    int has_val = 0;
    EXPECT_EQ(xblob_media_report_get_title_id(nullptr, &u32_val, &has_val),
              XBLOB_STATUS_ERROR_NULL_POINTER);
    EXPECT_EQ(xblob_media_report_get_disk_number(nullptr, &u32_val, &has_val),
              XBLOB_STATUS_ERROR_NULL_POINTER);
    EXPECT_EQ(xblob_media_report_get_game_region(nullptr, &u32_val, &has_val),
              XBLOB_STATUS_ERROR_NULL_POINTER);
    EXPECT_EQ(xblob_media_report_get_entry_point(nullptr, &u32_val, &has_val),
              XBLOB_STATUS_ERROR_NULL_POINTER);
    EXPECT_EQ(xblob_media_report_get_section_count(nullptr, &u32_val),
              XBLOB_STATUS_ERROR_NULL_POINTER);
}

TEST_CASE(TestCApiInvalidInspectArguments) {
    xblob_media_report_t report = reinterpret_cast<xblob_media_report_t>(0xDEADBEEF);

    // Null path
    EXPECT_EQ(xblob_media_inspect(nullptr, 10, &report), XBLOB_STATUS_ERROR_NULL_POINTER);
    EXPECT_TRUE(report == nullptr);

    // Zero length path
    report = reinterpret_cast<xblob_media_report_t>(0xDEADBEEF);
    EXPECT_EQ(xblob_media_inspect("some/path", 0, &report), XBLOB_STATUS_ERROR_INVALID_ARGUMENT);
    EXPECT_TRUE(report == nullptr);

    // Null out_report pointer
    EXPECT_EQ(xblob_media_inspect("some/path", 9, nullptr), XBLOB_STATUS_ERROR_NULL_POINTER);

    // Invalid UTF-8 path
    const char invalid_utf8[] = {'\xFF', '\xFE', '\x00'};
    report = reinterpret_cast<xblob_media_report_t>(0xDEADBEEF);
    EXPECT_EQ(xblob_media_inspect(invalid_utf8, 2, &report), XBLOB_STATUS_ERROR_INVALID_ARGUMENT);
    EXPECT_TRUE(report == nullptr);

    // Non-existent file
    std::string non_existent = "/nonexistent_path_xblob_123456789.xbe";
    report = reinterpret_cast<xblob_media_report_t>(0xDEADBEEF);
    EXPECT_EQ(xblob_media_inspect(non_existent.c_str(), non_existent.size(), &report),
              XBLOB_STATUS_ERROR_NOT_FOUND);
    EXPECT_TRUE(report == nullptr);

    // File with invalid format (plain text file)
    auto temp_dir = std::filesystem::temp_directory_path();
    auto bad_file = temp_dir / "xblob_test_bad_format.bin";
    {
        std::ofstream ofs(bad_file, std::ios::binary);
        ofs << "This is definitely not an XBE or XISO file at all.";
    }
    std::string bad_str = bad_file.string();
    report = reinterpret_cast<xblob_media_report_t>(0xDEADBEEF);
    EXPECT_EQ(xblob_media_inspect(bad_str.c_str(), bad_str.size(), &report),
              XBLOB_STATUS_ERROR_UNSUPPORTED_FORMAT);
    EXPECT_TRUE(report == nullptr);

    std::filesystem::remove(bad_file);
}

TEST_CASE(TestCApiInspectSyntheticXbeAndTwoCallBuffer) {
    auto xbe_data = xblob::testing::CreateValidSyntheticXbe(0x12345678, "Synthetic Game", 2);
    auto temp_dir = std::filesystem::temp_directory_path();
    auto xbe_path = temp_dir / "xblob_test_c_api.xbe";
    {
        std::ofstream ofs(xbe_path, std::ios::binary);
        ofs.write(reinterpret_cast<const char*>(xbe_data.data()),
                  static_cast<std::streamsize>(xbe_data.size()));
    }

    std::string path_str = xbe_path.string();
    xblob_media_report_t report = nullptr;
    xblob_status_t status = xblob_media_inspect(path_str.c_str(), path_str.size(), &report);
    EXPECT_EQ(status, XBLOB_STATUS_OK);
    EXPECT_TRUE(report != nullptr);

    // Media type
    xblob_media_type_t type = XBLOB_MEDIA_TYPE_UNKNOWN;
    EXPECT_EQ(xblob_media_report_get_type(report, &type), XBLOB_STATUS_OK);
    EXPECT_EQ(type, XBLOB_MEDIA_TYPE_XBE);

    // File size
    uint64_t file_size = 0;
    EXPECT_EQ(xblob_media_report_get_file_size(report, &file_size), XBLOB_STATUS_OK);
    EXPECT_EQ(file_size, static_cast<uint64_t>(xbe_data.size()));

    // Two-call buffer for file path:
    // 1. Buffer null: queries required size
    size_t req_path_sz = 0;
    EXPECT_EQ(xblob_media_report_get_file_path(report, nullptr, &req_path_sz), XBLOB_STATUS_OK);
    EXPECT_EQ(req_path_sz, path_str.size() + 1);

    // 2. Buffer too small
    char small_buf[4];
    size_t small_sz = sizeof(small_buf);
    EXPECT_EQ(xblob_media_report_get_file_path(report, small_buf, &small_sz),
              XBLOB_STATUS_ERROR_BUFFER_TOO_SMALL);
    EXPECT_EQ(small_sz, req_path_sz);

    // 3. Buffer exact
    std::vector<char> exact_path(req_path_sz);
    size_t exact_sz = req_path_sz;
    EXPECT_EQ(xblob_media_report_get_file_path(report, exact_path.data(), &exact_sz),
              XBLOB_STATUS_OK);
    EXPECT_EQ(std::string(exact_path.data()), path_str);

    // Two-call buffer for title
    size_t title_sz = 0;
    EXPECT_EQ(xblob_media_report_get_title(report, nullptr, &title_sz), XBLOB_STATUS_OK);
    std::vector<char> title_buf(title_sz);
    EXPECT_EQ(xblob_media_report_get_title(report, title_buf.data(), &title_sz), XBLOB_STATUS_OK);
    EXPECT_EQ(std::string(title_buf.data()), "Synthetic Game");

    // Title ID
    uint32_t title_id = 0;
    int has_title_id = 0;
    EXPECT_EQ(xblob_media_report_get_title_id(report, &title_id, &has_title_id), XBLOB_STATUS_OK);
    EXPECT_EQ(has_title_id, 1);
    EXPECT_EQ(title_id, 0x12345678u);

    // Disk number & region
    uint32_t disk_num = 0;
    int has_disk_num = 0;
    EXPECT_EQ(xblob_media_report_get_disk_number(report, &disk_num, &has_disk_num),
              XBLOB_STATUS_OK);
    EXPECT_EQ(has_disk_num, 1);
    EXPECT_EQ(disk_num, 1u);

    uint32_t region = 0;
    int has_region = 0;
    EXPECT_EQ(xblob_media_report_get_game_region(report, &region, &has_region), XBLOB_STATUS_OK);
    EXPECT_EQ(has_region, 1);

    // Entry point & sections
    uint32_t entry_point = 0;
    int has_entry_point = 0;
    EXPECT_EQ(xblob_media_report_get_entry_point(report, &entry_point, &has_entry_point),
              XBLOB_STATUS_OK);
    EXPECT_EQ(has_entry_point, 1);
    EXPECT_EQ(entry_point, 0x00011000u);

    uint32_t sec_count = 0;
    EXPECT_EQ(xblob_media_report_get_section_count(report, &sec_count), XBLOB_STATUS_OK);
    EXPECT_EQ(sec_count, 2u);

    // Human readable summary
    size_t summary_sz = 0;
    EXPECT_EQ(xblob_media_report_get_human_summary(report, nullptr, &summary_sz), XBLOB_STATUS_OK);
    EXPECT_TRUE(summary_sz > 0);
    std::vector<char> summary_buf(summary_sz);
    EXPECT_EQ(xblob_media_report_get_human_summary(report, summary_buf.data(), &summary_sz),
              XBLOB_STATUS_OK);
    EXPECT_TRUE(std::string(summary_buf.data()).find("Synthetic Game") != std::string::npos);

    xblob_media_report_destroy(report);
    std::filesystem::remove(xbe_path);
}

TEST_CASE(TestCApiInspectSyntheticXiso) {
    auto xiso_data = xblob::testing::CreateValidTrimmedXiso();
    auto temp_dir = std::filesystem::temp_directory_path();
    auto xiso_path = temp_dir / "xblob_test_c_api.iso";
    {
        std::ofstream ofs(xiso_path, std::ios::binary);
        ofs.write(reinterpret_cast<const char*>(xiso_data.data()),
                  static_cast<std::streamsize>(xiso_data.size()));
    }

    std::string path_str = xiso_path.string();
    xblob_media_report_t report = nullptr;
    EXPECT_EQ(xblob_media_inspect(path_str.c_str(), path_str.size(), &report), XBLOB_STATUS_OK);
    EXPECT_TRUE(report != nullptr);

    xblob_media_type_t type = XBLOB_MEDIA_TYPE_UNKNOWN;
    EXPECT_EQ(xblob_media_report_get_type(report, &type), XBLOB_STATUS_OK);
    EXPECT_EQ(type, XBLOB_MEDIA_TYPE_XISO_TRIMMED);

    uint64_t file_size = 0;
    EXPECT_EQ(xblob_media_report_get_file_size(report, &file_size), XBLOB_STATUS_OK);
    EXPECT_EQ(file_size, static_cast<uint64_t>(xiso_data.size()));

    xblob_media_report_destroy(report);
    std::filesystem::remove(xiso_path);
}

TEST_CASE(TestCApiMachineSessionAndDiagnostics) {
    xblob_machine_t machine = nullptr;
    EXPECT_EQ(xblob_machine_create(&machine), XBLOB_STATUS_OK);
    EXPECT_TRUE(machine != nullptr);

    xblob_machine_state_t state = XBLOB_MACHINE_STATE_FAULTED;
    EXPECT_EQ(xblob_machine_get_state(machine, &state), XBLOB_STATUS_OK);
    EXPECT_EQ(state, XBLOB_MACHINE_STATE_CREATED);

    // Prepare synthetic XBE
    auto xbe_data = xblob::testing::CreateValidSyntheticXbe(0x12345678, "Synthetic Game", 2);
    auto temp_dir = std::filesystem::temp_directory_path();
    auto xbe_path = temp_dir / "xblob_test_machine_prep.xbe";
    {
        std::ofstream ofs(xbe_path, std::ios::binary);
        ofs.write(reinterpret_cast<const char*>(xbe_data.data()),
                  static_cast<std::streamsize>(xbe_data.size()));
    }

    std::string path_str = xbe_path.string();
    xblob_prepare_diagnostic_t diag{};
    diag.struct_size = sizeof(xblob_prepare_diagnostic_t);
    EXPECT_EQ(xblob_machine_prepare_xbe(machine, path_str.c_str(), path_str.size(), &diag),
              XBLOB_STATUS_OK);

    EXPECT_EQ(diag.state, XBLOB_MACHINE_STATE_PREPARED);
    EXPECT_EQ(diag.is_prepared, 1);
    EXPECT_EQ(diag.entry_point, 0x00011000u);
    EXPECT_EQ(diag.section_count, 2u);
    EXPECT_EQ(diag.title_id, 0x12345678u);
    EXPECT_EQ(std::string(diag.title_name), "Synthetic Game");
    EXPECT_EQ(diag.ram_size_bytes, 64ULL * 1024 * 1024);

    // Compatibility check: query diagnostic with older/smaller struct size
    struct SmallerDiag {
        uint32_t struct_size;
        xblob_machine_state_t state;
        uint32_t entry_point;
    } smaller_diag{};
    smaller_diag.struct_size = sizeof(SmallerDiag);
    EXPECT_EQ(xblob_machine_get_diagnostic(
                  machine, reinterpret_cast<xblob_prepare_diagnostic_t*>(&smaller_diag)),
              XBLOB_STATUS_OK);
    EXPECT_EQ(smaller_diag.state, XBLOB_MACHINE_STATE_PREPARED);
    EXPECT_EQ(smaller_diag.entry_point, 0x00011000u);

    // Destroy machine
    xblob_machine_destroy(machine);
    // Destroying null pointer must be safe
    xblob_machine_destroy(nullptr);

    std::filesystem::remove(xbe_path);
}

TEST_CASE(TestCApiExecutionAndTrace) {
    xblob_machine_t machine = nullptr;
    EXPECT_EQ(xblob_machine_create(&machine), XBLOB_STATUS_OK);
    EXPECT_TRUE(machine != nullptr);

    // Prepare synthetic XBE with HLT
    auto xbe_data = xblob::testing::CreateValidSyntheticXbe(0x12345678, "Synthetic Exec", 1);
    xbe_data[0x1000] = 0xF4; // HLT opcode at entry point
    auto temp_dir = std::filesystem::temp_directory_path();
    auto xbe_path = temp_dir / "xblob_test_machine_exec.xbe";
    {
        std::ofstream ofs(xbe_path, std::ios::binary);
        ofs.write(reinterpret_cast<const char*>(xbe_data.data()),
                  static_cast<std::streamsize>(xbe_data.size()));
    }

    std::string path_str = xbe_path.string();
    EXPECT_EQ(xblob_machine_prepare_xbe(machine, path_str.c_str(), path_str.size(), nullptr),
              XBLOB_STATUS_OK);

    // Pause
    EXPECT_EQ(xblob_machine_pause(machine), XBLOB_STATUS_OK);

    // Step with budget
    xblob_machine_execution_result_t step_res{};
    step_res.struct_size = sizeof(xblob_machine_execution_result_t);
    EXPECT_EQ(xblob_machine_step(machine, 1, &step_res), XBLOB_STATUS_OK);
    EXPECT_EQ(step_res.instructions_executed, 1ULL);
    EXPECT_EQ(step_res.state, XBLOB_MACHINE_STATE_PAUSED);

    // Trace summary check
    xblob_trace_summary_t summary{};
    summary.struct_size = sizeof(xblob_trace_summary_t);
    EXPECT_EQ(xblob_machine_get_trace_summary(machine, &summary), XBLOB_STATUS_OK);
    EXPECT_TRUE(summary.event_count >= 1ULL);
    EXPECT_TRUE(summary.total_recorded >= 1ULL);
    EXPECT_EQ(summary.dropped_count, 0ULL);

    // Clear trace
    EXPECT_EQ(xblob_machine_clear_trace(machine), XBLOB_STATUS_OK);
    EXPECT_EQ(xblob_machine_get_trace_summary(machine, &summary), XBLOB_STATUS_OK);
    EXPECT_EQ(summary.event_count, 0ULL);

    // Stop
    EXPECT_EQ(xblob_machine_stop(machine), XBLOB_STATUS_OK);

    xblob_machine_destroy(machine);
    std::filesystem::remove(xbe_path);
}

TEST_CASE(TestCApiFramebufferPresentationAndBounds) {
    xblob_machine_t machine = nullptr;
    EXPECT_EQ(xblob_machine_create(&machine), XBLOB_STATUS_OK);
    EXPECT_TRUE(machine != nullptr);

    // Null safety
    xblob_frame_metadata_t meta{};
    meta.struct_size = sizeof(xblob_frame_metadata_t);
    EXPECT_EQ(xblob_machine_get_frame_metadata(nullptr, &meta), XBLOB_STATUS_ERROR_NULL_POINTER);
    EXPECT_EQ(xblob_machine_get_frame_metadata(machine, nullptr), XBLOB_STATUS_ERROR_NULL_POINTER);

    size_t inout_sz = 100;
    std::vector<uint8_t> test_buf(100);
    EXPECT_EQ(xblob_machine_copy_frame_pixels(nullptr, test_buf.data(), &inout_sz),
              XBLOB_STATUS_ERROR_NULL_POINTER);
    EXPECT_EQ(xblob_machine_copy_frame_pixels(machine, test_buf.data(), nullptr),
              XBLOB_STATUS_ERROR_NULL_POINTER);

    // Version checking
    meta.struct_size = 0;
    EXPECT_EQ(xblob_machine_get_frame_metadata(machine, &meta),
              XBLOB_STATUS_ERROR_INCOMPATIBLE_VERSION);

    // Initial state: no frames generated yet
    meta.struct_size = sizeof(xblob_frame_metadata_t);
    EXPECT_EQ(xblob_machine_get_frame_metadata(machine, &meta), XBLOB_STATUS_OK);
    EXPECT_EQ(meta.is_valid, 0);
    EXPECT_EQ(meta.sequence_number, 0ULL);
    EXPECT_EQ(meta.buffer_size, 0u);

    // Two-call with no frames returns 0 bytes needed
    size_t req_sz = 999;
    EXPECT_EQ(xblob_machine_copy_frame_pixels(machine, nullptr, &req_sz), XBLOB_STATUS_OK);
    EXPECT_EQ(req_sz, 0u);

    // Program a pushbuffer flip via machine session
    const xblob::GuestAddr pb_addr = 0x00010000u;
    const std::vector<xblob::u32> pb_words = {
        (2u << 18) | xblob::gpu::kMethodClearColor,
        0xFF00FF11u, // RGBA color (R=0x11, G=0xFF, B=0x00, A=0xFF)
        1u,          // Clear trigger
        (1u << 18) | xblob::gpu::kMethodFlip,
        1u, // Flip trigger
    };

    for (std::size_t i = 0; i < pb_words.size(); ++i) {
        EXPECT_TRUE(machine->session->address_space()
                        .Write32(static_cast<xblob::GuestAddr>(pb_addr + i * 4), pb_words[i])
                        .has_value());
    }

    auto pb_res =
        machine->session->ExecutePushbuffer(pb_addr, static_cast<xblob::u32>(pb_words.size()));
    EXPECT_TRUE(pb_res.has_value());

    // Advance scheduler / run events so flip completes
    EXPECT_TRUE(machine->session->scheduler().StepCycles(1000).has_value());

    // Frame metadata should now reflect the presented frame (default 640x480, pitch 2560)
    meta.struct_size = sizeof(xblob_frame_metadata_t);
    EXPECT_EQ(xblob_machine_get_frame_metadata(machine, &meta), XBLOB_STATUS_OK);
    EXPECT_EQ(meta.is_valid, 1);
    EXPECT_EQ(meta.width, 640u);
    EXPECT_EQ(meta.height, 480u);
    EXPECT_EQ(meta.pitch, 2560u);
    EXPECT_EQ(meta.buffer_size, 1228800u); // 640 * 480 * 4
    EXPECT_EQ(meta.sequence_number, 1ULL);

    // Two-call pattern: call 1 with null buffer query
    size_t query_sz = 0;
    EXPECT_EQ(xblob_machine_copy_frame_pixels(machine, nullptr, &query_sz), XBLOB_STATUS_OK);
    EXPECT_EQ(query_sz, 1228800u);

    // Insufficient buffer capacity: must return BUFFER_TOO_SMALL, set required size, and write no
    // bytes
    std::vector<uint8_t> tiny_buf(10, 0xAA);
    size_t tiny_sz = 10;
    EXPECT_EQ(xblob_machine_copy_frame_pixels(machine, tiny_buf.data(), &tiny_sz),
              XBLOB_STATUS_ERROR_BUFFER_TOO_SMALL);
    EXPECT_EQ(tiny_sz, 1228800u);
    for (uint8_t b : tiny_buf) {
        EXPECT_EQ(b, 0xAA);
    }

    // Full buffer copy: must succeed and copy golden pixels (RGBA 0x11, 0xFF, 0x00, 0xFF)
    std::vector<uint8_t> full_buf(1228800, 0);
    size_t full_sz = 1228800;
    EXPECT_EQ(xblob_machine_copy_frame_pixels(machine, full_buf.data(), &full_sz), XBLOB_STATUS_OK);
    EXPECT_EQ(full_sz, 1228800u);
    EXPECT_EQ(full_buf[0], 0x11); // R
    EXPECT_EQ(full_buf[1], 0xFF); // G
    EXPECT_EQ(full_buf[2], 0x00); // B
    EXPECT_EQ(full_buf[3], 0xFF); // A

    xblob_machine_destroy(machine);
}

TEST_CASE(TestCApiPrepareMediaAndBootReport) {
    xblob_machine_t machine = nullptr;
    EXPECT_EQ(xblob_machine_create(&machine), XBLOB_STATUS_OK);
    EXPECT_TRUE(machine != nullptr);

    // Create synthetic XDVDFS image containing default.xbe
    auto xbe_data = xblob::testing::CreateValidSyntheticXbe(0x55556666, "Media Boot Game", 1);
    xbe_data[0x1000] = 0xF4; // HLT

    auto img = xblob::testing::BuildValidTrimmedXdvdfsImage({
        {"DEFAULT.XBE", xbe_data},
        {"SYSTEM/CONFIG.TXT", {'O', 'K'}},
    });

    auto temp_dir = std::filesystem::temp_directory_path();
    auto iso_path = temp_dir / "xblob_test_prepare_media.iso";
    {
        std::ofstream ofs(iso_path, std::ios::binary);
        ofs.write(reinterpret_cast<const char*>(img.data()),
                  static_cast<std::streamsize>(img.size()));
    }

    std::string path_str = iso_path.string();

    // Incompatible boot report struct_size
    xblob_boot_report_t report{};
    report.struct_size = 12; // Too small
    EXPECT_EQ(xblob_machine_prepare_media(machine, path_str.c_str(), path_str.size(), &report),
              XBLOB_STATUS_ERROR_INVALID_ARGUMENT);

    // Prepare media with valid report
    report.struct_size = sizeof(xblob_boot_report_t);
    EXPECT_EQ(xblob_machine_prepare_media(machine, path_str.c_str(), path_str.size(), &report),
              XBLOB_STATUS_OK);

    EXPECT_EQ(report.is_bootable, 1);
    EXPECT_EQ(report.title_id, 0x55556666u);
    EXPECT_EQ(std::string(report.title_name), "Media Boot Game");
    EXPECT_EQ(std::string(report.default_xbe_path), "D:\\DEFAULT.XBE");
    EXPECT_EQ(report.section_count, 1u);

    // Verify machine state is PREPARED
    xblob_machine_state_t state = XBLOB_MACHINE_STATE_CREATED;
    EXPECT_EQ(xblob_machine_get_state(machine, &state), XBLOB_STATUS_OK);
    EXPECT_EQ(state, XBLOB_MACHINE_STATE_PREPARED);

    xblob_machine_destroy(machine);
    std::filesystem::remove(iso_path);
}

TEST_CASE(TestCApiVfsBrowserPagination) {
    auto img = xblob::testing::BuildValidTrimmedXdvdfsImage({
        {"FILE1.BIN", {'A'}},
        {"FILE2.BIN", {'B'}},
        {"FILE3.BIN", {'C'}},
        {"SUBDIR/TEST.DAT", {'D'}},
    });

    auto temp_dir = std::filesystem::temp_directory_path();
    auto iso_path = temp_dir / "xblob_test_vfs_browser.iso";
    {
        std::ofstream ofs(iso_path, std::ios::binary);
        ofs.write(reinterpret_cast<const char*>(img.data()),
                  static_cast<std::streamsize>(img.size()));
    }

    std::string path_str = iso_path.string();
    xblob_vfs_browser_t browser = nullptr;
    EXPECT_EQ(xblob_vfs_browser_create(path_str.c_str(), path_str.size(), &browser),
              XBLOB_STATUS_OK);
    EXPECT_TRUE(browser != nullptr);

    // Query count of root directory
    uint32_t count = 0;
    EXPECT_EQ(xblob_vfs_browser_get_entry_count(browser, "", 0, &count), XBLOB_STATUS_OK);
    EXPECT_EQ(count, 4u); // FILE1, FILE2, FILE3, SUBDIR

    // Two-call pagination: Page 1 (offset 0, limit 2)
    uint32_t page1_count = 0;
    EXPECT_EQ(xblob_vfs_browser_list_entries(browser, "", 0, 0, 2, nullptr, &page1_count),
              XBLOB_STATUS_OK);
    EXPECT_EQ(page1_count, 2u);

    std::vector<xblob_dir_entry_t> entries(page1_count);
    EXPECT_EQ(xblob_vfs_browser_list_entries(browser, "", 0, 0, 2, entries.data(), &page1_count),
              XBLOB_STATUS_OK);
    EXPECT_EQ(page1_count, 2u);
    EXPECT_FALSE(std::string(entries[0].name).empty());
    EXPECT_FALSE(std::string(entries[1].name).empty());

    // Page 2 (offset 2, limit 2)
    uint32_t page2_count = 2;
    std::vector<xblob_dir_entry_t> entries2(2);
    EXPECT_EQ(xblob_vfs_browser_list_entries(browser, "", 0, 2, 2, entries2.data(), &page2_count),
              XBLOB_STATUS_OK);
    EXPECT_EQ(page2_count, 2u);

    // Page 3 beyond end (offset 4, limit 2) -> 0 entries
    uint32_t page3_count = 2;
    std::vector<xblob_dir_entry_t> entries3(2);
    EXPECT_EQ(xblob_vfs_browser_list_entries(browser, "", 0, 4, 2, entries3.data(), &page3_count),
              XBLOB_STATUS_OK);
    EXPECT_EQ(page3_count, 0u);

    // Browse subdirectory "SUBDIR"
    uint32_t sub_count = 0;
    EXPECT_EQ(xblob_vfs_browser_get_entry_count(browser, "SUBDIR", 6, &sub_count), XBLOB_STATUS_OK);
    EXPECT_EQ(sub_count, 1u);

    xblob_vfs_browser_destroy(browser);
    std::filesystem::remove(iso_path);
}

TEST_CASE(TestCApiExperimentalExecutionAndDiagnostics) {
    xblob_machine_t machine = nullptr;
    EXPECT_EQ(xblob_machine_create(&machine), XBLOB_STATUS_OK);
    EXPECT_TRUE(machine != nullptr);

    // Create synthetic XBE: ADD EAX, 42; HLT
    auto xbe_data = xblob::testing::CreateValidSyntheticXbe(0x77778888, "ABI 1.5 Game", 1);
    xbe_data[0x1000] = 0x83;
    xbe_data[0x1001] = 0xC0;
    xbe_data[0x1002] = 0x2A; // ADD EAX, 42
    xbe_data[0x1003] = 0xF4; // HLT

    auto temp_dir = std::filesystem::temp_directory_path();
    auto xbe_path = temp_dir / "xblob_test_abi15.xbe";
    {
        std::ofstream ofs(xbe_path, std::ios::binary);
        ofs.write(reinterpret_cast<const char*>(xbe_data.data()),
                  static_cast<std::streamsize>(xbe_data.size()));
    }

    std::string path_str = xbe_path.string();
    xblob_prepare_diagnostic_t prep_diag{};
    prep_diag.struct_size = sizeof(xblob_prepare_diagnostic_t);
    EXPECT_EQ(xblob_machine_prepare_xbe(machine, path_str.c_str(), path_str.size(), &prep_diag),
              XBLOB_STATUS_OK);

    // Test start execution with budgets
    xblob_execution_budgets_t budgets{};
    budgets.struct_size = sizeof(xblob_execution_budgets_t);
    budgets.max_instructions = 100;
    budgets.max_cycles = 1000;
    budgets.chunk_instructions = 10;
    budgets.max_wall_time_ms = 1000;

    EXPECT_EQ(xblob_machine_start_execution(machine, &budgets), XBLOB_STATUS_OK);

    int completed = 0;
    EXPECT_EQ(xblob_machine_wait_completion(machine, 2000, &completed), XBLOB_STATUS_OK);
    EXPECT_EQ(completed, 1);

    // Snapshot
    xblob_machine_snapshot_t snap{};
    snap.struct_size = sizeof(xblob_machine_snapshot_t);
    EXPECT_EQ(xblob_machine_get_snapshot(machine, &snap), XBLOB_STATUS_OK);
    EXPECT_EQ(snap.state, XBLOB_MACHINE_STATE_PAUSED);
    EXPECT_EQ(snap.stop_reason_code, XBLOB_STOP_REASON_HALTED);
    EXPECT_EQ(snap.registers.eax, 42u);
    EXPECT_EQ(snap.instructions_executed, 2u);
    EXPECT_EQ(snap.stack_valid, 1);

    // Compatibility diagnostic
    xblob_compatibility_diagnostic_t diag{};
    diag.struct_size = sizeof(xblob_compatibility_diagnostic_t);
    EXPECT_EQ(xblob_machine_get_compatibility_diagnostic(machine, &diag), XBLOB_STATUS_OK);
    EXPECT_EQ(diag.first_blocker_code, XBLOB_STOP_REASON_HALTED);

    // Trace text two-call
    size_t trace_len = 0;
    EXPECT_EQ(xblob_machine_get_trace_text(machine, nullptr, &trace_len), XBLOB_STATUS_OK);
    EXPECT_TRUE(trace_len > 0);
    std::string trace_str(trace_len, '\0');
    EXPECT_EQ(xblob_machine_get_trace_text(machine, trace_str.data(), &trace_len), XBLOB_STATUS_OK);

    xblob_machine_destroy(machine);
    std::filesystem::remove(xbe_path);
}

int main() {
    return xblob::testing::RunAllTests();
}
