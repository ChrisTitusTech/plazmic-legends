#pragma once

#include <optional>
#include <string>
#include <string_view>

#include <sys/types.h>

namespace plazmic {

enum class CompatibilityState {
    not_configured,
    client_error,
    unsupported,
    supported,
};

enum class ProcessState {
    unavailable,
    not_running,
    running,
    ambiguous,
    access_error,
};

struct StatusSnapshot {
    CompatibilityState compatibility{CompatibilityState::not_configured};
    ProcessState process{ProcessState::unavailable};
    std::string profile{"none"};
    std::string detail{"Client path is not configured"};
    std::optional<pid_t> pid;

    [[nodiscard]] bool live_data_available() const {
        return compatibility == CompatibilityState::supported &&
               process == ProcessState::running;
    }
};

[[nodiscard]] std::string_view compatibility_label(
    CompatibilityState state);
[[nodiscard]] std::string_view process_label(ProcessState state);

}  // namespace plazmic
