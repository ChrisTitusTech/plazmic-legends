#include "game/spawn_reader.h"
#include "map/map_parser.h"
#include "model/player_snapshot.h"
#include "model/spawn_snapshot.h"
#include "ui/map_canvas.h"
#include "ui/spawn_table_model.h"

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QTableView>
#include <QTimer>

#include <unistd.h>

namespace {

using Clock = std::chrono::steady_clock;

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

double milliseconds(Clock::duration duration) {
    return std::chrono::duration<double, std::milli>(duration).count();
}

template <typename Value>
void store(std::vector<std::byte>& memory,
           std::size_t offset,
           Value value) {
    require(offset <= memory.size() - sizeof(value),
            "synthetic reader fixture write exceeds its memory");
    std::memcpy(memory.data() + offset, &value, sizeof(value));
}

struct SpawnReaderFixture {
    static constexpr std::size_t record_bytes = 0x6b5U;

    std::vector<std::byte> memory;
    std::uintptr_t base;
    plazmic::SpawnSymbols symbols{
        .next_offset = 0x08U,
        .previous_offset = 0x10U,
        .name_offset = 0xb8U,
        .name_bytes = 64U,
        .type_offset = 0x139U,
        .id_offset = 0x178U,
        .level_offset = 0x6b4U,
        .y_offset = 0x78U,
        .x_offset = 0x74U,
        .z_offset = 0x7cU,
        .record_bytes = record_bytes,
        .maximum_count = 2048U,
    };
    plazmic::ClientProcess process;
    plazmic::PlayerSnapshot player{
        .state = plazmic::PlayerSnapshotState::in_world,
        .zone = "synthetic",
        .x = 0.0,
        .y = 0.0,
        .z = 0.0,
        .heading_degrees = 0.0,
        .detail = "Synthetic player",
    };

    explicit SpawnReaderFixture(std::size_t count)
        : memory(count * record_bytes),
          base(reinterpret_cast<std::uintptr_t>(memory.data())),
          process{
              .pid = getpid(),
              .uid = getuid(),
              .command = "synthetic",
              .image_base = base,
              .client_mappings = {},
              .mappings =
                  {
                      {
                          .begin = base,
                          .end = base + memory.size(),
                          .file_offset = 0,
                          .permissions = "rw-p",
                          .path = {},
                      },
                  },
          } {
        for (std::size_t index = 0; index < count; ++index) {
            const std::size_t offset = index * record_bytes;
            const std::uintptr_t address = base + offset;
            const std::uintptr_t next =
                index + 1U < count ? address + record_bytes : 0U;
            const std::uintptr_t previous =
                index == 0U ? 0U : address - record_bytes;
            store(memory, offset, base + memory.size() + 0x1000U);
            store(memory, offset + 0x08U, next);
            store(memory, offset + 0x10U, previous);
            store(memory, offset + 0x74U, static_cast<float>(index));
            store(memory, offset + 0x78U, static_cast<float>(index / 2U));
            store(memory, offset + 0x7cU, static_cast<float>(index % 20U));
            store(
                memory, offset + 0x139U,
                static_cast<std::uint8_t>(index % 3U));
            store(
                memory, offset + 0x178U,
                static_cast<std::uint32_t>(index + 1U));
            store(
                memory, offset + 0x6b4U,
                static_cast<std::uint8_t>(index % 100U + 1U));
            const std::string name =
                "synthetic_spawn_" + std::to_string(index);
            std::memcpy(
                memory.data() + offset + 0xb8U,
                name.c_str(), name.size() + 1U);
        }
    }
};

plazmic::SpawnCollectionSnapshot spawn_fixture(
    std::size_t count, double movement = 0.0) {
    plazmic::SpawnCollectionSnapshot snapshot{
        .state = plazmic::PlayerSnapshotState::in_world,
        .zone = "synthetic",
        .player_level = 50,
        .player_name = "synthetic_player",
        .spawns = {},
        .detail = "Synthetic large spawn fixture",
    };
    snapshot.spawns.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const double x =
            static_cast<double>(index % 128U) * 5.0 + movement;
        const double y =
            static_cast<double>(index / 128U) * 5.0;
        snapshot.spawns.push_back({
            .id = static_cast<std::uint32_t>(index + 1U),
            .type =
                index % 17U == 0U
                    ? plazmic::SpawnType::player
                    : (index % 19U == 0U
                           ? plazmic::SpawnType::corpse
                           : plazmic::SpawnType::npc),
            .name = "synthetic_spawn_" + std::to_string(index),
            .level = static_cast<unsigned int>(index % 100U) + 1U,
            .x = x,
            .y = y,
            .z = static_cast<double>(index % 20U),
            .distance = std::hypot(x, y),
        });
    }
    return snapshot;
}

plazmic::ZoneMap map_fixture(std::size_t count) {
    plazmic::ZoneMap map{
        .zone = "synthetic",
        .layers =
            {
                {
                    .index = 0,
                    .source = "synthetic.txt",
                    .lines = {},
                    .labels = {},
                },
            },
    };
    map.layers.front().lines.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const double x = static_cast<double>(index % 500U);
        const double y = static_cast<double>(index / 500U);
        map.layers.front().lines.push_back({
            .start = {x, y, 0.0},
            .end = {x + 1.0, y + 1.0, 0.0},
            .color = {180, 180, 180},
        });
    }
    return map;
}

}  // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    try {
        constexpr std::size_t kSpawnCount = 2048U;
        SpawnReaderFixture reader_fixture(kSpawnCount);
        const std::uintptr_t anchor =
            reader_fixture.base +
            (kSpawnCount / 2U) * SpawnReaderFixture::record_bytes;
        const auto reader_start = Clock::now();
        for (int iteration = 0; iteration < 5; ++iteration) {
            const auto result = plazmic::read_spawn_collection(
                reader_fixture.process, anchor,
                reader_fixture.symbols, reader_fixture.player);
            require(
                result &&
                    result.snapshot->spawns.size() == kSpawnCount,
                "maximum-size spawn reader fixture failed");
        }
        const double reader_ms =
            milliseconds(Clock::now() - reader_start);
        require(reader_ms < 3000.0,
                "maximum-size spawn reader exceeded its budget");

        plazmic::SpawnTableModel model;
        plazmic::SpawnFilterProxyModel proxy;
        proxy.setSourceModel(&model);
        QTableView table;
        table.setModel(&proxy);
        table.setSortingEnabled(true);

        int heartbeats = 0;
        QTimer heartbeat;
        QObject::connect(
            &heartbeat, &QTimer::timeout, &application,
            [&heartbeats]() { ++heartbeats; });
        heartbeat.start(1);

        const auto publication_start = Clock::now();
        for (int update = 0; update < 100; ++update) {
            model.set_snapshot(
                spawn_fixture(kSpawnCount, update * 0.25));
            QApplication::processEvents();
        }
        const double publication_ms =
            milliseconds(Clock::now() - publication_start);
        require(model.rowCount() == static_cast<int>(kSpawnCount),
                "large spawn publication lost rows");
        require(heartbeats > 0,
                "large spawn publication starved the event loop");
        require(publication_ms < 3000.0,
                "large spawn publication exceeded its budget");

        const auto interaction_start = Clock::now();
        table.sortByColumn(
            plazmic::SpawnTableModel::distance_column,
            Qt::DescendingOrder);
        proxy.set_name_filter("synthetic_spawn_1999");
        QApplication::processEvents();
        require(proxy.rowCount() == 1,
                "large-fixture name filtering was incorrect");
        table.selectRow(0);
        require(table.currentIndex().data(plazmic::kSpawnIdRole).toUInt() ==
                    2000U,
                "large-fixture selection lost the stable ID");
        proxy.set_name_filter({});
        proxy.set_type_filter(plazmic::SpawnType::corpse);
        QApplication::processEvents();
        require(proxy.rowCount() > 0 &&
                    proxy.rowCount() <
                        static_cast<int>(kSpawnCount),
                "large-fixture type filtering was incorrect");
        const double interaction_ms =
            milliseconds(Clock::now() - interaction_start);
        require(interaction_ms < 1000.0,
                "spawn filtering, sorting, or selection exceeded budget");

        plazmic::MapCanvas canvas;
        canvas.resize(1200, 780);
        canvas.set_zone_map(map_fixture(10000U));
        canvas.set_player_snapshot({
            .state = plazmic::PlayerSnapshotState::in_world,
            .zone = "synthetic",
            .x = 100.0,
            .y = 100.0,
            .z = 10.0,
            .heading_degrees = 90.0,
            .detail = "Synthetic player",
        });
        canvas.set_height_filter_enabled(false);
        canvas.set_named_spawn_labels_visible(true);
        canvas.set_player_labels_visible(true);
        canvas.set_npc_labels_visible(true);
        canvas.set_spawn_snapshot(spawn_fixture(kSpawnCount));
        canvas.show();
        QApplication::processEvents();
        QImage image(canvas.size(), QImage::Format_ARGB32_Premultiplied);
        const auto render_start = Clock::now();
        for (int frame = 0; frame < 20; ++frame) {
            QPainter painter(&image);
            canvas.render(&painter);
        }
        const double render_ms =
            milliseconds(Clock::now() - render_start);
        require(render_ms < 3000.0,
                "large spawn marker rendering exceeded its budget");

        std::cout
            << "phase4 spawn_count=" << kSpawnCount
            << " reader_5_ms=" << reader_ms
            << " publication_100_ms=" << publication_ms
            << " interaction_ms=" << interaction_ms
            << " render_20_ms=" << render_ms
            << " heartbeats=" << heartbeats << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
