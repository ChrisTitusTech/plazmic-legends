#pragma once

#include "model/status_snapshot.h"
#include "model/player_snapshot.h"
#include "model/spawn_snapshot.h"
#include "ui/ui_settings.h"

#include <cstdint>
#include <optional>

#include <QMainWindow>

class QComboBox;
class QLabel;
class QLineEdit;
class QTableView;

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
                        QWidget* parent = nullptr);

    void update_snapshot(const StatusSnapshot& snapshot);
    void update_player_snapshot(const PlayerSnapshot& snapshot);
    void update_spawn_snapshot(SpawnCollectionSnapshot snapshot);
    void set_zone_map(ZoneMap map);
    void clear_zone_map(const QString& detail);
    [[nodiscard]] const StatusSnapshot& snapshot() const { return snapshot_; }
    [[nodiscard]] MapCanvas* map_canvas() const { return map_canvas_; }

  protected:
    void closeEvent(QCloseEvent* event) override;

  private:
    void build_ui();
    void restore_ui_state();
    void ensure_on_screen();
    void save_ui_state();
    void select_spawn(std::uint32_t id);
    void clear_spawn_selection();
    void update_spawn_detail(const SpawnSnapshot* spawn);

    StatusSnapshot snapshot_;
    UiSettings settings_;
    bool reset_layout_;
    MapCanvas* map_canvas_{nullptr};
    SpawnTableModel* spawn_model_{nullptr};
    SpawnFilterProxyModel* spawn_proxy_{nullptr};
    QTableView* spawn_table_{nullptr};
    QLineEdit* spawn_filter_{nullptr};
    QComboBox* spawn_type_filter_{nullptr};
    QLabel* spawn_state_{nullptr};
    QLabel* selection_detail_{nullptr};
    std::optional<std::uint32_t> selected_spawn_;
    QLabel* compatibility_value_{nullptr};
    QLabel* process_value_{nullptr};
    QLabel* profile_value_{nullptr};
    QLabel* detail_value_{nullptr};
};

}  // namespace plazmic
