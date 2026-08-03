#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace plazmic {

enum class CombatEncounterState {
    unavailable,
    idle,
    active,
    complete,
};

struct CombatParticipantSnapshot {
    std::string name;
    std::uint64_t damage{};
    std::uint32_t hits{};
    double dps{};
    double percentage{};
    double active_seconds{};
};

struct CombatEncounterSnapshot {
    CombatEncounterState state{CombatEncounterState::unavailable};
    std::string target;
    std::vector<CombatParticipantSnapshot> participants;
    std::uint64_t total_damage{};
    double duration_seconds{};
    double active_character_dps{};
    std::string detail{"Combat log unavailable"};

    [[nodiscard]] bool available() const {
        return state == CombatEncounterState::active ||
               state == CombatEncounterState::complete;
    }
};

}  // namespace plazmic
