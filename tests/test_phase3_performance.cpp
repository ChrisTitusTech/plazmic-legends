#include "launcher/player_lifecycle.h"
#include "map/map_parser.h"
#include "model/player_snapshot.h"
#include "ui/map_canvas.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include <QApplication>

#include <unistd.h>

namespace {

using Clock = std::chrono::steady_clock;

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
             "plazmic-performance-test-XXXXXX")
                .string();
        char* result = mkdtemp(pattern.data());
        require(result != nullptr,
                "cannot create performance test directory");
        path_ = result;
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

double milliseconds(Clock::duration duration) {
    return std::chrono::duration<double, std::milli>(duration).count();
}

plazmic::GameStateReadResult live_state(double coordinate) {
    return {
        .snapshot =
            plazmic::PlayerSnapshot{
                .state = plazmic::PlayerSnapshotState::in_world,
                .zone = "synthetic",
                .x = coordinate,
                .y = -coordinate,
                .z = coordinate / 100.0,
                .heading_degrees = coordinate,
                .detail = "Synthetic performance snapshot",
            },
        .error = plazmic::GameStateReadError::none,
        .detail = {},
    };
}

}  // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    try {
        constexpr std::size_t kMapRecords = 20000U;
        const TemporaryDirectory directory;
        const auto map_path = directory.path() / "synthetic.txt";
        std::ofstream output(map_path, std::ios::binary);
        require(output.good(),
                "cannot open synthetic performance map");
        for (std::size_t index = 0; index < kMapRecords; ++index) {
            output << "L " << index % 1000U << ", "
                   << index % 800U << ", " << index % 40U << ", "
                   << (index % 1000U) + 1U << ", "
                   << (index % 800U) + 1U << ", "
                   << index % 40U << ", 255, 255, 255\n";
        }
        output.close();
        require(output.good(),
                "cannot write synthetic performance map");

        const auto parse_start = Clock::now();
        auto loaded =
            plazmic::load_zone_map(directory.path(), "synthetic");
        const double parse_ms =
            milliseconds(Clock::now() - parse_start);
        require(static_cast<bool>(loaded),
                "synthetic performance map was rejected");
        require(loaded.map->record_count() == kMapRecords,
                "synthetic performance map record count changed");

        plazmic::PlayerLifecycle lifecycle;
        constexpr std::size_t kPublications = 100000U;
        const auto publication_start = Clock::now();
        for (std::size_t index = 0; index < kPublications; ++index) {
            const auto update = lifecycle.apply({
                .state = live_state(
                    static_cast<double>(index % 360U)),
                .map_load = std::nullopt,
            });
            require(update.player.available(),
                    "synthetic publication became unavailable");
        }
        const double publication_total_ms =
            milliseconds(Clock::now() - publication_start);
        const double publication_us =
            (publication_total_ms * 1000.0) /
            static_cast<double>(kPublications);

        plazmic::MapCanvas canvas;
        canvas.resize(1200, 780);
        canvas.show();
        const auto render_start = Clock::now();
        canvas.set_zone_map(std::move(*loaded.map));
        canvas.set_player_snapshot(
            std::move(*live_state(90.0).snapshot));
        QApplication::processEvents();
        const QPixmap initial_frame = canvas.grab();
        const double initial_render_ms =
            milliseconds(Clock::now() - render_start);
        require(!initial_frame.isNull(),
                "synthetic map frame was not rendered");

        constexpr std::size_t kUiUpdates = 50U;
        const auto ui_start = Clock::now();
        for (std::size_t index = 0; index < kUiUpdates; ++index) {
            canvas.set_player_snapshot(std::move(
                *live_state(static_cast<double>(index)).snapshot));
            QApplication::processEvents();
        }
        const double ui_total_ms =
            milliseconds(Clock::now() - ui_start);
        const double ui_average_ms =
            ui_total_ms / static_cast<double>(kUiUpdates);

        require(parse_ms < 5000.0,
                "bounded map parsing exceeded five seconds");
        require(publication_us < 1000.0,
                "snapshot publication exceeded one millisecond");
        require(initial_render_ms < 5000.0,
                "initial map render exceeded five seconds");
        require(ui_average_ms < 100.0,
                "average UI update exceeded 100 milliseconds");

        std::cout << "phase3 performance: records=" << kMapRecords
                  << " parse_ms=" << parse_ms
                  << " publication_us=" << publication_us
                  << " initial_render_ms=" << initial_render_ms
                  << " ui_average_ms=" << ui_average_ms << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
