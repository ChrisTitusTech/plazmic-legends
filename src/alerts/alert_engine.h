#pragma once

#include "model/alert_snapshot.h"

#include <chrono>
#include <deque>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace plazmic {

struct AlertRulePack {
    std::vector<AlertRule> rules;
    std::string source_name;
    std::uint64_t generation{};
};

[[nodiscard]] std::optional<AlertRulePack> load_alert_rule_pack(
    const std::filesystem::path& path,
    std::string& detail);

class AlertEngine {
  public:
    static constexpr std::size_t maximum_rules = 128U;
    static constexpr std::size_t maximum_timers = 256U;
    static constexpr std::size_t maximum_recent_alerts = 128U;
    static constexpr std::size_t maximum_observed_sources = 4096U;
    static constexpr std::uint32_t maximum_duration_seconds = 7U * 24U * 60U * 60U;
    static constexpr std::uint32_t maximum_cooldown_seconds = 24U * 60U * 60U;

    void set_rules(
        AlertRulePack pack,
        std::chrono::system_clock::time_point now =
            std::chrono::system_clock::now());
    void clear_rules();
    void set_enabled(
        bool enabled,
        std::chrono::system_clock::time_point now =
            std::chrono::system_clock::now());
    bool consume(
        std::string_view line,
        std::string_view zone,
        std::string_view source_id = {},
        std::chrono::system_clock::time_point now =
            std::chrono::system_clock::now());
    void reset_transient();
    void clear_observations();
    [[nodiscard]] bool has_rules() const { return enabled_ && !rules_.empty(); }
    [[nodiscard]] AlertAnalyticsSnapshot snapshot(
        std::chrono::system_clock::time_point now =
            std::chrono::system_clock::now());

  private:
    std::vector<AlertRule> rules_;
    std::vector<std::string> normalized_matches_;
    std::vector<AlertTimerSnapshot> timers_;
    std::vector<FiredAlertSnapshot> recent_alerts_;
    std::unordered_map<std::string, std::int64_t> last_fired_;
    std::deque<std::string> observed_source_order_;
    std::unordered_set<std::string> observed_sources_;
    std::string source_name_;
    std::uint64_t rules_generation_{};
    std::uint64_t next_sequence_{1U};
    std::uint64_t latest_sound_sequence_{};
    std::int64_t activation_unix_seconds_{};
    bool enabled_{false};
};

[[nodiscard]] std::string_view alert_timer_kind_label(AlertTimerKind kind);

}  // namespace plazmic
