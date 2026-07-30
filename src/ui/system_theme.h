#pragma once

#include <cstdint>
#include <optional>

#include <QObject>
#include <QPalette>
#include <QProcessEnvironment>
#include <QString>
#include <QTimer>

class QApplication;

namespace plazmic {

enum class SystemColorMode {
    light,
    dark,
};

[[nodiscard]] std::optional<SystemColorMode> color_mode_from_portal(
    std::uint32_t value);
[[nodiscard]] QString default_dwm_theme_path();
[[nodiscard]] QString dwm_theme_path_for_session(
    const QProcessEnvironment& environment);
[[nodiscard]] std::optional<SystemColorMode> read_dwm_color_mode(
    const QString& path);
[[nodiscard]] QPalette palette_for_color_mode(SystemColorMode mode);
void apply_color_mode(QApplication& application, SystemColorMode mode);

class SystemTheme final : public QObject {
  public:
    explicit SystemTheme(
        QApplication& application,
        QString dwm_theme_path = {});

    void start();
    void refresh();
    [[nodiscard]] std::optional<SystemColorMode> mode() const {
        return applied_mode_;
    }

  private:
    [[nodiscard]] std::optional<SystemColorMode> detect() const;

    QApplication& application_;
    QString dwm_theme_path_;
    QTimer refresh_timer_;
    std::optional<SystemColorMode> applied_mode_;
};

}  // namespace plazmic
