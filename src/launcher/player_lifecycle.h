#pragma once

#include "game/game_state_reader.h"
#include "map/map_parser.h"
#include "model/character_snapshot.h"
#include "model/player_snapshot.h"
#include "model/spawn_snapshot.h"

#include <optional>
#include <string>

namespace plazmic {

struct PlayerRefresh {
    GameStateReadResult state;
    std::optional<MapLoadResult> map_load;
};

struct PlayerLifecycleUpdate {
    PlayerSnapshot player;
    SpawnCollectionSnapshot spawns;
    CharacterSnapshot character;
    std::optional<ZoneMap> map;
    bool clear_map{false};
    bool reset_combat{false};
    bool reset_activity{false};
    bool preserve_respawn_alerts{false};
};

class PlayerLifecycle {
  public:
    [[nodiscard]] const std::string& handled_zone() const {
        return handled_zone_;
    }

    [[nodiscard]] PlayerLifecycleUpdate apply(PlayerRefresh refresh);

  private:
    struct CombatContextChange {
        bool changed{false};
        bool preserve_respawn_alerts{false};
    };

    [[nodiscard]] CombatContextChange combat_context_changed(
        const PlayerSnapshot& player,
        const CharacterSnapshot& character);

    std::string handled_zone_;
    std::string map_detail_;
    std::string combat_zone_;
    std::string combat_character_;
    bool combat_context_initialized_{false};
};

}  // namespace plazmic
