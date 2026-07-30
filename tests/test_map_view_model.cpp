#include "map/map_view_model.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool close_to(double actual, double expected, double tolerance = 0.001) {
    return std::abs(actual - expected) <= tolerance;
}

}  // namespace

int main() {
    try {
        const plazmic::ZoneMap map{
            .zone = "synthetic",
            .layers =
                {
                    {
                        .index = 0,
                        .source = "synthetic.txt",
                        .lines =
                            {
                                {
                                    .start = {-100.0, -50.0, 0.0},
                                    .end = {300.0, 150.0, 0.0},
                                    .color = {255, 255, 255},
                                },
                            },
                        .labels =
                            {
                                {
                                    .position = {5000.0, 6000.0, 0.0},
                                    .color = {255, 255, 255},
                                    .size = 3,
                                    .text = "Off-map legend",
                                },
                            },
                    },
                },
        };
        const auto bounds = plazmic::calculate_map_bounds(map);
        require(bounds.has_value(), "line geometry produced no bounds");
        require(close_to(bounds->minimum_x, -100.0),
                "minimum X is incorrect");
        require(close_to(bounds->maximum_x, 300.0),
                "maximum X is incorrect");
        require(close_to(bounds->minimum_y, -50.0),
                "minimum Y is incorrect");
        require(close_to(bounds->maximum_y, 150.0),
                "maximum Y is incorrect");
        require(bounds->maximum_x < 5000.0 &&
                    bounds->maximum_y < 6000.0,
                "off-map label expanded line-geometry fit bounds");

        const plazmic::ZoneMap label_map{
            .zone = "labels",
            .layers =
                {
                    {
                        .index = 0,
                        .source = "labels.txt",
                        .lines = {},
                        .labels =
                            {
                                {
                                    .position = {-25.0, 40.0, 0.0},
                                    .color = {255, 255, 255},
                                    .size = 3,
                                    .text = "Synthetic",
                                },
                            },
                    },
                },
        };
        const auto label_bounds =
            plazmic::calculate_map_bounds(label_map);
        require(label_bounds.has_value(),
                "label-only geometry produced no bounds");
        require(close_to(label_bounds->minimum_x, -25.5) &&
                    close_to(label_bounds->maximum_x, -24.5) &&
                    close_to(label_bounds->minimum_y, 39.5) &&
                    close_to(label_bounds->maximum_y, 40.5),
                "label-only bounds were not expanded");

        plazmic::MapViewport viewport;
        viewport.fit(*bounds, 1000.0, 500.0);
        const plazmic::MapPoint2D center =
            viewport.map_to_screen({100.0, 50.0});
        require(close_to(center.x, 500.0) && close_to(center.y, 250.0),
                "map center did not map to the viewport center");
        const plazmic::MapPoint2D upper =
            viewport.map_to_screen({100.0, -50.0});
        const plazmic::MapPoint2D lower =
            viewport.map_to_screen({100.0, 150.0});
        require(upper.y < center.y && lower.y > center.y,
                "native map Y orientation was flipped");

        const plazmic::MapPoint2D source{42.0, 73.0};
        const plazmic::MapPoint2D round_trip =
            viewport.screen_to_map(viewport.map_to_screen(source));
        require(close_to(round_trip.x, source.x) &&
                    close_to(round_trip.y, source.y),
                "map transform did not round trip");

        const double initial_scale = viewport.scale();
        viewport.zoom_at(2.0, center.x, center.y);
        require(close_to(viewport.scale(), initial_scale * 2.0),
                "zoom did not update scale");
        const plazmic::MapPoint2D zoom_anchor =
            viewport.map_to_screen({100.0, 50.0});
        require(close_to(zoom_anchor.x, center.x) &&
                    close_to(zoom_anchor.y, center.y),
                "zoom did not preserve its screen anchor");

        viewport.resize(1200.0, 700.0);
        const plazmic::MapPoint2D resized_center =
            viewport.map_to_screen({100.0, 50.0});
        require(close_to(resized_center.x, 600.0) &&
                    close_to(resized_center.y, 350.0),
                "viewport resize retained stale dimensions");
        viewport.center_on({42.0, 73.0});
        const plazmic::MapPoint2D recentered =
            viewport.map_to_screen({42.0, 73.0});
        require(close_to(recentered.x, 600.0) &&
                    close_to(recentered.y, 350.0),
                "player-follow center did not reach the viewport center");

        const plazmic::PlayerSnapshot player{
            .state = plazmic::PlayerSnapshotState::in_world,
            .zone = "synthetic",
            .x = 25.0,
            .y = -75.0,
            .z = 3.0,
            .heading_degrees = 90.0,
            .detail = {},
        };
        const plazmic::MapPoint2D marker =
            plazmic::player_map_position(player);
        require(close_to(marker.x, 75.0) && close_to(marker.y, -25.0),
                "EQ player coordinates did not convert to map coordinates");
        require(close_to(
                    plazmic::player_map_heading_degrees(player), 270.0),
                "EQ player heading did not convert to map heading");
        require(plazmic::map_height_range_visible(
                    -5.0, 5.0, 0.0, 10.0, 20.0),
                "height filter hid geometry inside its band");
        require(plazmic::map_height_range_visible(
                    -20.0, 5.0, 0.0, 10.0, 20.0),
                "height filter hid geometry crossing its band");
        require(!plazmic::map_height_range_visible(
                    -20.0, -11.0, 0.0, 10.0, 20.0),
                "height filter retained geometry below its band");
        require(!plazmic::map_height_range_visible(
                    21.0, 30.0, 0.0, 10.0, 20.0),
                "height filter retained geometry above its band");
        require(plazmic::map_height_range_visible(
                    117.0, 118.0, 100.0, 10.0, 20.0),
                "height filter was not centered on player Z");
        require(!plazmic::map_height_range_visible(
                    88.0, 89.0, 100.0, 10.0, 20.0),
                "player-Z filter retained geometry below its lower bound");
        require(plazmic::map_height_range_visible(
                    30.0, -20.0, 0.0, 10.0, 20.0),
                "height filter rejected reversed segment endpoints");
        require(!plazmic::map_height_range_visible(
                    0.0, 1.0, 0.0, -1.0, 10.0),
                "height filter accepted a negative range");

        std::cout << "map bounds, native orientation, transforms, player "
                     "conversion, and height filtering passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
