#pragma once

#include "model/status_snapshot.h"
#include "ui/ui_settings.h"

#include <QMainWindow>

class QLabel;

namespace plazmic {

class MainWindow final : public QMainWindow {
  public:
    explicit MainWindow(StatusSnapshot snapshot,
                        QString settings_path = UiSettings::default_path(),
                        bool reset_layout = false,
                        QWidget* parent = nullptr);

    void update_snapshot(const StatusSnapshot& snapshot);
    [[nodiscard]] const StatusSnapshot& snapshot() const { return snapshot_; }

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
    QLabel* compatibility_value_{nullptr};
    QLabel* process_value_{nullptr};
    QLabel* profile_value_{nullptr};
    QLabel* detail_value_{nullptr};
};

}  // namespace plazmic
