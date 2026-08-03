#pragma once

#include "game/client_profile.h"
#include "integration/process_discovery.h"
#include "model/character_snapshot.h"

#include <cstdint>
#include <optional>
#include <string>

namespace plazmic {

enum class CharacterReadError {
    none,
    invalid_profile,
    invalid_pointer,
    invalid_value,
    inconsistent_snapshot,
    read_failed,
};

struct CharacterReadResult {
    std::optional<CharacterSnapshot> snapshot;
    CharacterReadError error{CharacterReadError::invalid_profile};
    std::string detail;

    [[nodiscard]] explicit operator bool() const {
        return snapshot.has_value() && error == CharacterReadError::none;
    }
};

[[nodiscard]] CharacterReadResult read_character_snapshot(
    const ClientProcess& process,
    std::uintptr_t local_player,
    const CharacterSymbols& symbols);

}  // namespace plazmic
