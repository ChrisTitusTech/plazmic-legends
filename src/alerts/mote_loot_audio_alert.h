#pragma once

#include "model/activity_snapshot.h"

#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_set>

namespace plazmic {

[[nodiscard]] bool contains_mote_word(std::string_view item_name);
[[nodiscard]] bool play_mote_loot_desktop_sound();

class MoteLootAudioAlert {
  public:
    using Clock = std::function<std::chrono::steady_clock::time_point()>;
    using Sink = std::function<void()>;

    static constexpr auto rate_limit = std::chrono::seconds(2);
    static constexpr std::size_t maximum_snapshot_events = 512U;
    static constexpr std::size_t maximum_tracked_sources = 1024U;

    explicit MoteLootAudioAlert(Sink sink, Clock clock = {});

    void set_enabled(bool enabled);
    [[nodiscard]] bool enabled() const { return enabled_; }
    void observe(const ActivityAnalyticsSnapshot& snapshot);
    void reset();

  private:
    void establish_baseline(const ActivityAnalyticsSnapshot& snapshot);

    Sink sink_;
    Clock clock_;
    std::string storage_key_;
    std::unordered_set<std::string> seen_source_ids_;
    std::chrono::steady_clock::time_point next_dispatch_{};
    bool baseline_ready_{false};
    bool retention_enabled_{false};
    bool enabled_{false};
    bool pending_{false};
};

}  // namespace plazmic
