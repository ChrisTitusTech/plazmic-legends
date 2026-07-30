#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace plazmic {

struct MapPosition {
    double x;
    double y;
    double z;
};

struct MapColor {
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
};

struct MapLineRecord {
    MapPosition start;
    MapPosition end;
    MapColor color;
};

struct MapLabelRecord {
    MapPosition position;
    MapColor color;
    int size;
    std::string text;
};

struct MapLayer {
    unsigned int index;
    std::filesystem::path source;
    std::vector<MapLineRecord> lines;
    std::vector<MapLabelRecord> labels;

    [[nodiscard]] std::size_t record_count() const {
        return lines.size() + labels.size();
    }
};

struct ZoneMap {
    std::string zone;
    std::vector<MapLayer> layers;

    [[nodiscard]] std::size_t record_count() const;
};

struct MapParserLimits {
    std::uintmax_t maximum_file_bytes{8U * 1024U * 1024U};
    std::size_t maximum_line_bytes{4096U};
    std::size_t maximum_records_per_layer{100000U};
    std::size_t maximum_label_bytes{512U};
    double maximum_absolute_coordinate{1000000.0};
    unsigned int maximum_layer{9U};
};

enum class MapLoadError {
    none,
    invalid_zone,
    map_root_unavailable,
    missing_base_map,
    path_escape,
    file_unavailable,
    file_too_large,
    line_too_long,
    too_many_records,
    malformed_record,
    value_out_of_range,
};

struct MapLoadResult {
    std::optional<ZoneMap> map;
    MapLoadError error{MapLoadError::none};
    std::string detail;

    [[nodiscard]] explicit operator bool() const {
        return map.has_value() && error == MapLoadError::none;
    }
};

[[nodiscard]] bool valid_zone_short_name(std::string_view zone);
[[nodiscard]] MapLoadResult load_zone_map(
    const std::filesystem::path& map_root,
    std::string_view zone,
    const MapParserLimits& limits = {});

}  // namespace plazmic
