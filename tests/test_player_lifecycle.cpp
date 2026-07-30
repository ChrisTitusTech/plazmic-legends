#include "launcher/player_lifecycle.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

plazmic::GameStateReadResult live_state(std::string zone) {
    return {
        .snapshot =
            plazmic::PlayerSnapshot{
                .state = plazmic::PlayerSnapshotState::in_world,
                .zone = zone,
                .x = 10.0,
                .y = 20.0,
                .z = 30.0,
                .heading_degrees = 90.0,
                .detail = "Live synthetic player",
            },
        .spawns =
            plazmic::SpawnCollectionSnapshot{
                .state = plazmic::PlayerSnapshotState::in_world,
                .zone = zone,
                .spawns =
                    {
                        {
                            .id = 1,
                            .type = plazmic::SpawnType::npc,
                            .name = "synthetic_spawn",
                            .level = 10,
                            .x = 11.0,
                            .y = 21.0,
                            .z = 30.0,
                            .distance = 1.414,
                        },
                    },
                .detail = "Live synthetic spawns",
            },
        .error = plazmic::GameStateReadError::none,
        .detail = {},
    };
}

plazmic::MapLoadResult loaded_map(std::string zone) {
    return {
        .map =
            plazmic::ZoneMap{
                .zone = std::move(zone),
                .layers = {},
            },
        .error = plazmic::MapLoadError::none,
        .detail = {},
    };
}

plazmic::GameStateReadResult failed_state(
    plazmic::GameStateReadError error) {
    return {
        .snapshot = std::nullopt,
        .spawns = std::nullopt,
        .error = error,
        .detail = "synthetic transition",
    };
}

}  // namespace

int main() {
    try {
        plazmic::PlayerLifecycle lifecycle;
        auto first = lifecycle.apply({
            .state = live_state("zone_a"),
            .map_load = loaded_map("zone_a"),
        });
        require(first.player.available(),
                "initial live player was unavailable");
        require(first.spawns.available() &&
                    first.spawns.spawns.size() == 1,
                "initial live spawn collection was unavailable");
        require(first.map && first.map->zone == "zone_a",
                "initial zone map was not published");
        require(!first.clear_map,
                "initial map publication requested invalidation");
        require(lifecycle.handled_zone() == "zone_a",
                "initial handled zone was not retained");

        auto unchanged = lifecycle.apply({
            .state = live_state("zone_a"),
            .map_load = std::nullopt,
        });
        require(unchanged.player.available(),
                "unchanged player became unavailable");
        require(!unchanged.map && !unchanged.clear_map,
                "unchanged zone altered the map");

        auto second = lifecycle.apply({
            .state = live_state("zone_b"),
            .map_load = loaded_map("zone_b"),
        });
        require(second.player.available() &&
                    second.player.zone == "zone_b",
                "second-zone player was not published");
        require(second.map && second.map->zone == "zone_b",
                "second-zone map was not published");
        require(lifecycle.handled_zone() == "zone_b",
                "second zone did not replace the handled zone");

        const std::array transitions{
            std::pair{
                plazmic::GameStateReadError::process_unavailable,
                plazmic::PlayerSnapshotState::client_not_running,
            },
            std::pair{
                plazmic::GameStateReadError::not_in_world,
                plazmic::PlayerSnapshotState::not_in_world,
            },
            std::pair{
                plazmic::GameStateReadError::zoning,
                plazmic::PlayerSnapshotState::zoning,
            },
            std::pair{
                plazmic::GameStateReadError::inconsistent_snapshot,
                plazmic::PlayerSnapshotState::stale,
            },
            std::pair{
                plazmic::GameStateReadError::invalid_spawns,
                plazmic::PlayerSnapshotState::unavailable,
            },
            std::pair{
                plazmic::GameStateReadError::read_failed,
                plazmic::PlayerSnapshotState::unavailable,
            },
        };
        for (const auto& [error, expected_state] : transitions) {
            plazmic::PlayerLifecycle transition_lifecycle;
            (void)transition_lifecycle.apply({
                .state = live_state("zone_a"),
                .map_load = loaded_map("zone_a"),
            });
            const auto invalidated = transition_lifecycle.apply({
                .state = failed_state(error),
                .map_load = std::nullopt,
            });
            require(!invalidated.player.available(),
                    "lifecycle transition retained an available player");
            require(!invalidated.spawns.available() &&
                        invalidated.spawns.spawns.empty(),
                    "lifecycle transition retained stale spawns");
            require(invalidated.player.state == expected_state,
                    "lifecycle transition used the wrong explicit state");
            require(invalidated.player.zone.empty(),
                    "lifecycle transition retained a stale zone");
            require(invalidated.clear_map && !invalidated.map,
                    "lifecycle transition retained stale map geometry");
            require(transition_lifecycle.handled_zone().empty(),
                    "lifecycle transition retained the handled zone");
            require(!invalidated.player.detail.empty(),
                    "lifecycle transition omitted its diagnostic");
        }

        plazmic::PlayerLifecycle missing_map_lifecycle;
        auto missing = missing_map_lifecycle.apply({
            .state = live_state("missing"),
            .map_load =
                plazmic::MapLoadResult{
                    .map = std::nullopt,
                    .error = plazmic::MapLoadError::missing_base_map,
                    .detail = "synthetic base map is missing",
                },
        });
        require(missing.player.available(),
                "missing map invalidated a valid player");
        require(missing.clear_map && !missing.map,
                "missing map did not clear stale geometry");
        require(missing.player.detail.starts_with("Map missing:"),
                "missing map did not publish an explicit state");
        const auto missing_refresh = missing_map_lifecycle.apply({
            .state = live_state("missing"),
            .map_load = std::nullopt,
        });
        require(missing_refresh.player.detail.starts_with("Map missing:"),
                "missing-map state did not persist between reads");

        plazmic::PlayerLifecycle malformed_map_lifecycle;
        const auto malformed = malformed_map_lifecycle.apply({
            .state = live_state("malformed"),
            .map_load =
                plazmic::MapLoadResult{
                    .map = std::nullopt,
                    .error = plazmic::MapLoadError::malformed_record,
                    .detail = "synthetic line is malformed",
                },
        });
        require(malformed.player.available(),
                "malformed map invalidated a valid player");
        require(malformed.clear_map && !malformed.map,
                "malformed map did not clear stale geometry");
        require(malformed.player.detail.starts_with("Map malformed:"),
                "malformed map did not publish an explicit state");

        std::cout
            << "player lifecycle invalidation and second-zone publication passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
