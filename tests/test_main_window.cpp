#include "map/map_parser.h"
#include "model/player_snapshot.h"
#include "model/status_snapshot.h"
#include "ui/map_canvas.h"
#include "ui/main_window.h"
#include "ui/ui_settings.h"
#include "ui/x11_window_class.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <QApplication>
#include <QDockWidget>
#include <QLabel>
#include <QTemporaryDir>
#include <QTimer>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <unistd.h>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void process_events() {
    for (int index = 0; index < 10; ++index) {
        QApplication::processEvents();
    }
}

void require_window_class(Display* display,
                          QWidget* widget,
                          const std::string& label) {
    XClassHint hint{};
    XSync(display, False);
    require(XGetClassHint(
                display, static_cast<Window>(widget->winId()), &hint) != 0,
            label + " has no WM_CLASS");
    const std::string instance =
        hint.res_name == nullptr ? "" : hint.res_name;
    const std::string window_class =
        hint.res_class == nullptr ? "" : hint.res_class;
    if (hint.res_name != nullptr) {
        XFree(hint.res_name);
    }
    if (hint.res_class != nullptr) {
        XFree(hint.res_class);
    }
    require(instance == plazmic::kX11Instance,
            label + " X11 instance mismatch");
    require(window_class == plazmic::kX11Class,
            label + " X11 class mismatch");
}

}  // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    const auto* class_filter =
        plazmic::install_x11_window_class_filter(application);
    if (!class_filter->available()) {
        std::cout << "main window test skipped: cannot open X11 display\n";
        return 77;
    }

    try {
        QTemporaryDir directory;
        require(directory.isValid(), "cannot create UI test directory");
        const QString settings_path = directory.filePath("config.toml");
        const plazmic::StatusSnapshot snapshot{
            .compatibility = plazmic::CompatibilityState::supported,
            .process = plazmic::ProcessState::running,
            .profile = "synthetic-profile",
            .detail = "Synthetic Phase 2 status",
            .pid = getpid(),
        };

        plazmic::MainWindow window(snapshot, settings_path, true);
        window.show();
        process_events();
        require(window.isVisible(), "main window is not visible");
        require(window.objectName() == "plazmic-main-window",
                "main window object name mismatch");
        require(window.findChild<QWidget*>("map-view") != nullptr,
                "map canvas is missing");
        require(window.findChild<QWidget*>("spawn-table") != nullptr,
                "spawn table placeholder is missing");
        require(window.findChild<QLabel*>("compatibility-status")->text() ==
                    "Supported",
                "compatibility status did not render");
        require(window.findChild<QLabel*>("profile-status")->text() ==
                    "synthetic-profile",
                "profile status did not render");

        const plazmic::ZoneMap map{
            .zone = "synthetic",
            .layers =
                {
                    {
                        .index = 0,
                        .source = "synthetic.txt",
                        .lines =
                            {
                                {
                                    .start = {-10.0, -20.0, 0.0},
                                    .end = {30.0, 40.0, 0.0},
                                    .color = {255, 255, 255},
                                },
                            },
                        .labels = {},
                    },
                },
        };
        const plazmic::PlayerSnapshot player{
            .state = plazmic::PlayerSnapshotState::in_world,
            .zone = "synthetic",
            .x = 12.0,
            .y = -8.0,
            .z = 3.0,
            .heading_degrees = 180.0,
            .detail = "Synthetic player snapshot",
        };
        window.set_zone_map(map);
        window.update_player_snapshot(player);
        process_events();
        require(window.map_canvas()->zone_map().has_value(),
                "map canvas did not retain immutable geometry");
        require(window.map_canvas()->zone_map()->zone == "synthetic",
                "map canvas loaded the wrong zone");
        require(window.map_canvas()->player_snapshot().zone == "synthetic",
                "map canvas did not retain the player snapshot");
        require(window.map_canvas()->height_filter_enabled(),
                "height filter did not default to enabled");
        window.map_canvas()->set_height_filter_enabled(false);
        require(!window.map_canvas()->height_filter_enabled(),
                "height filter could not be disabled");
        window.map_canvas()->set_height_filter_enabled(true);
        require(window.map_canvas()->height_filter_enabled(),
                "height filter could not be re-enabled");
        window.map_canvas()->set_height_filter_range(10.0, 35.0);
        require(window.map_canvas()->height_filter_below() == 10.0 &&
                    window.map_canvas()->height_filter_above() == 35.0,
                "height filter did not retain asymmetric player-Z ranges");
        window.map_canvas()->set_height_filter_range(-5.0, 2000.0);
        require(window.map_canvas()->height_filter_below() == 0.0 &&
                    window.map_canvas()->height_filter_above() ==
                        plazmic::kMaximumHeightFilterRange,
                "height filter ranges were not bounded");
        require(!window.map_canvas()->player_follow_enabled(),
                "player follow did not default to disabled");
        window.map_canvas()->set_player_follow_enabled(true);
        require(window.map_canvas()->player_follow_enabled(),
                "player follow could not be enabled");

        plazmic::ZoneMap oversized_map{
            .zone = "oversized",
            .layers =
                {
                    {
                        .index = 0,
                        .source = "oversized.txt",
                        .lines = {},
                        .labels = {},
                    },
                },
        };
        oversized_map.layers.front().lines.resize(
            plazmic::kMaximumRenderableMapRecords + 1U,
            {
                .start = {0.0, 0.0, 0.0},
                .end = {1.0, 1.0, 0.0},
                .color = {255, 255, 255},
            });
        window.set_zone_map(std::move(oversized_map));
        require(!window.map_canvas()->zone_map().has_value(),
                "oversized map reached the renderer");
        window.set_zone_map(map);

        Display* display = XOpenDisplay(nullptr);
        require(display != nullptr, "cannot open verification display");
        require_window_class(display, &window, "main window");

        window.setGeometry(80, 80, 800, 600);
        process_events();
        auto* spawn_dock = window.findChild<QDockWidget*>("spawn-dock");
        require(spawn_dock != nullptr, "spawn dock is missing");
        spawn_dock->setFloating(true);
        spawn_dock->show();
        process_events();
        require(spawn_dock->isWindow(), "detached spawn dock is not top-level");
        require_window_class(display, spawn_dock, "detached spawn dock");
        XCloseDisplay(display);

        const QByteArray expected_geometry_state = window.saveGeometry();
        window.close();
        process_events();
        const auto saved_state = plazmic::UiSettings(settings_path).load();
        require(saved_state.has_value(),
                "close did not persist UI state");
        require(saved_state->geometry == expected_geometry_state,
                "persisted window geometry did not match the closed window");
        require(saved_state->height_filter_enabled &&
                    saved_state->height_filter_below == 0.0 &&
                    saved_state->height_filter_above ==
                        plazmic::kMaximumHeightFilterRange,
                "close did not persist the map height filter state");
        require(saved_state->player_follow_enabled,
                "close did not persist player-follow state");

        QMainWindow geometry_reference;
        require(geometry_reference.restoreGeometry(expected_geometry_state),
                "Qt rejected the saved geometry fixture");
        const QRect expected_geometry = geometry_reference.geometry();
        plazmic::MainWindow restored(snapshot, settings_path, false);
        const QRect restored_geometry = restored.geometry();
        require(
            restored_geometry == expected_geometry,
            "saved window geometry was not restored: expected " +
                std::to_string(expected_geometry.x()) + "," +
                std::to_string(expected_geometry.y()) + " " +
                std::to_string(expected_geometry.width()) + "x" +
                std::to_string(expected_geometry.height()) + ", got " +
                std::to_string(restored_geometry.x()) + "," +
                std::to_string(restored_geometry.y()) + " " +
                std::to_string(restored_geometry.width()) + "x" +
                std::to_string(restored_geometry.height()));
        restored.show();
        process_events();
        require(restored.isVisible(), "restored window is not visible");
        require(restored.map_canvas()->height_filter_enabled() &&
                    restored.map_canvas()->height_filter_below() == 0.0 &&
                    restored.map_canvas()->height_filter_above() ==
                        plazmic::kMaximumHeightFilterRange,
                "saved map height filter state was not restored");
        require(restored.map_canvas()->player_follow_enabled(),
                "saved player-follow state was not restored");
        auto* restored_spawn_dock =
            restored.findChild<QDockWidget*>("spawn-dock");
        require(restored_spawn_dock != nullptr,
                "restored spawn dock is missing");
        require(restored_spawn_dock->isFloating(),
                "saved floating dock state was not restored");
        restored.close();
        process_events();

        const QString shutdown_path = directory.filePath("shutdown.toml");
        plazmic::MainWindow lifecycle(snapshot, shutdown_path, true);
        lifecycle.show();
        process_events();
        auto* lifecycle_dock =
            lifecycle.findChild<QDockWidget*>("spawn-dock");
        require(lifecycle_dock != nullptr,
                "shutdown lifecycle spawn dock is missing");
        lifecycle_dock->setFloating(true);
        lifecycle_dock->show();
        process_events();

        QTimer watchdog;
        watchdog.setSingleShot(true);
        QObject::connect(&watchdog, &QTimer::timeout, &application,
                         [&application]() { application.exit(99); });
        watchdog.start(2000);
        QTimer::singleShot(0, &lifecycle, &QWidget::close);
        require(application.exec() == EXIT_SUCCESS,
                "detached dock prevented main-window shutdown");
        watchdog.stop();
        require(plazmic::UiSettings(shutdown_path).load().has_value(),
                "shutdown did not persist UI state");

        std::cout
            << "Qt shell, docks, persistence, and X11 class inheritance passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
