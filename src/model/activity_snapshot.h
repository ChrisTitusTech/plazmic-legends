#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace plazmic {

enum class ActivityEventKind {
    experience,
    alternate_advancement,
    loot,
    equipment_change,
    celebration,
};

struct ActivityEventSnapshot {
    ActivityEventKind kind{ActivityEventKind::experience};
    std::int64_t timestamp_unix_seconds{};
    std::string zone;
    std::string label;
    double amount{};
    std::optional<std::uint32_t> total;
    std::string evidence;
    std::string source_id;

    bool operator==(const ActivityEventSnapshot&) const = default;
};

struct AbilityActivitySnapshot {
    std::string name;
    std::string category;
    std::uint64_t damage{};
    std::uint32_t observations{};
    std::string confidence;

    bool operator==(const AbilityActivitySnapshot&) const = default;
};

struct InventoryEntrySnapshot {
    std::string location;
    std::string item;
    std::uint32_t quantity{};

    bool operator==(const InventoryEntrySnapshot&) const = default;
};

struct InventoryReconciliationSnapshot {
    std::vector<InventoryEntrySnapshot> entries;
    std::vector<std::string> equipped_not_in_import;
    std::vector<std::string> imported_equipped_items;
    std::string source_name;
    std::string detail{"Select an EverQuest inventory output file"};
    bool available{false};
};

struct ActivityAnalyticsSnapshot {
    std::string storage_key;
    std::vector<ActivityEventSnapshot> events;
    std::vector<AbilityActivitySnapshot> abilities;
    double experience_percent{};
    double experience_percent_per_hour{};
    std::optional<double> level_pace_hours;
    std::optional<std::uint32_t> alternate_advancement_points;
    double alternate_advancement_points_per_hour{};
    std::optional<double> next_alternate_advancement_hours;
    std::uint32_t recent_loot_count{};
    std::string class_activity_summary;
    std::optional<std::string> recent_celebration;
    bool retention_enabled{false};
    bool persisted{true};
    bool available{false};
    std::string detail{"Waiting for progression or activity events"};

    bool operator==(const ActivityAnalyticsSnapshot&) const = default;
};

inline bool same_activity_export_payload(
    const ActivityAnalyticsSnapshot& left,
    const ActivityAnalyticsSnapshot& right) {
    return left.events == right.events &&
           left.abilities == right.abilities;
}

}  // namespace plazmic
