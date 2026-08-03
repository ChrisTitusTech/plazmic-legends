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
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QIcon>
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
    std::uint64_t generation{};
    plazmic::CombatLogRefresh refresh;
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
    plazmic::MainWindow window(
        initial_status,
        settings_path,
        parser.isSet("reset-layout"),
        selection.should_persist ? selected_game_directory
                                 : QString{});
    window.setWindowIcon(application_icon);
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
    std::uint64_t combat_generation = 0U;
    QFutureWatcher<plazmic::PlayerRefresh> player_watcher;
    QObject::connect(
        &player_watcher,
        &QFutureWatcher<plazmic::PlayerRefresh>::finished,
        &window,
        [&window, &player_watcher, &player_lifecycle, &privacy_log,
         &active_character, &combat_generation]() {
            plazmic::PlayerRefresh refresh =
                player_watcher.result();
            privacy_log.record_game_state(refresh.state.error);
            if (refresh.map_load) {
                privacy_log.record_map_state(
                    refresh.map_load->error);
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
            const std::string next_character =
                update.character.available()
                    ? update.character.name
                    : std::string{};
            active_character = next_character;
            if (update.reset_combat) {
                ++combat_generation;
                window.update_combat_snapshot({});
            }
            window.update_character_snapshot(update.character);
        });

    QTimer player_timer;
    const auto start_player_refresh =
        [&player_watcher, &game_probe, &player_lifecycle, &map_root]() {
        if (player_watcher.isRunning()) {
            return;
        }
        const std::string current_zone =
            player_lifecycle.handled_zone();
        player_watcher.setFuture(QtConcurrent::run(
            [game_probe, map_root, current_zone]() {
                plazmic::PlayerRefresh refresh{
                    .state = game_probe->refresh(),
                    .map_load = std::nullopt,
                };
                if (refresh.state &&
                    refresh.state.snapshot->zone != current_zone) {
                    const std::string loaded_zone =
                        refresh.state.snapshot->zone;
                    refresh.map_load = plazmic::load_zone_map(
                        map_root, loaded_zone);
                    if (*refresh.map_load) {
                        plazmic::GameStateReadResult confirmation =
                            game_probe->refresh();
                        if (!confirmation) {
                            refresh.state = std::move(confirmation);
                            refresh.map_load.reset();
                            return refresh;
                        }
                        if (confirmation.snapshot->zone != loaded_zone) {
                            refresh.state = {
                                .snapshot = std::nullopt,
                                .spawns = std::nullopt,
                                .error =
                                    plazmic::GameStateReadError::
                                        zoning,
                                .detail =
                                    "zone changed while loading its map",
                                .character = std::nullopt,
                            };
                            refresh.map_load.reset();
                            return refresh;
                        }
                        refresh.state = std::move(confirmation);
                    }
                }
                return refresh;
            }));
    };
    QObject::connect(
        &player_timer, &QTimer::timeout, &window, start_player_refresh);
    player_timer.start(std::chrono::milliseconds(250));
    start_player_refresh();

    auto combat_tailer = std::make_shared<plazmic::CombatLogTailer>();
    std::uint64_t tailer_generation = combat_generation;
    QFutureWatcher<CombatRefreshEnvelope> combat_watcher;
    QObject::connect(
        &combat_watcher,
        &QFutureWatcher<CombatRefreshEnvelope>::finished,
        &window,
        [&window, &combat_watcher, &active_character,
         &combat_generation]() {
            const CombatRefreshEnvelope result = combat_watcher.result();
            if (result.character == active_character &&
                result.generation == combat_generation) {
                window.update_combat_snapshot(result.refresh.snapshot);
            }
        });
    QTimer combat_timer;
    const auto start_combat_refresh =
        [&combat_watcher, &combat_tailer, &active_character,
         &combat_generation, &tailer_generation,
         game_directory = selection.game_directory]() {
            if (combat_watcher.isRunning()) {
                return;
            }
            const std::string character = active_character;
            const std::uint64_t generation = combat_generation;
            const bool reset_tailer = tailer_generation != generation;
            tailer_generation = generation;
            combat_watcher.setFuture(QtConcurrent::run(
                [combat_tailer, game_directory, character, generation,
                 reset_tailer]() {
                    if (reset_tailer) {
                        combat_tailer->clear();
                    }
                    return CombatRefreshEnvelope{
                        .character = character,
                        .generation = generation,
                        .refresh = combat_tailer->refresh(
                            game_directory, character),
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
    const int result = application.exec();
    player_timer.stop();
    combat_timer.stop();
    if (player_watcher.isRunning()) {
        player_watcher.waitForFinished();
    }
    if (combat_watcher.isRunning()) {
        combat_watcher.waitForFinished();
    }
    privacy_log.record_shutdown();
    return result;
}
