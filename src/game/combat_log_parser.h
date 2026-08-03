#pragma once

#include "model/combat_snapshot.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace plazmic {

enum class DamageKind {
    melee,
    spell,
    damage_over_time,
    pet,
};

struct DamageEvent {
    std::chrono::system_clock::time_point timestamp;
    std::string attacker;
    std::string defender;
    std::uint64_t damage{};
    DamageKind kind{DamageKind::melee};
};

[[nodiscard]] std::optional<DamageEvent> parse_damage_line(
    std::string_view line,
    std::string_view active_character);

class CombatAccumulator {
  public:
    static constexpr auto inactivity = std::chrono::seconds(10);
    static constexpr std::size_t maximum_participants = 256U;

    void clear();
    [[nodiscard]] bool add(const DamageEvent& event,
                           std::string_view active_character = {});
    [[nodiscard]] CombatEncounterSnapshot snapshot(
        std::chrono::system_clock::time_point now,
        std::string_view active_character) const;

  private:
    struct Participant {
        std::uint64_t damage{};
        std::uint32_t hits{};
        std::chrono::system_clock::time_point first;
        std::chrono::system_clock::time_point last;
        std::array<std::uint64_t, 4> kind_damage{};
    };

    std::unordered_map<std::string, Participant> participants_;
    std::string target_;
    std::uint64_t total_damage_{};
    std::chrono::system_clock::time_point first_;
    std::chrono::system_clock::time_point last_;
};

enum class CombatLogError {
    none,
    invalid_character,
    missing,
    ambiguous,
    unavailable,
    read_failed,
};

struct CombatLogRefresh {
    CombatEncounterSnapshot snapshot;
    CombatLogError error{CombatLogError::none};
};

class CombatLogTailer {
  public:
    static constexpr std::size_t maximum_line_bytes = 4096U;
    static constexpr std::size_t maximum_read_bytes = 256U * 1024U;
    static constexpr std::size_t maximum_lines_per_refresh = 4096U;
    static constexpr std::size_t maximum_log_entries = 4096U;
    static constexpr std::size_t boundary_bytes = 64U;

    struct FileIdentity {
        std::uint64_t device{};
        std::uint64_t inode{};

        bool operator==(const FileIdentity&) const = default;
    };

    explicit CombatLogTailer(bool start_at_end = true)
        : start_at_end_(start_at_end) {}

    [[nodiscard]] CombatLogRefresh refresh(
        const std::filesystem::path& game_directory,
        std::string_view active_character,
        std::chrono::system_clock::time_point now =
            std::chrono::system_clock::now());
    void clear();

  private:
    [[nodiscard]] CombatLogRefresh failure(
        CombatLogError error,
        std::string detail) const;
    [[nodiscard]] std::optional<std::filesystem::path> select_log(
        const std::filesystem::path& game_directory,
        std::string_view active_character,
        CombatLogError& error) const;
    [[nodiscard]] bool consume(char byte,
                               std::string_view active_character);

    bool start_at_end_;
    std::string character_;
    std::filesystem::path path_;
    std::optional<FileIdentity> identity_;
    std::uintmax_t offset_{};
    std::string partial_line_;
    bool dropping_line_{false};
    bool oversized_line_seen_{false};
    std::size_t lines_this_refresh_{};
    std::string prefix_boundary_;
    std::string boundary_;
    bool reopen_from_start_{false};
    CombatAccumulator accumulator_;
};

}  // namespace plazmic
