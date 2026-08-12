#pragma once

#include "game/combat_history_store.h"
#include "model/activity_snapshot.h"
#include "model/character_snapshot.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace plazmic {

struct DamageEvent;

[[nodiscard]] std::optional<ActivityEventSnapshot> parse_activity_line(
    std::string_view line,
    std::string_view active_character,
    std::string_view zone);

class ActivityTracker {
  public:
    static constexpr std::size_t maximum_events = 512U;
    static constexpr std::size_t maximum_abilities = 512U;
    static constexpr std::size_t maximum_ability_observations = 4096U;
    static constexpr std::uint32_t maximum_occurrences_per_fingerprint = 4096U;
    static constexpr std::size_t maximum_sweep_partitions = 8U;
    static constexpr auto maximum_age = std::chrono::hours(24 * 90);
    static constexpr auto persistence_retry_delay = std::chrono::seconds(1);

    explicit ActivityTracker(
        std::filesystem::path state_root =
            CombatHistoryStore::default_state_root())
        : state_root_(std::move(state_root)) {}

    void clear();
    [[nodiscard]] bool select(std::string key);
    void set_retention_enabled(bool enabled);
    void begin_deferred_persistence() { persistence_deferred_ = true; }
    void commit_deferred_persistence(bool enabled);
    [[nodiscard]] bool delete_history(std::string_view key);
    [[nodiscard]] bool flush();
    std::string consume(std::string_view line,
                        std::string_view active_character,
                        std::string_view zone,
                        bool source_required = false);
    void observe_damage(const DamageEvent& event,
                        std::string_view active_character,
                        std::string source_id);
    void begin_log_stream(std::string_view replay_prefix = {});
    void break_equipment_baseline();
    void reset_transient_observations();
    void observe_character(const CharacterSnapshot& character,
                           std::string_view zone,
                           std::chrono::system_clock::time_point now =
                               std::chrono::system_clock::now());
    void maintain(
        std::chrono::system_clock::time_point now =
            std::chrono::system_clock::now());
    [[nodiscard]] ActivityAnalyticsSnapshot snapshot(
        std::chrono::system_clock::time_point now =
            std::chrono::system_clock::now());
    [[nodiscard]] ActivityAnalyticsSnapshot unavailable_snapshot(
        std::string detail) const;
    [[nodiscard]] std::string_view selected_key() const { return key_; }

  private:
    struct AbilityAggregate {
        std::string name;
        std::string category;
        struct Observation {
            std::string source_id;
            std::uint64_t damage{};
            std::int64_t timestamp_unix_seconds{};
        };
        std::vector<Observation> observations;
    };

    struct ReplaySource {
        std::string source_id;
        std::int64_t timestamp_unix_seconds{};
    };

    void append(ActivityEventSnapshot event);
    void prune(std::chrono::system_clock::time_point now);
    [[nodiscard]] bool load(
        std::chrono::system_clock::time_point now =
            std::chrono::system_clock::now());
    [[nodiscard]] bool save();
    [[nodiscard]] bool persist();
    void maintain_store(std::chrono::system_clock::time_point now);
    [[nodiscard]] std::string next_source_id(std::string_view line);
    [[nodiscard]] std::string allocate_stream_generation();
    void bound_ability_observations();
    void bound_ability_aggregates();
    void remember_replay_source(const std::string& source_id,
                                std::int64_t timestamp_unix_seconds);

    std::vector<ActivityEventSnapshot> events_;
    std::unordered_map<std::string, AbilityAggregate> abilities_;
    std::vector<EquipmentSlotSnapshot> equipment_;
    std::string character_;
    std::filesystem::path state_root_;
    std::string key_;
    bool retention_enabled_{false};
    bool persisted_{true};
    bool compatible_{true};
    bool dirty_{false};
    bool partition_loaded_{false};
    bool persistence_deferred_{false};
    std::chrono::steady_clock::time_point persistence_retry_after_{};
    std::chrono::system_clock::time_point sweep_after_{};
    std::string last_swept_key_;
    std::unordered_map<std::string, std::uint32_t> stream_occurrences_;
    std::unordered_map<std::string, std::vector<std::string>> replay_sources_;
    std::unordered_set<std::string> boundary_source_ids_;
    std::vector<ReplaySource> replay_history_;
    std::string stream_generation_;
    std::uint64_t stream_overflow_sequence_{};
    std::uint64_t generation_counter_{};
};

[[nodiscard]] bool save_activity_export(
    const std::filesystem::path& path,
    const ActivityAnalyticsSnapshot& snapshot);

[[nodiscard]] InventoryReconciliationSnapshot import_inventory_output(
    const std::filesystem::path& path,
    const CharacterSnapshot& character);
[[nodiscard]] InventoryReconciliationSnapshot reconcile_inventory_entries(
    std::vector<InventoryEntrySnapshot> entries,
    std::string source_name,
    const CharacterSnapshot& character);

}  // namespace plazmic
