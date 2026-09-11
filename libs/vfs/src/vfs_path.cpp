#include "vfs_path.hpp"

#include <algorithm>
#include <cctype>

namespace xblob {

namespace {

std::string ToUpperAscii(std::string_view s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    return result;
}

bool IsInvalidChar(char c) {
    auto uc = static_cast<unsigned char>(c);
    if (uc < 32) {
        return true;
    }
    return c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|' || c == ':';
}

} // namespace

Result<ParsedXboxPath> ParseAndNormalizeXboxPath(std::string_view raw_path) {
    if (raw_path.empty()) {
        return Error{ErrorCode::InvalidPath, "Xbox path is empty"};
    }

    std::string_view remaining = raw_path;
    std::string mount_alias = "D";

    // Check for Device path like \Device\CdRom0 or Device\CdRom0
    if (remaining.starts_with('\\') || remaining.starts_with('/')) {
        remaining.remove_prefix(1);
    }

    std::string first_part;
    size_t next_slash = remaining.find_first_of("/\\");
    if (next_slash == std::string_view::npos) {
        first_part = ToUpperAscii(remaining);
    } else {
        first_part = ToUpperAscii(remaining.substr(0, next_slash));
    }

    if (first_part == "DEVICE") {
        if (next_slash == std::string_view::npos) {
            return Error{ErrorCode::InvalidPath, "Incomplete device path"};
        }
        remaining = remaining.substr(next_slash + 1);
        next_slash = remaining.find_first_of("/\\");
        std::string dev_name;
        if (next_slash == std::string_view::npos) {
            dev_name = ToUpperAscii(remaining);
            remaining = "";
        } else {
            dev_name = ToUpperAscii(remaining.substr(0, next_slash));
            remaining = remaining.substr(next_slash + 1);
        }

        if (dev_name == "CDROM0") {
            mount_alias = "D";
        } else {
            mount_alias = "DEVICE\\" + dev_name;
        }
    } else if (remaining.size() >= 2 && remaining[1] == ':') {
        char drive = static_cast<char>(std::toupper(static_cast<unsigned char>(remaining[0])));
        if (drive < 'A' || drive > 'Z') {
            return Error{ErrorCode::InvalidPath, "Invalid drive letter"};
        }
        mount_alias = std::string(1, drive);
        remaining.remove_prefix(2);
    }

    // Now normalize remaining path
    std::string normalized_relative;
    size_t i = 0;
    while (i < remaining.size()) {
        // Skip slashes
        while (i < remaining.size() && (remaining[i] == '/' || remaining[i] == '\\')) {
            i++;
        }
        if (i >= remaining.size()) {
            break;
        }

        size_t start = i;
        while (i < remaining.size() && remaining[i] != '/' && remaining[i] != '\\') {
            if (IsInvalidChar(remaining[i])) {
                return Error{ErrorCode::InvalidPath, "Path contains invalid characters"};
            }
            i++;
        }

        std::string_view comp = remaining.substr(start, i - start);
        if (comp == "." || comp == "..") {
            return Error{ErrorCode::InvalidPath, "Path traversal forbidden in Xbox VFS path"};
        }

        if (!normalized_relative.empty()) {
            normalized_relative += "\\";
        }
        normalized_relative += comp;
    }

    return ParsedXboxPath{
        .mount_alias = mount_alias,
        .relative_path = normalized_relative,
    };
}

} // namespace xblob
