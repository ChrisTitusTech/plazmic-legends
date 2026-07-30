#include "game/game_state_reader.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include <unistd.h>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename Value, std::size_t Size>
void store(std::array<std::byte, Size>& memory,
           std::size_t offset,
           Value value) {
    require(offset <= memory.size() - sizeof(value),
            "synthetic fixture write exceeds its memory");
    std::memcpy(memory.data() + offset, &value, sizeof(value));
}

struct Fixture {
    alignas(16) std::array<std::byte, 0x2400> memory{};
    std::uintptr_t base{
        reinterpret_cast<std::uintptr_t>(memory.data())};
    plazmic::GameStateSymbols symbols{
        .local_player_pointer_rva = 0x100,
        .world_data_pointer_rva = 0x108,
        .player_y_offset = 0x78,
        .player_x_offset = 0x74,
        .player_z_offset = 0x7c,
        .player_heading_offset = 0x94,
        .player_zone_id_offset = 0x2fc,
        .zone_table_offset = 0x30,
        .zone_entry_id_offset = 0x0c,
        .zone_short_name_offset = 0x10,
        .zone_short_name_bytes = 64,
        .zone_id_mask = 0x7fff,
        .maximum_zone_id = 1000,
    };
    plazmic::ClientProcess process{
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
    };

    Fixture() {
        constexpr std::size_t kPlayer = 0x400;
        constexpr std::size_t kWorld = 0x900;
        constexpr std::size_t kZone = 0x1800;
        constexpr std::uint32_t kZoneId = 33;
        store(memory, 0x100, base + kPlayer);
        store(memory, 0x108, base + kWorld);
        store(memory, kPlayer + 0x74, -1057.25F);
        store(memory, kPlayer + 0x78, 616.0F);
        store(memory, kPlayer + 0x7c, -9.75F);
        store(memory, kPlayer + 0x94, 256.0F);
        store(memory, kPlayer + 0x2fc, 0x40000U | kZoneId);
        store(
            memory, kWorld + 0x30 +
                        static_cast<std::size_t>(kZoneId) *
                            sizeof(std::uintptr_t),
            base + kZone);
        store(memory, kZone + 0x0c, kZoneId);
        constexpr std::array<char, 10> kZoneName{
            's', 'y', 'n', 't', 'h', 'e', 't', 'i', 'c', '\0'};
        std::memcpy(
            memory.data() + kZone + 0x10,
            kZoneName.data(), kZoneName.size());
    }
};

}  // namespace

int main() {
    try {
        Fixture fixture;
        const plazmic::GameStateReadResult result =
            plazmic::read_game_state(fixture.process, fixture.symbols);
        require(static_cast<bool>(result),
                "valid synthetic game state was rejected");
        require(result.snapshot->zone == "synthetic",
                "zone short name was not resolved");
        require(std::abs(result.snapshot->x + 1057.25) < 0.001,
                "player X was not read");
        require(std::abs(result.snapshot->y - 616.0) < 0.001,
                "player Y was not read");
        require(std::abs(result.snapshot->z + 9.75) < 0.001,
                "player Z was not read");
        require(std::abs(result.snapshot->heading_degrees - 180.0) < 0.001,
                "player heading was not converted");

        Fixture invalid_zone;
        constexpr std::size_t kZone = 0x1800;
        constexpr std::array<char, 10> kEscapingName{
            '.', '.', '/', 'e', 's', 'c', 'a', 'p', 'e', '\0'};
        std::memcpy(
            invalid_zone.memory.data() + kZone + 0x10,
            kEscapingName.data(), kEscapingName.size());
        const auto invalid_zone_result = plazmic::read_game_state(
            invalid_zone.process, invalid_zone.symbols);
        require(
            invalid_zone_result.error ==
                plazmic::GameStateReadError::invalid_zone,
            "unsafe zone short name did not fail closed");

        Fixture invalid_player;
        store(
            invalid_player.memory, 0x400 + 0x78,
            std::numeric_limits<float>::infinity());
        const auto invalid_player_result = plazmic::read_game_state(
            invalid_player.process, invalid_player.symbols);
        require(
            invalid_player_result.error ==
                plazmic::GameStateReadError::invalid_player,
            "non-finite player coordinate did not fail closed");

        Fixture not_in_world;
        store(not_in_world.memory, 0x100, std::uintptr_t{0});
        const auto not_in_world_result = plazmic::read_game_state(
            not_in_world.process, not_in_world.symbols);
        require(
            not_in_world_result.error ==
                plazmic::GameStateReadError::not_in_world,
            "null local-player pointer did not produce not-in-world");

        Fixture invalid_profile;
        invalid_profile.symbols.zone_id_mask = 0;
        const auto invalid_profile_result = plazmic::read_game_state(
            invalid_profile.process, invalid_profile.symbols);
        require(
            invalid_profile_result.error ==
                plazmic::GameStateReadError::invalid_profile,
            "zero zone-ID mask did not reject the incomplete profile");

        std::cout << "bounded synthetic zone and player reader passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
