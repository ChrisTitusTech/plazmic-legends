#pragma once

#include "game/client_profile.h"
#include "integration/process_discovery.h"
#include "model/player_snapshot.h"
#include "model/spawn_snapshot.h"

#include <cstdint>
#include <optional>
#include <string>

namespace plazmic {

enum class SpawnReadError {
    none,
    invalid_profile,
    inconsistent_collection,
    invalid_collection,
    read_failed,
};

struct SpawnReadResult {
    std::optional<SpawnCollectionSnapshot> snapshot;
    SpawnReadError error{SpawnReadError::read_failed};
    std::string detail;

    [[nodiscard]] explicit operator bool() const {
        return snapshot.has_value() && error == SpawnReadError::none;
    }
};

[[nodiscard]] SpawnReadResult read_spawn_collection(
    const ClientProcess& process,
    std::uintptr_t root,
    const SpawnSymbols& symbols,
    const PlayerSnapshot& player);

}  // namespace plazmic
