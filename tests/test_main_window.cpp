#include "game/combat_history_store.h"
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
#include <numeric>
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
#include <QSplitter>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QWheelEvent>

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
        QAction* export_inventory_action =
            window.findChild<QAction*>("export-inventory-action");
        QAction* retain_history_action =
            window.findChild<QAction*>("retain-combat-history-action");
        QAction* import_inventory_action =
            window.findChild<QAction*>("import-inventory-action");
        QAction* retain_activity_action =
            window.findChild<QAction*>("retain-activity-history-action");
        QAction* export_activity_action =
            window.findChild<QAction*>("export-activity-action");
        QAction* delete_activity_action =
            window.findChild<QAction*>("delete-activity-action");
        require(
            menu_bar->actions().contains(user_menu->menuAction()) &&
                menu_bar->actions().contains(views_menu->menuAction()) &&
                ui_install_action != nullptr &&
                user_menu->actions().contains(ui_install_action) &&
                ui_install_action->text() == "UI File Install..." &&
                export_inventory_action != nullptr &&
                user_menu->actions().contains(export_inventory_action) &&
                export_inventory_action->text() == "Export Inventory..." &&
                !export_inventory_action->isEnabled() &&
                retain_history_action != nullptr &&
                user_menu->actions().contains(retain_history_action) &&
                retain_history_action->isCheckable() &&
                !retain_history_action->isChecked() &&
                !window.combat_history_enabled() &&
                import_inventory_action != nullptr &&
                user_menu->actions().contains(import_inventory_action) &&
                retain_activity_action != nullptr &&
                retain_activity_action->isCheckable() &&
                !retain_activity_action->isChecked() &&
                !window.activity_history_enabled() &&
                export_activity_action != nullptr &&
                !export_activity_action->isEnabled() &&
                delete_activity_action != nullptr &&
                !delete_activity_action->isEnabled(),
            "User actions or Views menu hierarchy is incomplete");
        retain_history_action->trigger();
        retain_activity_action->trigger();
        auto immediate_retention =
            plazmic::UiSettings(settings_path).load();
        require(immediate_retention &&
                    immediate_retention->activity_history_enabled &&
                    window.activity_history_enabled(),
                "activity retention opt-in was not persisted immediately");
        retain_activity_action->trigger();
        require(retain_history_action->isChecked() &&
                    window.combat_history_enabled(),
                "retention action did not update the window state");
        immediate_retention = plazmic::UiSettings(settings_path).load();
        require(immediate_retention &&
                    immediate_retention->combat_history_enabled,
                "retention opt-in was not persisted immediately");
        retain_history_action->trigger();
        immediate_retention = plazmic::UiSettings(settings_path).load();
        require(immediate_retention &&
                    !immediate_retention->combat_history_enabled,
                "retention opt-out was not persisted immediately");
        retain_history_action->trigger();
        {
            plazmic::MainWindow failed_preference_window(
                snapshot, "/proc/plazmic-legends-test/config.toml", true);
            auto* failed_activity_action =
                failed_preference_window.findChild<QAction*>(
                    "retain-activity-history-action");
            require(failed_activity_action != nullptr,
                    "failed-preference activity action is missing");
            failed_activity_action->trigger();
            require(!failed_activity_action->isChecked() &&
                        !failed_preference_window.activity_history_enabled() &&
                        failed_preference_window.statusBar()
                                ->currentMessage()
                                .contains("could not be saved"),
                    "failed activity preference save was not surfaced and "
                    "rolled back");
        }
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
        auto* activity_dock =
            window.findChild<QDockWidget*>("activity-dock");
        auto* spawn_dock = window.findChild<QDockWidget*>("spawn-dock");
        auto* detail_dock = window.findChild<QDockWidget*>("detail-dock");
        auto* activity_summary_bar =
            window.findChild<QWidget*>("activity-summary-bar");
        auto* activity_summary_splitter =
            window.findChild<QSplitter*>("activity-summary-splitter");
        auto* selection_detail =
            window.findChild<QLabel*>("selection-detail");
        require(character_dock != nullptr && parse_dock != nullptr &&
                    activity_dock != nullptr &&
                    spawn_dock != nullptr && detail_dock != nullptr &&
                    activity_summary_bar != nullptr &&
                    activity_summary_splitter != nullptr &&
                    selection_detail != nullptr &&
                    window.dockWidgetArea(character_dock) ==
                        Qt::LeftDockWidgetArea &&
                    window.dockWidgetArea(parse_dock) ==
                        Qt::LeftDockWidgetArea &&
                    window.dockWidgetArea(activity_dock) ==
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
                    detail_dock->geometry().top() &&
                detail_dock->isAncestorOf(activity_summary_bar) &&
                activity_summary_bar->geometry().left() ==
                    selection_detail->geometry().left() &&
                activity_summary_bar->width() == selection_detail->width() &&
                activity_summary_splitter->count() == 3 &&
                activity_summary_splitter->handleWidth() == 6 &&
                activity_summary_splitter->sizes()[1] >
                    activity_summary_splitter->sizes()[0] &&
                activity_summary_splitter->sizes()[2] >
                    activity_summary_splitter->sizes()[0] &&
                !activity_dock->isAncestorOf(activity_summary_bar) &&
                character_dock->findChild<QLabel*>(
                    "activity-summary-dps") == nullptr,
            "Details is not below the map and Spawns with a full-height "
            "Character/Parse column and full-width activity summary");
        const std::array<std::pair<const char*, QDockWidget*>, 5>
            view_actions{
                std::pair{"view-character-action", character_dock},
                std::pair{"view-parse-action", parse_dock},
                std::pair{"view-activity-action", activity_dock},
                std::pair{"view-spawns-action", spawn_dock},
                std::pair{"view-details-action", detail_dock},
            };
        for (const auto& [action_name, dock] : view_actions) {
            QAction* action =
                window.findChild<QAction*>(action_name);
            require(action != nullptr && dock != nullptr,
                    std::string(action_name) + " has no dock or action");
            dock->raise();
            process_events();
            require(views_menu->actions().contains(action) &&
                        action->isCheckable() && action->isChecked(),
                    std::string(action_name) +
                        " is missing from Views or not checked");
            action->trigger();
            dock->raise();
            process_events();
            require(!dock->isVisible() && !action->isChecked(),
                    std::string(action_name) +
                        " did not hide its view");
            action->trigger();
            dock->raise();
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
            dock->raise();
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
            .health = {.current = 90, .maximum = 100},
            .mana = {.current = 40, .maximum = 50},
            .experience_percent = 24.845,
            .alternate_advancement_percent = 66.48,
            .alternate_advancement_points = 42U,
            .equipment =
                {
                    {.slot = "Head", .item = "Synthetic Helm"},
                    {.slot = "Primary", .item = "Synthetic Sword"},
                },
            .detail = "Synthetic character snapshot",
        };
        window.update_character_snapshot(character);
        require(export_inventory_action->isEnabled(),
                "available character data did not enable inventory export");
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
                        .abilities = {
                            {
                                .name = "Synthetic Bolt",
                                .category = "Spell",
                                .damage = 900,
                                .hits = 3,
                            },
                        },
                    },
                    {
                        .name = "synthetic_ally",
                        .damage = 300,
                        .hits = 2,
                        .dps = 100.0,
                        .percentage = 25.0,
                        .active_seconds = 3.0,
                        .abilities = {},
                    },
                },
            .healers = {},
            .timeline = {},
            .zone = "synthetic_zone",
            .started_unix_seconds = 1,
            .total_damage = 1200,
            .total_healing = 0,
            .duration_seconds = 3.0,
            .active_character_dps = 300.0,
            .detail = "Current encounter",
        };
        window.update_combat_snapshot(combat);
        process_events();
        auto* activity_inventory =
            window.findChild<QWidget*>("activity-inventory");
        auto* equipment_table =
            window.findChild<QTableWidget*>("equipment-table");
        auto* equipment_state =
            window.findChild<QLabel*>("inventory-equipment-state");
        require(window.findChild<QLabel*>("character-name")->text() ==
                    "synthetic_character" &&
                    window.findChild<QProgressBar*>("character-health")
                        ->text() == "HP 90 / 100 (90%)" &&
                    window.findChild<QProgressBar*>("character-health")
                        ->maximum() == 100 &&
                    window.findChild<QProgressBar*>("character-health")
                        ->value() == 90 &&
                    window.findChild<QProgressBar*>("character-health")
                        ->styleSheet().contains("#d32f2f") &&
                    window.findChild<QProgressBar*>("character-mana")
                        ->text() == "MP 40 / 50 (80%)" &&
                    window.findChild<QProgressBar*>("character-mana")
                        ->maximum() == 50 &&
                    window.findChild<QProgressBar*>("character-mana")
                        ->value() == 40 &&
                    window.findChild<QProgressBar*>("character-mana")
                        ->toolTip() == "MP 40 / 50 (80%)" &&
                    window.findChild<QProgressBar*>("character-mana")
                        ->minimumWidth() >=
                        window.findChild<QProgressBar*>("character-mana")
                                ->fontMetrics()
                                .horizontalAdvance("MP 40 / 50 (80%)") +
                            32 &&
                    window.findChild<QProgressBar*>("character-mana")
                        ->styleSheet().contains("#1976d2") &&
                    equipment_table != nullptr &&
                    equipment_table->rowCount() == 2 &&
                    equipment_state != nullptr &&
                    equipment_state->isHidden() &&
                    activity_inventory != nullptr &&
                    activity_inventory->isAncestorOf(equipment_table) &&
                    character_dock != nullptr &&
                    !character_dock->isAncestorOf(equipment_table),
                "character vitals or Activity inventory equipment did not "
                "render in the expected panel");

        plazmic::CharacterSnapshot fractional_mana = character;
        fractional_mana.mana = {.current = 2, .maximum = 3};
        window.update_character_snapshot(fractional_mana);
        require(
            window.findChild<QProgressBar*>("character-mana")->text() ==
                    "MP 2 / 3 (67%)" &&
                window.findChild<QProgressBar*>("character-mana")->maximum() ==
                    3 &&
                window.findChild<QProgressBar*>("character-mana")->value() ==
                    2,
            "mana gauge did not round its exact current/maximum percentage");

        plazmic::CharacterSnapshot zero_mana = character;
        zero_mana.mana = {.current = 0, .maximum = 0};
        window.update_character_snapshot(zero_mana);
        require(
            window.findChild<QProgressBar*>("character-mana")->text() ==
                "MP 0 / 0 (0%)",
            "zero mana gauge did not display maximum and percentage");
        window.update_character_snapshot(character);
        require(window.findChild<QLabel*>("activity-summary-dps")
                        ->text()
                        .contains("DPS: 300.0") &&
                    window.findChild<QTableWidget*>("parse-table")
                        ->rowCount() == 2 &&
                    window.findChild<QLabel*>("parse-state")
                        ->text()
                        .contains("synthetic_target"),
                "parse dock did not render the encounter summary");
        plazmic::CombatEncounterSnapshot retained_combat = combat;
        retained_combat.state = plazmic::CombatEncounterState::complete;
        retained_combat.target = "retained_target";
        retained_combat.started_unix_seconds = 2;
        retained_combat.total_damage = 77;
        retained_combat.participants.resize(1);
        retained_combat.participants.front().damage = 77;
        retained_combat.participants.front().abilities.clear();
        retained_combat.detail = "Retained encounter";
        plazmic::CombatAnalyticsSnapshot analytics{
            .encounter = combat,
            .history = {retained_combat},
            .zone_damage = 1200,
            .zone_healing = 0,
            .zone_encounters = 1,
            .history_retention_enabled = true,
            .history_persisted = true,
            .history_detail = {},
        };
        analytics.encounter.healers.push_back({
            .name = "synthetic_healer",
            .healing = 250,
            .casts = 2,
            .hps = 83.3,
            .percentage = 100.0,
        });
        analytics.encounter.total_healing = 250;
        analytics.encounter.timeline.push_back({
            .elapsed_seconds = 1,
            .damage = 1200,
            .healing = 250,
        });
        window.update_combat_snapshot(analytics);
        require(window.findChild<QTableWidget*>("combat-healing")
                        ->rowCount() == 1 &&
                    window.findChild<QTableWidget*>("combat-timeline")
                        ->rowCount() == 1 &&
                    window.findChild<QTableWidget*>("combat-history")
                        ->rowCount() == 1 &&
                    window.findChild<QTableWidget*>("combat-abilities")
                        ->rowCount() == 1 &&
                    window.findChild<QLabel*>("combat-overview")
                        ->text()
                        .contains("1 completed"),
                "combat analytics tabs did not render history and healing");
        plazmic::ActivityAnalyticsSnapshot activity;
        activity.storage_key =
            "0123456789abcdef0123456789abcdef";
        activity.events.push_back({
            .kind = plazmic::ActivityEventKind::experience,
            .timestamp_unix_seconds = 1,
            .zone = "synthetic_zone",
            .label = "Experience gained",
            .amount = 0.125,
            .total = std::nullopt,
            .evidence = "Synthetic exact log percentage",
            .source_id = {},
        });
        activity.events.push_back({
            .kind = plazmic::ActivityEventKind::loot,
            .timestamp_unix_seconds = 2,
            .zone = "synthetic_zone",
            .label = "Synthetic Gem",
            .amount = 1.0,
            .total = std::nullopt,
            .evidence = "Synthetic exact loot line",
            .source_id = {},
        });
        activity.events.push_back({
            .kind = plazmic::ActivityEventKind::alternate_advancement,
            .timestamp_unix_seconds = 3,
            .zone = "synthetic_zone",
            .label = "Alternate Advancement points gained",
            .amount = 2.0,
            .total = 42U,
            .evidence = "Synthetic exact AA total",
            .source_id = {},
        });
        activity.abilities.push_back({
            .name = "Synthetic Burst",
            .category = "Spell",
            .damage = 500,
            .observations = 2,
            .confidence = "Observed; proc unconfirmed",
        });
        activity.experience_gained_percent = 133.0;
        activity.experience_percent_per_hour = 0.25;
        activity.level_pace_hours = 400.0;
        activity.alternate_advancement_percent = 66.48;
        activity.alternate_advancement_points = 42;
        activity.alternate_advancement_points_per_hour = 1.0;
        activity.next_alternate_advancement_hours = 1.0;
        activity.recent_loot_count = 1;
        activity.available = true;
        activity.detail = "Synthetic activity";
        auto changed_activity = activity;
        changed_activity.events.front().amount = 0.250;
        require(changed_activity != activity &&
                    !plazmic::same_activity_export_payload(
                        changed_activity, activity),
                "activity export consistency ignored changed bounded data");
        auto derived_activity = activity;
        derived_activity.experience_percent_per_hour = 0.5;
        derived_activity.level_pace_hours = 200.0;
        require(derived_activity != activity &&
                    plazmic::same_activity_export_payload(
                        derived_activity, activity),
                "derived activity fields invalidated an unchanged export");
        window.update_activity_snapshot(activity);
        require(window.findChild<QTableWidget*>("activity-events")
                            ->rowCount() == 3 &&
                    window.findChild<QTableWidget*>("activity-events")
                            ->item(0, 2)
                            ->text()
                            .contains("(2 points; total 42)") &&
                    window.findChild<QTableWidget*>("activity-abilities")
                            ->rowCount() == 1 &&
                    window.findChild<QLabel*>("activity-summary-aa")
                            ->text()
                            .contains("AA: 66.480% | banked: 42 | 1.00/h") &&
                    window.findChild<QLabel*>("activity-summary-latest") ==
                        nullptr &&
                    export_activity_action->isEnabled() &&
                    delete_activity_action->isEnabled() &&
                    window.findChild<QTableWidget*>(
                               "inventory-reconciliation") != nullptr &&
                    window.findChild<QLabel*>("activity-summary-xp")
                            ->textFormat() == Qt::PlainText &&
                    window.findChild<QLabel*>("activity-summary-xp")
                            ->text()
                            .contains("XP: 24.845%") &&
                    window.findChild<QLabel*>("activity-summary-xp")
                            ->text()
                            .contains("gain rate 0.250%/h") &&
                    !window.findChild<QLabel*>("activity-summary-xp")
                         ->text()
                         .contains("133.000%") &&
                    window.findChild<QLabel*>(
                               "inventory-reconciliation-state")
                            ->textFormat() == Qt::PlainText,
                "progression, activity, or inventory views did not render");

        auto memory_backed_character = character;
        memory_backed_character.alternate_advancement_percent = 71.25;
        memory_backed_character.alternate_advancement_points = 7U;
        window.update_character_snapshot(memory_backed_character);
        require(window.findChild<QLabel*>("activity-summary-aa")
                        ->text()
                        .contains("AA: 71.250% | banked: 7 | 1.00/h"),
                "AA summary did not prefer the current memory snapshot");
        auto unavailable_log_activity = activity;
        unavailable_log_activity.available = false;
        unavailable_log_activity.detail = "Synthetic log unavailable";
        window.update_activity_snapshot(unavailable_log_activity);
        require(
            window.findChild<QLabel*>("activity-summary-aa")->text() ==
                "AA: 71.250% | banked: 7 | gain rate: unavailable",
            "unavailable activity input blanked memory-backed AA state");
        window.update_character_snapshot(character);
        window.update_activity_snapshot(activity);

        auto unsaved_activity = activity;
        unsaved_activity.persisted = false;
        unsaved_activity.detail =
            "Local observations only; activity history could not be saved";
        window.update_activity_snapshot(unsaved_activity);
        auto* activity_state =
            window.findChild<QLabel*>("activity-state");
        require(activity_state != nullptr && activity_state->isVisible() &&
                    activity_state->text().contains(
                        "Local observations only") &&
                    window.findChild<QLabel*>("activity-summary-latest") ==
                        nullptr,
                "activity persistence failure was hidden or the removed "
                "latest summary returned");
        window.update_activity_snapshot(activity);
        require(!activity_state->isVisible(),
                "resolved activity failure remained visible");
        window.report_activity_deletion_result(activity.storage_key, false);
        require(activity_state->text().contains("deletion failed") &&
                    window.statusBar()->currentMessage().contains(
                        "deletion failed"),
                "failed activity deletion displayed a misleading storage "
                "state");
        window.statusBar()->clearMessage();
        window.update_activity_snapshot(activity);
        auto* activity_events =
            window.findChild<QTableWidget*>("activity-events");
        auto* activity_abilities =
            window.findChild<QTableWidget*>("activity-abilities");
        auto* retained_event_item = activity_events->item(0, 2);
        auto* retained_ability_item = activity_abilities->item(0, 2);
        window.update_activity_snapshot(derived_activity);
        require(activity_events->item(0, 2) == retained_event_item &&
                    activity_abilities->item(0, 2) == retained_ability_item &&
                    window.findChild<QLabel*>("activity-summary-xp")
                        ->text()
                        .contains("0.500%/h"),
                "derived refresh rebuilt unchanged activity tables or left "
                "the overview stale");
        auto retained_but_hidden = plazmic::ActivityAnalyticsSnapshot{};
        retained_but_hidden.storage_key = "confirmed-partition";
        retained_but_hidden.available = true;
        window.update_activity_snapshot(retained_but_hidden);
        require(delete_activity_action->isEnabled(),
                "confirmed retained partition was not deletable when hidden");
        window.update_activity_snapshot(activity);
        retain_activity_action->trigger();
        require(!window.queue_activity_history_deletion(
                    activity.storage_key) &&
                    retain_activity_action->isChecked() &&
                    delete_activity_action->isEnabled() &&
                    window.statusBar()->currentMessage().contains(
                        "deletion is unavailable"),
                "missing deletion callback changed retention or queued work");
        auto unknown_aa_character = character;
        unknown_aa_character.alternate_advancement_percent.reset();
        unknown_aa_character.alternate_advancement_points.reset();
        window.update_character_snapshot(unknown_aa_character);
        window.update_activity_snapshot(activity);
        require(window.findChild<QLabel*>("activity-summary-aa")
                    ->text()
                    .contains("AA: unavailable | banked: unavailable"),
                "log-derived AA total replaced unavailable memory state");
        window.update_character_snapshot(character);
        auto incompatible_activity = activity;
        incompatible_activity.events.clear();
        incompatible_activity.abilities.clear();
        incompatible_activity.persisted = false;
        incompatible_activity.detail =
            "Stored activity uses an unsupported or invalid schema";
        window.update_activity_snapshot(incompatible_activity);
        require(delete_activity_action->isEnabled() &&
                    activity_state->isVisible() &&
                    activity_state->text().contains("unsupported"),
                "incompatible activity partition could not be deleted or "
                "did not show its failure state");
        auto unavailable_activity = incompatible_activity;
        unavailable_activity.persisted = true;
        unavailable_activity.available = false;
        unavailable_activity.storage_key.clear();
        unavailable_activity.detail = "Activity unavailable";
        window.update_activity_snapshot(unavailable_activity);
        require(!delete_activity_action->isEnabled() &&
                    !export_activity_action->isEnabled() &&
                    window.findChild<QTableWidget*>("activity-events")
                            ->rowCount() == 0 &&
                    window.findChild<QTableWidget*>("activity-abilities")
                            ->rowCount() == 0 &&
                    window.findChild<QLabel*>("activity-summary-xp")
                            ->toolTip() ==
                        "XP: 24.845% | gain rate: unavailable" &&
                    window.findChild<QLabel*>("activity-summary-aa")
                            ->toolTip() ==
                        "AA: 66.480% | banked: 42 | gain rate: unavailable" &&
                    window.findChild<QLabel*>("activity-summary-latest") ==
                        nullptr &&
                    activity_state->isVisible() &&
                    activity_state->text() == "Activity unavailable",
                "unavailable lifecycle state retained stale activity");
        window.update_activity_snapshot(activity);
        require(window.findChild<QTableWidget*>("activity-events")
                            ->rowCount() == 3 &&
                    export_activity_action->isEnabled(),
                "recovered lifecycle state did not republish activity");
        std::vector<std::string> deleted_activity_keys;
        window.set_delete_activity_callback(
            [&deleted_activity_keys](std::string key) {
                deleted_activity_keys.push_back(std::move(key));
            });
        auto replacement_activity = activity;
        replacement_activity.storage_key =
            "fedcba9876543210fedcba9876543210";
        window.update_activity_snapshot(replacement_activity);
        require(!window.queue_activity_history_deletion(
                    activity.storage_key) &&
                    deleted_activity_keys.empty() &&
                    delete_activity_action->isEnabled() &&
                    window.statusBar()->currentMessage().contains(
                        "partition changed"),
                "activity deletion ignored a confirmed partition change");
        window.update_activity_snapshot(activity);
        require(window.queue_activity_history_deletion(activity.storage_key) &&
                    deleted_activity_keys.size() == 1U &&
                    deleted_activity_keys.front() == activity.storage_key &&
                    !retain_activity_action->isChecked() &&
                    !delete_activity_action->isEnabled(),
                "confirmed activity deletion was not queued safely");
        window.report_activity_deletion_result(activity.storage_key, true);
        require(window.findChild<QTableWidget*>("activity-events")
                            ->rowCount() == 0 &&
                    window.findChild<QTableWidget*>("activity-abilities")
                            ->rowCount() == 0 &&
                    !export_activity_action->isEnabled() &&
                    !delete_activity_action->isEnabled(),
                "successful activity deletion retained exportable UI data");
        auto* history_table =
            window.findChild<QTableWidget*>("combat-history");
        require(history_table->selectionMode() ==
                    QAbstractItemView::SingleSelection,
                "combat history did not enforce single-row selection");
        history_table->selectRow(0);
        process_events();
        require(window.findChild<QLabel*>("parse-state")
                        ->text()
                        .contains("retained_target") &&
                    window.findChild<QTableWidget*>("parse-table")
                        ->rowCount() == 1 &&
                    window.findChild<QTableWidget*>("combat-abilities")
                            ->rowCount() == 0 &&
                    window.findChild<QLabel*>("activity-summary-dps")->text() ==
                        "DPS: 300.0",
                "retained history selection did not drive drill-down tabs");
        auto unavailable_analytics = analytics;
        unavailable_analytics.encounter = {
            .state = plazmic::CombatEncounterState::unavailable,
            .target = {},
            .participants = {},
            .healers = {},
            .timeline = {},
            .zone = {},
            .started_unix_seconds = 0,
            .total_damage = 0,
            .total_healing = 0,
            .duration_seconds = 0.0,
            .active_character_dps = 0.0,
            .detail = "Combat log unavailable",
        };
        window.update_combat_snapshot(unavailable_analytics);
        require(window.findChild<QLabel*>("parse-state")->text() ==
                    "Combat log unavailable" &&
                    window.findChild<QTableWidget*>("parse-table")
                            ->rowCount() == 1,
                "retained selection masked the current parser failure");
        window.update_combat_snapshot(analytics);
        history_table->clearSelection();
        process_events();
        require(window.findChild<QLabel*>("parse-state")
                        ->text()
                        .contains("synthetic_target") &&
                    window.findChild<QTableWidget*>("parse-table")
                            ->rowCount() == 2 &&
                    window.findChild<QTableWidget*>("combat-abilities")
                            ->rowCount() == 1,
                "cleared history selection did not restore the current "
                "encounter");
        plazmic::CombatEncounterSnapshot completed = combat;
        completed.state = plazmic::CombatEncounterState::complete;
        completed.detail = "Most recent encounter";
        window.update_combat_snapshot(completed);
        require(window.findChild<QLabel*>("activity-summary-dps")->text() ==
                    "DPS: 0.0" &&
                    window.findChild<QTableWidget*>("parse-table")
                            ->rowCount() == 2,
                "completed encounter did not clear current DPS or retain parse");
        window.update_inventory_reconciliation(
            {
                .entries = {{.location = "General1",
                             .item = "Synthetic Helm",
                             .quantity = 1}},
                .equipped_not_in_import = {},
                .imported_equipped_items = {"Head: Synthetic Helm"},
                .source_name = "synthetic-inventory.txt",
                .detail = "Synthetic inventory",
                .available = true,
            },
            "/synthetic/inventory.txt");
        require(window.findChild<QTableWidget*>("inventory-reconciliation")
                            ->rowCount() == 1,
                "available inventory reconciliation did not render");
        window.update_inventory_reconciliation(
            {
                .entries = {},
                .equipped_not_in_import = {},
                .imported_equipped_items = {},
                .source_name = {},
                .detail = "Synthetic inventory read failed",
                .available = false,
            },
            {});
        require(window.findChild<QTableWidget*>("inventory-reconciliation")
                            ->rowCount() == 0 &&
                    window.findChild<QLabel*>(
                               "inventory-reconciliation-state")
                        ->text()
                        .contains("Synthetic inventory read failed"),
                "failed inventory import retained stale rows or lost detail");
        window.update_inventory_reconciliation(
            {
                .entries = {{.location = "General1",
                             .item = "Synthetic Helm",
                             .quantity = 1}},
                .equipped_not_in_import = {},
                .imported_equipped_items = {"Head: Synthetic Helm"},
                .source_name = "synthetic-inventory.txt",
                .detail = "Synthetic inventory",
                .available = true,
            },
            "/synthetic/inventory.txt");
        window.update_character_snapshot({});
        require(!export_inventory_action->isEnabled(),
                "unavailable character data left inventory export enabled");
        require(window.findChild<QProgressBar*>("character-health")->text() ==
                    "HP unavailable" &&
                    window.findChild<QProgressBar*>("character-mana")->text() ==
                        "MP unavailable" &&
                    window.findChild<QLabel*>("activity-summary-xp")
                        ->text()
                        .contains("XP: unavailable") &&
                    window.findChild<QLabel*>("activity-summary-aa")
                            ->text() ==
                        "AA: unavailable" &&
                    !equipment_state->isHidden() &&
                    equipment_state->text() ==
                        "Character information unavailable" &&
                    equipment_table->rowCount() == 0 &&
                    window.findChild<QTableWidget*>(
                               "inventory-reconciliation")
                            ->rowCount() == 0 &&
                    window.findChild<QLabel*>(
                               "inventory-reconciliation-state")
                        ->text()
                        .contains("became unavailable"),
                "unavailable character retained vitals or imported inventory");

        window.update_character_snapshot({
            .state = plazmic::PlayerSnapshotState::unavailable,
            .name = "synthetic_character",
            .health = {},
            .mana = {},
            .experience_percent = std::nullopt,
            .alternate_advancement_percent = std::nullopt,
            .alternate_advancement_points = std::nullopt,
            .equipment = {},
            .detail =
                "Character vitals and equipment unavailable for this client",
        });
        require(
            window.findChild<QLabel*>("character-name")->text() ==
                    "synthetic_character" &&
                window.findChild<QProgressBar*>("character-health")->text() ==
                    "HP unavailable" &&
                window.findChild<QProgressBar*>("character-mana")->text() ==
                    "MP unavailable" &&
                equipment_state->text().contains(
                    "Vitals and equipment", Qt::CaseInsensitive),
            "validated identity fallback hid the player or exposed vitals");

        auto empty_equipment = character;
        empty_equipment.equipment.clear();
        window.update_character_snapshot(empty_equipment);
        require(!equipment_state->isHidden() &&
                    equipment_state->text() ==
                        "No equipped items reported" &&
                    equipment_table->rowCount() == 0,
                "available empty equipment state was not explicit");

        plazmic::CombatEncounterSnapshot large_combat{
            .state = plazmic::CombatEncounterState::active,
            .target = "synthetic_target",
            .participants = {},
            .healers = {},
            .timeline = {},
            .zone = "synthetic_zone",
            .started_unix_seconds = 1,
            .total_damage = 256,
            .total_healing = 0,
            .duration_seconds = 10.0,
            .active_character_dps = 1.0,
            .detail = "Current encounter",
        };
        for (std::size_t index = 0U; index < 256U; ++index) {
            plazmic::CombatParticipantSnapshot participant{
                .name = "synthetic_" + std::to_string(index),
                .damage = 1,
                .hits = 1,
                .dps = 1.0,
                .percentage = 100.0 / 256.0,
                .active_seconds = 1.0,
                .abilities = {},
            };
            for (std::size_t ability = 0U; ability < 128U; ++ability) {
                participant.abilities.push_back({
                    .name = "ability_" + std::to_string(ability),
                    .category = "Melee",
                    .damage = 1,
                    .hits = 1,
                });
            }
            large_combat.participants.push_back(std::move(participant));
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
        require(window.findChild<QTableWidget*>("combat-abilities")
                        ->rowCount() ==
                    static_cast<int>(
                        plazmic::CombatHistoryStore::maximum_total_abilities),
                "combat ability table exceeded its aggregate row bound");

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
            .player_name = "synthetic_player",
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

        const double fitted_scale =
            window.map_canvas()->viewport_scale();
        const QPoint canvas_center =
            window.map_canvas()->rect().center();
        QWheelEvent zoom_event(
            QPointF(canvas_center),
            QPointF(window.map_canvas()->mapToGlobal(canvas_center)),
            {}, QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
            Qt::NoScrollPhase, false);
        QApplication::sendEvent(window.map_canvas(), &zoom_event);
        process_events();
        const double zoomed_scale =
            window.map_canvas()->viewport_scale();
        require(zoomed_scale > fitted_scale,
                "synthetic wheel input did not zoom the map");

        window.set_zone_map(map);
        process_events();
        require(window.map_canvas()->viewport_scale() == zoomed_scale,
                "same-zone map refresh reset the zoom");
        window.clear_zone_map("Synthetic transient refresh");
        window.set_zone_map(map);
        process_events();
        require(window.map_canvas()->viewport_scale() == zoomed_scale,
                "same-zone map recovery reset the zoom");

        auto next_zone_map = map;
        next_zone_map.zone = "synthetic_next";
        window.set_zone_map(next_zone_map);
        process_events();
        require(window.map_canvas()->viewport_scale() == fitted_scale,
                "new-zone map did not fit its viewport");
        window.set_zone_map(map);
        process_events();
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
        QModelIndex ordinary_npc;
        for (int row = 0; row < spawn_table->model()->rowCount(); ++row) {
            const QModelIndex candidate =
                spawn_table->model()->index(row, 0);
            if (candidate.data(plazmic::kSpawnIdRole).toUInt() == 11U) {
                ordinary_npc = candidate;
                break;
            }
        }
        require(
            ordinary_npc.isValid() &&
                ordinary_npc.data(Qt::ForegroundRole).value<QColor>() ==
                plazmic::spawn_marker_color(
                    plazmic::SpawnPresentationCategory::npc,
                    spawns.player_level,
                    spawns.spawns[1].level),
            "spawn table did not retain ordinary NPC consider color");
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
            .player_level = 0U,
            .player_name = {},
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
            .player_level = 0U,
            .player_name = "synthetic_player",
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

        activity_summary_splitter->setSizes({80, 180, 220});
        process_events();
        const QList<int> expected_summary_sizes =
            activity_summary_splitter->sizes();

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
        require(saved_state->combat_history_enabled,
                "close did not persist combat-history retention consent");
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
                    saved_state->spawn_column_widths[0] == 260 &&
                    saved_state->activity_summary_widths ==
                        std::array<int, 3>{expected_summary_sizes[0],
                                           expected_summary_sizes[1],
                                           expected_summary_sizes[2]},
                "close did not persist table or activity summary state");

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
        const QList<int> restored_summary_sizes =
            restored.findChild<QSplitter*>("activity-summary-splitter")
                ->sizes();
        const int expected_summary_total =
            std::accumulate(expected_summary_sizes.cbegin(),
                            expected_summary_sizes.cend(), 0);
        const int restored_summary_total =
            std::accumulate(restored_summary_sizes.cbegin(),
                            restored_summary_sizes.cend(), 0);
        bool summary_proportions_restored =
            expected_summary_sizes.size() == 3 &&
            restored_summary_sizes.size() == 3 &&
            expected_summary_total > 0 && restored_summary_total > 0;
        for (qsizetype index = 0;
             summary_proportions_restored &&
             index < restored_summary_sizes.size(); ++index) {
            const int scaled_expected =
                expected_summary_sizes[index] * restored_summary_total /
                expected_summary_total;
            summary_proportions_restored =
                std::abs(restored_summary_sizes[index] - scaled_expected) <= 2;
        }
        require(
            summary_proportions_restored,
            "saved three-column activity summary proportions were not "
            "restored");
        require(restored.map_canvas()->height_filter_enabled() &&
                    restored.map_canvas()->height_filter_below() == 0.0 &&
                    restored.map_canvas()->height_filter_above() ==
                        plazmic::kMaximumHeightFilterRange,
                "saved map height filter state was not restored");
        require(restored.map_canvas()->player_follow_enabled(),
                "saved player-follow state was not restored");
        require(restored.combat_history_enabled(),
                "saved combat-history retention consent was not restored");
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
        auto* restored_detail_dock =
            restored.findChild<QDockWidget*>("detail-dock");
        require(restored_detail_dock != nullptr,
                "restored detail dock is missing");
        require(
            restored.corner(Qt::BottomLeftCorner) ==
                    Qt::BottomDockWidgetArea &&
                restored.corner(Qt::BottomRightCorner) ==
                    Qt::RightDockWidgetArea,
            "saved dock-corner ownership was not restored");
        restored_detail_dock->hide();
        process_events();
        restored.close();
        process_events();
        const auto restored_saved_state =
            plazmic::UiSettings(settings_path).load();
        require(
            restored_saved_state &&
                restored_saved_state->client_directory ==
                    client_directory &&
                restored_saved_state->activity_summary_widths ==
                    saved_state->activity_summary_widths,
            "hidden Details did not preserve the client directory or "
            "activity summary widths");

        plazmic::MainWindow hidden_details_restored(
            snapshot, settings_path, false);
        hidden_details_restored.show();
        process_events();
        auto* hidden_detail_dock =
            hidden_details_restored.findChild<QDockWidget*>("detail-dock");
        require(hidden_detail_dock != nullptr &&
                    !hidden_detail_dock->isVisible(),
                "saved hidden Details state was not restored");
        hidden_details_restored.close();
        process_events();
        const auto hidden_details_saved_state =
            plazmic::UiSettings(settings_path).load();
        require(
            hidden_details_saved_state &&
                hidden_details_saved_state->activity_summary_widths ==
                    saved_state->activity_summary_widths,
            "closing with Details restored hidden changed activity summary "
            "widths");

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
