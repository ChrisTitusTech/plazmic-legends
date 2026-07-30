#include "game/game_state_reader.h"
#include "launcher/client_status.h"
#include "map/map_parser.h"
#include "ui/main_window.h"
#include "ui/system_theme.h"
#include "ui/x11_window_class.h"

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFutureWatcher>
#include <QProcessEnvironment>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

namespace {

std::filesystem::path selected_client(const QCommandLineParser& parser) {
    if (parser.isSet("client")) {
        return parser.value("client").toStdString();
    }
    const QString directory =
        QProcessEnvironment::systemEnvironment().value("EQ_LEGENDS_DIR");
    if (directory.isEmpty()) {
        return {};
    }
    return QDir(directory).filePath("eqgame.exe").toStdString();
}

struct PlayerRefresh {
    plazmic::GameStateReadResult state;
    std::optional<plazmic::MapLoadResult> map_load;
};

}  // namespace

int main(int argc, char** argv) {
    QApplication::setApplicationName("plazmic-legends");
    QApplication::setApplicationDisplayName("Plazmic Legends");
    QApplication::setDesktopFileName("plazmic-legends");
    QApplication application(argc, argv);
    application.setQuitOnLastWindowClosed(true);
    plazmic::SystemTheme system_theme(
        application,
        plazmic::dwm_theme_path_for_session(
            QProcessEnvironment::systemEnvironment()));
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
        "Path to the installed Legends eqgame.exe.",
        "path",
    });
    parser.addOption({
        "duration",
        "Exit automatically after the specified number of seconds.",
        "seconds",
        "0",
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

    const std::filesystem::path client = selected_client(parser);
    auto probe =
        std::make_unique<plazmic::ClientStatusProbe>(client);
    auto game_probe = std::make_shared<plazmic::LiveGameStateProbe>(
        client, probe->profile());
    const QString settings_path = parser.isSet("settings-file")
                                      ? parser.value("settings-file")
                                      : plazmic::UiSettings::default_path();
    plazmic::MainWindow window(
        probe->refresh(), settings_path, parser.isSet("reset-layout"));
    window.show();

    QTimer status_timer;
    QObject::connect(&status_timer, &QTimer::timeout, &window,
                     [&window, &probe]() {
                         window.update_snapshot(probe->refresh());
                     });
    status_timer.start(std::chrono::seconds(1));

    std::string handled_zone;
    QFutureWatcher<PlayerRefresh> player_watcher;
    QObject::connect(
        &player_watcher, &QFutureWatcher<PlayerRefresh>::finished,
        &window,
        [&window, &player_watcher, &handled_zone]() {
            PlayerRefresh refresh = player_watcher.result();
            if (!refresh.state) {
                window.update_player_snapshot({
                    .state = plazmic::PlayerSnapshotState::unavailable,
                    .zone = {},
                    .x = 0.0,
                    .y = 0.0,
                    .z = 0.0,
                    .heading_degrees = 0.0,
                    .detail = refresh.state.detail,
                });
                handled_zone.clear();
                window.clear_zone_map(
                    QString::fromStdString(refresh.state.detail));
                return;
            }

            if (refresh.map_load) {
                handled_zone = refresh.state.snapshot->zone;
                if (!*refresh.map_load) {
                    window.clear_zone_map(QString::fromStdString(
                        refresh.map_load->detail));
                } else {
                    window.set_zone_map(
                        std::move(*refresh.map_load->map));
                }
            }
            window.update_player_snapshot(*refresh.state.snapshot);
        });

    QTimer player_timer;
    const auto start_player_refresh =
        [&player_watcher, &game_probe, &handled_zone, &client]() {
        if (player_watcher.isRunning()) {
            return;
        }
        const std::string current_zone = handled_zone;
        player_watcher.setFuture(QtConcurrent::run(
            [game_probe, client, current_zone]() {
                PlayerRefresh refresh{
                    .state = game_probe->refresh(),
                    .map_load = std::nullopt,
                };
                if (refresh.state &&
                    refresh.state.snapshot->zone != current_zone) {
                    const std::string loaded_zone =
                        refresh.state.snapshot->zone;
                    refresh.map_load = plazmic::load_zone_map(
                        client.parent_path() / "maps",
                        loaded_zone);
                    if (*refresh.map_load) {
                        plazmic::GameStateReadResult confirmation =
                            game_probe->refresh();
                        if (!confirmation ||
                            confirmation.snapshot->zone != loaded_zone) {
                            refresh.state = {
                                .snapshot = std::nullopt,
                                .error =
                                    plazmic::GameStateReadError::
                                        inconsistent_snapshot,
                                .detail =
                                    "zone changed while loading its map",
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

    if (duration_seconds > 0) {
        QTimer::singleShot(std::chrono::seconds(duration_seconds),
                           &window, &QWidget::close);
    }
    const int result = application.exec();
    player_timer.stop();
    if (player_watcher.isRunning()) {
        player_watcher.waitForFinished();
    }
    return result;
}
