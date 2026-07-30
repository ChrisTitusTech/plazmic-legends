#pragma once

#include "map/map_parser.h"
#include "model/player_snapshot.h"

#include <optional>

namespace plazmic {

struct MapPoint2D {
    double x;
    double y;
};

struct MapBounds {
    double minimum_x;
    double maximum_x;
    double minimum_y;
    double maximum_y;

    [[nodiscard]] double width() const {
        return maximum_x - minimum_x;
    }
    [[nodiscard]] double height() const {
        return maximum_y - minimum_y;
    }
};

[[nodiscard]] std::optional<MapBounds> calculate_map_bounds(
    const ZoneMap& map);
[[nodiscard]] MapPoint2D player_map_position(
    const PlayerSnapshot& player);
[[nodiscard]] double player_map_heading_degrees(
    const PlayerSnapshot& player);
[[nodiscard]] bool map_height_range_visible(
    double minimum_z,
    double maximum_z,
    double center_z,
    double below_range,
    double above_range);

class MapViewport {
  public:
    void fit(const MapBounds& bounds, double width, double height);
    void resize(double width, double height);
    void pan(double screen_dx, double screen_dy);
    void zoom_at(double factor, double screen_x, double screen_y);

    [[nodiscard]] MapPoint2D map_to_screen(MapPoint2D point) const;
    [[nodiscard]] MapPoint2D screen_to_map(MapPoint2D point) const;
    [[nodiscard]] double scale() const { return scale_; }

  private:
    double center_x_{};
    double center_y_{};
    double scale_{1.0};
    double width_{1.0};
    double height_{1.0};
};

}  // namespace plazmic
