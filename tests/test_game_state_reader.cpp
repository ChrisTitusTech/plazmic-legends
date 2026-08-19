#include "game/game_state_reader.h"
#include "integration/process_reader.h"

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
#include <string_view>

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
    plazmic::SpawnSymbols spawn_symbols{
        .next_offset = 0x08,
        .previous_offset = 0x10,
        .name_offset = 0xb8,
        .name_bytes = 64,
        .type_offset = 0x139,
        .id_offset = 0x178,
        .level_offset = 0x6b4,
        .y_offset = 0x78,
        .x_offset = 0x74,
        .z_offset = 0x7c,
        .record_bytes = 0x6b5,
        .maximum_count = 32,
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
        constexpr std::size_t kSecondSpawn = 0xb00;
        constexpr std::size_t kWorld = 0x1400;
        constexpr std::size_t kZone = 0x1c00;
        constexpr std::uint32_t kZoneId = 33;
        store(memory, 0x100, base + kPlayer);
        store(memory, 0x108, base + kWorld);
        store(memory, kPlayer + 0x74, -1057.25F);
        store(memory, kPlayer + 0x78, 616.0F);
        store(memory, kPlayer + 0x7c, -9.75F);
        store(memory, kPlayer + 0x94, 256.0F);
        store(memory, kPlayer + 0x2fc, 0x40000U | kZoneId);
        store(memory, kPlayer, base + 0x200);
        store(memory, kPlayer + 0x08, base + kSecondSpawn);
        store(memory, kPlayer + 0x10, std::uintptr_t{0});
        store(memory, kPlayer + 0x139, std::uint8_t{0});
        store(memory, kPlayer + 0x178, std::uint32_t{100});
        store(memory, kPlayer + 0x6b4, std::uint8_t{50});
        constexpr std::array<char, 7> kPlayerName{
            'p', 'l', 'a', 'y', 'e', 'r', '\0'};
        std::memcpy(
            memory.data() + kPlayer + 0xb8,
            kPlayerName.data(), kPlayerName.size());

        store(memory, kSecondSpawn, base + 0x200);
        store(memory, kSecondSpawn + 0x08, std::uintptr_t{0});
        store(memory, kSecondSpawn + 0x10, base + kPlayer);
        store(memory, kSecondSpawn + 0x74, -1000.0F);
        store(memory, kSecondSpawn + 0x78, 600.0F);
        store(memory, kSecondSpawn + 0x7c, -8.0F);
        store(memory, kSecondSpawn + 0x139, std::uint8_t{1});
        store(memory, kSecondSpawn + 0x178, std::uint32_t{101});
        store(memory, kSecondSpawn + 0x6b4, std::uint8_t{12});
        constexpr std::array<char, 10> kSpawnName{
            's', 'y', 'n', 't', 'h', '_', 'n', 'p', 'c', '\0'};
        std::memcpy(
            memory.data() + kSecondSpawn + 0xb8,
            kSpawnName.data(), kSpawnName.size());
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
        constexpr std::string_view kCurrentDigest =
            "d9784a58bb03cb70177d6a494fa71bca2b13ab3c5b8b9d6c26e45bae01597e51";
        const auto* current_profile =
            plazmic::select_client_profile(kCurrentDigest);
        require(current_profile != nullptr,
                "current profile was unavailable to the identity test");
        constexpr std::uintptr_t kMappedImageBase =
            0x6ffffaf70000ULL;
        const plazmic::RemotePeIdentity exact_identity{
            .machine = current_profile->machine,
            .timestamp = current_profile->timestamp,
            .optional_magic = current_profile->optional_magic,
            .image_base = kMappedImageBase,
            .image_size = current_profile->image_size,
        };
        require(
            plazmic::matches_client_profile_identity(
                *current_profile, exact_identity, kMappedImageBase),
            "exact mapped PE identity did not match the current profile");

        auto remapped_identity = exact_identity;
        remapped_identity.timestamp ^= 1U;
        require(
            !plazmic::matches_client_profile_identity(
                *current_profile, remapped_identity,
                kMappedImageBase),
            "same-PID remap with a changed PE timestamp was accepted");
        remapped_identity = exact_identity;
        remapped_identity.machine ^= 1U;
        require(
            !plazmic::matches_client_profile_identity(
                *current_profile, remapped_identity,
                kMappedImageBase),
            "same-PID remap with a changed PE machine was accepted");
        remapped_identity = exact_identity;
        remapped_identity.optional_magic ^= 1U;
        require(
            !plazmic::matches_client_profile_identity(
                *current_profile, remapped_identity,
                kMappedImageBase),
            "same-PID remap with a changed PE format was accepted");
        remapped_identity = exact_identity;
        remapped_identity.image_size ^= 1U;
        require(
            !plazmic::matches_client_profile_identity(
                *current_profile, remapped_identity,
                kMappedImageBase),
            "same-PID remap with a changed PE image size was accepted");
        remapped_identity = exact_identity;
        remapped_identity.image_base ^= 0x10000ULL;
        require(
            !plazmic::matches_client_profile_identity(
                *current_profile, remapped_identity,
                kMappedImageBase),
            "same-PID remap with a mismatched live ImageBase was accepted");

        Fixture fixture;
        const plazmic::GameStateReadResult result =
            plazmic::read_game_state(
                fixture.process, fixture.symbols,
                fixture.spawn_symbols);
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
        require(result.spawns->spawns.size() == 2,
                "spawn collection was not published");
        require(result.spawns->player_level == 50,
                "local-player anchor level was not published");
        require(result.spawns->player_name == "player",
                "local-player anchor name was not published");
        require(result.spawns->spawns[1].id == 101 &&
                    result.spawns->spawns[1].name == "synth_npc" &&
                    result.spawns->spawns[1].level == 12,
                "spawn fields were not decoded");

        Fixture player_not_root;
        store(
            player_not_root.memory, 0x400 + 0x08,
            std::uintptr_t{0});
        store(
            player_not_root.memory, 0x400 + 0x10,
            player_not_root.base + 0xb00);
        store(
            player_not_root.memory, 0xb00 + 0x08,
            player_not_root.base + 0x400);
        store(
            player_not_root.memory, 0xb00 + 0x10,
            std::uintptr_t{0});
        const auto player_not_root_result =
            plazmic::read_game_state(
                player_not_root.process,
                player_not_root.symbols,
                player_not_root.spawn_symbols);
        require(
            player_not_root_result &&
                player_not_root_result.spawns->spawns.size() == 2 &&
                player_not_root_result.spawns->player_level == 50 &&
                player_not_root_result.spawns->player_name == "player" &&
                player_not_root_result.spawns->spawns[0].id == 101 &&
                player_not_root_result.spawns->spawns[1].id == 100,
            "non-root local-player anchor did not resolve the list root");

        Fixture invalid_reverse_resolution;
        store(
            invalid_reverse_resolution.memory, 0x400 + 0x10,
            invalid_reverse_resolution.base + 0xb00);
        store(
            invalid_reverse_resolution.memory, 0xb00 + 0x08,
            std::uintptr_t{0});
        store(
            invalid_reverse_resolution.memory, 0xb00 + 0x10,
            std::uintptr_t{0});
        const auto invalid_reverse_result =
            plazmic::read_game_state(
                invalid_reverse_resolution.process,
                invalid_reverse_resolution.symbols,
                invalid_reverse_resolution.spawn_symbols);
        require(
            invalid_reverse_result.error ==
                plazmic::GameStateReadError::inconsistent_snapshot,
            "broken reverse root resolution did not fail closed");

        Fixture invalid_zone;
        constexpr std::size_t kZone = 0x1c00;
        constexpr std::array<char, 10> kEscapingName{
            '.', '.', '/', 'e', 's', 'c', 'a', 'p', 'e', '\0'};
        std::memcpy(
            invalid_zone.memory.data() + kZone + 0x10,
            kEscapingName.data(), kEscapingName.size());
        const auto invalid_zone_result = plazmic::read_game_state(
            invalid_zone.process, invalid_zone.symbols,
            invalid_zone.spawn_symbols);
        require(
            invalid_zone_result.error ==
                plazmic::GameStateReadError::invalid_zone,
            "unsafe zone short name did not fail closed");

        Fixture invalid_player;
        store(
            invalid_player.memory, 0x400 + 0x78,
            std::numeric_limits<float>::infinity());
        const auto invalid_player_result = plazmic::read_game_state(
            invalid_player.process, invalid_player.symbols,
            invalid_player.spawn_symbols);
        require(
            invalid_player_result.error ==
                plazmic::GameStateReadError::invalid_player,
            "non-finite player coordinate did not fail closed");

        Fixture not_in_world;
        store(not_in_world.memory, 0x100, std::uintptr_t{0});
        const auto not_in_world_result = plazmic::read_game_state(
            not_in_world.process, not_in_world.symbols,
            not_in_world.spawn_symbols);
        require(
            not_in_world_result.error ==
                plazmic::GameStateReadError::not_in_world,
            "null local-player pointer did not produce not-in-world");

        Fixture invalid_profile;
        invalid_profile.symbols.zone_id_mask = 0;
        const auto invalid_profile_result = plazmic::read_game_state(
            invalid_profile.process, invalid_profile.symbols,
            invalid_profile.spawn_symbols);
        require(
            invalid_profile_result.error ==
                plazmic::GameStateReadError::invalid_profile,
            "zero zone-ID mask did not reject the incomplete profile");

        Fixture invalid_spawn_profile;
        invalid_spawn_profile.spawn_symbols.next_offset = 24U;
        const auto invalid_spawn_profile_result =
            plazmic::read_game_state(
                invalid_spawn_profile.process,
                invalid_spawn_profile.symbols,
                invalid_spawn_profile.spawn_symbols);
        require(
            invalid_spawn_profile_result.error ==
                plazmic::GameStateReadError::invalid_profile,
            "spawn link outside the bounded link record was accepted");

        Fixture duplicate_spawn;
        store(
            duplicate_spawn.memory, 0xb00 + 0x178,
            std::uint32_t{100});
        const auto duplicate_result = plazmic::read_game_state(
            duplicate_spawn.process, duplicate_spawn.symbols,
            duplicate_spawn.spawn_symbols);
        require(
            duplicate_result.error ==
                plazmic::GameStateReadError::invalid_spawns,
            "duplicate stable spawn ID did not fail closed");

        Fixture unterminated_name;
        std::memset(
            unterminated_name.memory.data() + 0xb00 + 0xb8,
            'x', 64);
        const auto unterminated_result = plazmic::read_game_state(
            unterminated_name.process, unterminated_name.symbols,
            unterminated_name.spawn_symbols);
        require(
            unterminated_result.error ==
                plazmic::GameStateReadError::invalid_spawns,
            "unterminated spawn name did not fail closed");

        Fixture bounded_count;
        bounded_count.spawn_symbols.maximum_count = 1;
        const auto bounded_count_result = plazmic::read_game_state(
            bounded_count.process, bounded_count.symbols,
            bounded_count.spawn_symbols);
        require(
            bounded_count_result.error ==
                plazmic::GameStateReadError::inconsistent_snapshot,
            "spawn count above the profile bound did not fail closed");

        Fixture invalid_pointer;
        store(
            invalid_pointer.memory, 0x400 + 0x08,
            std::uintptr_t{1});
        const auto invalid_pointer_result = plazmic::read_game_state(
            invalid_pointer.process, invalid_pointer.symbols,
            invalid_pointer.spawn_symbols);
        require(
            invalid_pointer_result.error ==
                plazmic::GameStateReadError::inconsistent_snapshot,
            "unreadable spawn link did not fail closed");

        Fixture cyclic_list;
        store(
            cyclic_list.memory, 0xb00 + 0x08,
            cyclic_list.base + 0x400);
        const auto cyclic_result = plazmic::read_game_state(
            cyclic_list.process, cyclic_list.symbols,
            cyclic_list.spawn_symbols);
        require(
            cyclic_result.error ==
                plazmic::GameStateReadError::inconsistent_snapshot,
            "cyclic spawn links did not fail closed");

        Fixture cyclic_reverse_list;
        store(
            cyclic_reverse_list.memory, 0x400 + 0x10,
            cyclic_reverse_list.base + 0xb00);
        store(
            cyclic_reverse_list.memory, 0xb00 + 0x08,
            cyclic_reverse_list.base + 0x400);
        store(
            cyclic_reverse_list.memory, 0xb00 + 0x10,
            cyclic_reverse_list.base + 0x400);
        const auto cyclic_reverse_result =
            plazmic::read_game_state(
                cyclic_reverse_list.process,
                cyclic_reverse_list.symbols,
                cyclic_reverse_list.spawn_symbols);
        require(
            cyclic_reverse_result.error ==
                plazmic::GameStateReadError::inconsistent_snapshot,
            "cyclic reverse spawn links did not fail closed");

        Fixture broken_backlink;
        store(
            broken_backlink.memory, 0xb00 + 0x10,
            std::uintptr_t{0});
        const auto broken_backlink_result =
            plazmic::read_game_state(
                broken_backlink.process, broken_backlink.symbols,
                broken_backlink.spawn_symbols);
        require(
            broken_backlink_result.error ==
                plazmic::GameStateReadError::inconsistent_snapshot,
            "inconsistent spawn backlink did not fail closed");

        Fixture invalid_type;
        store(
            invalid_type.memory, 0xb00 + 0x139,
            std::uint8_t{3});
        const auto invalid_type_result = plazmic::read_game_state(
            invalid_type.process, invalid_type.symbols,
            invalid_type.spawn_symbols);
        require(
            invalid_type_result.error ==
                plazmic::GameStateReadError::invalid_spawns,
            "unapproved spawn type did not fail closed");

        Fixture invalid_level;
        store(
            invalid_level.memory, 0xb00 + 0x6b4,
            std::uint8_t{0});
        const auto invalid_level_result = plazmic::read_game_state(
            invalid_level.process, invalid_level.symbols,
            invalid_level.spawn_symbols);
        require(
            invalid_level_result.error ==
                plazmic::GameStateReadError::invalid_spawns,
            "invalid spawn level did not fail closed");

        Fixture invalid_position;
        store(
            invalid_position.memory, 0xb00 + 0x74,
            std::numeric_limits<float>::quiet_NaN());
        const auto invalid_position_result = plazmic::read_game_state(
            invalid_position.process, invalid_position.symbols,
            invalid_position.spawn_symbols);
        require(
            invalid_position_result.error ==
                plazmic::GameStateReadError::invalid_spawns,
            "non-finite spawn position did not fail closed");

        std::cout
            << "bounded synthetic player and spawn reader passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
