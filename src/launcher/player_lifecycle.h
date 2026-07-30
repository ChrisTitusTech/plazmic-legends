#pragma once

#include "game/game_state_reader.h"
#include "map/map_parser.h"
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
    std::optional<ZoneMap> map;
    bool clear_map{false};
};

class PlayerLifecycle {
  public:
    [[nodiscard]] const std::string& handled_zone() const {
        return handled_zone_;
    }

    [[nodiscard]] PlayerLifecycleUpdate apply(PlayerRefresh refresh);

  private:
    std::string handled_zone_;
    std::string map_detail_;
};

}  // namespace plazmic
