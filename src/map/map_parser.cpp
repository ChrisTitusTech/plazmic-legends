#include "map/map_parser.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <fstream>
#include <numeric>
#include <ranges>
#include <system_error>
#include <utility>

namespace plazmic {
namespace {

struct LayerResult {
    std::optional<MapLayer> layer;
    MapLoadError error{MapLoadError::none};
    std::string detail;

    [[nodiscard]] explicit operator bool() const {
        return layer.has_value() && error == MapLoadError::none;
    }
};

std::string_view trim(std::string_view value) {
    while (!value.empty() &&
           (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }
    while (!value.empty() &&
           (value.back() == ' ' || value.back() == '\t')) {
        value.remove_suffix(1);
    }
    return value;
}

template <std::size_t Count>
std::optional<std::array<std::string_view, Count>> split_fields(
    std::string_view value) {
    std::array<std::string_view, Count> fields;
    for (std::size_t index = 0; index + 1 < Count; ++index) {
        const std::size_t comma = value.find(',');
        if (comma == std::string_view::npos) {
            return std::nullopt;
        }
        fields[index] = trim(value.substr(0, comma));
        value.remove_prefix(comma + 1);
    }
    fields.back() = trim(value);
    return fields;
}

template <typename Number>
std::optional<Number> parse_number(std::string_view value) {
    value = trim(value);
    if (value.empty()) {
        return std::nullopt;
    }
    Number result{};
    const auto parsed = std::from_chars(
        value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} ||
        parsed.ptr != value.data() + value.size()) {
        return std::nullopt;
    }
    return result;
}

bool valid_coordinate(double value, const MapParserLimits& limits) {
    return std::isfinite(value) &&
           std::abs(value) <= limits.maximum_absolute_coordinate;
}

std::optional<MapColor> parse_color(std::string_view red,
                                    std::string_view green,
                                    std::string_view blue) {
    const auto red_value = parse_number<int>(red);
    const auto green_value = parse_number<int>(green);
    const auto blue_value = parse_number<int>(blue);
    if (!red_value || !green_value || !blue_value ||
        *red_value < 0 || *red_value > 255 || *green_value < 0 ||
        *green_value > 255 || *blue_value < 0 || *blue_value > 255) {
        return std::nullopt;
    }
    return MapColor{
        .red = static_cast<std::uint8_t>(*red_value),
        .green = static_cast<std::uint8_t>(*green_value),
        .blue = static_cast<std::uint8_t>(*blue_value),
    };
}

bool valid_utf8(std::string_view value) {
    std::size_t index = 0;
    while (index < value.size()) {
        const auto first =
            static_cast<unsigned char>(value[index]);
        if (first <= 0x7fU) {
            if (first < 0x20U || first == 0x7fU) {
                return false;
            }
            ++index;
            continue;
        }

        std::size_t continuation_count = 0;
        std::uint32_t code_point = 0;
        if ((first & 0xe0U) == 0xc0U) {
            continuation_count = 1;
            code_point = first & 0x1fU;
        } else if ((first & 0xf0U) == 0xe0U) {
            continuation_count = 2;
            code_point = first & 0x0fU;
        } else if ((first & 0xf8U) == 0xf0U) {
            continuation_count = 3;
            code_point = first & 0x07U;
        } else {
            return false;
        }
        if (index + continuation_count >= value.size()) {
            return false;
        }
        for (std::size_t offset = 1; offset <= continuation_count;
             ++offset) {
            const auto next =
                static_cast<unsigned char>(value[index + offset]);
            if ((next & 0xc0U) != 0x80U) {
                return false;
            }
            code_point = (code_point << 6U) | (next & 0x3fU);
        }
        const bool overlong =
            (continuation_count == 1 && code_point < 0x80U) ||
            (continuation_count == 2 && code_point < 0x800U) ||
            (continuation_count == 3 && code_point < 0x10000U);
        if (overlong || code_point > 0x10ffffU ||
            (code_point >= 0xd800U && code_point <= 0xdfffU)) {
            return false;
        }
        index += continuation_count + 1;
    }
    return true;
}

std::optional<MapPosition> parse_position(
    std::string_view x,
    std::string_view y,
    std::string_view z,
    const MapParserLimits& limits) {
    const auto x_value = parse_number<double>(x);
    const auto y_value = parse_number<double>(y);
    const auto z_value = parse_number<double>(z);
    if (!x_value || !y_value || !z_value ||
        !valid_coordinate(*x_value, limits) ||
        !valid_coordinate(*y_value, limits) ||
        !valid_coordinate(*z_value, limits)) {
        return std::nullopt;
    }
    return MapPosition{
        .x = *x_value,
        .y = *y_value,
        .z = *z_value,
    };
}

LayerResult failure(MapLoadError error,
                    unsigned int layer,
                    std::size_t line,
                    std::string_view reason) {
    std::string detail =
        "Layer " + std::to_string(layer);
    if (line != 0) {
        detail += ", line " + std::to_string(line);
    }
    detail += ": ";
    detail += reason;
    return {
        .layer = std::nullopt,
        .error = error,
        .detail = std::move(detail),
    };
}

std::optional<MapLoadError> parse_line_record(
    std::string_view payload,
    const MapParserLimits& limits,
    MapLayer& layer) {
    const auto fields = split_fields<9>(payload);
    if (!fields) {
        return MapLoadError::malformed_record;
    }
    const auto start = parse_position(
        (*fields)[0], (*fields)[1], (*fields)[2], limits);
    const auto end = parse_position(
        (*fields)[3], (*fields)[4], (*fields)[5], limits);
    const auto color =
        parse_color((*fields)[6], (*fields)[7], (*fields)[8]);
    if (!start || !end || !color) {
        return MapLoadError::value_out_of_range;
    }
    layer.lines.push_back({
        .start = *start,
        .end = *end,
        .color = *color,
    });
    return std::nullopt;
}

std::optional<MapLoadError> parse_label_record(
    std::string_view payload,
    const MapParserLimits& limits,
    MapLayer& layer) {
    const auto fields = split_fields<8>(payload);
    if (!fields) {
        return MapLoadError::malformed_record;
    }
    const auto position = parse_position(
        (*fields)[0], (*fields)[1], (*fields)[2], limits);
    const auto color =
        parse_color((*fields)[3], (*fields)[4], (*fields)[5]);
    const auto size = parse_number<int>((*fields)[6]);
    const std::string_view label = (*fields)[7];
    if (!position || !color || !size || *size < 0 || *size > 255) {
        return MapLoadError::value_out_of_range;
    }
    if (label.size() > limits.maximum_label_bytes || !valid_utf8(label)) {
        return MapLoadError::malformed_record;
    }

    std::string text(label);
    std::ranges::replace(text, '_', ' ');
    layer.labels.push_back({
        .position = *position,
        .color = *color,
        .size = *size,
        .text = std::move(text),
    });
    return std::nullopt;
}

LayerResult parse_layer(const std::filesystem::path& path,
                        unsigned int index,
                        const MapParserLimits& limits) {
    std::error_code error;
    const std::uintmax_t file_bytes =
        std::filesystem::file_size(path, error);
    if (error) {
        return failure(
            MapLoadError::file_unavailable, index, 0,
            "cannot inspect map file");
    }
    if (file_bytes > limits.maximum_file_bytes) {
        return failure(
            MapLoadError::file_too_large, index, 0,
            "map file exceeds the byte limit");
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return failure(
            MapLoadError::file_unavailable, index, 0,
            "cannot open map file");
    }
    MapLayer layer{
        .index = index,
        .source = path,
        .lines = {},
        .labels = {},
    };

    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.size() > limits.maximum_line_bytes) {
            return failure(
                MapLoadError::line_too_long, index, line_number,
                "map record exceeds the line limit");
        }
        const std::string_view record = trim(line);
        if (record.empty()) {
            continue;
        }
        if (layer.record_count() >= limits.maximum_records_per_layer) {
            return failure(
                MapLoadError::too_many_records, index, line_number,
                "map layer exceeds the record limit");
        }

        std::optional<MapLoadError> parse_error;
        if (record.starts_with("L ")) {
            parse_error =
                parse_line_record(record.substr(2), limits, layer);
        } else if (record.starts_with("P ")) {
            parse_error =
                parse_label_record(record.substr(2), limits, layer);
        } else {
            parse_error = MapLoadError::malformed_record;
        }
        if (parse_error) {
            const std::string_view reason =
                *parse_error == MapLoadError::value_out_of_range
                    ? "map value is invalid or out of range"
                    : "map record is malformed";
            return failure(*parse_error, index, line_number, reason);
        }
    }
    if (input.bad()) {
        return failure(
            MapLoadError::file_unavailable, index, line_number,
            "map file read failed");
    }
    return {
        .layer = std::move(layer),
        .error = MapLoadError::none,
        .detail = {},
    };
}

bool contained_by(const std::filesystem::path& root,
                  const std::filesystem::path& candidate) {
    const std::filesystem::path relative =
        candidate.lexically_relative(root);
    if (relative.empty() || relative.is_absolute()) {
        return false;
    }
    return *relative.begin() != "..";
}

}  // namespace

std::size_t ZoneMap::record_count() const {
    return std::accumulate(
        layers.begin(), layers.end(), std::size_t{0},
        [](std::size_t total, const MapLayer& layer) {
            return total + layer.record_count();
        });
}

bool valid_zone_short_name(std::string_view zone) {
    if (zone.empty() || zone.size() > 64U) {
        return false;
    }
    return std::ranges::all_of(zone, [](char character) {
        const auto byte = static_cast<unsigned char>(character);
        return (byte >= 'A' && byte <= 'Z') ||
               (byte >= 'a' && byte <= 'z') ||
               (byte >= '0' && byte <= '9') || byte == '_' || byte == '-';
    });
}

MapLoadResult load_zone_map(const std::filesystem::path& map_root,
                            std::string_view zone,
                            const MapParserLimits& limits) {
    if (!valid_zone_short_name(zone)) {
        return {
            .map = std::nullopt,
            .error = MapLoadError::invalid_zone,
            .detail = "Zone short name is invalid",
        };
    }

    std::error_code error;
    const std::filesystem::path root =
        std::filesystem::canonical(map_root, error);
    const bool root_is_directory =
        !error && std::filesystem::is_directory(root, error);
    if (error || !root_is_directory) {
        return {
            .map = std::nullopt,
            .error = MapLoadError::map_root_unavailable,
            .detail = "Map root is unavailable",
        };
    }

    ZoneMap map{
        .zone = std::string(zone),
        .layers = {},
    };
    for (unsigned int layer_index = 0;
         layer_index <= limits.maximum_layer;
         ++layer_index) {
        const std::string filename =
            std::string(zone) +
            (layer_index == 0
                 ? ".txt"
                 : "_" + std::to_string(layer_index) + ".txt");
        const std::filesystem::path requested = root / filename;
        const bool exists = std::filesystem::exists(requested, error);
        if (error) {
            return {
                .map = std::nullopt,
                .error = MapLoadError::file_unavailable,
                .detail = "Cannot inspect a map layer",
            };
        }
        if (!exists) {
            if (layer_index == 0) {
                return {
                    .map = std::nullopt,
                    .error = MapLoadError::missing_base_map,
                    .detail = "Base map file is missing",
                };
            }
            continue;
        }

        const std::filesystem::path resolved =
            std::filesystem::canonical(requested, error);
        if (error || !contained_by(root, resolved)) {
            return {
                .map = std::nullopt,
                .error = MapLoadError::path_escape,
                .detail = "Map layer resolves outside the map root",
            };
        }
        LayerResult layer = parse_layer(resolved, layer_index, limits);
        if (!layer) {
            return {
                .map = std::nullopt,
                .error = layer.error,
                .detail = std::move(layer.detail),
            };
        }
        map.layers.push_back(std::move(*layer.layer));
    }
    return {
        .map = std::move(map),
        .error = MapLoadError::none,
        .detail = {},
    };
}

}  // namespace plazmic
