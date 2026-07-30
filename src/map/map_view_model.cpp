#include "map/map_view_model.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace plazmic {

std::optional<MapBounds> calculate_map_bounds(const ZoneMap& map) {
    MapBounds bounds{
        .minimum_x = std::numeric_limits<double>::max(),
        .maximum_x = std::numeric_limits<double>::lowest(),
        .minimum_y = std::numeric_limits<double>::max(),
        .maximum_y = std::numeric_limits<double>::lowest(),
    };
    bool found = false;
    for (const MapLayer& layer : map.layers) {
        for (const MapLineRecord& line : layer.lines) {
            bounds.minimum_x =
                std::min({bounds.minimum_x, line.start.x, line.end.x});
            bounds.maximum_x =
                std::max({bounds.maximum_x, line.start.x, line.end.x});
            bounds.minimum_y =
                std::min({bounds.minimum_y, line.start.y, line.end.y});
            bounds.maximum_y =
                std::max({bounds.maximum_y, line.start.y, line.end.y});
            found = true;
        }
        for (const MapLabelRecord& label : layer.labels) {
            bounds.minimum_x =
                std::min(bounds.minimum_x, label.position.x);
            bounds.maximum_x =
                std::max(bounds.maximum_x, label.position.x);
            bounds.minimum_y =
                std::min(bounds.minimum_y, label.position.y);
            bounds.maximum_y =
                std::max(bounds.maximum_y, label.position.y);
            found = true;
        }
    }
    if (!found) {
        return std::nullopt;
    }
    if (bounds.width() < 1.0) {
        bounds.minimum_x -= 0.5;
        bounds.maximum_x += 0.5;
    }
    if (bounds.height() < 1.0) {
        bounds.minimum_y -= 0.5;
        bounds.maximum_y += 0.5;
    }
    return bounds;
}

MapPoint2D player_map_position(const PlayerSnapshot& player) {
    return {
        .x = -player.y,
        .y = -player.x,
    };
}

double player_map_heading_degrees(const PlayerSnapshot& player) {
    double heading = 360.0 - player.heading_degrees;
    if (heading >= 360.0) {
        heading -= 360.0;
    }
    return heading;
}

bool map_height_range_visible(double minimum_z,
                              double maximum_z,
                              double center_z,
                              double below_range,
                              double above_range) {
    if (!std::isfinite(minimum_z) || !std::isfinite(maximum_z) ||
        !std::isfinite(center_z) || !std::isfinite(below_range) ||
        !std::isfinite(above_range) || below_range < 0.0 ||
        above_range < 0.0) {
        return false;
    }
    if (minimum_z > maximum_z) {
        std::swap(minimum_z, maximum_z);
    }
    return maximum_z >= center_z - below_range &&
           minimum_z <= center_z + above_range;
}

void MapViewport::fit(const MapBounds& bounds,
                      double width,
                      double height) {
    width_ = std::max(width, 1.0);
    height_ = std::max(height, 1.0);
    center_x_ = (bounds.minimum_x + bounds.maximum_x) / 2.0;
    center_y_ = (bounds.minimum_y + bounds.maximum_y) / 2.0;
    constexpr double kMargin = 0.92;
    scale_ = kMargin * std::min(
        width_ / std::max(bounds.width(), 1.0),
        height_ / std::max(bounds.height(), 1.0));
    scale_ = std::clamp(scale_, 0.0001, 10000.0);
}

void MapViewport::resize(double width, double height) {
    width_ = std::max(width, 1.0);
    height_ = std::max(height, 1.0);
}

void MapViewport::center_on(MapPoint2D point) {
    center_x_ = point.x;
    center_y_ = point.y;
}

void MapViewport::pan(double screen_dx, double screen_dy) {
    center_x_ -= screen_dx / scale_;
    center_y_ -= screen_dy / scale_;
}

void MapViewport::zoom_at(double factor,
                          double screen_x,
                          double screen_y) {
    factor = std::clamp(factor, 0.25, 4.0);
    const MapPoint2D before = screen_to_map({screen_x, screen_y});
    scale_ = std::clamp(scale_ * factor, 0.0001, 10000.0);
    const MapPoint2D after = screen_to_map({screen_x, screen_y});
    center_x_ += before.x - after.x;
    center_y_ += before.y - after.y;
}

MapPoint2D MapViewport::map_to_screen(MapPoint2D point) const {
    return {
        .x = width_ / 2.0 + (point.x - center_x_) * scale_,
        .y = height_ / 2.0 + (point.y - center_y_) * scale_,
    };
}

MapPoint2D MapViewport::screen_to_map(MapPoint2D point) const {
    return {
        .x = center_x_ + (point.x - width_ / 2.0) / scale_,
        .y = center_y_ + (point.y - height_ / 2.0) / scale_,
    };
}

}  // namespace plazmic
