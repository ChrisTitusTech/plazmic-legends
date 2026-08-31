#include "alerts/mote_loot_audio_alert.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

plazmic::ActivityEventSnapshot loot(std::string label,
                                    std::string source_id) {
    return {
        .kind = plazmic::ActivityEventKind::loot,
        .timestamp_unix_seconds = 1,
        .zone = "synthetic_zone",
        .label = std::move(label),
        .amount = 1.0,
        .total = std::nullopt,
        .evidence = "Synthetic exact loot line",
        .source_id = std::move(source_id),
    };
}

}  // namespace

int main() {
    try {
        require(plazmic::contains_mote_word("Mote") &&
                    plazmic::contains_mote_word("a mote of synthetic dust") &&
                    plazmic::contains_mote_word("Synthetic-MOTE (Pristine)"),
                "case-insensitive Mote word matching rejected valid items");
        require(!plazmic::contains_mote_word("Remote Crystal") &&
                    !plazmic::contains_mote_word("Motet") &&
                    !plazmic::contains_mote_word("Mote_One") &&
                    !plazmic::contains_mote_word(""),
                "Mote matching accepted a partial word");

        auto now = std::chrono::steady_clock::time_point{};
        int dispatches = 0;
        plazmic::MoteLootAudioAlert alert(
            [&dispatches]() { ++dispatches; }, [&now]() { return now; });
        alert.set_enabled(true);

        plazmic::ActivityAnalyticsSnapshot snapshot;
        snapshot.storage_key = "synthetic-partition-one";
        snapshot.available = true;
        snapshot.events.push_back(loot("Existing Mote", "source-1"));
        alert.observe(snapshot);
        require(dispatches == 0,
                "restored or initial Mote history emitted an alert");

        snapshot.events.push_back(loot("Synthetic Gem", "source-2"));
        alert.observe(snapshot);
        require(dispatches == 0, "non-Mote loot emitted an alert");

        snapshot.events.push_back(loot("Mote of Synthetic Light", "source-3"));
        alert.observe(snapshot);
        alert.observe(snapshot);
        require(dispatches == 1,
                "new Mote loot did not alert exactly once");

        now += std::chrono::seconds(1);
        snapshot.events.push_back(loot("Second Mote", "source-4"));
        alert.observe(snapshot);
        require(dispatches == 1,
                "rate limit dispatched a second alert too early");
        now += std::chrono::seconds(1);
        alert.observe(snapshot);
        require(dispatches == 2,
                "rate-limited Mote alert was not coalesced and dispatched");

        alert.set_enabled(false);
        snapshot.events.push_back(loot("Disabled Mote", "source-5"));
        alert.observe(snapshot);
        snapshot.events.push_back(
            loot("Late Disabled Mote", "source-5-late"));
        alert.set_enabled(true);
        now += std::chrono::seconds(2);
        alert.observe(snapshot);
        require(dispatches == 2,
                "disabled Mote observations were replayed after opt-in");

        snapshot.storage_key = "synthetic-partition-two";
        snapshot.events = {loot("Restored Mote", "source-6")};
        alert.observe(snapshot);
        require(dispatches == 2,
                "character partition switch replayed retained loot");
        snapshot.events.push_back(loot("Fresh mote", "source-7"));
        alert.observe(snapshot);
        require(dispatches == 3,
                "fresh Mote loot after a partition switch did not alert");

        snapshot.available = false;
        alert.observe(snapshot);
        snapshot.available = true;
        alert.observe(snapshot);
        require(dispatches == 3,
                "lifecycle recovery replayed existing Mote loot");
        snapshot.events.push_back(loot("Unidentified Mote", {}));
        alert.observe(snapshot);
        require(dispatches == 3,
                "loot without a stable source identity emitted an alert");

        auto lifecycle_now = std::chrono::steady_clock::time_point{};
        int lifecycle_dispatches = 0;
        plazmic::MoteLootAudioAlert lifecycle_alert(
            [&lifecycle_dispatches]() { ++lifecycle_dispatches; },
            [&lifecycle_now]() { return lifecycle_now; });
        lifecycle_alert.set_enabled(true);
        plazmic::ActivityAnalyticsSnapshot lifecycle_snapshot;
        lifecycle_snapshot.storage_key = "synthetic-lifecycle-partition";
        lifecycle_snapshot.available = true;
        lifecycle_alert.observe(lifecycle_snapshot);
        lifecycle_snapshot.events.push_back(
            loot("First Lifecycle Mote", "lifecycle-source-1"));
        lifecycle_alert.observe(lifecycle_snapshot);
        require(lifecycle_dispatches == 1,
                "lifecycle rate-limit fixture did not dispatch initially");
        lifecycle_snapshot.available = false;
        lifecycle_alert.observe(lifecycle_snapshot);
        lifecycle_snapshot.available = true;
        lifecycle_alert.observe(lifecycle_snapshot);
        lifecycle_now += std::chrono::seconds(1);
        lifecycle_snapshot.events.push_back(
            loot("Second Lifecycle Mote", "lifecycle-source-2"));
        lifecycle_alert.observe(lifecycle_snapshot);
        require(lifecycle_dispatches == 1,
                "lifecycle reset bypassed the dispatch rate limit");
        lifecycle_now += std::chrono::seconds(1);
        lifecycle_alert.observe(lifecycle_snapshot);
        require(lifecycle_dispatches == 2,
                "lifecycle reset discarded a coalesced Mote alert");

        auto retention_now = std::chrono::steady_clock::time_point{};
        int retention_dispatches = 0;
        plazmic::MoteLootAudioAlert retention_alert(
            [&retention_dispatches]() { ++retention_dispatches; },
            [&retention_now]() { return retention_now; });
        retention_alert.set_enabled(true);
        plazmic::ActivityAnalyticsSnapshot retention_snapshot;
        retention_snapshot.storage_key = "synthetic-retention-partition";
        retention_snapshot.available = true;
        retention_snapshot.events.push_back(
            loot("Session Gem", "retention-session-source"));
        retention_alert.observe(retention_snapshot);
        retention_snapshot.retention_enabled = true;
        retention_snapshot.events.push_back(
            loot("Restored Retained Mote", "retention-history-source"));
        retention_alert.observe(retention_snapshot);
        require(retention_dispatches == 0,
                "enabling retention replayed a restored Mote alert");
        retention_snapshot.events.push_back(
            loot("Fresh Retained Mote", "retention-fresh-source"));
        retention_alert.observe(retention_snapshot);
        require(retention_dispatches == 1,
                "fresh Mote after retained-history baseline did not alert");

        plazmic::ActivityAnalyticsSnapshot bounded;
        bounded.storage_key = "synthetic-bounded-partition";
        bounded.available = true;
        for (std::size_t index = 0U; index < 512U; ++index) {
            bounded.events.push_back(
                loot("Synthetic Gem", "baseline-" + std::to_string(index)));
        }
        alert.observe(bounded);
        bounded.events.clear();
        for (std::size_t index = 0U; index < 512U; ++index) {
            bounded.events.push_back(
                loot("Synthetic Gem", "second-" + std::to_string(index)));
        }
        alert.observe(bounded);
        require(dispatches == 3,
                "bounded non-Mote source tracking emitted an alert");
        bounded.events.erase(bounded.events.begin());
        bounded.events.push_back(loot("Boundary Mote", "boundary-mote"));
        now += std::chrono::seconds(2);
        alert.observe(bounded);
        require(dispatches == 4,
                "source-cap rebaseline lost a newly observed Mote alert");

        auto oversized = bounded;
        oversized.events.push_back(loot("Oversized Mote", "oversized-mote"));
        alert.observe(oversized);
        require(dispatches == 4,
                "oversized activity snapshot emitted an alert");
        alert.observe(bounded);
        bounded.events.push_back(loot("Recovered Mote", "recovered-mote"));
        bounded.events.erase(bounded.events.begin());
        now += std::chrono::seconds(2);
        alert.observe(bounded);
        require(dispatches == 5,
                "valid activity did not recover after oversized input");

        std::cout << "Mote loot audio matching and lifecycle tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
