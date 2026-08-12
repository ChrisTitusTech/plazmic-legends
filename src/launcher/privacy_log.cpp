#include "launcher/privacy_log.h"

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <system_error>
#include <utility>

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

class OwnedDescriptor {
  public:
    explicit OwnedDescriptor(int value = -1) : value_(value) {}
    ~OwnedDescriptor() {
        if (value_ >= 0) {
            (void)::close(value_);
        }
    }
    OwnedDescriptor(const OwnedDescriptor&) = delete;
    OwnedDescriptor& operator=(const OwnedDescriptor&) = delete;
    OwnedDescriptor(OwnedDescriptor&& other) noexcept
        : value_(std::exchange(other.value_, -1)) {}
    OwnedDescriptor& operator=(OwnedDescriptor&& other) noexcept {
        if (this != &other) {
            if (value_ >= 0) {
                (void)::close(value_);
            }
            value_ = std::exchange(other.value_, -1);
        }
        return *this;
    }
    [[nodiscard]] int get() const { return value_; }
    [[nodiscard]] int release() { return std::exchange(value_, -1); }
    [[nodiscard]] explicit operator bool() const { return value_ >= 0; }

  private:
    int value_;
};

std::optional<OwnedDescriptor> open_directory_chain(
    const std::filesystem::path& directory) {
    if (directory.empty()) {
        return std::nullopt;
    }
    std::error_code error;
    const std::filesystem::path absolute =
        std::filesystem::absolute(directory, error).lexically_normal();
    if (error || absolute.empty()) {
        return std::nullopt;
    }
    OwnedDescriptor current(::open(
        absolute.root_path().c_str(),
        O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    if (!current) {
        return std::nullopt;
    }
    for (const auto& component : absolute.relative_path()) {
        const std::string name = component.string();
        if (name.empty() || name == "." || name == "..") {
            return std::nullopt;
        }
        int next = ::openat(
            current.get(), name.c_str(),
            O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        if (next < 0 && errno == ENOENT) {
            if (::mkdirat(current.get(), name.c_str(), 0700) != 0 &&
                errno != EEXIST) {
                return std::nullopt;
            }
            next = ::openat(
                current.get(), name.c_str(),
                O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        }
        if (next < 0) {
            return std::nullopt;
        }
        current = OwnedDescriptor(next);
    }
    return current;
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
    auto directory = open_directory_chain(path_.parent_path());
    const std::string name = path_.filename().string();
    if (!directory || name.empty() || name == "." || name == "..") {
        healthy_ = false;
        return;
    }
    struct stat directory_status {};
    if (::fstat(directory->get(), &directory_status) != 0 ||
        !S_ISDIR(directory_status.st_mode) ||
        directory_status.st_uid != ::geteuid() ||
        ::fchmod(directory->get(), 0700) != 0 ||
        ::fstat(directory->get(), &directory_status) != 0 ||
        (directory_status.st_mode & 0777) != 0700) {
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
    if (::fstatat(directory->get(), name.c_str(), &status,
                  AT_SYMLINK_NOFOLLOW) == 0) {
        if (!S_ISREG(status.st_mode) || status.st_uid != ::geteuid() ||
            status.st_size < 0) {
            healthy_ = false;
            return;
        }
        const auto current_size =
            static_cast<std::uintmax_t>(status.st_size);
        if (current_size >
            static_cast<std::uintmax_t>(maximum_bytes_ - line.size())) {
            const std::string previous = name + ".previous";
            struct stat previous_status {};
            if (::fstatat(directory->get(), previous.c_str(),
                          &previous_status, AT_SYMLINK_NOFOLLOW) == 0) {
                if (!S_ISREG(previous_status.st_mode) ||
                    previous_status.st_uid != ::geteuid() ||
                    ::unlinkat(directory->get(), previous.c_str(), 0) != 0) {
                    healthy_ = false;
                    return;
                }
            } else if (errno != ENOENT) {
                healthy_ = false;
                return;
            }
            struct stat current_status {};
            if (::fstatat(directory->get(), name.c_str(), &current_status,
                          AT_SYMLINK_NOFOLLOW) != 0 ||
                current_status.st_dev != status.st_dev ||
                current_status.st_ino != status.st_ino ||
                !S_ISREG(current_status.st_mode) ||
                current_status.st_uid != ::geteuid() ||
                ::renameat(directory->get(), name.c_str(), directory->get(),
                           previous.c_str()) != 0) {
                healthy_ = false;
                return;
            }
        }
    } else if (errno != ENOENT) {
        healthy_ = false;
        return;
    }

    OwnedDescriptor descriptor(::openat(
        directory->get(), name.c_str(),
        O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC | O_NOFOLLOW,
        S_IRUSR | S_IWUSR));
    if (!descriptor) {
        healthy_ = false;
        return;
    }
    struct stat opened {};
    if (::fstat(descriptor.get(), &opened) != 0 ||
        !S_ISREG(opened.st_mode) || opened.st_uid != ::geteuid() ||
        ::fchmod(descriptor.get(), S_IRUSR | S_IWUSR) != 0) {
        healthy_ = false;
        return;
    }
    const bool wrote = write_all(descriptor.get(), line);
    const bool closed = ::close(descriptor.release()) == 0;
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
