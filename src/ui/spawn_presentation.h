#pragma once

#include "model/spawn_snapshot.h"

namespace plazmic {

enum class SpawnPresentationCategory {
    player,
    named_npc,
    npc,
    other,
};

enum class SpawnConsiderColor {
    gray,
    green,
    light_blue,
    blue,
    white,
    yellow,
    red,
};

[[nodiscard]] constexpr SpawnConsiderColor spawn_consider_color(
    unsigned int player_level,
    unsigned int npc_level) {
    if (player_level == 0U || npc_level == 0U) {
        return SpawnConsiderColor::gray;
    }
    if (npc_level > player_level) {
        return npc_level - player_level >= 4U
                   ? SpawnConsiderColor::red
                   : SpawnConsiderColor::yellow;
    }
    if (npc_level == player_level) {
        return SpawnConsiderColor::white;
    }

    const unsigned int difference = player_level - npc_level;
    if (player_level <= 15U) {
        return difference >= 6U ? SpawnConsiderColor::gray
                                : SpawnConsiderColor::blue;
    }

    const unsigned int gray_level =
        player_level - ((player_level + 5U) / 3U);
    const unsigned int green_level =
        player_level - ((player_level + 7U) / 4U);
    if (npc_level <= gray_level) {
        return SpawnConsiderColor::gray;
    }
    if (npc_level <= green_level) {
        return SpawnConsiderColor::green;
    }
    if (difference >= 6U) {
        return player_level >= 25U ? SpawnConsiderColor::light_blue
                                   : SpawnConsiderColor::green;
    }
    return SpawnConsiderColor::blue;
}

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
