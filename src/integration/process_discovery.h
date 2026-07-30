#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <sys/types.h>
#include <unistd.h>

namespace plazmic {

struct ProcessMapping {
    std::uintptr_t begin{};
    std::uintptr_t end{};
    std::uint64_t file_offset{};
    std::string permissions;
    std::filesystem::path path;
};

struct ClientProcess {
    pid_t pid{};
    uid_t uid{};
    std::string command;
    std::uintptr_t image_base{};
    std::vector<ProcessMapping> client_mappings;
    std::vector<ProcessMapping> mappings;
};

enum class DiscoveryError {
    none,
    no_candidate,
    ambiguous_candidates,
    proc_access,
};

struct DiscoveryResult {
    DiscoveryError error{DiscoveryError::no_candidate};
    ClientProcess process;
    std::string detail;

    [[nodiscard]] explicit operator bool() const {
        return error == DiscoveryError::none;
    }
};

[[nodiscard]] DiscoveryResult discover_client_process(
    const std::filesystem::path& client_executable,
    const std::filesystem::path& proc_root = "/proc",
    uid_t expected_uid = getuid());

[[nodiscard]] bool is_process_alive(pid_t pid);

}  // namespace plazmic
