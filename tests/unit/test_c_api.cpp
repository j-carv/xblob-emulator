#include "tests/fixtures/synthetic_media.hpp"
#include "tests/test_framework.hpp"
#include "xblob/c_api.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

TEST_CASE(TestCApiVersionAndCapabilities) {
    EXPECT_EQ(xblob_get_abi_version_major(), 1u);
    EXPECT_EQ(xblob_get_abi_version_minor(), 2u);
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
    EXPECT_EQ(info.abi_version_minor, 2u);
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

int main() {
    return xblob::testing::RunAllTests();
}
