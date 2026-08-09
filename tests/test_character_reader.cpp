#include "game/character_reader.h"
#include "game/client_profile.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
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

template <typename Value, std::size_t Size>
void store(std::array<std::byte, Size>& memory,
           std::size_t offset,
           Value value) {
    require(offset <= memory.size() - sizeof(value),
            "synthetic character fixture write exceeds memory");
    std::memcpy(memory.data() + offset, &value, sizeof(value));
}

struct Fixture {
    alignas(16) std::array<std::byte, 0x18000> memory{};
    std::uintptr_t base{
        reinterpret_cast<std::uintptr_t>(memory.data())};
    plazmic::CharacterSymbols symbols{
        plazmic::legends_reference_profile().character};
    std::uintptr_t local_player{base + 0x5000};
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
        constexpr std::size_t kRoot = 0x1000;
        constexpr std::size_t kDescriptor = 0x8000;
        constexpr std::size_t kRecord = 0x9000;
        constexpr std::size_t kProfile = 0xa000;
        constexpr std::size_t kItems = 0xd000;
        constexpr std::size_t kItemObjects = 0xe000;
        constexpr std::size_t kNames = 0x13000;
        constexpr std::int32_t kDisplacement = 0x740;
        symbols.local_character_pointer_rva = 0x100;

        store(memory, 0x100, base + kRoot);
        constexpr std::array<char, 20> kCharacterName{
            's', 'y', 'n', 't', 'h', 'e', 't', 'i', 'c', '_',
            'c', 'h', 'a', 'r', 'a', 'c', 't', 'e', 'r', '\0'};
        std::memcpy(
            memory.data() + 0x5000 + symbols.player_name_offset,
            kCharacterName.data(), kCharacterName.size());
        store(
            memory,
            0x5000 + symbols.player_maximum_health_offset,
            std::int64_t{1000});
        store(
            memory,
            0x5000 + symbols.player_maximum_mana_offset,
            std::int64_t{500});

        const std::size_t character_zone =
            kRoot + symbols.character_zone_offset;
        store(
            memory,
            character_zone + symbols.character_type_descriptor_offset,
            base + kDescriptor);
        store(
            memory,
            kDescriptor + symbols.type_descriptor_displacement_offset,
            kDisplacement);
        store(
            memory,
            character_zone + symbols.health_adjustment_offset,
            std::int32_t{5});

        const std::size_t lookup = character_zone +
            symbols.stats_lookup_offset +
            static_cast<std::size_t>(kDisplacement);
        store(memory, lookup, base + kRecord);
        store(
            memory,
            lookup + symbols.stats_lookup_key_offset,
            std::int32_t{0});
        store(
            memory,
            kRecord + symbols.stats_record_key_offset,
            std::int32_t{0});
        store(
            memory,
            kRecord + symbols.stats_record_value_offset,
            base + kProfile);
        store(
            memory,
            kRecord + symbols.stats_record_next_offset,
            std::uintptr_t{0});

        store(
            memory,
            kProfile + symbols.stats_current_health_offset,
            std::int64_t{900});
        store(
            memory,
            kProfile + symbols.stats_current_mana_offset,
            std::int32_t{400});
        const std::size_t inventory =
            kProfile + symbols.inventory_container_offset;
        store(
            memory,
            inventory + symbols.inventory_size_offset,
            std::uint32_t{36});
        store(
            memory,
            inventory + symbols.inventory_data_offset,
            base + kItems);

        for (std::size_t slot = 0U; slot < 23U; ++slot) {
            const std::size_t item = kItemObjects + slot * 0x100U;
            const std::size_t name = kNames + slot * 0x40U;
            store(
                memory,
                kItems + slot * symbols.inventory_entry_bytes,
                base + item);
            store(
                memory,
                item + symbols.item_name_pointer_offset,
                base + name);
            const std::string value = "Synthetic item " +
                                      std::to_string(slot);
            std::memcpy(
                memory.data() + name,
                value.c_str(), value.size() + 1U);
        }
    }
};

}  // namespace

int main() {
    try {
        Fixture fixture;
        const auto result = plazmic::read_character_snapshot(
            fixture.process, fixture.local_player, fixture.symbols);
        require(static_cast<bool>(result),
                "valid synthetic character was rejected");
        require(result.snapshot->name == "synthetic_character",
                "character name was not decoded");
        require(result.snapshot->health.current == 905 &&
                    result.snapshot->health.maximum == 1000,
                "health current/maximum were not decoded");
        require(result.snapshot->mana.current == 400 &&
                    result.snapshot->mana.maximum == 500,
                "mana current/maximum were not decoded");
        require(result.snapshot->equipment.size() == 23U &&
                    result.snapshot->equipment[13].slot == "Primary" &&
                    result.snapshot->equipment[13].item ==
                        "Synthetic item 13",
                "equipment slot and item names were not decoded");
        constexpr std::size_t kPerformanceReads = 100U;
        const auto performance_start = std::chrono::steady_clock::now();
        for (std::size_t index = 0U; index < kPerformanceReads; ++index) {
            const auto performance_result =
                plazmic::read_character_snapshot(
                    fixture.process,
                    fixture.local_player,
                    fixture.symbols);
            require(static_cast<bool>(performance_result),
                    "bounded character performance read failed");
        }
        const double average_read_ms =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - performance_start)
                .count() /
            static_cast<double>(kPerformanceReads);
        require(average_read_ms < 250.0,
                "character reader exceeded the polling budget");

        Fixture no_mana;
        store(
            no_mana.memory,
            0xa000 + no_mana.symbols.stats_current_mana_offset,
            std::int32_t{0});
        store(
            no_mana.memory,
            0x5000 + no_mana.symbols.player_maximum_mana_offset,
            std::int64_t{0});
        const auto no_mana_result = plazmic::read_character_snapshot(
            no_mana.process,
            no_mana.local_player,
            no_mana.symbols);
        require(no_mana_result &&
                    no_mana_result.snapshot->mana.current == 0 &&
                    no_mana_result.snapshot->mana.maximum == 0,
                "valid zero-mana character was rejected");

        Fixture empty_slot;
        store(
            empty_slot.memory,
            0xd000 + 5U * empty_slot.symbols.inventory_entry_bytes,
            std::uintptr_t{0});
        const auto empty_slot_result = plazmic::read_character_snapshot(
            empty_slot.process,
            empty_slot.local_player,
            empty_slot.symbols);
        require(empty_slot_result &&
                    empty_slot_result.snapshot->equipment[5].item.empty(),
                "empty equipment slot was not preserved");

        Fixture invalid_mana;
        store(
            invalid_mana.memory,
            0xa000 + invalid_mana.symbols.stats_current_mana_offset,
            std::int32_t{501});
        const auto invalid_mana_result =
            plazmic::read_character_snapshot(
                invalid_mana.process,
                invalid_mana.local_player,
                invalid_mana.symbols);
        require(invalid_mana_result.error ==
                    plazmic::CharacterReadError::invalid_value,
                "mana above maximum did not fail closed");

        Fixture invalid_health;
        store(
            invalid_health.memory,
            0x5000 + invalid_health.symbols.player_maximum_health_offset,
            std::int64_t{904});
        const auto invalid_health_result =
            plazmic::read_character_snapshot(
                invalid_health.process,
                invalid_health.local_player,
                invalid_health.symbols);
        require(invalid_health_result.error ==
                    plazmic::CharacterReadError::invalid_value,
                "health above maximum did not fail closed");

        Fixture invalid_inventory;
        store(
            invalid_inventory.memory,
            0xa000 + invalid_inventory.symbols.inventory_container_offset,
            std::uint32_t{300});
        const auto invalid_inventory_result =
            plazmic::read_character_snapshot(
                invalid_inventory.process,
                invalid_inventory.local_player,
                invalid_inventory.symbols);
        require(invalid_inventory_result.error ==
                    plazmic::CharacterReadError::invalid_value,
                "oversized equipment container did not fail closed");

        Fixture short_inventory;
        store(
            short_inventory.memory,
            0xa000 + short_inventory.symbols.inventory_container_offset,
            std::uint32_t{22});
        const auto short_inventory_result =
            plazmic::read_character_snapshot(
                short_inventory.process,
                short_inventory.local_player,
                short_inventory.symbols);
        require(short_inventory_result.error ==
                    plazmic::CharacterReadError::invalid_value,
                "undersized equipment container did not fail closed");

        Fixture malformed_name;
        malformed_name.memory[
            0x5000 + malformed_name.symbols.player_name_offset] =
                std::byte{0x01};
        const auto malformed_name_result =
            plazmic::read_character_snapshot(
                malformed_name.process,
                malformed_name.local_player,
                malformed_name.symbols);
        require(malformed_name_result.error ==
                    plazmic::CharacterReadError::read_failed,
                "malformed character name did not fail closed");

        Fixture invalid_item_name;
        std::memset(
            invalid_item_name.memory.data() + 0x13000,
            'x',
            invalid_item_name.symbols.item_name_bytes);
        const auto invalid_item_name_result =
            plazmic::read_character_snapshot(
                invalid_item_name.process,
                invalid_item_name.local_player,
                invalid_item_name.symbols);
        require(invalid_item_name_result.error ==
                    plazmic::CharacterReadError::invalid_value,
                "unterminated item name did not fail closed");

        Fixture invalid_profile;
        invalid_profile.symbols.item_name_bytes = 0U;
        const auto invalid_profile_result =
            plazmic::read_character_snapshot(
                invalid_profile.process,
                invalid_profile.local_player,
                invalid_profile.symbols);
        require(invalid_profile_result.error ==
                    plazmic::CharacterReadError::invalid_profile,
                "incomplete character profile did not fail closed");

        Fixture bounded_cycle;
        store(
            bounded_cycle.memory,
            0x9000 + bounded_cycle.symbols.stats_record_key_offset,
            std::int32_t{1});
        store(
            bounded_cycle.memory,
            0x9000 + bounded_cycle.symbols.stats_record_next_offset,
            bounded_cycle.base + 0x9000);
        const auto bounded_cycle_result =
            plazmic::read_character_snapshot(
                bounded_cycle.process,
                bounded_cycle.local_player,
                bounded_cycle.symbols);
        require(bounded_cycle_result.error ==
                    plazmic::CharacterReadError::invalid_pointer,
                "cyclic stats list escaped its record bound");

        std::cout << "bounded character and equipment reader passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
