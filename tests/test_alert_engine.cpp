#include "alerts/alert_engine.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>
#include <stdexcept>
#include <string>

#include <sys/stat.h>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::filesystem::path fixture_directory() {
    char path[] = "/tmp/plazmic-alert-test-XXXXXX";
    const char* created = ::mkdtemp(path);
    if (created == nullptr) {
        throw std::runtime_error("cannot create alert fixture directory");
    }
    return created;
}

}  // namespace

int main() {
    std::filesystem::path directory;
    try {
        directory = fixture_directory();
        const auto pack_path = directory / "synthetic-alerts.json";
        {
            std::ofstream output(pack_path, std::ios::binary);
            output << R"({"schema":1,"rules":[)"
                   << R"({"id":"buff","name":"Synthetic Ward","match":"ward settles over you","kind":"buff","durationSeconds":60,"cooldownSeconds":2,"sound":false},)"
                   << R"({"id":"respawn","name":"Synthetic Captain","match":"captain has been slain","kind":"respawn","durationSeconds":120,"cooldownSeconds":10,"sound":true},)"
                   << R"({"id":"notice","name":"Synthetic Notice","match":"important synthetic event","kind":"custom","durationSeconds":0,"cooldownSeconds":5,"sound":true})"
                   << "]}";
        }
        std::string detail;
        auto pack = plazmic::load_alert_rule_pack(pack_path, detail);
        require(pack && pack->rules.size() == 3U &&
                    pack->source_name == "synthetic-alerts.json",
                "valid bounded alert pack was rejected");

        plazmic::AlertEngine engine;
        engine.set_rules(std::move(*pack));
        require(!engine.snapshot().available,
                "alert rules activated without independent consent");
        std::tm fixture_clock{};
        fixture_clock.tm_year = 126;
        fixture_clock.tm_mon = 7;
        fixture_clock.tm_mday = 3;
        fixture_clock.tm_hour = 12;
        fixture_clock.tm_sec = 4;
        fixture_clock.tm_isdst = -1;
        const std::time_t fixture_time = std::mktime(&fixture_clock);
        require(fixture_time != static_cast<std::time_t>(-1),
                "cannot construct local alert fixture clock");
        const auto fixture_now =
            std::chrono::system_clock::from_time_t(fixture_time);
        engine.set_enabled(true, fixture_now - std::chrono::seconds(10));
        engine.consume(
            "[Wed Aug 12 12:00:00 2020] An important synthetic event occurred.",
            "synthetic_zone", "pre-session-source", fixture_now);
        require(engine.snapshot(fixture_now).recent_alerts.empty(),
                "pre-session log line fired an alert");
        engine.consume(
            "[Sun Aug 03 12:00:00 2026] A ward settles over you.",
            "synthetic_zone", "source-1", fixture_now);
        engine.consume(
            "[Sun Aug 03 12:00:01 2026] A ward settles over you.",
            "synthetic_zone", "source-2", fixture_now);
        engine.consume(
            "[Sun Aug 03 12:00:02 2026] Synthetic captain has been slain by You!",
            "synthetic_zone", "source-3", fixture_now);
        engine.consume(
            "[Sun Aug 03 12:00:03 2026] An important synthetic event occurred.",
            "synthetic_zone", "source-4", fixture_now);
        const auto active = engine.snapshot(fixture_now);
        require(active.available && active.timers.size() == 2U &&
                    active.recent_alerts.size() == 3U &&
                    active.recent_alerts.back().sound,
                "timer, cooldown, or alert aggregation was not deterministic");
        engine.consume(
            "[Sun Aug 03 11:59:59 2026] An important synthetic event occurred.",
            "synthetic_zone", "source-5", fixture_now);
        require(engine.snapshot(fixture_now).recent_alerts.size() == 3U,
                "backward clock correction bypassed the alert cooldown");
        engine.consume(
            "[Sun Aug 03 11:59:59 2026] An important synthetic event occurred.",
            "synthetic_zone", "source-5", fixture_now);
        require(engine.snapshot(fixture_now).recent_alerts.size() == 3U,
                "replayed source fired the same alert twice");
        engine.consume(
            "[Sun Aug 03 12:00:04 2026] An important synthetic event occurred.",
            "synthetic_zone", "source-6", fixture_now);
        require(engine.snapshot(fixture_now).recent_alerts.size() == 3U,
                "backward clock correction rewound the cooldown baseline");
        engine.consume(
            "[Sun Aug 03 12:00:08 2026] An important synthetic event occurred.",
            "synthetic_zone", "source-7", fixture_now);
        require(engine.snapshot(fixture_now).recent_alerts.size() == 4U,
                "forward clock recovery did not release the cooldown");
        engine.consume(
            "[Sun Aug 03 12:00:00 2036] An important synthetic event occurred.",
            "synthetic_zone", "future-source", fixture_now);
        require(engine.snapshot(fixture_now).recent_alerts.size() == 4U,
                "future-dated alert line bypassed the supplied clock bound");
        plazmic::AlertEngine rollback_engine;
        rollback_engine.set_rules({
            .rules = {{
                .id = "rollback",
                .name = "Rollback",
                .match = "clock rollback event",
                .kind = plazmic::AlertTimerKind::custom,
                .duration_seconds = 0U,
                .cooldown_seconds = 0U,
                .sound = false,
            }},
            .source_name = "rollback.json",
        });
        rollback_engine.set_enabled(true, fixture_now);
        const auto rolled_back_now = fixture_now - std::chrono::hours(1);
        rollback_engine.consume(
            "[Sun Aug 03 11:00:04 2026] A clock rollback event occurred.",
            "synthetic_zone", "rollback-source", rolled_back_now);
        require(rollback_engine.snapshot(rolled_back_now)
                        .recent_alerts.size() == 1U,
                "system-clock rollback blocked a newly observed line");
        for (std::size_t index = 0U;
             index < plazmic::AlertEngine::maximum_observed_sources;
             ++index) {
            engine.consume(
                "[Sun Aug 03 12:00:04 2026] An unmatched synthetic line.",
                "synthetic_zone", "unmatched-" + std::to_string(index),
                fixture_now);
        }
        engine.consume(
            "[Sun Aug 03 12:00:20 2026] An important synthetic event occurred.",
            "synthetic_zone", "source-5", fixture_now);
        require(engine.snapshot(fixture_now).recent_alerts.size() == 4U,
                "unmatched lines evicted matching replay protection");
        engine.reset_transient();
        const auto reset = engine.snapshot(fixture_now);
        require(reset.timers.size() == 1U &&
                    reset.timers.front().kind ==
                        plazmic::AlertTimerKind::respawn,
                "lifecycle reset did not retain only respawn clocks");
        engine.clear_observations();
        require(engine.snapshot(fixture_now).recent_alerts.empty() &&
                    engine.snapshot(fixture_now).timers.empty() &&
                    engine.snapshot(fixture_now).available,
                "character reset retained alert observations or removed rules");
        engine.set_enabled(false);
        require(!engine.snapshot(fixture_now).available,
                "disabled alert engine remained available");
        engine.set_enabled(true, fixture_now);
        engine.consume(
            "[Sun Aug 03 12:00:05 2026] A ward settles over you.",
            "synthetic_zone", "expiry-source", fixture_now);
        require(engine.snapshot(fixture_now).timers.size() == 1U,
                "expiry fixture did not create a timer");
        const auto expired =
            engine.snapshot(fixture_now + std::chrono::minutes(4));
        require(expired.timers.empty(),
                "expired alert timer remained active");
        plazmic::AlertEngine out_of_order_timer_engine;
        out_of_order_timer_engine.set_rules({
            .rules = {{
                .id = "ordered-timer",
                .name = "Ordered Timer",
                .match = "ordered timer event",
                .kind = plazmic::AlertTimerKind::buff,
                .duration_seconds = 120U,
                .cooldown_seconds = 0U,
                .sound = false,
            }},
            .source_name = "ordered-timer.json",
        }, fixture_now - std::chrono::minutes(10));
        out_of_order_timer_engine.set_enabled(
            true, fixture_now - std::chrono::minutes(10));
        out_of_order_timer_engine.consume(
            "[Sun Aug 03 12:02:00 2026] An ordered timer event occurred.",
            "synthetic_zone", "ordered-newer", fixture_now);
        out_of_order_timer_engine.consume(
            "[Sun Aug 03 12:01:00 2026] An ordered timer event occurred.",
            "synthetic_zone", "ordered-older", fixture_now);
        const auto ordered_timer =
            out_of_order_timer_engine.snapshot(fixture_now);
        require(ordered_timer.recent_alerts.size() == 2U &&
                    ordered_timer.timers.size() == 1U &&
                    ordered_timer.timers.front().started_unix_seconds ==
                        std::chrono::duration_cast<std::chrono::seconds>(
                            (fixture_now + std::chrono::seconds(116))
                                .time_since_epoch())
                            .count(),
                "out-of-order match rewound the active timer");
        engine.clear_observations();
        engine.set_rules({
            .rules = {{
                .id = "unicode",
                .name = "Unicode",
                .match = "ÜBER",
                .kind = plazmic::AlertTimerKind::custom,
                .duration_seconds = 0U,
                .cooldown_seconds = 0U,
                .sound = false,
            }},
            .source_name = "unicode.json",
        }, fixture_now - std::chrono::seconds(1));
        engine.consume(
            "[Sun Aug 03 12:00:06 2026] über",
            "synthetic_zone", "unicode-source", fixture_now);
        require(engine.snapshot(fixture_now).recent_alerts.size() == 1U,
                "accepted Unicode literal was not case folded");
        engine.set_rules({
            .rules = {{
                .id = "replacement",
                .name = "Replacement",
                .match = "replacement event",
                .kind = plazmic::AlertTimerKind::custom,
                .duration_seconds = 0U,
                .cooldown_seconds = 0U,
                .sound = false,
            }},
            .source_name = "replacement.json",
        }, fixture_now);
        engine.consume(
            "[Sun Aug 03 11:59:59 2026] A replacement event occurred.",
            "synthetic_zone", "replacement-old", fixture_now);
        require(engine.snapshot(fixture_now).recent_alerts.empty(),
                "replacement rule pack replayed a pre-import line");
        engine.consume(
            "[Sun Aug 03 12:00:04 2026] A replacement event occurred.",
            "synthetic_zone", "replacement-new", fixture_now);
        require(engine.snapshot(fixture_now).recent_alerts.size() == 1U,
                "replacement rule pack rejected a post-import line");

        plazmic::AlertEngine trimmed_sound_engine;
        trimmed_sound_engine.set_rules({
            .rules = {
                {
                    .id = "sound",
                    .name = "Sound",
                    .match = "sound event",
                    .kind = plazmic::AlertTimerKind::custom,
                    .duration_seconds = 0U,
                    .cooldown_seconds = 0U,
                    .sound = true,
                },
                {
                    .id = "quiet",
                    .name = "Quiet",
                    .match = "quiet event",
                    .kind = plazmic::AlertTimerKind::custom,
                    .duration_seconds = 0U,
                    .cooldown_seconds = 0U,
                    .sound = false,
                },
            },
            .source_name = "trimmed-sound.json",
        });
        trimmed_sound_engine.set_enabled(
            true, fixture_now - std::chrono::seconds(10));
        trimmed_sound_engine.consume(
            "[Sun Aug 03 12:00:00 2026] A sound event occurred.",
            "synthetic_zone", "trimmed-sound", fixture_now);
        for (std::size_t index = 0U;
             index < plazmic::AlertEngine::maximum_recent_alerts;
             ++index) {
            trimmed_sound_engine.consume(
                "[Sun Aug 03 12:00:01 2026] A quiet event occurred.",
                "synthetic_zone", "quiet-" + std::to_string(index),
                fixture_now);
        }
        const auto trimmed_sound = trimmed_sound_engine.snapshot(fixture_now);
        require(trimmed_sound.recent_alerts.size() ==
                    plazmic::AlertEngine::maximum_recent_alerts &&
                    std::ranges::none_of(
                        trimmed_sound.recent_alerts,
                        &plazmic::FiredAlertSnapshot::sound) &&
                    trimmed_sound.latest_sound_sequence == 1U,
                "trimmed visible history discarded pending sound intent");

        const auto duplicate_path = directory / "duplicate.json";
        {
            std::ofstream output(duplicate_path, std::ios::binary);
            output << R"({"schema":1,"rules":[)"
                   << R"({"id":"same","name":"One","match":"one","kind":"custom","durationSeconds":0,"cooldownSeconds":0},)"
                   << R"({"id":"same","name":"Two","match":"two","kind":"custom","durationSeconds":0,"cooldownSeconds":0})"
                   << "]}";
        }
        require(!plazmic::load_alert_rule_pack(duplicate_path, detail),
                "duplicate alert-rule id was accepted");
        const auto fifo_path = directory / "rules.fifo";
        require(::mkfifo(fifo_path.c_str(), 0600) == 0,
                "could not create alert FIFO fixture");
        require(!plazmic::load_alert_rule_pack(fifo_path, detail),
                "alert pack accepted a FIFO");
        const auto control_path = directory / "control.json";
        {
            std::ofstream output(control_path, std::ios::binary);
            output << R"({"schema":1,"rules":[{"id":"control","name":"bad\u0085name","match":"synthetic","kind":"custom","durationSeconds":0,"cooldownSeconds":0}]})";
        }
        require(!plazmic::load_alert_rule_pack(control_path, detail),
                "alert pack accepted a Unicode control character");
        const auto target = directory / "target.json";
        {
            std::ofstream output(target, std::ios::binary);
            output << "{}";
        }
        const auto link = directory / "link.json";
        std::filesystem::create_symlink(target, link);
        require(!plazmic::load_alert_rule_pack(link, detail),
                "alert pack loader followed a symlink");

        std::filesystem::remove_all(directory);
        directory.clear();
        std::cout << "bounded local alert engine passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        if (!directory.empty()) {
            std::error_code ignored;
            std::filesystem::remove_all(directory, ignored);
        }
        std::cerr << "test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
