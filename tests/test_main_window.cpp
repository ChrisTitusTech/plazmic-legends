#include "map/map_parser.h"
#include "map/map_view_model.h"
#include "model/player_snapshot.h"
#include "model/status_snapshot.h"
#include "ui/map_canvas.h"
#include "ui/main_window.h"
#include "ui/spawn_table_model.h"
#include "ui/ui_settings.h"
#include "ui/x11_window_class.h"

#include <array>
#include <cstdlib>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>

#include <QApplication>
#include <QAction>
#include <QDockWidget>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QHeaderView>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QProgressBar>
#include <QTableWidget>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>

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

QAction* find_menu_action(QMenu* root,
                          const QString& submenu,
                          const QString& text) {
    for (QAction* action : root->actions()) {
        QMenu* menu = action->menu();
        if (menu == nullptr || menu->title() != submenu) {
            continue;
        }
        for (QAction* child : menu->actions()) {
            if (child->text() == text) {
                return child;
            }
        }
    }
    return nullptr;
}

}  // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QApplication::setApplicationVersion("test-version");
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
        const QString client_directory =
            directory.filePath("EverQuest Legends");
        const plazmic::StatusSnapshot snapshot{
            .compatibility = plazmic::CompatibilityState::supported,
            .process = plazmic::ProcessState::running,
            .profile = "synthetic-profile",
            .detail = "Synthetic Phase 2 status",
            .pid = getpid(),
        };

        plazmic::MainWindow window(
            snapshot, settings_path, true, client_directory);
        window.show();
        process_events();
        require(window.isVisible(), "main window is not visible");
        require(window.objectName() == "plazmic-main-window",
                "main window object name mismatch");
        require(window.windowTitle() ==
                    "Plazmic Legends test-version",
                "main window did not expose the project version");
        auto* menu_bar = window.findChild<QMenuBar*>("main-menu-bar");
        auto* user_menu = window.findChild<QMenu*>("user-menu");
        auto* views_menu = window.findChild<QMenu*>("views-menu");
        require(menu_bar != nullptr && user_menu != nullptr &&
                    views_menu != nullptr &&
                    !menu_bar->isNativeMenuBar(),
                "embedded top, User, or Views menu is missing");
        QAction* ui_install_action =
            window.findChild<QAction*>("ui-file-install-action");
        require(
            menu_bar->actions().contains(user_menu->menuAction()) &&
                menu_bar->actions().contains(views_menu->menuAction()) &&
                ui_install_action != nullptr &&
                user_menu->actions().contains(ui_install_action) &&
                ui_install_action->text() == "UI File Install...",
            "User/UI File Install or Views menu hierarchy is incomplete");
        QWidget* window_controls =
            menu_bar->cornerWidget(Qt::TopRightCorner);
        require(window_controls != nullptr &&
                    window_controls->objectName() == "window-controls",
                "top-right window controls are missing");
        auto* minimize_button =
            window_controls->findChild<QToolButton*>(
                "window-minimize-button");
        auto* maximize_button =
            window_controls->findChild<QToolButton*>(
                "window-maximize-button");
        auto* close_button =
            window_controls->findChild<QToolButton*>(
                "window-close-button");
        require(minimize_button != nullptr &&
                    maximize_button != nullptr &&
                    close_button != nullptr &&
                    minimize_button->focusPolicy() == Qt::StrongFocus &&
                    maximize_button->focusPolicy() == Qt::StrongFocus &&
                    close_button->focusPolicy() == Qt::StrongFocus &&
                    minimize_button->toolTip() == "Minimize" &&
                    maximize_button->toolTip() == "Maximize" &&
                    close_button->toolTip() == "Close",
                "window control buttons are incomplete");
        require(window.findChild<QWidget*>("map-view") != nullptr,
                "map canvas is missing");
        require(window.findChild<QWidget*>("spawn-table") != nullptr,
                "spawn table placeholder is missing");
        auto* character_dock =
            window.findChild<QDockWidget*>("character-dock");
        auto* parse_dock = window.findChild<QDockWidget*>("parse-dock");
        auto* spawn_dock = window.findChild<QDockWidget*>("spawn-dock");
        auto* detail_dock = window.findChild<QDockWidget*>("detail-dock");
        require(character_dock != nullptr && parse_dock != nullptr &&
                    spawn_dock != nullptr && detail_dock != nullptr &&
                    window.dockWidgetArea(character_dock) ==
                        Qt::LeftDockWidgetArea &&
                    window.dockWidgetArea(parse_dock) ==
                        Qt::LeftDockWidgetArea &&
                    character_dock->geometry().top() <
                        parse_dock->geometry().top(),
                "character and parse docks are not stacked on the left");
        const QRect map_geometry(
            window.map_canvas()->mapTo(&window, QPoint(0, 0)),
            window.map_canvas()->size());
        require(
            window.corner(Qt::BottomLeftCorner) ==
                    Qt::LeftDockWidgetArea &&
                window.corner(Qt::BottomRightCorner) ==
                    Qt::BottomDockWidgetArea &&
                detail_dock->geometry().left() >
                    parse_dock->geometry().right() &&
                detail_dock->geometry().left() <= map_geometry.left() &&
                detail_dock->geometry().right() >= map_geometry.right() &&
                detail_dock->geometry().right() >=
                    spawn_dock->geometry().right() &&
                map_geometry.bottom() < detail_dock->geometry().top() &&
                parse_dock->geometry().bottom() >
                    detail_dock->geometry().top() &&
                spawn_dock->geometry().bottom() <
                    detail_dock->geometry().top(),
            "Details is not below the map and Spawns with a full-height "
            "Character/Parse column");
        const std::array<std::pair<const char*, QDockWidget*>, 4>
            view_actions{
                std::pair{"view-character-action", character_dock},
                std::pair{"view-parse-action", parse_dock},
                std::pair{"view-spawns-action", spawn_dock},
                std::pair{"view-details-action", detail_dock},
            };
        for (const auto& [action_name, dock] : view_actions) {
            QAction* action =
                window.findChild<QAction*>(action_name);
            require(action != nullptr && dock != nullptr &&
                        views_menu->actions().contains(action) &&
                        action->isCheckable() && action->isChecked(),
                    std::string(action_name) +
                        " is missing from Views or not checked");
            action->trigger();
            process_events();
            require(!dock->isVisible() && !action->isChecked(),
                    std::string(action_name) +
                        " did not hide its view");
            action->trigger();
            process_events();
            require(dock->isVisible() && action->isChecked(),
                    std::string(action_name) +
                        " did not restore its view");
            dock->close();
            process_events();
            require(!dock->isVisible() && !action->isChecked(),
                    std::string(action_name) +
                        " did not track a closed dock");
            action->trigger();
            process_events();
            require(dock->isVisible() && action->isChecked(),
                    std::string(action_name) +
                        " did not reopen a closed dock");
        }
        minimize_button->click();
        process_events();
        require(window.isMinimized(),
                "minimize button did not minimize the window");
        window.showNormal();
        process_events();
        maximize_button->click();
        process_events();
        require(window.isMaximized() &&
                    maximize_button->toolTip() == "Restore",
                "maximize button did not maximize the window");
        maximize_button->click();
        process_events();
        require(!window.isMaximized() &&
                    maximize_button->toolTip() == "Maximize",
                "maximize button did not restore the window");
        window.showMaximized();
        process_events();
        require(maximize_button->toolTip() == "Restore",
                "external maximize did not update the window control");
        window.showNormal();
        process_events();
        require(maximize_button->toolTip() == "Maximize",
                "external restore did not update the window control");
        require(window.findChild<QLabel*>("compatibility-status")->text() ==
                    "Supported",
                "compatibility status did not render");
        require(window.findChild<QLabel*>("profile-status")->text() ==
                    "synthetic-profile",
                "profile status did not render");

        const plazmic::CharacterSnapshot character{
            .state = plazmic::PlayerSnapshotState::in_world,
            .name = "synthetic_character",
            .health = {.current = 90, .maximum = std::nullopt},
            .mana = {.current = 40, .maximum = 50},
            .equipment =
                {
                    {.slot = "Head", .item = "Synthetic Helm"},
                    {.slot = "Primary", .item = "Synthetic Sword"},
                },
            .detail = "Synthetic character snapshot",
        };
        window.update_character_snapshot(character);
        const plazmic::CombatEncounterSnapshot combat{
            .state = plazmic::CombatEncounterState::active,
            .target = "synthetic_target",
            .participants =
                {
                    {
                        .name = "synthetic_character",
                        .damage = 900,
                        .hits = 3,
                        .dps = 300.0,
                        .percentage = 75.0,
                        .active_seconds = 3.0,
                    },
                    {
                        .name = "synthetic_ally",
                        .damage = 300,
                        .hits = 2,
                        .dps = 100.0,
                        .percentage = 25.0,
                        .active_seconds = 3.0,
                    },
                },
            .total_damage = 1200,
            .duration_seconds = 3.0,
            .active_character_dps = 300.0,
            .detail = "Current encounter",
        };
        window.update_combat_snapshot(combat);
        process_events();
        require(window.findChild<QLabel*>("character-name")->text() ==
                    "synthetic_character" &&
                    window.findChild<QProgressBar*>("character-health")
                        ->text() == "HP 90" &&
                    window.findChild<QProgressBar*>("character-mana")
                        ->text()
                        .contains("40 / 50") &&
                    window.findChild<QTableWidget*>("equipment-table")
                        ->rowCount() == 2,
                "character dock did not render vitals and equipment");
        require(window.findChild<QLabel*>("character-dps")
                        ->text()
                        .contains("300.0") &&
                    window.findChild<QTableWidget*>("parse-table")
                        ->rowCount() == 2 &&
                    window.findChild<QLabel*>("parse-state")
                        ->text()
                        .contains("synthetic_target"),
                "parse dock did not render the encounter summary");
        plazmic::CombatEncounterSnapshot completed = combat;
        completed.state = plazmic::CombatEncounterState::complete;
        completed.detail = "Most recent encounter";
        window.update_combat_snapshot(completed);
        require(window.findChild<QLabel*>("character-dps")->text() ==
                    "Current DPS: 0.0" &&
                    window.findChild<QTableWidget*>("parse-table")
                            ->rowCount() == 2,
                "completed encounter did not clear current DPS or retain parse");
        window.update_character_snapshot({});
        require(window.findChild<QProgressBar*>("character-health")->text() ==
                    "HP unavailable" &&
                    window.findChild<QProgressBar*>("character-mana")->text() ==
                        "Mana unavailable",
                "unavailable character vitals were rendered as zero");

        plazmic::CombatEncounterSnapshot large_combat{
            .state = plazmic::CombatEncounterState::active,
            .target = "synthetic_target",
            .participants = {},
            .total_damage = 256,
            .duration_seconds = 10.0,
            .active_character_dps = 1.0,
            .detail = "Current encounter",
        };
        for (std::size_t index = 0U; index < 256U; ++index) {
            large_combat.participants.push_back({
                .name = "synthetic_" + std::to_string(index),
                .damage = 1,
                .hits = 1,
                .dps = 1.0,
                .percentage = 100.0 / 256.0,
                .active_seconds = 1.0,
            });
        }
        const auto ui_start = std::chrono::steady_clock::now();
        for (std::size_t update = 0U; update < 10U; ++update) {
            window.update_combat_snapshot(large_combat);
        }
        const double average_ui_ms =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - ui_start)
                .count() /
            10.0;
        require(average_ui_ms < 250.0,
                "bounded parse UI exceeded the update budget: " +
                    std::to_string(average_ui_ms) + " ms");

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
        const plazmic::SpawnCollectionSnapshot spawns{
            .state = plazmic::PlayerSnapshotState::in_world,
            .zone = "synthetic",
            .player_level = 50,
            .spawns =
                {
                    {
                        .id = 10,
                        .type = plazmic::SpawnType::player,
                        .name = "synthetic_player",
                        .level = 50,
                        .x = 12.0,
                        .y = -8.0,
                        .z = 3.0,
                        .distance = 0.0,
                    },
                    {
                        .id = 11,
                        .type = plazmic::SpawnType::npc,
                        .name = "synthetic_<guard>",
                        .level = 12,
                        .x = 20.0,
                        .y = -10.0,
                        .z = 3.0,
                        .distance = 8.2,
                    },
                    {
                        .id = 12,
                        .type = plazmic::SpawnType::corpse,
                        .name = "synthetic_corpse",
                        .level = 8,
                        .x = 25.0,
                        .y = -12.0,
                        .z = 3.0,
                        .distance = 13.6,
                    },
                    {
                        .id = 13,
                        .type = plazmic::SpawnType::npc,
                        .name = "#synthetic_named",
                        .level = 20,
                        .x = 18.0,
                        .y = -4.0,
                        .z = 3.0,
                        .distance = 7.2,
                    },
                },
            .detail = "Synthetic spawn snapshot",
        };
        window.update_spawn_snapshot(spawns);
        process_events();
        require(window.map_canvas()->zone_map().has_value(),
                "map canvas did not retain immutable geometry");
        require(window.map_canvas()->zone_map()->zone == "synthetic",
                "map canvas loaded the wrong zone");
        require(window.map_canvas()->player_snapshot().zone == "synthetic",
                "map canvas did not retain the player snapshot");
        require(window.map_canvas()->spawn_snapshot().spawns.size() == 4,
                "map canvas did not retain the spawn snapshot");
        require(
            plazmic::spawn_presentation_category(spawns.spawns[0]) ==
                    plazmic::SpawnPresentationCategory::player &&
                plazmic::spawn_presentation_category(spawns.spawns[1]) ==
                    plazmic::SpawnPresentationCategory::npc &&
                plazmic::spawn_presentation_category(spawns.spawns[2]) ==
                    plazmic::SpawnPresentationCategory::other &&
                plazmic::spawn_presentation_category(spawns.spawns[3]) ==
                    plazmic::SpawnPresentationCategory::named_npc,
            "spawn map categories did not distinguish named and Other records");
        const QColor named_color = plazmic::spawn_marker_color(
            plazmic::SpawnPresentationCategory::named_npc, 50, 20);
        const QColor npc_color = plazmic::spawn_marker_color(
            plazmic::SpawnPresentationCategory::npc, 50, 50);
        const QColor other_color = plazmic::spawn_marker_color(
            plazmic::SpawnPresentationCategory::other, 50, 8);
        require(named_color != npc_color && named_color != other_color &&
                    other_color.red() == other_color.green() &&
                    other_color.green() == other_color.blue(),
                "named and Other marker colors were not distinct and neutral");

        struct ConsiderCase {
            unsigned int player_level;
            unsigned int npc_level;
            plazmic::SpawnConsiderColor expected;
        };
        constexpr std::array consider_cases{
            ConsiderCase{50, 32, plazmic::SpawnConsiderColor::gray},
            ConsiderCase{50, 36, plazmic::SpawnConsiderColor::green},
            ConsiderCase{50, 44, plazmic::SpawnConsiderColor::light_blue},
            ConsiderCase{50, 45, plazmic::SpawnConsiderColor::blue},
            ConsiderCase{50, 50, plazmic::SpawnConsiderColor::white},
            ConsiderCase{50, 53, plazmic::SpawnConsiderColor::yellow},
            ConsiderCase{50, 54, plazmic::SpawnConsiderColor::red},
        };
        for (const auto& test : consider_cases) {
            require(
                plazmic::spawn_consider_color(
                    test.player_level, test.npc_level) == test.expected,
                "ordinary NPC consider color classification was incorrect");
        }

        constexpr std::array consider_band_levels{
            16U, 20U, 21U, 24U, 25U, 50U, 100U};
        for (const unsigned int player_level : consider_band_levels) {
            const unsigned int gray_level =
                player_level - ((player_level + 5U) / 3U);
            const unsigned int green_level =
                player_level - ((player_level + 7U) / 4U);
            require(
                plazmic::spawn_consider_color(
                    player_level, gray_level) ==
                    plazmic::SpawnConsiderColor::gray &&
                plazmic::spawn_consider_color(
                    player_level, gray_level + 1U) ==
                    plazmic::SpawnConsiderColor::green &&
                plazmic::spawn_consider_color(
                    player_level, green_level) ==
                    plazmic::SpawnConsiderColor::green,
                "gray or green consider boundary was incorrect");
            if (player_level < 25U) {
                require(
                    plazmic::spawn_consider_color(
                        player_level, green_level + 1U) ==
                        (player_level <= 20U
                             ? plazmic::SpawnConsiderColor::blue
                             : plazmic::SpawnConsiderColor::green),
                    "pre-25 blue or green consider boundary was incorrect");
            } else {
                require(
                    plazmic::spawn_consider_color(
                        player_level, green_level + 1U) ==
                            plazmic::SpawnConsiderColor::light_blue &&
                        plazmic::spawn_consider_color(
                            player_level, player_level - 6U) ==
                            plazmic::SpawnConsiderColor::light_blue &&
                        plazmic::spawn_consider_color(
                            player_level, player_level - 5U) ==
                            plazmic::SpawnConsiderColor::blue,
                    "light-blue or blue consider boundary was incorrect");
            }
        }
        require(
            plazmic::spawn_consider_color(15, 9) ==
                    plazmic::SpawnConsiderColor::gray &&
                plazmic::spawn_consider_color(15, 10) ==
                    plazmic::SpawnConsiderColor::blue,
            "low-level gray or blue consider boundary was incorrect");
        require(
            plazmic::spawn_consider_color(0, 50) ==
                plazmic::SpawnConsiderColor::gray,
            "missing player level did not use the neutral fallback");

        constexpr std::array consider_levels{
            32U, 36U, 44U, 45U, 50U, 53U, 54U};
        std::array<QColor, consider_levels.size()> consider_colors{};
        for (std::size_t index = 0; index < consider_levels.size(); ++index) {
            consider_colors[index] = plazmic::spawn_marker_color(
                plazmic::SpawnPresentationCategory::npc,
                50, consider_levels[index]);
        }
        for (std::size_t left = 0; left < consider_colors.size(); ++left) {
            for (std::size_t right = left + 1U;
                 right < consider_colors.size(); ++right) {
                require(
                    consider_colors[left] != consider_colors[right],
                    "two ordinary NPC consider colors were indistinguishable");
            }
        }
        auto* spawn_table =
            window.findChild<QTableView*>("spawn-table");
        auto* spawn_filter =
            window.findChild<QLineEdit*>("spawn-filter");
        auto* spawn_type =
            window.findChild<QComboBox*>("spawn-type-filter");
        auto* map_filters =
            window.findChild<QToolButton*>("spawn-filter-menu-button");
        require(spawn_table != nullptr && spawn_filter != nullptr &&
                    spawn_type != nullptr && map_filters != nullptr &&
                    map_filters->menu() != nullptr,
                "spawn table controls are incomplete");
        require(map_filters->text() == "Filters / Labels" &&
                    find_menu_action(
                        map_filters->menu(), "Show markers", "Named NPCs") &&
                    find_menu_action(
                        map_filters->menu(), "Show markers", "PCs") &&
                    find_menu_action(
                        map_filters->menu(), "Show markers", "NPCs") &&
                    find_menu_action(
                        map_filters->menu(), "Show markers", "Ground / Other") &&
                    find_menu_action(
                        map_filters->menu(), "Show labels", "Named NPCs") &&
                    find_menu_action(
                        map_filters->menu(), "Show labels", "PCs") &&
                    find_menu_action(
                        map_filters->menu(), "Show labels", "NPCs"),
                "Filters / Labels dropdown is incomplete");
        QAction* npc_marker_action = find_menu_action(
            map_filters->menu(), "Show markers", "NPCs");
        npc_marker_action->setChecked(false);
        require(!window.map_canvas()->npc_spawns_visible(),
                "NPC dropdown toggle did not update map visibility");
        npc_marker_action->setChecked(true);
        require(spawn_table->model()->rowCount() == 4,
                "spawn table did not publish all rows");
        require(spawn_type->itemText(spawn_type->findData(2)) == "Other",
                "non-player, non-NPC type filter was not labeled Other");
        QString named_type;
        QString other_type;
        for (int row = 0; row < spawn_table->model()->rowCount(); ++row) {
            const QModelIndex name_index =
                spawn_table->model()->index(row, 0);
            const QModelIndex type_index =
                spawn_table->model()->index(
                    row, plazmic::SpawnTableModel::type_column);
            const std::uint32_t id =
                name_index.data(plazmic::kSpawnIdRole).toUInt();
            if (id == 13U) {
                named_type = type_index.data().toString();
            } else if (id == 12U) {
                other_type = type_index.data().toString();
            }
        }
        require(named_type == "Named NPC" && other_type == "Other",
                "spawn table did not publish presentation categories");
        spawn_filter->setText("guard");
        process_events();
        require(spawn_table->model()->rowCount() == 1,
                "spawn name filter did not narrow the table");
        spawn_table->selectRow(0);
        process_events();
        require(window.map_canvas()->selected_spawn() == 11,
                "table selection did not synchronize to the map");
        require(
            window.findChild<QLabel*>("selection-detail")
                ->text()
                .contains("synthetic_&lt;guard&gt;"),
            "spawn selection did not publish escaped details");
        auto removed = spawns;
        removed.spawns.erase(removed.spawns.begin() + 1);
        window.update_spawn_snapshot(std::move(removed));
        process_events();
        require(spawn_table->model()->rowCount() == 0,
                "selected spawn removal did not update the active filter");
        require(!window.map_canvas()->selected_spawn(),
                "removed stable ID remained selected on the map");
        window.update_spawn_snapshot({
            .state = plazmic::PlayerSnapshotState::stale,
            .zone = {},
            .spawns = {},
            .detail = "Stale synthetic snapshot rejected",
        });
        process_events();
        require(spawn_table->model()->rowCount() == 0 &&
                    window.findChild<QLabel*>("spawn-state")
                        ->text()
                        .contains("Stale"),
                "stale spawn state retained rows or lost its detail");
        window.update_spawn_snapshot({
            .state = plazmic::PlayerSnapshotState::in_world,
            .zone = "synthetic",
            .spawns = {},
            .detail = "Synthetic empty snapshot",
        });
        process_events();
        require(
            window.findChild<QLabel*>("spawn-state")
                ->text()
                .startsWith("0 live"),
            "empty live spawn state was not explicit");
        window.update_spawn_snapshot(spawns);
        process_events();
        plazmic::MapViewport click_viewport;
        click_viewport.fit(
            *plazmic::calculate_map_bounds(map),
            static_cast<double>(window.map_canvas()->width()),
            static_cast<double>(window.map_canvas()->height()));
        const plazmic::MapPoint2D corpse_screen =
            click_viewport.map_to_screen(
                plazmic::spawn_map_position(spawns.spawns[2]));
        const QPointF corpse_point(corpse_screen.x, corpse_screen.y);
        const auto click_map = [&window](const QPointF& point) {
            QMouseEvent press(
                QEvent::MouseButtonPress, point,
                window.map_canvas()->mapToGlobal(point.toPoint()),
                Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(window.map_canvas(), &press);
            QMouseEvent release(
                QEvent::MouseButtonRelease, point,
                window.map_canvas()->mapToGlobal(point.toPoint()),
                Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(window.map_canvas(), &release);
        };
        QMouseEvent map_press(
            QEvent::MouseButtonPress, corpse_point,
            window.map_canvas()->mapToGlobal(corpse_point.toPoint()),
            Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(window.map_canvas(), &map_press);
        process_events();
        require(
            spawn_table->currentIndex()
                    .data(plazmic::kSpawnIdRole)
                    .toUInt() == 12U,
            "map marker selection did not synchronize to the table");
        require(spawn_filter->text().isEmpty(),
                "map selection did not reveal a filtered table row");
        spawn_type->setCurrentIndex(spawn_type->findData(2));
        process_events();
        require(spawn_table->model()->rowCount() == 1,
                "spawn type filter did not narrow the table");
        const plazmic::MapPoint2D npc_screen =
            click_viewport.map_to_screen(
                plazmic::spawn_map_position(spawns.spawns[1]));
        const QPointF npc_point(npc_screen.x, npc_screen.y);
        window.map_canvas()->set_selected_spawn(std::nullopt);
        window.map_canvas()->set_npc_spawns_visible(false);
        click_map(npc_point);
        require(!window.map_canvas()->selected_spawn(),
                "hidden NPC marker remained selectable from the map");
        window.map_canvas()->set_npc_spawns_visible(true);

        const plazmic::MapPoint2D named_screen =
            click_viewport.map_to_screen(
                plazmic::spawn_map_position(spawns.spawns[3]));
        window.map_canvas()->set_named_spawns_visible(false);
        click_map(QPointF(named_screen.x, named_screen.y));
        require(!window.map_canvas()->selected_spawn(),
                "hidden named-NPC marker remained selectable from the map");
        window.map_canvas()->set_named_spawns_visible(true);

        const plazmic::MapPoint2D player_screen =
            click_viewport.map_to_screen(
                plazmic::spawn_map_position(spawns.spawns[0]));
        window.map_canvas()->set_player_spawns_visible(false);
        click_map(QPointF(player_screen.x, player_screen.y));
        require(!window.map_canvas()->selected_spawn(),
                "hidden PC marker remained selectable from the map");
        window.map_canvas()->set_player_spawns_visible(true);

        window.map_canvas()->set_selected_spawn(std::nullopt);
        window.map_canvas()->set_other_spawns_visible(false);
        click_map(corpse_point);
        require(!window.map_canvas()->selected_spawn(),
                "hidden Other marker remained selectable from the map");
        spawn_type->setCurrentIndex(spawn_type->findData(1));
        spawn_filter->setText("guard");
        spawn_table->setColumnWidth(0, 260);
        spawn_table->sortByColumn(1, Qt::DescendingOrder);
        require(window.map_canvas()->height_filter_enabled(),
                "height filter did not default to enabled");
        require(window.map_canvas()->height_filter_below() == 15.0 &&
                    window.map_canvas()->height_filter_above() == 15.0,
                "height filter did not default to +/-15 player-Z units");
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
        require(!window.map_canvas()->named_spawn_labels_visible() &&
                    !window.map_canvas()->player_labels_visible() &&
                    !window.map_canvas()->npc_labels_visible() &&
                    !window.map_canvas()->other_spawns_visible(),
                "spawn presentation controls did not retain explicit state");
        window.map_canvas()->set_named_spawn_labels_visible(true);
        window.map_canvas()->set_player_labels_visible(true);
        window.map_canvas()->set_npc_labels_visible(true);
        window.map_canvas()->set_named_spawns_visible(false);
        window.map_canvas()->set_player_spawns_visible(false);
        window.map_canvas()->set_npc_spawns_visible(false);
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
        require(spawn_dock != nullptr, "spawn dock is missing");
        spawn_dock->setFloating(true);
        spawn_dock->show();
        process_events();
        require(spawn_dock->isWindow(), "detached spawn dock is not top-level");
        require_window_class(display, spawn_dock, "detached spawn dock");
        XCloseDisplay(display);

        window.setCorner(
            Qt::BottomLeftCorner, Qt::BottomDockWidgetArea);
        window.setCorner(
            Qt::BottomRightCorner, Qt::RightDockWidgetArea);

        const QByteArray expected_geometry_state = window.saveGeometry();
        close_button->click();
        process_events();
        require(!window.isVisible(),
                "close button did not close the main window");
        const auto saved_state = plazmic::UiSettings(settings_path).load();
        require(saved_state.has_value(),
                "close did not persist UI state");
        require(saved_state->client_directory == client_directory,
                "close did not persist the selected client directory");
        require(saved_state->geometry == expected_geometry_state,
                "persisted window geometry did not match the closed window");
        require(saved_state->height_filter_enabled &&
                    saved_state->height_filter_below == 0.0 &&
                    saved_state->height_filter_above ==
                        plazmic::kMaximumHeightFilterRange,
                "close did not persist the map height filter state");
        require(saved_state->player_follow_enabled,
                "close did not persist player-follow state");
        require(saved_state->named_spawn_labels_visible &&
                    saved_state->player_labels_visible &&
                    saved_state->npc_labels_visible &&
                    !saved_state->named_spawns_visible &&
                    !saved_state->player_spawns_visible &&
                    !saved_state->npc_spawns_visible &&
                    !saved_state->other_spawns_visible,
                "close did not persist spawn map presentation state");
        require(saved_state->spawn_filter == "guard" &&
                    saved_state->spawn_type_filter == 1 &&
                    saved_state->spawn_sort_column == 1 &&
                    saved_state->spawn_sort_descending &&
                    saved_state->spawn_column_widths[0] == 260,
                "close did not persist spawn table state");

        {
            plazmic::MainWindow reset_layout(
                snapshot, settings_path, true);
            auto* reset_spawn_dock =
                reset_layout.findChild<QDockWidget*>("spawn-dock");
            require(reset_layout.width() == 1200 &&
                        reset_layout.height() == 780 &&
                        reset_spawn_dock != nullptr &&
                        !reset_spawn_dock->isFloating(),
                    "reset layout unexpectedly restored geometry or docks");
            require(
                reset_layout.map_canvas()->height_filter_enabled() &&
                    reset_layout.map_canvas()->height_filter_below() == 0.0 &&
                    reset_layout.map_canvas()->height_filter_above() ==
                        plazmic::kMaximumHeightFilterRange &&
                    reset_layout.map_canvas()->player_follow_enabled() &&
                    reset_layout.map_canvas()->named_spawn_labels_visible() &&
                    reset_layout.map_canvas()->player_labels_visible() &&
                    reset_layout.map_canvas()->npc_labels_visible() &&
                    !reset_layout.map_canvas()->named_spawns_visible() &&
                    !reset_layout.map_canvas()->player_spawns_visible() &&
                    !reset_layout.map_canvas()->npc_spawns_visible() &&
                    !reset_layout.map_canvas()->other_spawns_visible(),
                "reset layout discarded saved map preferences");
            require(
                reset_layout.findChild<QLineEdit*>("spawn-filter")->text() ==
                        "guard" &&
                    reset_layout.findChild<QComboBox*>("spawn-type-filter")
                            ->currentData()
                            .toInt() == 1 &&
                    reset_layout.findChild<QTableView*>("spawn-table")
                            ->columnWidth(0) == 260,
                "reset layout discarded saved spawn preferences");
        }

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
        require(restored.map_canvas()->named_spawn_labels_visible() &&
                    restored.map_canvas()->player_labels_visible() &&
                    restored.map_canvas()->npc_labels_visible() &&
                    !restored.map_canvas()->named_spawns_visible() &&
                    !restored.map_canvas()->player_spawns_visible() &&
                    !restored.map_canvas()->npc_spawns_visible() &&
                    !restored.map_canvas()->other_spawns_visible(),
                "saved spawn map presentation state was not restored");
        require(
            restored.findChild<QLineEdit*>("spawn-filter")->text() ==
                    "guard" &&
                restored.findChild<QComboBox*>("spawn-type-filter")
                        ->currentData()
                        .toInt() == 1,
            "saved spawn filters were not restored");
        require(
            restored.findChild<QTableView*>("spawn-table")
                    ->horizontalHeader()
                    ->sortIndicatorSection() == 1 &&
                restored.findChild<QTableView*>("spawn-table")
                        ->horizontalHeader()
                        ->sortIndicatorOrder() == Qt::DescendingOrder,
            "saved spawn sorting was not restored");
        auto* restored_spawn_dock =
            restored.findChild<QDockWidget*>("spawn-dock");
        require(restored_spawn_dock != nullptr,
                "restored spawn dock is missing");
        require(restored_spawn_dock->isFloating(),
                "saved floating dock state was not restored");
        require(
            restored.corner(Qt::BottomLeftCorner) ==
                    Qt::BottomDockWidgetArea &&
                restored.corner(Qt::BottomRightCorner) ==
                    Qt::RightDockWidgetArea,
            "saved dock-corner ownership was not restored");
        restored.close();
        process_events();
        const auto restored_saved_state =
            plazmic::UiSettings(settings_path).load();
        require(
            restored_saved_state &&
                restored_saved_state->client_directory ==
                    client_directory,
            "restored window did not preserve the client directory");

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
