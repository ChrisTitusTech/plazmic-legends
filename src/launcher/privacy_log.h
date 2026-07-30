#pragma once

#include "game/game_state_reader.h"
#include "map/map_parser.h"
#include "model/player_snapshot.h"
#include "model/status_snapshot.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string_view>

namespace plazmic {

class PrivacyLog {
  public:
    explicit PrivacyLog(
        std::filesystem::path path,
        std::size_t maximum_bytes = 1024U * 1024U);

    [[nodiscard]] static std::filesystem::path default_path(
        std::string_view xdg_state_home,
        std::string_view home);

    void record_startup(std::string_view version,
                        std::string_view profile);
    void record_status(const StatusSnapshot& status);
    void record_game_state(GameStateReadError error);
    void record_player_state(PlayerSnapshotState state);
    void record_map_state(MapLoadError error);
    void record_shutdown();

    [[nodiscard]] bool healthy() const { return healthy_; }
    [[nodiscard]] const std::filesystem::path& path() const {
        return path_;
    }

  private:
    void append(std::string_view fields);

    std::filesystem::path path_;
    std::size_t maximum_bytes_;
    bool healthy_{true};
    std::optional<CompatibilityState> compatibility_;
    std::optional<ProcessState> process_;
    std::string profile_;
    std::optional<GameStateReadError> game_state_;
    std::optional<PlayerSnapshotState> player_state_;
    std::optional<MapLoadError> map_state_;
};

}  // namespace plazmic
