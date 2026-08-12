#pragma once

#include "activity/activity_tracker.h"
#include "game/combat_history_store.h"
#include "model/combat_snapshot.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

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
    std::string ability;
};

[[nodiscard]] bool is_activity_ability(const DamageEvent& event);

struct HealingEvent {
    std::chrono::system_clock::time_point timestamp;
    std::string healer;
    std::string target;
    std::uint64_t healing{};
};

[[nodiscard]] std::optional<DamageEvent> parse_damage_line(
    std::string_view line,
    std::string_view active_character);
[[nodiscard]] std::optional<HealingEvent> parse_healing_line(
    std::string_view line,
    std::string_view active_character);

class CombatAccumulator {
  public:
    static constexpr auto inactivity = std::chrono::seconds(10);
    static constexpr std::size_t maximum_participants = 256U;
    static constexpr std::size_t maximum_abilities_per_participant = 128U;
    static constexpr std::size_t maximum_total_abilities =
        CombatHistoryStore::maximum_total_abilities;

    void clear();
    [[nodiscard]] bool add(const DamageEvent& event,
                           std::string_view active_character = {},
                           std::string_view zone = {});
    [[nodiscard]] bool add(const HealingEvent& event,
                           std::string_view active_character = {},
                           std::string_view zone = {});
    [[nodiscard]] std::optional<CombatEncounterSnapshot> take_completed();
    [[nodiscard]] std::optional<CombatEncounterSnapshot> finalize(
        std::string_view active_character);
    [[nodiscard]] CombatEncounterSnapshot snapshot(
        std::chrono::system_clock::time_point now,
        std::string_view active_character) const;

  private:
    struct Participant {
        struct Ability {
            std::string name;
            DamageKind kind{DamageKind::melee};
            std::uint64_t damage{};
            std::uint32_t hits{};
        };

        std::uint64_t damage{};
        std::uint32_t hits{};
        std::chrono::system_clock::time_point first;
        std::chrono::system_clock::time_point last;
        std::array<std::uint64_t, 4> kind_damage{};
        std::unordered_map<std::string, Ability> abilities;
    };

    struct Healer {
        std::uint64_t healing{};
        std::uint32_t casts{};
    };

    struct TimelineBucket {
        std::uint64_t damage{};
        std::uint64_t healing{};
    };

    std::unordered_map<std::string, Participant> participants_;
    std::unordered_map<std::string, Healer> healers_;
    std::unordered_map<std::uint32_t, TimelineBucket> timeline_;
    std::string target_;
    std::string zone_;
    std::uint64_t total_damage_{};
    std::uint64_t total_healing_{};
    std::size_t total_abilities_{};
    std::chrono::system_clock::time_point first_;
    std::chrono::system_clock::time_point last_;
    std::optional<CombatEncounterSnapshot> completed_;
    bool active_{false};
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
    CombatAnalyticsSnapshot snapshot;
    ActivityAnalyticsSnapshot activity;
    CombatLogError error{CombatLogError::none};
};

class CombatLogTailer {
  public:
    static constexpr std::size_t maximum_line_bytes = 4096U;
    static constexpr std::size_t maximum_read_bytes = 256U * 1024U;
    static constexpr std::size_t maximum_lines_per_refresh = 4096U;
    static constexpr std::size_t maximum_log_entries = 4096U;
    static constexpr std::size_t boundary_bytes = 64U;
    static constexpr std::size_t replay_prefix_bytes = 256U * 1024U;
    static constexpr std::size_t maximum_replay_paths = 8U;
    static constexpr auto history_retry_delay = std::chrono::seconds(1);

    struct FileIdentity {
        std::uint64_t device{};
        std::uint64_t inode{};

        bool operator==(const FileIdentity&) const = default;
    };

    struct ReplayContinuity {
        FileIdentity identity;
        std::shared_ptr<int> descriptor;
        std::uintmax_t offset{};
        std::string partial_line;
        bool dropping_line{false};
        bool oversized_line_seen{false};
        std::string prefix;
        std::string boundary;
    };

    explicit CombatLogTailer(
        bool start_at_end = true,
        std::filesystem::path state_root =
            CombatHistoryStore::default_state_root(),
        bool history_enabled = false)
        : start_at_end_(start_at_end),
          activity_tracker_(state_root),
          history_store_(std::move(state_root)),
          history_enabled_(history_enabled) {}

    void set_history_enabled(bool enabled);
    void set_activity_history_enabled(bool enabled) {
        activity_tracker_.set_retention_enabled(enabled);
    }
    void begin_deferred_persistence();
    void commit_deferred_persistence(bool retain_history,
                                     bool retain_activity);
    [[nodiscard]] bool delete_activity_history(std::string_view key) {
        return activity_tracker_.delete_history(key);
    }
    void observe_character(
        const CharacterSnapshot& character,
        std::string_view zone,
        std::chrono::system_clock::time_point now =
            std::chrono::system_clock::now());
    [[nodiscard]] ActivityAnalyticsSnapshot activity_snapshot(
        std::chrono::system_clock::time_point now =
            std::chrono::system_clock::now()) {
        return activity_tracker_.snapshot(now);
    }

    [[nodiscard]] CombatLogRefresh refresh(
        const std::filesystem::path& game_directory,
        std::string_view active_character,
        std::string_view zone = "Unknown",
        std::chrono::system_clock::time_point now =
            std::chrono::system_clock::now());
    [[nodiscard]] CombatLogRefresh refresh(
        const std::filesystem::path& game_directory,
        std::string_view active_character,
        std::chrono::system_clock::time_point now) {
        return refresh(game_directory, active_character, "Unknown", now);
    }
    [[nodiscard]] CombatLogRefresh current_snapshot(
        std::string_view active_character,
        std::string_view zone = "Unknown",
        std::chrono::system_clock::time_point now =
            std::chrono::system_clock::now());
    void maintain_retained_state(
        std::chrono::system_clock::time_point now =
            std::chrono::system_clock::now()) {
        maintain_history_store();
        activity_tracker_.maintain(now);
    }
    void clear();

  private:
    [[nodiscard]] CombatLogRefresh failure(
        CombatLogError error,
        std::string detail);
    [[nodiscard]] std::optional<std::filesystem::path> select_log(
        const std::filesystem::path& game_directory,
        std::string_view active_character,
        CombatLogError& error) const;
    [[nodiscard]] bool consume(char byte,
                               std::string_view active_character,
                               std::string_view zone);
    [[nodiscard]] bool select_history(
        std::string_view active_character,
        const std::filesystem::path& log_path,
        std::string& failure_detail);
    void retain_completed();
    void finalize_current();
    void retain_encounter(const CombatEncounterSnapshot& encounter);
    void maintain_history();
    void maintain_history_store();

    bool start_at_end_;
    std::string character_;
    std::filesystem::path path_;
    std::optional<FileIdentity> identity_;
    std::shared_ptr<int> descriptor_;
    std::uintmax_t offset_{};
    std::string partial_line_;
    bool dropping_line_{false};
    bool oversized_line_seen_{false};
    std::size_t lines_this_refresh_{};
    std::string prefix_boundary_;
    std::unordered_map<std::string, ReplayContinuity> replay_continuity_;
    std::unordered_set<std::string> seen_log_paths_;
    std::string boundary_;
    bool reopen_from_start_{false};
    ActivityTracker activity_tracker_;
    CombatAccumulator accumulator_;
    CombatHistoryStore history_store_;
    std::string history_key_;
    std::vector<CombatEncounterSnapshot> history_;
    std::string history_zone_;
    bool history_enabled_{false};
    bool history_visible_{false};
    bool history_load_compatible_{true};
    bool history_persisted_{true};
    bool activity_partition_confirmed_{false};
    bool persistence_deferred_{false};
    std::chrono::steady_clock::time_point history_retry_after_{};
    std::chrono::system_clock::time_point history_sweep_after_{};
};

}  // namespace plazmic
