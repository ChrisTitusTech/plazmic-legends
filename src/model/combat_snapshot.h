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

struct CombatAbilitySnapshot {
    std::string name;
    std::string category;
    std::uint64_t damage{};
    std::uint32_t hits{};

    bool operator==(const CombatAbilitySnapshot&) const = default;
};

struct CombatParticipantSnapshot {
    std::string name;
    std::uint64_t damage{};
    std::uint32_t hits{};
    double dps{};
    double percentage{};
    double active_seconds{};
    std::uint64_t melee_damage{};
    std::uint64_t spell_damage{};
    std::uint64_t damage_over_time{};
    std::uint64_t pet_damage{};
    std::vector<CombatAbilitySnapshot> abilities;

    bool operator==(const CombatParticipantSnapshot&) const = default;
};

struct CombatHealerSnapshot {
    std::string name;
    std::uint64_t healing{};
    std::uint32_t casts{};
    double hps{};
    double percentage{};

    bool operator==(const CombatHealerSnapshot&) const = default;
};

struct CombatTimelinePoint {
    std::uint32_t elapsed_seconds{};
    std::uint64_t damage{};
    std::uint64_t healing{};

    bool operator==(const CombatTimelinePoint&) const = default;
};

struct CombatEncounterSnapshot {
    CombatEncounterState state{CombatEncounterState::unavailable};
    std::string target;
    std::vector<CombatParticipantSnapshot> participants;
    std::vector<CombatHealerSnapshot> healers;
    std::vector<CombatTimelinePoint> timeline;
    std::string zone;
    std::int64_t started_unix_seconds{};
    std::uint64_t total_damage{};
    std::uint64_t total_healing{};
    double duration_seconds{};
    double active_character_dps{};
    std::string detail{"Combat log unavailable"};

    bool operator==(const CombatEncounterSnapshot&) const = default;

    [[nodiscard]] bool available() const {
        return state == CombatEncounterState::active ||
               state == CombatEncounterState::complete;
    }
};

struct CombatAnalyticsSnapshot {
    CombatEncounterSnapshot encounter;
    std::vector<CombatEncounterSnapshot> history;
    std::uint64_t zone_damage{};
    std::uint64_t zone_healing{};
    std::uint32_t zone_encounters{};
    bool history_retention_enabled{false};
    bool history_persisted{true};
    std::string history_detail;
};

}  // namespace plazmic
