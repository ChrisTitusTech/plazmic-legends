#pragma once

#include "model/spawn_snapshot.h"

namespace plazmic {

enum class SpawnPresentationCategory {
    player,
    named_npc,
    npc,
    other,
};

[[nodiscard]] inline SpawnPresentationCategory spawn_presentation_category(
    const SpawnSnapshot& spawn) {
    if (spawn.type == SpawnType::player) {
        return SpawnPresentationCategory::player;
    }
    if (spawn.type == SpawnType::npc) {
        return !spawn.name.empty() && spawn.name.front() == '#'
                   ? SpawnPresentationCategory::named_npc
                   : SpawnPresentationCategory::npc;
    }
    return SpawnPresentationCategory::other;
}

[[nodiscard]] inline const char* spawn_presentation_label(
    const SpawnSnapshot& spawn) {
    switch (spawn_presentation_category(spawn)) {
        case SpawnPresentationCategory::player:
            return "Player";
        case SpawnPresentationCategory::named_npc:
            return "Named NPC";
        case SpawnPresentationCategory::npc:
            return "NPC";
        case SpawnPresentationCategory::other:
            return "Other";
    }
    return "Other";
}

}  // namespace plazmic
