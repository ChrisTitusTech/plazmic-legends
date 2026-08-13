#pragma once

#include <array>
#include <optional>

#include <QByteArray>
#include <QString>

namespace plazmic {

struct UiState {
    QString client_directory;
    QByteArray geometry;
    QByteArray layout;
    bool height_filter_enabled{true};
    double height_filter_below{15.0};
    double height_filter_above{15.0};
    bool player_follow_enabled{false};
    bool named_spawn_labels_visible{false};
    bool player_labels_visible{false};
    bool npc_labels_visible{false};
    bool named_spawns_visible{true};
    bool player_spawns_visible{true};
    bool npc_spawns_visible{true};
    bool other_spawns_visible{true};
    bool combat_history_enabled{false};
    bool activity_history_enabled{false};
    std::array<int, 4> activity_summary_widths{100, 240, 300, 260};
    QString spawn_filter;
    int spawn_type_filter{-1};
    int spawn_sort_column{3};
    bool spawn_sort_descending{false};
    std::array<int, 4> spawn_column_widths{220, 70, 90, 100};
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
