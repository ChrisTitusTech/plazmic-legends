#include "map/map_parser.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include <unistd.h>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        std::string pattern =
            (std::filesystem::temp_directory_path() /
             "plazmic-map-test-XXXXXX")
                .string();
        char* result = mkdtemp(pattern.data());
        require(result != nullptr, "cannot create map test directory");
        path_ = result;
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

  private:
    std::filesystem::path path_;
};

void write_file(const std::filesystem::path& path,
                const std::string& contents) {
    std::ofstream output(path, std::ios::binary);
    require(output.good(), "cannot open synthetic map fixture");
    output << contents;
    require(output.good(), "cannot write synthetic map fixture");
}

void require_error(const plazmic::MapLoadResult& result,
                   plazmic::MapLoadError expected,
                   const std::string& message) {
    require(!result, message + " unexpectedly succeeded");
    require(result.error == expected, message + " returned the wrong error");
    require(!result.detail.empty(), message + " omitted its diagnostic");
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 3) {
            const auto result =
                plazmic::load_zone_map(argv[1], argv[2]);
            if (!result) {
                std::cerr << "map check failed: " << result.detail << '\n';
                return EXIT_FAILURE;
            }
            std::cout << "zone=" << result.map->zone
                      << " layers=" << result.map->layers.size()
                      << " records=" << result.map->record_count() << '\n';
            return EXIT_SUCCESS;
        }
        require(argc == 1, "usage: map_parser_test [MAP_ROOT ZONE]");

        require(plazmic::valid_zone_short_name("synthetic_zone-1"),
                "valid zone name was rejected");
        require(!plazmic::valid_zone_short_name(""),
                "empty zone name was accepted");
        require(!plazmic::valid_zone_short_name("../synthetic"),
                "relative path escape was accepted");
        require(!plazmic::valid_zone_short_name("/tmp/synthetic"),
                "absolute path was accepted");
        require(!plazmic::valid_zone_short_name("synthetic.txt"),
                "zone name with extension was accepted");
        require(!plazmic::valid_zone_short_name(std::string(65, 'a')),
                "oversized zone name was accepted");

        const TemporaryDirectory directory;
        write_file(
            directory.path() / "synthetic.txt",
            "L 0, 1, 2, 10, 11, 12, 255, 128, 0\r\n"
            "P 5, 6, 7, 10, 20, 30, 3, Synthetic_Label\n");
        write_file(
            directory.path() / "synthetic_1.txt",
            "L -1.5, -2.5, -3.5, 4.5, 5.5, 6.5, 1, 2, 3\n");

        const auto parsed =
            plazmic::load_zone_map(directory.path(), "synthetic");
        require(
            static_cast<bool>(parsed), "valid synthetic map was rejected");
        require(parsed.map->zone == "synthetic", "zone name was not retained");
        require(parsed.map->layers.size() == 2,
                "numbered map layer was not loaded");
        require(parsed.map->record_count() == 3,
                "map record count is incorrect");
        require(parsed.map->layers[0].lines.size() == 1,
                "base line record was not parsed");
        require(parsed.map->layers[0].labels.size() == 1,
                "base label record was not parsed");
        require(parsed.map->layers[0].labels[0].text == "Synthetic Label",
                "label separators were not normalized");
        require(parsed.map->layers[1].index == 1,
                "numbered layer index is incorrect");

        write_file(
            directory.path() / "emptylabel.txt",
            "P 0, 0, 0, 0, 0, 0, 3,  \n");
        const auto empty_label =
            plazmic::load_zone_map(directory.path(), "emptylabel");
        require(static_cast<bool>(empty_label),
                "empty label disabled an otherwise valid map");
        require(empty_label.map->layers[0].labels[0].text.empty(),
                "empty label was not retained safely");

        require_error(
            plazmic::load_zone_map(directory.path(), "missing"),
            plazmic::MapLoadError::missing_base_map,
            "missing base map");
        require_error(
            plazmic::load_zone_map(
                directory.path() / "missing-root", "synthetic"),
            plazmic::MapLoadError::map_root_unavailable,
            "missing map root");
        require_error(
            plazmic::load_zone_map(directory.path(), "../synthetic"),
            plazmic::MapLoadError::invalid_zone,
            "path escape");

        write_file(directory.path() / "malformed.txt", "L 1, 2, 3\n");
        require_error(
            plazmic::load_zone_map(directory.path(), "malformed"),
            plazmic::MapLoadError::malformed_record,
            "malformed record");

        write_file(
            directory.path() / "range.txt",
            "L 0, 0, 0, 1000001, 0, 0, 0, 0, 0\n");
        require_error(
            plazmic::load_zone_map(directory.path(), "range"),
            plazmic::MapLoadError::value_out_of_range,
            "out-of-range coordinate");

        write_file(
            directory.path() / "nonfinite.txt",
            "L 0, 0, 0, nan, 0, 0, 0, 0, 0\n");
        require_error(
            plazmic::load_zone_map(directory.path(), "nonfinite"),
            plazmic::MapLoadError::value_out_of_range,
            "non-finite coordinate");

        write_file(
            directory.path() / "color.txt",
            "P 0, 0, 0, 256, 0, 0, 3, Invalid_Color\n");
        require_error(
            plazmic::load_zone_map(directory.path(), "color"),
            plazmic::MapLoadError::value_out_of_range,
            "out-of-range color");

        write_file(
            directory.path() / "size.txt",
            "P 0, 0, 0, 0, 0, 0, 256, Invalid_Size\n");
        require_error(
            plazmic::load_zone_map(directory.path(), "size"),
            plazmic::MapLoadError::value_out_of_range,
            "out-of-range label size");

        std::error_code directory_error;
        std::filesystem::create_directory(
            directory.path() / "unavailable.txt", directory_error);
        require(!directory_error, "cannot create unavailable-file fixture");
        require_error(
            plazmic::load_zone_map(directory.path(), "unavailable"),
            plazmic::MapLoadError::file_unavailable,
            "unavailable map file");

        write_file(
            directory.path() / "longline.txt",
            "P 0, 0, 0, 0, 0, 0, 3, " + std::string(128, 'a') + "\n");
        plazmic::MapParserLimits short_line_limits;
        short_line_limits.maximum_line_bytes = 64;
        require_error(
            plazmic::load_zone_map(
                directory.path(), "longline", short_line_limits),
            plazmic::MapLoadError::line_too_long,
            "oversized line");

        write_file(
            directory.path() / "longlabel.txt",
            "P 0, 0, 0, 0, 0, 0, 3, Synthetic_Label\n");
        plazmic::MapParserLimits short_label_limits;
        short_label_limits.maximum_label_bytes = 8;
        require_error(
            plazmic::load_zone_map(
                directory.path(), "longlabel", short_label_limits),
            plazmic::MapLoadError::malformed_record,
            "oversized label");

        write_file(
            directory.path() / "encoding.txt",
            std::string("P 0, 0, 0, 0, 0, 0, 3, ") +
                static_cast<char>(0xc0) + static_cast<char>(0xaf) + "\n");
        require_error(
            plazmic::load_zone_map(directory.path(), "encoding"),
            plazmic::MapLoadError::malformed_record,
            "invalid UTF-8 label");

        plazmic::MapParserLimits one_record_limits;
        one_record_limits.maximum_records_per_layer = 1;
        require_error(
            plazmic::load_zone_map(
                directory.path(), "synthetic", one_record_limits),
            plazmic::MapLoadError::too_many_records,
            "excessive record count");

        plazmic::MapParserLimits tiny_file_limits;
        tiny_file_limits.maximum_file_bytes = 8;
        require_error(
            plazmic::load_zone_map(
                directory.path(), "synthetic", tiny_file_limits),
            plazmic::MapLoadError::file_too_large,
            "oversized map file");

        const TemporaryDirectory outside;
        write_file(
            outside.path() / "outside.txt",
            "L 0, 0, 0, 1, 1, 1, 0, 0, 0\n");
        std::error_code symlink_error;
        std::filesystem::create_symlink(
            outside.path() / "outside.txt",
            directory.path() / "escape.txt",
            symlink_error);
        require(!symlink_error, "cannot create path-escape fixture");
        require_error(
            plazmic::load_zone_map(directory.path(), "escape"),
            plazmic::MapLoadError::path_escape,
            "symlink path escape");

        std::cout << "bounded synthetic map parser passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
