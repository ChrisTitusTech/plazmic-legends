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

PlayerLifecycleUpdate PlayerLifecycle::apply(PlayerRefresh refresh) {
    if (!refresh.state) {
        handled_zone_.clear();
        map_detail_.clear();
        return {
            .player = invalid_snapshot(refresh.state),
            .map = std::nullopt,
            .clear_map = true,
        };
    }

    PlayerSnapshot player = std::move(*refresh.state.snapshot);
    if (!refresh.map_load) {
        if (!map_detail_.empty()) {
            player.detail = map_detail_;
        }
        return {
            .player = std::move(player),
            .map = std::nullopt,
            .clear_map = false,
        };
    }

    handled_zone_ = player.zone;
    if (!*refresh.map_load) {
        map_detail_ = map_failure_detail(*refresh.map_load);
        player.detail = map_detail_;
        return {
            .player = std::move(player),
            .map = std::nullopt,
            .clear_map = true,
        };
    }

    map_detail_.clear();
    return {
        .player = std::move(player),
        .map = std::move(refresh.map_load->map),
        .clear_map = false,
    };
}

}  // namespace plazmic
