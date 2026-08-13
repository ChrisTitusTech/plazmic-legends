#pragma once

#include "model/combat_snapshot.h"

#include <chrono>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace plazmic {

struct CombatHistoryLoad {
    std::vector<CombatEncounterSnapshot> history;
    bool persisted{true};
};

struct CombatHistoryPrune {
    bool healthy{true};
    std::optional<std::chrono::system_clock::time_point> next_expiration;
};

class CombatHistoryStore {
  public:
    static constexpr std::size_t maximum_encounters = 50U;
    static constexpr std::size_t maximum_file_bytes = 2U * 1024U * 1024U;
    static constexpr std::size_t maximum_total_abilities = 4096U;
    static constexpr std::uint64_t maximum_aggregate =
        1'000'000'000'000'000ULL;
    static constexpr auto maximum_age = std::chrono::days(90);

    explicit CombatHistoryStore(std::filesystem::path state_root);

    [[nodiscard]] std::vector<CombatEncounterSnapshot> load(
        std::string_view local_character_key) const;
    [[nodiscard]] std::optional<CombatHistoryLoad>
    load_checked(std::string_view local_character_key) const;
    [[nodiscard]] bool bound(
        std::vector<CombatEncounterSnapshot>& history) const;
    [[nodiscard]] bool save(
        std::string_view local_character_key,
        std::vector<CombatEncounterSnapshot>& history) const;
    [[nodiscard]] CombatHistoryPrune prune_expired() const;

    [[nodiscard]] static std::filesystem::path default_state_root();
    [[nodiscard]] static std::string privacy_key(std::string_view identity);

  private:
    [[nodiscard]] std::filesystem::path path_for(
        std::string_view local_character_key) const;

    std::filesystem::path state_root_;
};

}  // namespace plazmic
