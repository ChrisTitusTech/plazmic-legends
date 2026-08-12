#include "common/client_file_monitor.h"
#include "common/sha256.h"
#include "game/client_profile.h"
#include "launcher/client_status.h"
#include "model/status_snapshot.h"

#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <unistd.h>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class TemporaryFile {
  public:
    TemporaryFile() {
        std::string pattern =
            (std::filesystem::temp_directory_path() /
             "plazmic-unsupported-XXXXXX")
                .string();
        const int descriptor = mkstemp(pattern.data());
        require(descriptor >= 0, "cannot create unsupported client fixture");
        close(descriptor);
        path_ = pattern;
        std::ofstream output(path_, std::ios::binary);
        output << "not a supported client";
        require(output.good(), "cannot write unsupported client fixture");
    }

    ~TemporaryFile() {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

    void append(std::string_view value) const {
        std::ofstream output(path_, std::ios::binary | std::ios::app);
        output << value;
        require(output.good(), "cannot update client fixture");
    }

    void replace(std::string_view value) const {
        std::ofstream output(
            path_, std::ios::binary | std::ios::trunc);
        output << value;
        require(output.good(), "cannot replace client fixture");
    }

    void remove() const {
        std::error_code error;
        require(std::filesystem::remove(path_, error) && !error,
                "cannot remove client fixture");
    }

  private:
    std::filesystem::path path_;
};

}  // namespace

int main() {
    try {
        const auto& profile = plazmic::legends_reference_profile();
        require(plazmic::select_client_profile(profile.sha256) == &profile,
                "reference profile did not select by exact digest");
        constexpr std::string_view kAugustThirdDigest =
            "f8af4e704746118f8dd94b688e585bc5c37c3d085da620136bcacad5486145ac";
        const auto* august_third =
            plazmic::select_client_profile(kAugustThirdDigest);
        require(
            august_third != nullptr && august_third != &profile &&
                august_third->id == "legends-2026-08-03" &&
                august_third->sha256 == kAugustThirdDigest &&
                august_third->machine == 0x8664U &&
                august_third->timestamp == 0x6a711da7U &&
                august_third->optional_magic == 0x20bU &&
                august_third->image_size == 0x16c0000U &&
                !august_third->character_snapshot_supported &&
                august_third->game_state.local_player_pointer_rva ==
                    0x00f05ff8U &&
                august_third->game_state.world_data_pointer_rva ==
                    0x00f05fe8U &&
                august_third->game_state.player_zone_id_offset == 0x4a0U &&
                august_third->spawns.level_offset == 0x620U &&
                august_third->spawns.record_bytes == 0x621U,
            "August 3 profile did not preserve its exact supported boundary");
        constexpr std::string_view kAugustFourthDigest =
            "d9784a58bb03cb70177d6a494fa71bca2b13ab3c5b8b9d6c26e45bae01597e51";
        const auto* august_fourth =
            plazmic::select_client_profile(kAugustFourthDigest);
        require(
            august_fourth != nullptr && august_fourth != &profile &&
                august_fourth->id == "legends-2026-08-04" &&
                august_fourth->sha256 == kAugustFourthDigest &&
                august_fourth->machine == 0x8664U &&
                august_fourth->timestamp == 0x6a725275U &&
                august_fourth->optional_magic == 0x20bU &&
                august_fourth->image_size == 0x16bf000U &&
                august_fourth->character_snapshot_supported &&
                august_fourth->game_state.local_player_pointer_rva ==
                    0x00f04ff8U &&
                august_fourth->game_state.world_data_pointer_rva ==
                    0x00f04fe8U &&
                august_fourth->game_state.player_zone_id_offset == 0x5b0U &&
                august_fourth->spawns.level_offset == 0x64cU &&
                august_fourth->spawns.record_bytes == 0x64dU &&
                august_fourth->character.local_character_pointer_rva ==
                    0x00f05140U &&
                august_fourth->character.player_maximum_mana_offset ==
                    0x2e8U &&
                august_fourth->character.item_name_pointer_offset == 0x60U,
            "August 4 profile did not preserve its exact identity and offsets");
        constexpr std::string_view kAugustSixthDigest =
            "bf34438c6460acde463692fa09ea28f0d12a204e3445a9da356645fc0d475561";
        const auto* august_sixth =
            plazmic::select_client_profile(kAugustSixthDigest);
        require(
            august_sixth != nullptr && august_sixth != &profile &&
                august_sixth != august_fourth &&
                august_sixth->id == "legends-2026-08-06" &&
                august_sixth->sha256 == kAugustSixthDigest &&
                august_sixth->machine == 0x8664U &&
                august_sixth->timestamp == 0x6a74f50aU &&
                august_sixth->optional_magic == 0x20bU &&
                august_sixth->image_size == 0x16c0000U &&
                august_sixth->character_snapshot_supported &&
                august_sixth->game_state.local_player_pointer_rva ==
                    0x00f05ff8U &&
                august_sixth->game_state.world_data_pointer_rva ==
                    0x00f05fe8U &&
                august_sixth->game_state.player_zone_id_offset == 0x5b0U &&
                august_sixth->spawns.level_offset == 0x275U &&
                august_sixth->spawns.record_bytes == 0x276U &&
                august_sixth->character.local_character_pointer_rva ==
                    0x00f06140U &&
                august_sixth->character.player_maximum_mana_offset ==
                    0x678U &&
                august_sixth->character.item_name_pointer_offset == 0x60U,
            "August 6 profile did not preserve its exact identity and offsets");
        require(plazmic::select_client_profile("changed") == nullptr,
                "unknown digest unexpectedly selected");

        plazmic::ClientStatusProbe unconfigured(
            std::filesystem::path{});
        const auto unconfigured_status = unconfigured.refresh();
        require(
            unconfigured_status.compatibility ==
                plazmic::CompatibilityState::not_configured,
            "empty path did not report not configured");
        require(unconfigured_status.process ==
                    plazmic::ProcessState::unavailable,
                "empty path unexpectedly attempted discovery");

        const TemporaryFile unsupported_file;
        plazmic::ClientStatusProbe unsupported(unsupported_file.path());
        const auto unsupported_status = unsupported.refresh();
        require(unsupported_status.compatibility ==
                    plazmic::CompatibilityState::unsupported,
                "changed digest did not fail closed");
        require(unsupported_status.process ==
                    plazmic::ProcessState::unavailable,
                "unsupported client unexpectedly attempted discovery");
        require(!unsupported_status.pid,
                "unsupported client unexpectedly returned a PID");

        const TemporaryFile changed_file;
        const std::string expected =
            plazmic::sha256_file(changed_file.path());
        plazmic::ClientFileMonitor changed_monitor(
            changed_file.path(), expected);
        require(static_cast<bool>(changed_monitor.check()),
                "unchanged client metadata was rejected");
        changed_file.append("patch");
        const auto changed = changed_monitor.check();
        require(
            changed.state == plazmic::ClientFileState::changed &&
                changed.detail.find("new compatibility profile") !=
                    std::string::npos,
            "changed client did not produce an actionable fail-closed result");
        require(changed.detail.find(changed_file.path().string()) ==
                    std::string::npos,
                "changed-client diagnostic leaked its local path");
        require(changed_monitor.check().state ==
                    plazmic::ClientFileState::changed,
                "changed-client result was not latched for the run");

        const TemporaryFile preserved_metadata_file;
        const auto original_time = std::filesystem::last_write_time(
            preserved_metadata_file.path());
        plazmic::ClientFileMonitor periodic_monitor(
            preserved_metadata_file.path(),
            plazmic::sha256_file(preserved_metadata_file.path()),
            std::chrono::steady_clock::duration::zero());
        preserved_metadata_file.replace("ton a supported client");
        std::filesystem::last_write_time(
            preserved_metadata_file.path(), original_time);
        require(
            periodic_monitor.check().state ==
                plazmic::ClientFileState::changed,
            "periodic digest check missed preserved client metadata");

        const TemporaryFile missing_file;
        plazmic::ClientFileMonitor missing_monitor(
            missing_file.path(),
            plazmic::sha256_file(missing_file.path()));
        missing_file.remove();
        const auto missing = missing_monitor.check();
        require(
            missing.state == plazmic::ClientFileState::unavailable &&
                missing.detail.find("restart") != std::string::npos,
            "removed client did not produce an actionable unavailable result");

        require(plazmic::compatibility_label(
                    plazmic::CompatibilityState::supported) == "Supported",
                "compatibility label mismatch");
        require(plazmic::process_label(plazmic::ProcessState::running) ==
                    "Running",
                "process label mismatch");
        std::cout << "client profile and status boundary passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
