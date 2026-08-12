#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace plazmic {

enum class AlertTimerKind {
    buff,
    crowd_control,
    respawn,
    custom,
};

struct AlertRule {
    std::string id;
    std::string name;
    std::string match;
    AlertTimerKind kind{AlertTimerKind::custom};
    std::uint32_t duration_seconds{};
    std::uint32_t cooldown_seconds{2U};
    bool sound{false};

    bool operator==(const AlertRule&) const = default;
};

struct AlertTimerSnapshot {
    std::string id;
    std::string name;
    AlertTimerKind kind{AlertTimerKind::custom};
    std::string zone;
    std::int64_t started_unix_seconds{};
    std::int64_t ends_unix_seconds{};
    bool observed{true};

    bool operator==(const AlertTimerSnapshot&) const = default;
};

struct FiredAlertSnapshot {
    std::string rule_id;
    std::string label;
    std::string zone;
    std::int64_t timestamp_unix_seconds{};
    std::uint64_t sequence{};
    bool sound{false};

    bool operator==(const FiredAlertSnapshot&) const = default;
};

struct AlertAnalyticsSnapshot {
    std::vector<AlertTimerSnapshot> timers;
    std::vector<FiredAlertSnapshot> recent_alerts;
    std::string rules_source;
    std::uint64_t rules_generation{};
    std::uint64_t latest_sound_sequence{};
    bool available{false};
    std::string detail{"Import a local alert rule pack"};

    bool operator==(const AlertAnalyticsSnapshot&) const = default;
};

}  // namespace plazmic
