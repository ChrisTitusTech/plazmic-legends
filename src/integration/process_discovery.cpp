#include "integration/process_discovery.h"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <charconv>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <system_error>

namespace plazmic {
namespace {

std::optional<std::string> read_first_line(const std::filesystem::path& path) {
    std::ifstream input(path);
    std::string line;
    if (!input || !std::getline(input, line)) {
        return std::nullopt;
    }
    return line;
}

std::optional<uid_t> read_real_uid(const std::filesystem::path& status_path) {
    std::ifstream input(status_path);
    std::string line;
    while (std::getline(input, line)) {
        if (!line.starts_with("Uid:")) {
            continue;
        }
        std::istringstream fields(line.substr(4));
        unsigned long uid = 0;
        if (fields >> uid &&
            uid <=
                static_cast<unsigned long>(
                    std::numeric_limits<uid_t>::max())) {
            return static_cast<uid_t>(uid);
        }
    }
    return std::nullopt;
}

std::optional<std::uintptr_t> parse_hex_address(std::string_view value) {
    std::uintptr_t result = 0;
    const auto parsed =
        std::from_chars(value.data(), value.data() + value.size(), result, 16);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
        return std::nullopt;
    }
    return result;
}

std::optional<std::uint64_t> parse_hex_offset(std::string_view value) {
    std::uint64_t result = 0;
    const auto parsed =
        std::from_chars(value.data(), value.data() + value.size(), result, 16);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
        return std::nullopt;
    }
    return result;
}

std::optional<ProcessMapping> parse_mapping(const std::string& line) {
    std::istringstream fields(line);
    std::string range;
    std::string permissions;
    std::string offset;
    std::string device;
    std::string inode;
    if (!(fields >> range >> permissions >> offset >> device >> inode)) {
        return std::nullopt;
    }

    const std::size_t separator = range.find('-');
    if (separator == std::string::npos) {
        return std::nullopt;
    }
    const auto begin = parse_hex_address(
        std::string_view(range).substr(0, separator));
    const auto end = parse_hex_address(
        std::string_view(range).substr(separator + 1));
    const auto file_offset = parse_hex_offset(offset);
    if (!begin || !end || !file_offset || *begin >= *end ||
        permissions.size() != 4U) {
        return std::nullopt;
    }

    std::string path;
    std::getline(fields, path);
    const std::size_t first = path.find_first_not_of(' ');
    if (first != std::string::npos) {
        path.erase(0, first);
    } else {
        path.clear();
    }
    constexpr std::string_view kDeletedSuffix = " (deleted)";
    if (path.ends_with(kDeletedSuffix)) {
        return std::nullopt;
    }

    return ProcessMapping{
        .begin = *begin,
        .end = *end,
        .file_offset = *file_offset,
        .permissions = permissions,
        .path = path,
    };
}

std::filesystem::path normalized_path(const std::filesystem::path& path) {
    std::error_code error;
    const auto canonical = std::filesystem::weakly_canonical(path, error);
    return error ? path.lexically_normal() : canonical;
}

std::optional<pid_t> directory_pid(const std::filesystem::path& path) {
    const std::string name = path.filename().string();
    int value = 0;
    const auto parsed =
        std::from_chars(name.data(), name.data() + name.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != name.data() + name.size() ||
        value <= 0) {
        return std::nullopt;
    }
    return static_cast<pid_t>(value);
}

}  // namespace

DiscoveryResult discover_client_process(
    const std::filesystem::path& client_executable,
    const std::filesystem::path& proc_root,
    uid_t expected_uid) {
    const auto expected_path = normalized_path(client_executable);
    std::vector<ClientProcess> candidates;
    bool eqgame_access_failed = false;
    std::error_code iterator_error;
    std::filesystem::directory_iterator entries(proc_root, iterator_error);
    if (iterator_error) {
        return {
            .error = DiscoveryError::proc_access,
            .process = {},
            .detail = "cannot enumerate " + proc_root.string() + ": " +
                      iterator_error.message(),
        };
    }

    for (const auto& entry : entries) {
        const auto pid = directory_pid(entry.path());
        if (!pid) {
            continue;
        }
        const auto command = read_first_line(entry.path() / "comm");
        if (!command || *command != "eqgame.exe") {
            continue;
        }
        const auto uid = read_real_uid(entry.path() / "status");
        if (!uid) {
            eqgame_access_failed = true;
            continue;
        }
        if (*uid != expected_uid) {
            continue;
        }

        std::ifstream maps(entry.path() / "maps");
        if (!maps) {
            eqgame_access_failed = true;
            continue;
        }
        std::string line;
        std::vector<ProcessMapping> matching_maps;
        std::vector<ProcessMapping> process_mappings;
        while (std::getline(maps, line)) {
            const auto mapping = parse_mapping(line);
            if (!mapping) {
                continue;
            }
            process_mappings.push_back(*mapping);
            if (!mapping->path.empty() &&
                normalized_path(mapping->path) == expected_path) {
                matching_maps.push_back(*mapping);
            }
        }
        if (maps.bad()) {
            eqgame_access_failed = true;
            continue;
        }
        if (matching_maps.empty()) {
            continue;
        }

        const auto image = std::find_if(
            matching_maps.begin(), matching_maps.end(),
            [](const ProcessMapping& mapping) {
                return mapping.file_offset == 0U;
            });
        if (image == matching_maps.end()) {
            continue;
        }
        candidates.push_back({
            .pid = *pid,
            .uid = *uid,
            .command = *command,
            .image_base = image->begin,
            .client_mappings = std::move(matching_maps),
            .mappings = std::move(process_mappings),
        });
    }

    if (eqgame_access_failed) {
        return {
            .error = DiscoveryError::proc_access,
            .process = {},
            .detail =
                "could not fully inspect one or more eqgame.exe processes",
        };
    }
    if (candidates.empty()) {
        return {
            .error = DiscoveryError::no_candidate,
            .process = {},
            .detail =
                "no same-user eqgame.exe process maps the selected client",
        };
    }
    if (candidates.size() != 1U) {
        return {
            .error = DiscoveryError::ambiguous_candidates,
            .process = {},
            .detail = "multiple same-user eqgame.exe processes map the "
                      "selected client",
        };
    }
    return {
        .error = DiscoveryError::none,
        .process = std::move(candidates.front()),
        .detail = "one exact client process selected",
    };
}

bool is_process_alive(pid_t pid) {
    if (pid <= 0) {
        return false;
    }
    return kill(pid, 0) == 0 || errno == EPERM;
}

}  // namespace plazmic
