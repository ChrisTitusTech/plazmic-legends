#pragma once

#include "model/player_snapshot.h"

#include <cstdint>
#include <string>
#include <vector>

namespace plazmic {

enum class SpawnType {
    player,
    npc,
    corpse,
};

struct SpawnSnapshot {
    std::uint32_t id{};
    SpawnType type{SpawnType::npc};
    std::string name;
    unsigned int level{};
    double x{};
    double y{};
    double z{};
    double distance{};

    bool operator==(const SpawnSnapshot&) const = default;
};

struct SpawnCollectionSnapshot {
    PlayerSnapshotState state{PlayerSnapshotState::unavailable};
    std::string zone;
    unsigned int player_level{};
    std::vector<SpawnSnapshot> spawns;
    std::string detail;

    [[nodiscard]] bool available() const {
        return state == PlayerSnapshotState::in_world;
    }

    bool operator==(const SpawnCollectionSnapshot&) const = default;
};

[[nodiscard]] constexpr const char* spawn_type_label(SpawnType type) {
    switch (type) {
        case SpawnType::player:
            return "Player";
        case SpawnType::npc:
            return "NPC";
        case SpawnType::corpse:
            return "Corpse";
    }
    return "Unknown";
}

}  // namespace plazmic
