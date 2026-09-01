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
        constexpr std::string_view kAugustSeventeenthDigest =
            "3451069e63d5118a703a237f121a3ea7c20c973477a69fdd0d66bcdaa7d80b29";
        const auto* august_seventeenth =
            plazmic::select_client_profile(kAugustSeventeenthDigest);
        require(
            august_seventeenth != nullptr &&
                august_seventeenth != &profile &&
                august_seventeenth != august_third &&
                august_seventeenth != august_fourth &&
                august_seventeenth != august_sixth &&
                august_seventeenth->id == "legends-2026-08-17" &&
                august_seventeenth->sha256 == kAugustSeventeenthDigest &&
                august_seventeenth->machine == 0x8664U &&
                august_seventeenth->timestamp == 0x6a837d49U &&
                august_seventeenth->optional_magic == 0x20bU &&
                august_seventeenth->image_size == 0x16c2000U &&
                august_seventeenth->character_snapshot_supported &&
                august_seventeenth->game_state.local_player_pointer_rva ==
                    0x00f08248U &&
                august_seventeenth->game_state.world_data_pointer_rva ==
                    0x00f07d38U &&
                august_seventeenth->game_state.player_zone_id_offset ==
                    0x3b8U &&
                august_seventeenth->spawns.level_offset == 0x391U &&
                august_seventeenth->spawns.record_bytes == 0x392U &&
                august_seventeenth->character.local_character_pointer_rva ==
                    0x00f08398U &&
                august_seventeenth->character.player_maximum_health_offset ==
                    0x310U &&
                august_seventeenth->character.player_maximum_mana_offset ==
                    0x238U &&
                august_seventeenth->character.item_name_pointer_offset ==
                    0x98U &&
                august_seventeenth->character.experience_percent_rva ==
                    0x00fa6688U &&
                august_seventeenth->character.progression_cache_rva ==
                    0x00fa6440U &&
                    august_seventeenth->character
                            .alternate_advancement_percent_offset == 0x24cU &&
                    august_seventeenth->character
                            .alternate_advancement_points_offset == 0U &&
                    august_seventeenth->character
                            .unallocated_alternate_advancement_points_offset ==
                        0xaa5cU,
            "August 17 profile did not preserve its exact candidate boundary");
        constexpr std::string_view kAugustTwentyFifthDigest =
            "6807ac5c672ffee98fcc6a62a5e87d0ec6af1a323251280144a4f1461829f0d4";
        const auto* august_twenty_fifth =
            plazmic::select_client_profile(kAugustTwentyFifthDigest);
        require(
            august_twenty_fifth != nullptr &&
                august_twenty_fifth != &profile &&
                august_twenty_fifth != august_third &&
                august_twenty_fifth != august_fourth &&
                august_twenty_fifth != august_sixth &&
                august_twenty_fifth != august_seventeenth &&
                august_twenty_fifth->id == "legends-2026-08-25" &&
                august_twenty_fifth->sha256 == kAugustTwentyFifthDigest &&
                august_twenty_fifth->machine == 0x8664U &&
                august_twenty_fifth->timestamp == 0x6a8e193aU &&
                august_twenty_fifth->optional_magic == 0x20bU &&
                august_twenty_fifth->image_size == 0x16c3000U &&
                august_twenty_fifth->character_snapshot_supported &&
                august_twenty_fifth->game_state.local_player_pointer_rva ==
                    0x00f09248U &&
                august_twenty_fifth->game_state.world_data_pointer_rva ==
                    0x00f08d38U &&
                august_twenty_fifth->game_state.player_zone_id_offset ==
                    0x21cU &&
                august_twenty_fifth->spawns.level_offset == 0x610U &&
                august_twenty_fifth->spawns.record_bytes == 0x611U &&
                august_twenty_fifth->character.local_character_pointer_rva ==
                    0x00f09398U &&
                august_twenty_fifth->character.player_maximum_health_offset ==
                    0x420U &&
                august_twenty_fifth->character.player_maximum_mana_offset ==
                    0x618U &&
                august_twenty_fifth->character.item_name_pointer_offset ==
                    0x38U &&
                august_twenty_fifth->character.experience_percent_rva ==
                    0x00fa7688U &&
                august_twenty_fifth->character.progression_cache_rva ==
                    0x00fa7440U &&
                august_twenty_fifth->character
                        .alternate_advancement_percent_offset == 0x24cU &&
                august_twenty_fifth->character
                        .alternate_advancement_points_offset == 0U &&
                august_twenty_fifth->character
                        .unallocated_alternate_advancement_points_offset ==
                    0xaa5cU,
            "August 25 profile did not preserve its exact supported boundary");
        constexpr std::string_view kSeptemberFirstDigest =
            "bc1eb76dae1a3544f37fe216a0dccc71d2323e9d11d8b612368785e80114c1c5";
        const auto* september_first =
            plazmic::select_client_profile(kSeptemberFirstDigest);
        require(
            september_first != nullptr &&
                september_first != &profile &&
                september_first != august_third &&
                september_first != august_fourth &&
                september_first != august_sixth &&
                september_first != august_seventeenth &&
                september_first != august_twenty_fifth &&
                september_first->id == "legends-2026-09-01" &&
                september_first->sha256 == kSeptemberFirstDigest &&
                september_first->machine == 0x8664U &&
                september_first->timestamp == 0x6a96dba9U &&
                september_first->optional_magic == 0x20bU &&
                september_first->image_size == 0x16c7000U &&
                september_first->character_snapshot_supported &&
                september_first->game_state.local_player_pointer_rva ==
                    0x00f0d360U &&
                september_first->game_state.world_data_pointer_rva ==
                    0x00f0ce50U &&
                september_first->game_state.player_zone_id_offset ==
                    0x59cU &&
                september_first->spawns.level_offset == 0x4bcU &&
                september_first->spawns.record_bytes == 0x4bdU &&
                september_first->character.local_character_pointer_rva ==
                    0x00f0d4b0U &&
                september_first->character.stats_current_mana_offset ==
                    0x276cU &&
                september_first->character.stats_current_health_bytes ==
                    sizeof(std::int32_t) &&
                !september_first->character.apply_health_adjustment &&
                september_first->character.player_maximum_health_offset ==
                    0x4d0U &&
                september_first->character.player_maximum_health_bytes ==
                    sizeof(std::int32_t) &&
                september_first->character.player_maximum_mana_offset ==
                    0x5a8U &&
                september_first->character.player_maximum_mana_bytes ==
                    sizeof(std::int64_t) &&
                september_first->character.item_name_pointer_offset ==
                    0x68U &&
                september_first->character.experience_percent_rva ==
                    0x00fab528U &&
                september_first->character.progression_cache_rva ==
                    0x00fab528U &&
                september_first->character
                        .alternate_advancement_percent_offset == 0x04U &&
                september_first->character
                        .alternate_advancement_points_offset == 0x08U &&
                september_first->character
                        .unallocated_alternate_advancement_points_offset ==
                    0U,
            "September 1 profile did not preserve its exact supported boundary");
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
