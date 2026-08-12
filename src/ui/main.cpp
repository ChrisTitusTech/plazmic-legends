#include "game/game_state_reader.h"
#include "game/combat_log_parser.h"
#include "launcher/client_selection.h"
#include "launcher/client_status.h"
#include "launcher/player_lifecycle.h"
#include "launcher/privacy_log.h"
#include "map/map_parser.h"
#include "ui/main_window.h"
#include "ui/system_theme.h"
#include "ui/x11_window_class.h"

#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QIcon>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QPixmap>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

namespace {

std::filesystem::path selected_maps(
    const QCommandLineParser& parser,
    const std::filesystem::path& client) {
    if (parser.isSet("maps")) {
        return parser.value("maps").toStdString();
    }
    return client.parent_path() / "maps";
}

struct CombatRefreshEnvelope {
    std::string character;
    std::string zone;
    std::uint64_t generation{};
    std::shared_ptr<plazmic::CombatLogTailer> tailer;
    plazmic::CombatLogRefresh refresh;
};

struct PlayerRefreshEnvelope {
    std::uint64_t generation{};
    std::string source;
    std::string expected_character;
    std::string expected_zone;
    plazmic::PlayerRefresh refresh;
};

}  // namespace

int main(int argc, char** argv) {
    QApplication::setApplicationName("plazmic-legends");
    QApplication::setApplicationDisplayName("Plazmic Legends");
    QApplication::setApplicationVersion(PLAZMIC_VERSION);
    QApplication::setDesktopFileName("plazmic-legends");
    QApplication application(argc, argv);
    const QProcessEnvironment environment =
        QProcessEnvironment::systemEnvironment();
    const QPixmap icon_source(":/icons/plazmic-legends.png");
    QIcon application_icon;
    application_icon.addPixmap(icon_source.scaled(
        64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    application_icon.addPixmap(icon_source.scaled(
        128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    application.setWindowIcon(application_icon);
    application.setQuitOnLastWindowClosed(true);
    plazmic::SystemTheme system_theme(
        application,
        plazmic::dwm_theme_path_for_session(environment));
    system_theme.start();
    const auto* class_filter =
        plazmic::install_x11_window_class_filter(application);
    if (!class_filter->available()) {
        return 20;
    }

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Read-only EverQuest Legends companion window");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({
        "client",
        "Path to the installed Legends eqgame.exe; valid selections are saved.",
        "path",
    });
    parser.addOption({
        "duration",
        "Exit automatically after the specified number of seconds.",
        "seconds",
        "0",
    });
    parser.addOption({
        "maps",
        "Override the installed map directory for validation.",
        "path",
    });
    parser.addOption({
        "reset-layout",
        "Ignore saved window geometry and dock state for this run.",
    });
    parser.addOption({
        "settings-file",
        "Override the per-user settings path for testing.",
        "path",
    });
    parser.process(application);

    bool duration_valid = false;
    const int duration_seconds =
        parser.value("duration").toInt(&duration_valid);
    if (!duration_valid || duration_seconds < 0) {
        parser.showHelp(2);
    }

    const QString settings_path = parser.isSet("settings-file")
                                      ? parser.value("settings-file")
                                      : plazmic::UiSettings::default_path();
    const plazmic::UiSettings settings(settings_path);
    const std::optional<plazmic::UiState> saved_state =
        settings.load();
    const QString environment_directory =
        environment.value("EQ_LEGENDS_DIR");
    const plazmic::ClientSelection selection =
        plazmic::select_client({
            .command_line_client =
                parser.isSet("client")
                    ? std::optional<std::filesystem::path>(
                          parser.value("client").toStdString())
                    : std::nullopt,
            .environment_directory =
                environment_directory.isEmpty()
                    ? std::nullopt
                    : std::optional<std::filesystem::path>(
                          environment_directory.toStdString()),
            .saved_directory =
                saved_state &&
                        !saved_state->client_directory.isEmpty()
                    ? std::optional<std::filesystem::path>(
                          saved_state->client_directory.toStdString())
                    : std::nullopt,
            .home_directory =
                environment.value("HOME").toStdString(),
        });
    const std::filesystem::path client = selection.client;
    const QString selected_game_directory =
        QString::fromStdString(selection.game_directory.string());
    bool client_directory_saved = true;
    if (selection.should_persist &&
        (!saved_state ||
         saved_state->client_directory != selected_game_directory)) {
        if (!saved_state && QFileInfo::exists(settings_path)) {
            client_directory_saved = false;
        } else {
            plazmic::UiState updated =
                saved_state.value_or(plazmic::UiState{});
            updated.client_directory = selected_game_directory;
            client_directory_saved = settings.save(updated);
        }
    }
    const std::filesystem::path map_root =
        selected_maps(parser, client);
    auto probe =
        std::make_unique<plazmic::ClientStatusProbe>(client);
    auto game_probe = std::make_shared<plazmic::LiveGameStateProbe>(
        client, probe->profile());
    plazmic::PrivacyLog privacy_log(
        plazmic::PrivacyLog::default_path(
            environment.value("XDG_STATE_HOME").toStdString(),
            environment.value("HOME").toStdString()));
    plazmic::StatusSnapshot initial_status = probe->refresh();
    if (!selection.detail.empty()) {
        initial_status.detail = selection.detail;
    }
    if (!client_directory_saved) {
        initial_status.detail +=
            "; client directory could not be saved";
    }
    privacy_log.record_startup(
        QApplication::applicationVersion().toStdString(),
        initial_status.profile);
    privacy_log.record_status(initial_status);
    if (!privacy_log.healthy()) {
        initial_status.detail +=
            "; local privacy log is unavailable";
    }
    std::deque<std::string> activity_delete_queue;
    std::unordered_map<std::string, std::string> activity_delete_errors;
    std::optional<plazmic::AlertRulePack> pending_alert_rules;
    std::deque<bool> pending_alert_enabled;
    plazmic::MainWindow window(
        initial_status,
        settings_path,
        parser.isSet("reset-layout"),
        selected_game_directory);
    window.setWindowIcon(application_icon);
    window.set_delete_activity_callback(
        [&activity_delete_queue](std::string key) {
            activity_delete_queue.push_back(std::move(key));
        });
    window.set_alert_rules_callback(
        [&pending_alert_rules](plazmic::AlertRulePack pack) {
            pending_alert_rules = std::move(pack);
        });
    window.set_alert_enabled_callback(
        [&pending_alert_enabled](bool enabled) {
            pending_alert_enabled.push_back(enabled);
        });
    window.show();

    QTimer status_timer;
    QObject::connect(&status_timer, &QTimer::timeout, &window,
                     [&window, &probe, &privacy_log,
                      selection_detail = selection.detail]() {
                         plazmic::StatusSnapshot status =
                             probe->refresh();
                         if (!selection_detail.empty()) {
                             status.detail = selection_detail;
                         }
                         privacy_log.record_status(status);
                         window.update_snapshot(status);
                     });
    status_timer.start(std::chrono::seconds(1));

    plazmic::PlayerLifecycle player_lifecycle;
    std::string active_character;
    std::string active_zone;
    plazmic::CharacterSnapshot active_character_snapshot;
    std::uint64_t combat_generation = 0U;
    bool preserve_respawn_alerts_on_reset = false;
    const std::string player_source = client.string();
    QFutureWatcher<PlayerRefreshEnvelope> player_watcher;
    QFutureWatcher<PlayerRefreshEnvelope> map_watcher;
    const auto publish_player_refresh =
        [&window, &player_lifecycle, &privacy_log, &active_character,
         &active_zone, &active_character_snapshot,
         &combat_generation, &preserve_respawn_alerts_on_reset,
         &player_source](PlayerRefreshEnvelope envelope) {
            if (envelope.generation != combat_generation ||
                envelope.source != player_source ||
                (!envelope.expected_character.empty() &&
                 envelope.expected_character != active_character) ||
                (!envelope.expected_zone.empty() &&
                 envelope.expected_zone != active_zone)) {
                return false;
            }
            plazmic::PlayerRefresh refresh = std::move(envelope.refresh);
            privacy_log.record_game_state(refresh.state.error);
            if (refresh.map_load) {
                privacy_log.record_map_state(refresh.map_load->error);
            }
            plazmic::PlayerLifecycleUpdate update =
                player_lifecycle.apply(std::move(refresh));
            privacy_log.record_player_state(update.player.state);
            if (update.map) {
                window.set_zone_map(std::move(*update.map));
            } else if (update.clear_map) {
                window.clear_zone_map(
                    QString::fromStdString(update.player.detail));
            }
            window.update_player_snapshot(update.player);
            window.update_spawn_snapshot(std::move(update.spawns));
            active_character = update.character.available()
                                   ? update.character.name
                                   : std::string{};
            active_zone = update.player.available()
                              ? update.player.zone
                              : std::string{};
            active_character_snapshot = update.character;
            if (update.reset_combat) {
                preserve_respawn_alerts_on_reset =
                    update.preserve_respawn_alerts;
                ++combat_generation;
                window.update_combat_snapshot(
                    plazmic::CombatAnalyticsSnapshot{});
            }
            if (update.reset_activity) {
                window.update_activity_snapshot(
                    plazmic::ActivityAnalyticsSnapshot{});
                window.reset_alert_snapshot(update.preserve_respawn_alerts);
            }
            window.update_character_snapshot(update.character);
            return true;
        };
    QObject::connect(
        &map_watcher,
        &QFutureWatcher<PlayerRefreshEnvelope>::finished,
        &window,
        [&map_watcher, &publish_player_refresh]() {
            (void)publish_player_refresh(map_watcher.result());
        });
    QObject::connect(
        &player_watcher,
        &QFutureWatcher<PlayerRefreshEnvelope>::finished,
        &window,
        [&player_watcher, &map_watcher, &player_lifecycle, &game_probe,
         &map_root, &publish_player_refresh, &active_character,
         &active_zone, &combat_generation, &player_source]() {
            PlayerRefreshEnvelope envelope = player_watcher.result();
            std::optional<std::string> zone_to_load;
            if (envelope.refresh.state &&
                envelope.refresh.state.snapshot->zone !=
                    player_lifecycle.handled_zone()) {
                zone_to_load = envelope.refresh.state.snapshot->zone;
            }
            if (!publish_player_refresh(std::move(envelope)) ||
                !zone_to_load || map_watcher.isRunning()) {
                return;
            }
            const std::uint64_t generation = combat_generation;
            const std::string character = active_character;
            const std::string zone = active_zone;
            map_watcher.setFuture(QtConcurrent::run(
                [game_probe, map_root, generation, character, zone,
                 player_source, loaded_zone = *zone_to_load]() {
                    plazmic::MapLoadResult map_load =
                        plazmic::load_zone_map(map_root, loaded_zone);
                    plazmic::GameStateReadResult confirmation =
                        game_probe->refresh();
                    if (!confirmation) {
                        return PlayerRefreshEnvelope{
                            .generation = generation,
                            .source = player_source,
                            .expected_character = character,
                            .expected_zone = zone,
                            .refresh = {
                                .state = std::move(confirmation),
                                .map_load = std::nullopt,
                            },
                        };
                    }
                    if (confirmation.snapshot->zone != loaded_zone) {
                        return PlayerRefreshEnvelope{
                            .generation = generation,
                            .source = player_source,
                            .expected_character = character,
                            .expected_zone = zone,
                            .refresh = {
                                .state = {
                                    .snapshot = std::nullopt,
                                    .spawns = std::nullopt,
                                    .error =
                                        plazmic::GameStateReadError::zoning,
                                    .detail =
                                        "zone changed while loading its map",
                                    .character = std::nullopt,
                                },
                                .map_load = std::nullopt,
                            },
                        };
                    }
                    return PlayerRefreshEnvelope{
                        .generation = generation,
                        .source = player_source,
                        .expected_character = character,
                        .expected_zone = zone,
                        .refresh = {
                            .state = std::move(confirmation),
                            .map_load = std::move(map_load),
                        },
                    };
                }));
        });

    QTimer player_timer;
    const auto start_player_refresh =
        [&player_watcher, &map_watcher, &game_probe, &combat_generation,
         &player_source]() {
        if (player_watcher.isRunning() || map_watcher.isRunning()) {
            return;
        }
        player_watcher.setFuture(QtConcurrent::run(
            [game_probe, generation = combat_generation, player_source]() {
                return PlayerRefreshEnvelope{
                    .generation = generation,
                    .source = player_source,
                    .expected_character = {},
                    .expected_zone = {},
                    .refresh = {
                        .state = game_probe->refresh(),
                        .map_load = std::nullopt,
                    },
                };
            }));
    };
    QObject::connect(
        &player_timer, &QTimer::timeout, &window, start_player_refresh);
    player_timer.start(std::chrono::milliseconds(250));
    start_player_refresh();

    auto combat_tailer = std::make_shared<plazmic::CombatLogTailer>();
    std::uint64_t tailer_generation = combat_generation;
    bool combat_result_consumed = true;
    QFutureWatcher<CombatRefreshEnvelope> combat_watcher;
    const auto commit_combat_refresh =
        [&window, &combat_tailer, &active_character, &combat_generation,
         &activity_delete_errors,
         &pending_alert_enabled](CombatRefreshEnvelope result,
                                 bool publish_ui) {
            if (result.character != active_character ||
                result.generation != combat_generation) {
                return;
            }
            combat_tailer = std::move(result.tailer);
            const bool retain_history = window.combat_history_enabled();
            const bool retain_activity = window.activity_history_enabled();
            combat_tailer->commit_deferred_persistence(
                retain_history, retain_activity);
            combat_tailer->maintain_retained_state();
            if (result.refresh.error == plazmic::CombatLogError::none) {
                result.refresh = combat_tailer->current_snapshot(
                    result.character,
                    result.zone.empty() ? "Unknown" : result.zone);
            } else {
                result.refresh.snapshot.history_retention_enabled =
                    retain_history;
                result.refresh.activity.retention_enabled =
                    retain_activity;
            }
            const auto activity_delete_error = activity_delete_errors.find(
                result.refresh.activity.storage_key);
            if (activity_delete_error != activity_delete_errors.end()) {
                result.refresh.activity.persisted = false;
                result.refresh.activity.detail = activity_delete_error->second;
            }
            if (publish_ui) {
                window.update_combat_snapshot(result.refresh.snapshot);
                window.update_activity_snapshot(result.refresh.activity);
                if (pending_alert_enabled.empty()) {
                    window.update_alert_snapshot(result.refresh.alerts);
                }
            }
        };
    QObject::connect(
        &combat_watcher,
        &QFutureWatcher<CombatRefreshEnvelope>::finished,
        &window,
        [&combat_watcher, &combat_result_consumed,
         &commit_combat_refresh]() {
            combat_result_consumed = true;
            commit_combat_refresh(combat_watcher.result(), true);
        });
    QTimer combat_timer;
    const auto start_combat_refresh =
        [&combat_watcher, &combat_tailer, &active_character, &active_zone,
         &active_character_snapshot, &combat_generation,
         &tailer_generation, &combat_result_consumed,
         &preserve_respawn_alerts_on_reset,
         &activity_delete_queue, &activity_delete_errors, &window,
         &pending_alert_rules, &pending_alert_enabled,
         game_directory = selection.game_directory]() {
            if (combat_watcher.isRunning()) {
                return;
            }
            const std::string character = active_character;
            const std::string zone = active_zone;
            const plazmic::CharacterSnapshot character_snapshot =
                active_character_snapshot;
            const std::uint64_t generation = combat_generation;
            const bool reset_tailer = tailer_generation != generation;
            const bool preserve_respawn_alerts =
                reset_tailer && preserve_respawn_alerts_on_reset;
            while (!activity_delete_queue.empty()) {
                std::string key = std::move(activity_delete_queue.front());
                activity_delete_queue.pop_front();
                const bool succeeded =
                    combat_tailer->delete_activity_history(key);
                if (succeeded) {
                    activity_delete_errors.erase(key);
                } else {
                    activity_delete_errors[key] =
                        "Activity history deletion failed; the retained file "
                        "remains on disk and can be retried";
                }
                window.report_activity_deletion_result(key, succeeded);
            }
            if (pending_alert_rules) {
                combat_tailer->set_alert_rules(
                    std::move(*pending_alert_rules));
                pending_alert_rules.reset();
            }
            while (!pending_alert_enabled.empty()) {
                combat_tailer->set_alert_enabled(
                    pending_alert_enabled.front());
                pending_alert_enabled.pop_front();
            }
            const auto source_tailer = combat_tailer;
            tailer_generation = generation;
            combat_result_consumed = false;
            combat_watcher.setFuture(QtConcurrent::run(
                [source_tailer, game_directory, character, zone, generation,
                 character_snapshot, reset_tailer,
                 preserve_respawn_alerts]() {
                    auto staged_tailer =
                        std::make_shared<plazmic::CombatLogTailer>(
                            *source_tailer);
                    staged_tailer->begin_deferred_persistence();
                    if (reset_tailer) {
                        staged_tailer->reset_context(
                            character, preserve_respawn_alerts);
                    }
                    plazmic::CombatLogRefresh refresh =
                        staged_tailer->refresh(
                            game_directory, character,
                            zone.empty() ? "Unknown" : zone);
                    if (refresh.error == plazmic::CombatLogError::none) {
                        staged_tailer->observe_character(
                            character_snapshot,
                            zone.empty() ? "Unknown" : zone);
                        refresh.activity =
                            staged_tailer->activity_snapshot();
                    }
                    return CombatRefreshEnvelope{
                        .character = character,
                        .zone = zone,
                        .generation = generation,
                        .tailer = std::move(staged_tailer),
                        .refresh = std::move(refresh),
                    };
                }));
        };
    QObject::connect(
        &combat_timer, &QTimer::timeout, &window, start_combat_refresh);
    combat_timer.start(std::chrono::milliseconds(250));

    if (duration_seconds > 0) {
        QTimer::singleShot(std::chrono::seconds(duration_seconds),
                           &window, &QWidget::close);
    }
    int result = application.exec();
    player_timer.stop();
    combat_timer.stop();
    if (player_watcher.isRunning()) {
        player_watcher.waitForFinished();
    }
    if (map_watcher.isRunning()) {
        map_watcher.waitForFinished();
    }
    if (combat_watcher.isRunning()) {
        combat_watcher.waitForFinished();
    }
    if (!combat_result_consumed) {
        combat_result_consumed = true;
        commit_combat_refresh(combat_watcher.result(), false);
    }
    for (const auto& [key, detail] : activity_delete_errors) {
        (void)detail;
        activity_delete_queue.push_back(key);
    }
    activity_delete_errors.clear();
    combat_tailer->set_history_enabled(window.combat_history_enabled());
    combat_tailer->set_activity_history_enabled(
        window.activity_history_enabled());
    bool activity_deletion_failed = false;
    for (const auto& key : activity_delete_queue) {
        if (!combat_tailer->delete_activity_history(key)) {
            activity_deletion_failed = true;
            window.report_activity_deletion_result(key, false);
        }
    }
    if (activity_deletion_failed) {
        QMessageBox::warning(
            nullptr, "Activity History Deletion Failed",
            "One or more confirmed activity-history deletions could not be "
            "completed. The retained files remain on disk; reopen Plazmic "
            "Legends to retry.");
        result = EXIT_FAILURE;
    }
    combat_tailer->clear();
    privacy_log.record_shutdown();
    return result;
}
