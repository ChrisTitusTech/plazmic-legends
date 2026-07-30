#pragma once

#include <optional>

#include <QByteArray>
#include <QString>

namespace plazmic {

struct UiState {
    QByteArray geometry;
    QByteArray layout;
    bool height_filter_enabled{true};
    double height_filter_below{15.0};
    double height_filter_above{15.0};
    bool player_follow_enabled{false};
};

class UiSettings {
  public:
    explicit UiSettings(QString path = default_path());

    [[nodiscard]] const QString& path() const { return path_; }
    [[nodiscard]] std::optional<UiState> load() const;
    [[nodiscard]] bool save(const UiState& state) const;

    [[nodiscard]] static QString default_path();

  private:
    QString path_;
};

}  // namespace plazmic
