#include "launcher/privacy_log.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <sys/stat.h>
#include <unistd.h>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        std::string pattern =
            (std::filesystem::temp_directory_path() /
             "plazmic-log-XXXXXX")
                .string();
        char* created = mkdtemp(pattern.data());
        require(created != nullptr, "cannot create log fixture directory");
        path_ = created;
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(input.good(), "cannot open privacy log fixture");
    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>(),
    };
}

}  // namespace

int main() {
    try {
        require(
            plazmic::PrivacyLog::default_path("/state", "/home") ==
                "/state/plazmic-legends/plazmic-legends.log",
            "XDG state path was not preferred");
        require(
            plazmic::PrivacyLog::default_path({}, "/home/user") ==
                "/home/user/.local/state/plazmic-legends/"
                "plazmic-legends.log",
            "home state fallback was incorrect");
        require(plazmic::PrivacyLog::default_path({}, {}).empty(),
                "missing state roots produced a log path");

        TemporaryDirectory directory;
        const auto path = directory.path() / "state/plazmic.log";
        plazmic::PrivacyLog log(path, 4096U);
        log.record_startup("0.1.0\nsecret", "profile with spaces");
        const plazmic::StatusSnapshot status{
            .compatibility = plazmic::CompatibilityState::supported,
            .process = plazmic::ProcessState::running,
            .profile = "synthetic-profile",
            .detail =
                "private_character private_spawn session_token "
                "0x7fff12345678",
            .pid = 12345,
        };
        log.record_status(status);
        log.record_status(status);
        log.record_game_state(
            plazmic::GameStateReadError::invalid_profile);
        log.record_player_state(
            plazmic::PlayerSnapshotState::stale);
        log.record_map_state(
            plazmic::MapLoadError::missing_base_map);
        log.record_shutdown();
        require(log.healthy(), "privacy log became unhealthy");

        const std::string contents = read_file(path);
        require(contents.find("event=start version=invalid profile=invalid") !=
                    std::string::npos,
                "unsafe startup tokens were not rejected");
        require(contents.find(
                    "compatibility=supported process=running "
                    "profile=synthetic-profile") != std::string::npos,
                "approved compatibility categories were not logged");
        require(contents.find("state=invalid_profile") !=
                    std::string::npos &&
                    contents.find("state=stale") != std::string::npos &&
                    contents.find("state=missing_base_map") !=
                        std::string::npos,
                "approved lifecycle categories were not logged");
        for (const std::string_view forbidden :
             {"private_character", "private_spawn", "session_token",
              "0x7fff12345678", "12345"}) {
            require(contents.find(forbidden) == std::string::npos,
                    "privacy log retained forbidden runtime detail");
        }

        struct stat status_buffer {};
        require(::stat(path.c_str(), &status_buffer) == 0,
                "cannot inspect privacy log permissions");
        require((status_buffer.st_mode & 0777) == 0600,
                "privacy log permissions are not owner-only");

        const auto rotation_path =
            directory.path() / "state/rotation.log";
        plazmic::PrivacyLog rotation(rotation_path, 512U);
        rotation.record_startup("0.1.0", "synthetic-profile");
        for (int index = 0; index < 20; ++index) {
            auto changed = status;
            changed.process =
                index % 2 == 0
                    ? plazmic::ProcessState::not_running
                    : plazmic::ProcessState::running;
            rotation.record_status(changed);
        }
        require(rotation.healthy(), "bounded privacy log rotation failed");
        require(std::filesystem::is_regular_file(rotation_path),
                "active privacy log disappeared after rotation");
        require(std::filesystem::is_regular_file(
                    rotation_path.string() + ".previous"),
                "bounded privacy log did not rotate");
        require(std::filesystem::file_size(rotation_path) <= 512U &&
                    std::filesystem::file_size(
                        rotation_path.string() + ".previous") <= 512U,
                "privacy log exceeded its configured bound");

        std::cout
            << "privacy-safe bounded XDG logging passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
