#pragma once

#include <optional>

#include <QByteArray>
#include <QString>

namespace plazmic {

struct UiState {
    QByteArray geometry;
    QByteArray layout;
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
