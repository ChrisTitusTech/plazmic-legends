#pragma once

#include "model/status_snapshot.h"
#include "model/player_snapshot.h"
#include "ui/ui_settings.h"

#include <QMainWindow>

class QLabel;

namespace plazmic {

class MapCanvas;
struct ZoneMap;

class MainWindow final : public QMainWindow {
  public:
    explicit MainWindow(StatusSnapshot snapshot,
                        QString settings_path = UiSettings::default_path(),
                        bool reset_layout = false,
                        QWidget* parent = nullptr);

    void update_snapshot(const StatusSnapshot& snapshot);
    void update_player_snapshot(const PlayerSnapshot& snapshot);
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

    StatusSnapshot snapshot_;
    UiSettings settings_;
    bool reset_layout_;
    MapCanvas* map_canvas_{nullptr};
    QLabel* compatibility_value_{nullptr};
    QLabel* process_value_{nullptr};
    QLabel* profile_value_{nullptr};
    QLabel* detail_value_{nullptr};
};

}  // namespace plazmic
