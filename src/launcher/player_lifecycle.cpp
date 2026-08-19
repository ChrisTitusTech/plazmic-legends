#include "launcher/player_lifecycle.h"

#include <string_view>
#include <utility>

namespace plazmic {
namespace {

std::string prefixed_detail(std::string_view prefix,
                            const std::string& detail) {
    if (detail.empty()) {
        return std::string(prefix);
    }
    return std::string(prefix) + ": " + detail;
}

PlayerSnapshot invalid_snapshot(const GameStateReadResult& result) {
    PlayerSnapshotState state = PlayerSnapshotState::unavailable;
    std::string_view label = "Unavailable";
    switch (result.error) {
        case GameStateReadError::process_unavailable:
            state = PlayerSnapshotState::client_not_running;
            label = "Client not running";
            break;
        case GameStateReadError::not_in_world:
            state = PlayerSnapshotState::not_in_world;
            label = "Not in world";
            break;
        case GameStateReadError::zoning:
            state = PlayerSnapshotState::zoning;
            label = "Zoning";
            break;
        case GameStateReadError::inconsistent_snapshot:
            state = PlayerSnapshotState::stale;
            label = "Stale snapshot rejected";
            break;
        case GameStateReadError::none:
        case GameStateReadError::invalid_profile:
        case GameStateReadError::invalid_pointer:
        case GameStateReadError::invalid_zone:
        case GameStateReadError::invalid_player:
        case GameStateReadError::invalid_spawns:
        case GameStateReadError::read_failed:
            break;
    }
    return {
        .state = state,
        .zone = {},
        .x = 0.0,
        .y = 0.0,
        .z = 0.0,
        .heading_degrees = 0.0,
        .detail = prefixed_detail(label, result.detail),
    };
}

SpawnCollectionSnapshot invalid_spawns(
    const GameStateReadResult& result) {
    const PlayerSnapshot player = invalid_snapshot(result);
    return {
        .state = player.state,
        .zone = {},
        .player_level = 0U,
        .player_name = {},
        .spawns = {},
        .detail = player.detail,
    };
}

CharacterSnapshot invalid_character(const GameStateReadResult& result) {
    const PlayerSnapshot player = invalid_snapshot(result);
    return {
        .state = player.state,
        .name = {},
        .health = {},
        .mana = {},
        .alternate_advancement_percent = std::nullopt,
        .alternate_advancement_points = std::nullopt,
        .equipment = {},
        .detail = player.detail,
    };
}

std::string map_failure_detail(const MapLoadResult& result) {
    switch (result.error) {
        case MapLoadError::missing_base_map:
            return prefixed_detail("Map missing", result.detail);
        case MapLoadError::file_too_large:
        case MapLoadError::line_too_long:
        case MapLoadError::too_many_records:
        case MapLoadError::malformed_record:
        case MapLoadError::value_out_of_range:
            return prefixed_detail("Map malformed", result.detail);
        case MapLoadError::none:
        case MapLoadError::invalid_zone:
        case MapLoadError::map_root_unavailable:
        case MapLoadError::path_escape:
        case MapLoadError::file_unavailable:
            return prefixed_detail("Map unavailable", result.detail);
    }
    return "Map unavailable";
}

}  // namespace

bool PlayerLifecycle::combat_context_changed(
    const PlayerSnapshot& player,
    const CharacterSnapshot& character) {
    const std::string next_zone =
        player.available() ? player.zone : std::string{};
    const std::string next_character =
        player.available() ? character.name : std::string{};
    const bool changed = !combat_context_initialized_ ||
                         combat_zone_ != next_zone ||
                         combat_character_ != next_character;
    combat_context_initialized_ = true;
    combat_zone_ = next_zone;
    combat_character_ = next_character;
    return changed;
}

PlayerLifecycleUpdate PlayerLifecycle::apply(PlayerRefresh refresh) {
    if (!refresh.state) {
        handled_zone_.clear();
        map_detail_.clear();
        PlayerSnapshot player = invalid_snapshot(refresh.state);
        SpawnCollectionSnapshot spawns = invalid_spawns(refresh.state);
        CharacterSnapshot character = invalid_character(refresh.state);
        const bool reset_combat =
            combat_context_changed(player, character);
        return {
            .player = std::move(player),
            .spawns = std::move(spawns),
            .character = std::move(character),
            .map = std::nullopt,
            .clear_map = true,
            .reset_combat = reset_combat,
            .reset_activity = reset_combat,
        };
    }

    PlayerSnapshot player = std::move(*refresh.state.snapshot);
    SpawnCollectionSnapshot spawns =
        std::move(*refresh.state.spawns);
    CharacterSnapshot character = refresh.state.character
                                      ? std::move(*refresh.state.character)
                                      : CharacterSnapshot{};
    if (!character.available() && !spawns.player_name.empty()) {
        character.name = spawns.player_name;
        character.detail =
            "Character vitals and equipment unavailable for this client";
    }
    const bool reset_combat = combat_context_changed(player, character);
    if (!refresh.map_load) {
        const bool awaiting_new_map =
            !handled_zone_.empty() && handled_zone_ != player.zone;
        if (!map_detail_.empty()) {
            player.detail = map_detail_;
        }
        return {
            .player = std::move(player),
            .spawns = std::move(spawns),
            .character = std::move(character),
            .map = std::nullopt,
            .clear_map = awaiting_new_map,
            .reset_combat = reset_combat,
            .reset_activity = reset_combat,
        };
    }

    handled_zone_ = player.zone;
    if (!*refresh.map_load) {
        map_detail_ = map_failure_detail(*refresh.map_load);
        player.detail = map_detail_;
        return {
            .player = std::move(player),
            .spawns = std::move(spawns),
            .character = std::move(character),
            .map = std::nullopt,
            .clear_map = true,
            .reset_combat = reset_combat,
            .reset_activity = reset_combat,
        };
    }

    map_detail_.clear();
    return {
        .player = std::move(player),
        .spawns = std::move(spawns),
        .character = std::move(character),
        .map = std::move(refresh.map_load->map),
        .clear_map = false,
        .reset_combat = reset_combat,
        .reset_activity = reset_combat,
    };
}

}  // namespace plazmic
