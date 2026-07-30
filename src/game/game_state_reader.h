#pragma once

#include "game/client_profile.h"
#include "integration/process_discovery.h"
#include "model/player_snapshot.h"

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>

namespace plazmic {

enum class GameStateReadError {
    none,
    process_unavailable,
    not_in_world,
    invalid_profile,
    invalid_pointer,
    inconsistent_snapshot,
    invalid_zone,
    invalid_player,
    read_failed,
};

struct GameStateReadResult {
    std::optional<PlayerSnapshot> snapshot;
    GameStateReadError error{GameStateReadError::process_unavailable};
    std::string detail;

    [[nodiscard]] explicit operator bool() const {
        return snapshot.has_value() && error == GameStateReadError::none;
    }
};

[[nodiscard]] GameStateReadResult read_game_state(
    const ClientProcess& process,
    const GameStateSymbols& symbols);

class LiveGameStateProbe {
  public:
    LiveGameStateProbe(std::filesystem::path client,
                       const ClientProfile* profile);

    [[nodiscard]] GameStateReadResult refresh();

  private:
    std::filesystem::path client_;
    const ClientProfile* profile_{};
    std::optional<ClientProcess> process_;
    std::chrono::steady_clock::time_point next_discovery_check_{};
    std::string last_discovery_detail_{"client discovery has not run"};
};

}  // namespace plazmic
