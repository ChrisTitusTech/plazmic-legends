#include "launcher/privacy_log.h"

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <system_error>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace plazmic {
namespace {

std::string safe_token(std::string_view value) {
    if (value.empty() || value.size() > 64U) {
        return "invalid";
    }
    for (const char character : value) {
        const auto byte = static_cast<unsigned char>(character);
        if (!((byte >= 'A' && byte <= 'Z') ||
              (byte >= 'a' && byte <= 'z') ||
              (byte >= '0' && byte <= '9') || byte == '.' ||
              byte == '_' || byte == '-')) {
            return "invalid";
        }
    }
    return std::string(value);
}

std::string timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t value =
        std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
    if (gmtime_r(&value, &utc) == nullptr) {
        return "1970-01-01T00:00:00Z";
    }
    std::array<char, 32> output{};
    if (std::strftime(
            output.data(), output.size(), "%Y-%m-%dT%H:%M:%SZ",
            &utc) == 0U) {
        return "1970-01-01T00:00:00Z";
    }
    return output.data();
}

std::string_view game_state_label(GameStateReadError error) {
    switch (error) {
        case GameStateReadError::none:
            return "in_world";
        case GameStateReadError::process_unavailable:
            return "process_unavailable";
        case GameStateReadError::not_in_world:
            return "not_in_world";
        case GameStateReadError::zoning:
            return "zoning";
        case GameStateReadError::invalid_profile:
            return "invalid_profile";
        case GameStateReadError::invalid_pointer:
            return "invalid_pointer";
        case GameStateReadError::inconsistent_snapshot:
            return "inconsistent_snapshot";
        case GameStateReadError::invalid_zone:
            return "invalid_zone";
        case GameStateReadError::invalid_player:
            return "invalid_player";
        case GameStateReadError::invalid_spawns:
            return "invalid_spawns";
        case GameStateReadError::read_failed:
            return "read_failed";
    }
    return "unknown";
}

std::string_view compatibility_state_label(CompatibilityState state) {
    switch (state) {
        case CompatibilityState::not_configured:
            return "not_configured";
        case CompatibilityState::client_error:
            return "client_error";
        case CompatibilityState::unsupported:
            return "unsupported";
        case CompatibilityState::supported:
            return "supported";
    }
    return "unknown";
}

std::string_view process_state_label(ProcessState state) {
    switch (state) {
        case ProcessState::unavailable:
            return "unavailable";
        case ProcessState::not_running:
            return "not_running";
        case ProcessState::running:
            return "running";
        case ProcessState::ambiguous:
            return "ambiguous";
        case ProcessState::access_error:
            return "access_error";
    }
    return "unknown";
}

std::string_view player_state_label(PlayerSnapshotState state) {
    switch (state) {
        case PlayerSnapshotState::unavailable:
            return "unavailable";
        case PlayerSnapshotState::client_not_running:
            return "client_not_running";
        case PlayerSnapshotState::not_in_world:
            return "not_in_world";
        case PlayerSnapshotState::zoning:
            return "zoning";
        case PlayerSnapshotState::stale:
            return "stale";
        case PlayerSnapshotState::in_world:
            return "in_world";
    }
    return "unknown";
}

std::string_view map_state_label(MapLoadError error) {
    switch (error) {
        case MapLoadError::none:
            return "loaded";
        case MapLoadError::invalid_zone:
            return "invalid_zone";
        case MapLoadError::map_root_unavailable:
            return "root_unavailable";
        case MapLoadError::path_escape:
            return "path_escape";
        case MapLoadError::missing_base_map:
            return "missing_base_map";
        case MapLoadError::file_unavailable:
            return "file_unavailable";
        case MapLoadError::file_too_large:
            return "file_too_large";
        case MapLoadError::line_too_long:
            return "line_too_long";
        case MapLoadError::too_many_records:
            return "too_many_records";
        case MapLoadError::malformed_record:
            return "malformed_record";
        case MapLoadError::value_out_of_range:
            return "value_out_of_range";
    }
    return "unknown";
}

bool write_all(int descriptor, std::string_view value) {
    std::size_t written = 0;
    while (written < value.size()) {
        const ssize_t result = ::write(
            descriptor, value.data() + written,
            value.size() - written);
        if (result < 0 && errno == EINTR) {
            continue;
        }
        if (result <= 0) {
            return false;
        }
        written += static_cast<std::size_t>(result);
    }
    return true;
}

}  // namespace

PrivacyLog::PrivacyLog(std::filesystem::path path,
                       std::size_t maximum_bytes)
    : path_(std::move(path)),
      maximum_bytes_(maximum_bytes) {
    if (path_.empty() || maximum_bytes_ < 256U) {
        healthy_ = false;
    }
}

std::filesystem::path PrivacyLog::default_path(
    std::string_view xdg_state_home,
    std::string_view home) {
    if (!xdg_state_home.empty()) {
        return std::filesystem::path(xdg_state_home) /
               "plazmic-legends/plazmic-legends.log";
    }
    if (!home.empty()) {
        return std::filesystem::path(home) /
               ".local/state/plazmic-legends/plazmic-legends.log";
    }
    return {};
}

void PrivacyLog::append(std::string_view fields) {
    if (!healthy_ || fields.empty() || fields.size() > 384U) {
        healthy_ = false;
        return;
    }
    std::error_code error;
    std::filesystem::create_directories(path_.parent_path(), error);
    if (error) {
        healthy_ = false;
        return;
    }
    std::filesystem::permissions(
        path_.parent_path(),
        std::filesystem::perms::owner_all,
        std::filesystem::perm_options::replace, error);
    if (error) {
        healthy_ = false;
        return;
    }

    const std::string line =
        timestamp() + " " + std::string(fields) + "\n";
    if (line.size() > maximum_bytes_) {
        healthy_ = false;
        return;
    }
    struct stat status {};
    if (::lstat(path_.c_str(), &status) == 0) {
        if (!S_ISREG(status.st_mode)) {
            healthy_ = false;
            return;
        }
        const auto current_size =
            static_cast<std::uintmax_t>(status.st_size);
        if (current_size >
            static_cast<std::uintmax_t>(maximum_bytes_ - line.size())) {
            const std::filesystem::path previous =
                path_.string() + ".previous";
            std::filesystem::remove(previous, error);
            error.clear();
            std::filesystem::rename(path_, previous, error);
            if (error) {
                healthy_ = false;
                return;
            }
        }
    } else if (errno != ENOENT) {
        healthy_ = false;
        return;
    }

    const int descriptor = ::open(
        path_.c_str(),
        O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC | O_NOFOLLOW,
        S_IRUSR | S_IWUSR);
    if (descriptor < 0) {
        healthy_ = false;
        return;
    }
    if (::fchmod(descriptor, S_IRUSR | S_IWUSR) != 0) {
        (void)::close(descriptor);
        healthy_ = false;
        return;
    }
    const bool wrote = write_all(descriptor, line);
    const bool closed = ::close(descriptor) == 0;
    if (!wrote || !closed) {
        healthy_ = false;
    }
}

void PrivacyLog::record_startup(std::string_view version,
                                std::string_view profile) {
    profile_ = safe_token(profile);
    append(
        "event=start version=" + safe_token(version) +
        " profile=" + profile_);
}

void PrivacyLog::record_status(const StatusSnapshot& status) {
    const std::string profile = safe_token(status.profile);
    if (compatibility_ == status.compatibility &&
        process_ == status.process && profile_ == profile) {
        return;
    }
    compatibility_ = status.compatibility;
    process_ = status.process;
    profile_ = profile;
    append(
        "event=status compatibility=" +
        std::string(compatibility_state_label(status.compatibility)) +
        " process=" + std::string(process_state_label(status.process)) +
        " profile=" + profile_);
}

void PrivacyLog::record_game_state(GameStateReadError error) {
    if (game_state_ == error) {
        return;
    }
    game_state_ = error;
    append(
        "event=reader state=" +
        std::string(game_state_label(error)));
}

void PrivacyLog::record_player_state(PlayerSnapshotState state) {
    if (player_state_ == state) {
        return;
    }
    player_state_ = state;
    append(
        "event=lifecycle state=" +
        std::string(player_state_label(state)));
}

void PrivacyLog::record_map_state(MapLoadError error) {
    if (map_state_ == error) {
        return;
    }
    map_state_ = error;
    append(
        "event=map state=" +
        std::string(map_state_label(error)));
}

void PrivacyLog::record_shutdown() {
    append("event=stop");
}

}  // namespace plazmic
