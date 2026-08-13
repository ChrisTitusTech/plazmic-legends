#pragma once

#include "model/character_snapshot.h"
#include "model/activity_snapshot.h"
#include "model/combat_snapshot.h"
#include "model/status_snapshot.h"
#include "model/player_snapshot.h"
#include "model/spawn_snapshot.h"
#include "ui/ui_settings.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <utility>

#include <QMainWindow>

class QAction;
class QComboBox;
class QDockWidget;
class QEvent;
class QLabel;
class QLineEdit;
class QProgressBar;
class QTableWidget;
class QTableView;
class QTabWidget;
class QToolButton;

namespace plazmic {

class MapCanvas;
class SpawnFilterProxyModel;
class SpawnTableModel;
struct ZoneMap;

class MainWindow final : public QMainWindow {
  public:
    explicit MainWindow(StatusSnapshot snapshot,
                        QString settings_path = UiSettings::default_path(),
                        bool reset_layout = false,
                        QString client_directory = {},
                        QWidget* parent = nullptr);

    void update_snapshot(const StatusSnapshot& snapshot);
    void update_player_snapshot(const PlayerSnapshot& snapshot);
    void update_character_snapshot(const CharacterSnapshot& snapshot);
    void update_activity_snapshot(const ActivityAnalyticsSnapshot& snapshot);
    void update_inventory_reconciliation(
        InventoryReconciliationSnapshot snapshot,
        QString source_path);
    void update_combat_snapshot(const CombatAnalyticsSnapshot& snapshot);
    void update_combat_snapshot(const CombatEncounterSnapshot& snapshot) {
        update_combat_snapshot(CombatAnalyticsSnapshot{
            .encounter = snapshot,
            .history = {},
            .zone_damage = 0U,
            .zone_healing = 0U,
            .zone_encounters = 0U,
            .history_retention_enabled = false,
            .history_persisted = true,
            .history_detail = {},
        });
    }
    void update_spawn_snapshot(SpawnCollectionSnapshot snapshot);
    void set_zone_map(ZoneMap map);
    void clear_zone_map(const QString& detail);
    void report_activity_deletion_result(std::string_view key,
                                         bool succeeded);
    [[nodiscard]] bool queue_activity_history_deletion(
        std::string_view confirmed_key);
    [[nodiscard]] const StatusSnapshot& snapshot() const { return snapshot_; }
    [[nodiscard]] MapCanvas* map_canvas() const { return map_canvas_; }
    [[nodiscard]] bool combat_history_enabled() const;
    [[nodiscard]] bool activity_history_enabled() const;
    void set_delete_activity_callback(
        std::function<void(std::string)> callback) {
        delete_activity_callback_ = std::move(callback);
    }

  protected:
    void changeEvent(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

  private:
    void build_ui();
    void build_menu_bar(QDockWidget* character_dock,
                        QDockWidget* parse_dock,
                        QDockWidget* activity_dock,
                        QDockWidget* spawn_dock,
                        QDockWidget* detail_dock);
    void open_inventory_export();
    void open_inventory_import();
    void export_activity_history();
    void delete_activity_history();
    void open_ui_file_install();
    void update_maximize_button();
    void restore_ui_state();
    void ensure_on_screen();
    [[nodiscard]] bool save_ui_state();
    void save_combat_history_preference();
    void save_activity_history_preference();
    void select_spawn(std::uint32_t id);
    void clear_spawn_selection();
    void update_spawn_detail(const SpawnSnapshot* spawn);
    void render_combat_encounter(const CombatEncounterSnapshot& snapshot);
    void render_inventory_reconciliation(
        const InventoryReconciliationSnapshot& snapshot);

    StatusSnapshot snapshot_;
    UiSettings settings_;
    bool reset_layout_;
    QString client_directory_;
    MapCanvas* map_canvas_{nullptr};
    SpawnTableModel* spawn_model_{nullptr};
    SpawnFilterProxyModel* spawn_proxy_{nullptr};
    QTableView* spawn_table_{nullptr};
    QLineEdit* spawn_filter_{nullptr};
    QComboBox* spawn_type_filter_{nullptr};
    QLabel* spawn_state_{nullptr};
    QLabel* selection_detail_{nullptr};
    QLabel* character_name_{nullptr};
    QProgressBar* health_bar_{nullptr};
    QProgressBar* mana_bar_{nullptr};
    QLabel* current_dps_{nullptr};
    QTableWidget* equipment_table_{nullptr};
    QAction* export_inventory_action_{nullptr};
    QAction* import_inventory_action_{nullptr};
    QAction* retain_combat_history_action_{nullptr};
    QAction* retain_activity_history_action_{nullptr};
    QAction* export_activity_action_{nullptr};
    QAction* delete_activity_action_{nullptr};
    QLabel* parse_state_{nullptr};
    QTableWidget* parse_table_{nullptr};
    QLabel* combat_overview_{nullptr};
    QTableWidget* combat_history_{nullptr};
    QTableWidget* combat_healing_{nullptr};
    QTableWidget* combat_abilities_{nullptr};
    QTableWidget* combat_timeline_{nullptr};
    QLabel* activity_overview_{nullptr};
    QTableWidget* activity_events_{nullptr};
    QTableWidget* activity_abilities_{nullptr};
    QLabel* inventory_state_{nullptr};
    QTableWidget* inventory_reconciliation_{nullptr};
    CombatAnalyticsSnapshot combat_analytics_;
    ActivityAnalyticsSnapshot activity_analytics_;
    InventoryReconciliationSnapshot inventory_snapshot_;
    QString inventory_path_;
    std::string inventory_character_name_;
    std::function<void(std::string)> delete_activity_callback_;
    std::optional<std::int64_t> selected_combat_history_;
    bool updating_combat_history_{false};
    QToolButton* maximize_button_{nullptr};
    CharacterSnapshot character_snapshot_;
    std::optional<std::uint32_t> selected_spawn_;
    QLabel* compatibility_value_{nullptr};
    QLabel* process_value_{nullptr};
    QLabel* profile_value_{nullptr};
    QLabel* detail_value_{nullptr};
};

}  // namespace plazmic
