#pragma once

#include "map/map_parser.h"
#include "map/map_view_model.h"
#include "model/player_snapshot.h"

#include <cstddef>
#include <optional>
#include <vector>

#include <QPoint>
#include <QPixmap>
#include <QString>
#include <QWidget>

class QEvent;
class QContextMenuEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QWheelEvent;

namespace plazmic {

inline constexpr std::size_t kMaximumRenderableMapRecords = 50000U;
inline constexpr double kDefaultHeightFilterBelow = 25.0;
inline constexpr double kDefaultHeightFilterAbove = 25.0;
inline constexpr double kMaximumHeightFilterRange = 1000.0;

class MapCanvas final : public QWidget {
  public:
    explicit MapCanvas(QWidget* parent = nullptr);

    void set_zone_map(ZoneMap map);
    void clear_zone_map(QString detail);
    void set_player_snapshot(PlayerSnapshot snapshot);
    void set_layer_visible(unsigned int layer, bool visible);
    void set_height_filter_enabled(bool enabled);
    void set_height_filter_range(double below, double above);
    void set_player_follow_enabled(bool enabled);
    void reset_view();

    [[nodiscard]] const std::optional<ZoneMap>& zone_map() const {
        return map_;
    }
    [[nodiscard]] const PlayerSnapshot& player_snapshot() const {
        return player_;
    }
    [[nodiscard]] bool height_filter_enabled() const {
        return height_filter_enabled_;
    }
    [[nodiscard]] double height_filter_below() const {
        return height_filter_below_;
    }
    [[nodiscard]] double height_filter_above() const {
        return height_filter_above_;
    }
    [[nodiscard]] bool player_follow_enabled() const {
        return player_follow_enabled_;
    }

  protected:
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

  private:
    void fit_map();
    void render_map_cache();
    void refresh_height_filter_center(bool force);
    void adjust_height_filter_range();
    [[nodiscard]] bool layer_visible(unsigned int layer) const;

    std::optional<ZoneMap> map_;
    std::optional<MapBounds> bounds_;
    PlayerSnapshot player_;
    QString empty_detail_{"Waiting for live zone data"};
    MapViewport viewport_;
    std::vector<bool> visible_layers_;
    QPoint drag_origin_;
    bool dragging_{false};
    bool needs_fit_{true};
    bool map_cache_dirty_{true};
    bool height_filter_enabled_{true};
    bool player_follow_enabled_{false};
    double height_filter_below_{kDefaultHeightFilterBelow};
    double height_filter_above_{kDefaultHeightFilterAbove};
    std::optional<double> height_filter_center_;
    QPixmap map_cache_;
};

}  // namespace plazmic
