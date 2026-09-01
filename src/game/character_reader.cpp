#include "game/character_reader.h"

#include "integration/process_reader.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>
#include <span>
#include <string_view>
#include <utility>

namespace plazmic {
namespace {

constexpr std::array<std::string_view, 23> kEquipmentSlots{
    "Charm", "Left Ear", "Head", "Face", "Right Ear", "Neck",
    "Shoulders", "Arms", "Back", "Left Wrist", "Right Wrist", "Range",
    "Hands", "Primary", "Secondary", "Left Finger", "Right Finger",
    "Chest", "Legs", "Feet", "Waist", "Power Source", "Ammo",
};

template <typename Value>
std::optional<Value> read_value(const ProcessMemoryReader& reader,
                                std::uintptr_t address,
                                std::string& detail) {
    std::array<std::byte, sizeof(Value)> bytes{};
    const ProcessReadResult result = reader.read_exact(address, bytes);
    if (!result) {
        detail = result.detail;
        return std::nullopt;
    }
    Value value{};
    std::memcpy(&value, bytes.data(), sizeof(value));
    return value;
}

bool checked_add(std::uintptr_t base,
                 std::size_t offset,
                 std::uintptr_t& result);

template <typename Value>
std::optional<Value> read_value_at(const ProcessMemoryReader& reader,
                                   std::uintptr_t base,
                                   std::size_t offset,
                                   std::string& detail) {
    std::uintptr_t address = 0U;
    if (!checked_add(base, offset, address)) {
        detail = "character read address overflows";
        return std::nullopt;
    }
    return read_value<Value>(reader, address, detail);
}

std::optional<std::int64_t> read_vital_at(
    const ProcessMemoryReader& reader,
    std::uintptr_t base,
    std::size_t offset,
    std::size_t width,
    std::string& detail) {
    if (width == sizeof(std::int32_t)) {
        const auto value = read_value_at<std::int32_t>(
            reader, base, offset, detail);
        return value ? std::optional<std::int64_t>{*value} : std::nullopt;
    }
    if (width == sizeof(std::int64_t)) {
        return read_value_at<std::int64_t>(reader, base, offset, detail);
    }
    detail = "profile vital width is invalid";
    return std::nullopt;
}

std::optional<std::uint32_t> read_unsigned_at(
    const ProcessMemoryReader& reader,
    std::uintptr_t base,
    std::size_t offset,
    std::size_t width,
    std::string& detail) {
    if (width == sizeof(std::uint8_t)) {
        const auto value = read_value_at<std::uint8_t>(
            reader, base, offset, detail);
        return value ? std::optional<std::uint32_t>{*value} : std::nullopt;
    }
    if (width == sizeof(std::uint32_t)) {
        return read_value_at<std::uint32_t>(reader, base, offset, detail);
    }
    detail = "profile unsigned width is invalid";
    return std::nullopt;
}

bool checked_add(std::uintptr_t base,
                 std::size_t offset,
                 std::uintptr_t& result) {
    if (offset > std::numeric_limits<std::uintptr_t>::max() - base) {
        return false;
    }
    result = base + offset;
    return true;
}

bool checked_add_signed(std::uintptr_t base,
                        std::int32_t displacement,
                        std::uintptr_t& result) {
    if (displacement >= 0) {
        return checked_add(base, static_cast<std::size_t>(displacement), result);
    }
    const auto magnitude = static_cast<std::uint32_t>(
        -static_cast<std::int64_t>(displacement));
    if (magnitude > base) {
        return false;
    }
    result = base - magnitude;
    return true;
}

CharacterReadResult failure(CharacterReadError error, std::string detail) {
    return {
        .snapshot = std::nullopt,
        .error = error,
        .detail = std::move(detail),
    };
}

std::optional<std::string> read_text(const ProcessMemoryReader& reader,
                                     std::uintptr_t address,
                                     std::size_t bound,
                                     std::string& detail) {
    if (bound == 0U || bound > 128U) {
        detail = "profile text bound is invalid";
        return std::nullopt;
    }
    std::array<std::byte, 128> bytes{};
    auto output = std::span<std::byte>(bytes).first(bound);
    const ProcessReadResult result = reader.read_exact(address, output);
    if (!result) {
        detail = result.detail;
        return std::nullopt;
    }
    const auto end = std::find(output.begin(), output.end(), std::byte{0});
    if (end == output.begin() || end == output.end()) {
        detail = "profile text is empty or unterminated";
        return std::nullopt;
    }
    const std::string value(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::size_t>(end - output.begin()));
    if (!std::ranges::all_of(value, [](unsigned char byte) {
            return byte >= 0x20U && byte <= 0x7eU;
        })) {
        detail = "profile text contains invalid bytes";
        return std::nullopt;
    }
    return value;
}

std::optional<std::uintptr_t> resolve_stats(
    const ProcessMemoryReader& reader,
    std::uintptr_t lookup,
    const CharacterSymbols& symbols,
    std::string& detail) {
    const auto record_key = read_value_at<std::int32_t>(
        reader, lookup, symbols.stats_lookup_key_offset, detail);
    auto record = read_value<std::uintptr_t>(reader, lookup, detail);
    if (!record_key || !record) {
        return std::nullopt;
    }
    for (std::size_t count = 0U;
         *record != 0U && count < symbols.stats_maximum_records;
         ++count) {
        const auto key = read_value_at<std::int32_t>(
            reader, *record, symbols.stats_record_key_offset, detail);
        if (!key) {
            return std::nullopt;
        }
        if (*key == *record_key) {
            return read_value_at<std::uintptr_t>(
                reader, *record, symbols.stats_record_value_offset, detail);
        }
        record = read_value_at<std::uintptr_t>(
            reader, *record, symbols.stats_record_next_offset, detail);
        if (!record) {
            return std::nullopt;
        }
    }
    detail = "character stats record is unavailable";
    return std::nullopt;
}

std::optional<std::uintptr_t> resolve_profile(
    const ProcessMemoryReader& reader,
    std::uintptr_t profile_manager,
    std::int32_t current_type,
    const CharacterSymbols& symbols,
    std::string& detail) {
    auto profile_list = read_value<std::uintptr_t>(
        reader, profile_manager, detail);
    if (!profile_list) {
        return std::nullopt;
    }
    for (std::size_t count = 0U;
         *profile_list != 0U && count < symbols.profile_list_maximum_count;
         ++count) {
        const auto type = read_value_at<std::int32_t>(
            reader, *profile_list, symbols.profile_list_type_offset, detail);
        if (!type) {
            return std::nullopt;
        }
        if (*type == current_type) {
            return read_value_at<std::uintptr_t>(
                reader,
                *profile_list,
                symbols.profile_list_first_profile_offset,
                detail);
        }
        profile_list = read_value_at<std::uintptr_t>(
            reader, *profile_list, symbols.profile_list_next_offset, detail);
        if (!profile_list) {
            return std::nullopt;
        }
    }
    detail = "active character profile is unavailable";
    return std::nullopt;
}

bool valid_profile(const CharacterSymbols& symbols) {
    constexpr std::size_t kMaximumProgressionCacheOffset = 4096U;
    constexpr std::size_t kMaximumCharacterProfileOffset = 64U * 1024U;
    const bool has_cache_points =
        symbols.alternate_advancement_points_offset != 0U;
    const bool has_unallocated_points =
        symbols.unallocated_alternate_advancement_points_offset != 0U;
    const bool any_progression = symbols.progression_cache_rva != 0U ||
                                 symbols.alternate_advancement_percent_offset !=
                                     0U ||
                                 has_cache_points || has_unallocated_points;
    const bool progression_profile_valid =
        !any_progression ||
        (symbols.progression_cache_rva != 0U &&
         symbols.alternate_advancement_percent_offset <=
             kMaximumProgressionCacheOffset &&
         has_cache_points != has_unallocated_points &&
         (!has_cache_points ||
          (symbols.alternate_advancement_points_offset <=
               kMaximumProgressionCacheOffset &&
           symbols.alternate_advancement_percent_offset !=
               symbols.alternate_advancement_points_offset)) &&
         (!has_unallocated_points ||
          ((symbols.unallocated_alternate_advancement_points_bytes ==
                sizeof(std::uint8_t) ||
            symbols.unallocated_alternate_advancement_points_bytes ==
                sizeof(std::uint32_t)) &&
           symbols.unallocated_alternate_advancement_points_offset <=
               kMaximumCharacterProfileOffset -
                   symbols.unallocated_alternate_advancement_points_bytes)));
    const auto valid_vital_width = [](std::size_t width) {
        return width == sizeof(std::int32_t) ||
               width == sizeof(std::int64_t);
    };
    return symbols.local_character_pointer_rva != 0U &&
           symbols.player_name_bytes > 0U &&
           symbols.player_name_bytes <= 128U &&
           symbols.stats_maximum_records > 0U &&
           symbols.stats_maximum_records <= 256U &&
           symbols.profile_list_maximum_count > 0U &&
           symbols.profile_list_maximum_count <= 32U &&
           symbols.inventory_entry_bytes >= sizeof(std::uintptr_t) &&
           symbols.inventory_entry_bytes <= 4096U &&
           symbols.inventory_maximum_slots >= kEquipmentSlots.size() &&
           symbols.inventory_maximum_slots <= 256U &&
           symbols.item_name_bytes > 0U &&
           symbols.item_name_bytes <= 128U &&
           valid_vital_width(symbols.stats_current_health_bytes) &&
           valid_vital_width(symbols.player_maximum_health_bytes) &&
           valid_vital_width(symbols.player_maximum_mana_bytes) &&
           progression_profile_valid;
}

}  // namespace

CharacterReadResult read_character_snapshot(
    const ClientProcess& process,
    std::uintptr_t local_player,
    const CharacterSymbols& symbols) {
    if (process.pid <= 0 || process.image_base == 0U || local_player == 0U ||
        !valid_profile(symbols)) {
        return failure(
            CharacterReadError::invalid_profile,
            "character profile is incomplete");
    }
    const ProcessMemoryReader reader(process);
    std::string detail;
    std::uintptr_t local_character_global = 0U;
    std::uintptr_t name_address = 0U;
    if (!checked_add(process.image_base,
                     symbols.local_character_pointer_rva,
                     local_character_global) ||
        !checked_add(local_player, symbols.player_name_offset, name_address)) {
        return failure(
            CharacterReadError::invalid_pointer,
            "character global or name address overflows");
    }
    const auto character_root = read_value<std::uintptr_t>(
        reader, local_character_global, detail);
    const auto name = read_text(
        reader, name_address, symbols.player_name_bytes, detail);
    if (!character_root || !name) {
        return failure(CharacterReadError::read_failed, std::move(detail));
    }
    if (*character_root == 0U) {
        return failure(
            CharacterReadError::invalid_pointer,
            "character root is unavailable");
    }

    std::uintptr_t character_zone = 0U;
    std::uintptr_t descriptor_address = 0U;
    if (!checked_add(*character_root,
                     symbols.character_zone_offset,
                     character_zone) ||
        !checked_add(character_zone,
                     symbols.character_type_descriptor_offset,
                     descriptor_address)) {
        return failure(
            CharacterReadError::invalid_pointer,
            "character-zone address overflows");
    }
    const auto type_descriptor = read_value<std::uintptr_t>(
        reader, descriptor_address, detail);
    if (!type_descriptor || *type_descriptor == 0U) {
        return failure(CharacterReadError::read_failed, std::move(detail));
    }
    std::uintptr_t displacement_address = 0U;
    if (!checked_add(*type_descriptor,
                     symbols.type_descriptor_displacement_offset,
                     displacement_address)) {
        return failure(
            CharacterReadError::invalid_pointer,
            "character displacement address overflows");
    }
    const auto displacement = read_value<std::int32_t>(
        reader, displacement_address, detail);
    std::uintptr_t lookup = 0U;
    std::uintptr_t lookup_base = 0U;
    if (!displacement ||
        !checked_add(character_zone, symbols.stats_lookup_offset, lookup_base) ||
        !checked_add_signed(lookup_base, *displacement, lookup)) {
        return failure(
            CharacterReadError::invalid_pointer,
            "character stats lookup address overflows");
    }
    const auto stats = resolve_stats(reader, lookup, symbols, detail);
    if (!stats || *stats == 0U) {
        return failure(
            CharacterReadError::invalid_pointer,
            std::move(detail));
    }
    const auto health_base = read_vital_at(
        reader,
        *stats,
        symbols.stats_current_health_offset,
        symbols.stats_current_health_bytes,
        detail);
    const std::optional<std::int32_t> health_adjustment =
        symbols.apply_health_adjustment
            ? read_value_at<std::int32_t>(
                  reader,
                  character_zone,
                  symbols.health_adjustment_offset,
                  detail)
            : std::optional<std::int32_t>{0};
    const auto mana = read_value_at<std::int32_t>(
        reader, *stats, symbols.stats_current_mana_offset, detail);
    const auto maximum_health = read_vital_at(
        reader,
        local_player,
        symbols.player_maximum_health_offset,
        symbols.player_maximum_health_bytes,
        detail);
    const auto maximum_mana = read_vital_at(
        reader,
        local_player,
        symbols.player_maximum_mana_offset,
        symbols.player_maximum_mana_bytes,
        detail);
    if (!health_base || !health_adjustment || !mana || !maximum_health ||
        !maximum_mana) {
        return failure(CharacterReadError::read_failed, std::move(detail));
    }
    if ((*health_adjustment > 0 &&
         *health_base > std::numeric_limits<std::int64_t>::max() -
                            *health_adjustment) ||
        (*health_adjustment < 0 &&
         *health_base < std::numeric_limits<std::int64_t>::min() -
                            *health_adjustment)) {
        return failure(
            CharacterReadError::invalid_value,
            "health value overflows");
    }
    const std::int64_t health = *health_base + *health_adjustment;
    constexpr std::int64_t kMaximumVital = 100'000'000;
    if (health < 0 || health > kMaximumVital || *maximum_health < 0 ||
        *maximum_health > kMaximumVital ||
        (*maximum_health == 0 && health != 0) ||
        (*maximum_health > 0 && health > *maximum_health) || *mana < 0 ||
        *mana > kMaximumVital || *maximum_mana < 0 ||
        *maximum_mana > kMaximumVital ||
        (*maximum_mana == 0 && *mana != 0) ||
        (*maximum_mana > 0 && *mana > *maximum_mana)) {
        return failure(
            CharacterReadError::invalid_value,
            "character vital is outside exact-profile bounds");
    }

    std::uintptr_t character_base = 0U;
    std::uintptr_t character_base_origin = 0U;
    if (!checked_add(character_zone,
                     symbols.character_base_bias,
                     character_base_origin) ||
        !checked_add_signed(character_base_origin,
                            *displacement,
                            character_base)) {
        return failure(
            CharacterReadError::invalid_pointer,
            "character base address overflows");
    }
    std::uintptr_t profile_manager = 0U;
    if (!checked_add(character_base,
                     symbols.profile_manager_offset,
                     profile_manager)) {
        return failure(
            CharacterReadError::invalid_pointer,
            "profile manager address overflows");
    }
    const auto current_type = read_value_at<std::int32_t>(
        reader,
        profile_manager,
        symbols.profile_manager_current_type_offset,
        detail);
    if (!current_type) {
        return failure(CharacterReadError::read_failed, std::move(detail));
    }
    const auto profile = resolve_profile(
        reader, profile_manager, *current_type, symbols, detail);
    if (!profile || *profile == 0U) {
        return failure(
            CharacterReadError::invalid_pointer,
            std::move(detail));
    }
    std::uintptr_t inventory = 0U;
    if (!checked_add(*profile, symbols.inventory_container_offset, inventory)) {
        return failure(
            CharacterReadError::invalid_pointer,
            "inventory container address overflows");
    }
    const auto inventory_size = read_value_at<std::uint32_t>(
        reader, inventory, symbols.inventory_size_offset, detail);
    const auto inventory_data = read_value_at<std::uintptr_t>(
        reader, inventory, symbols.inventory_data_offset, detail);
    if (!inventory_size || !inventory_data) {
        return failure(CharacterReadError::read_failed, std::move(detail));
    }
    if (*inventory_size < kEquipmentSlots.size() ||
        *inventory_size > symbols.inventory_maximum_slots ||
        *inventory_data == 0U) {
        return failure(
            CharacterReadError::invalid_value,
            "equipment container is outside exact-profile bounds");
    }

    std::vector<EquipmentSlotSnapshot> equipment;
    equipment.reserve(kEquipmentSlots.size());
    std::array<std::uintptr_t, kEquipmentSlots.size()> item_pointers{};
    std::array<std::uintptr_t, kEquipmentSlots.size()> name_pointers{};
    for (std::size_t slot = 0U; slot < kEquipmentSlots.size(); ++slot) {
        std::uintptr_t entry = 0U;
        const std::size_t entry_offset = slot * symbols.inventory_entry_bytes;
        if (!checked_add(*inventory_data, entry_offset, entry)) {
            return failure(
                CharacterReadError::invalid_pointer,
                "equipment entry address overflows");
        }
        const auto item = read_value<std::uintptr_t>(reader, entry, detail);
        if (!item) {
            return failure(CharacterReadError::read_failed, std::move(detail));
        }
        item_pointers[slot] = *item;
        std::string item_name;
        if (*item != 0U) {
            const auto item_name_pointer = read_value_at<std::uintptr_t>(
                reader, *item, symbols.item_name_pointer_offset, detail);
            if (!item_name_pointer || *item_name_pointer == 0U) {
                return failure(
                    CharacterReadError::invalid_pointer,
                    "equipped item name is unavailable");
            }
            name_pointers[slot] = *item_name_pointer;
            const auto decoded = read_text(
                reader, *item_name_pointer, symbols.item_name_bytes, detail);
            if (!decoded) {
                return failure(
                    CharacterReadError::invalid_value, std::move(detail));
            }
            item_name = *decoded;
        }
        equipment.push_back({
            .slot = std::string(kEquipmentSlots[slot]),
            .item = std::move(item_name),
        });
    }

    std::optional<double> experience_percent;
    std::uintptr_t experience_address = 0U;
    std::string experience_detail;
    if (symbols.experience_percent_rva != 0U &&
        checked_add(process.image_base,
                    symbols.experience_percent_rva,
                    experience_address)) {
        const auto percent = read_value<float>(
            reader, experience_address, experience_detail);
        if (percent && std::isfinite(*percent) && *percent >= 0.0F &&
            *percent <= 100.0F) {
            experience_percent = static_cast<double>(*percent);
        }
    }

    std::optional<double> alternate_advancement_percent;
    std::optional<std::uint32_t> alternate_advancement_points;
    std::uintptr_t progression_cache = 0U;
    std::uintptr_t alternate_advancement_points_base = 0U;
    std::size_t alternate_advancement_points_offset = 0U;
    std::string progression_detail;
    if (symbols.progression_cache_rva != 0U &&
        checked_add(process.image_base,
                    symbols.progression_cache_rva,
                    progression_cache)) {
        const auto percent = read_value_at<float>(
            reader,
            progression_cache,
            symbols.alternate_advancement_percent_offset,
            progression_detail);
        if (symbols.unallocated_alternate_advancement_points_offset != 0U) {
            alternate_advancement_points_base = *profile;
            alternate_advancement_points_offset =
                symbols.unallocated_alternate_advancement_points_offset;
        } else {
            alternate_advancement_points_base = progression_cache;
            alternate_advancement_points_offset =
                symbols.alternate_advancement_points_offset;
        }
        const std::size_t points_width =
            symbols.unallocated_alternate_advancement_points_offset != 0U
                ? symbols.unallocated_alternate_advancement_points_bytes
                : sizeof(std::uint32_t);
        const auto points = read_unsigned_at(
            reader,
            alternate_advancement_points_base,
            alternate_advancement_points_offset,
            points_width,
            progression_detail);
        if (percent && std::isfinite(*percent) && *percent >= 0.0F &&
            *percent <= 100.0F) {
            alternate_advancement_percent =
                static_cast<double>(*percent);
        }
        if (points && *points <= 10'000'000U) {
            alternate_advancement_points = *points;
        }
    }

    const auto final_character = read_value<std::uintptr_t>(
        reader, local_character_global, detail);
    const auto final_name = read_text(
        reader, name_address, symbols.player_name_bytes, detail);
    const auto final_type_descriptor = read_value<std::uintptr_t>(
        reader, descriptor_address, detail);
    const auto final_displacement = read_value<std::int32_t>(
        reader, displacement_address, detail);
    const auto final_stats = resolve_stats(reader, lookup, symbols, detail);
    const auto final_health_base = final_stats
        ? read_vital_at(
              reader,
              *final_stats,
              symbols.stats_current_health_offset,
              symbols.stats_current_health_bytes,
              detail)
        : std::nullopt;
    const std::optional<std::int32_t> final_health_adjustment =
        symbols.apply_health_adjustment
            ? read_value_at<std::int32_t>(
                  reader,
                  character_zone,
                  symbols.health_adjustment_offset,
                  detail)
            : std::optional<std::int32_t>{0};
    const auto final_mana = final_stats
        ? read_value_at<std::int32_t>(
              reader, *final_stats, symbols.stats_current_mana_offset, detail)
        : std::nullopt;
    const auto final_maximum_health = read_vital_at(
        reader,
        local_player,
        symbols.player_maximum_health_offset,
        symbols.player_maximum_health_bytes,
        detail);
    const auto final_maximum_mana = read_vital_at(
        reader,
        local_player,
        symbols.player_maximum_mana_offset,
        symbols.player_maximum_mana_bytes,
        detail);
    const auto final_current_type = read_value_at<std::int32_t>(
        reader,
        profile_manager,
        symbols.profile_manager_current_type_offset,
        detail);
    const auto final_profile = final_current_type
        ? resolve_profile(
              reader, profile_manager, *final_current_type, symbols, detail)
        : std::nullopt;
    const auto final_inventory_size = read_value_at<std::uint32_t>(
        reader, inventory, symbols.inventory_size_offset, detail);
    const auto final_inventory_data = read_value_at<std::uintptr_t>(
        reader, inventory, symbols.inventory_data_offset, detail);
    if (!final_character || !final_name || !final_type_descriptor ||
        !final_displacement || !final_stats || !final_health_base ||
        !final_health_adjustment || !final_mana || !final_maximum_health ||
        !final_maximum_mana || !final_current_type || !final_profile ||
        !final_inventory_size || !final_inventory_data ||
        *final_character != *character_root ||
        *final_name != *name || *final_type_descriptor != *type_descriptor ||
        *final_displacement != *displacement || *final_stats != *stats ||
        *final_health_base != *health_base ||
        *final_health_adjustment != *health_adjustment ||
        *final_mana != *mana ||
        *final_maximum_health != *maximum_health ||
        *final_maximum_mana != *maximum_mana ||
        *final_current_type != *current_type || *final_profile != *profile ||
        *final_inventory_size != *inventory_size ||
        *final_inventory_data != *inventory_data) {
        return failure(
            CharacterReadError::inconsistent_snapshot,
            "character changed during the snapshot read");
    }
    for (std::size_t slot = 0U; slot < kEquipmentSlots.size(); ++slot) {
        std::uintptr_t entry = 0U;
        if (!checked_add(
                *final_inventory_data,
                slot * symbols.inventory_entry_bytes,
                entry)) {
            return failure(
                CharacterReadError::invalid_pointer,
                "equipment revalidation address overflows");
        }
        const auto final_item = read_value<std::uintptr_t>(
            reader, entry, detail);
        if (!final_item || *final_item != item_pointers[slot]) {
            return failure(
                CharacterReadError::inconsistent_snapshot,
                "equipment changed during the snapshot read");
        }
        if (*final_item == 0U) {
            continue;
        }
        const auto final_name_pointer = read_value_at<std::uintptr_t>(
            reader, *final_item, symbols.item_name_pointer_offset, detail);
        const auto final_item_name = final_name_pointer
            ? read_text(
                  reader,
                  *final_name_pointer,
                  symbols.item_name_bytes,
                  detail)
            : std::nullopt;
        if (!final_name_pointer || !final_item_name ||
            *final_name_pointer != name_pointers[slot] ||
            *final_item_name != equipment[slot].item) {
            return failure(
                CharacterReadError::inconsistent_snapshot,
                "equipped item changed during the snapshot read");
        }
    }
    if (experience_percent) {
        const auto final_percent = read_value<float>(
            reader, experience_address, experience_detail);
        if (!final_percent || !std::isfinite(*final_percent) ||
            *final_percent < 0.0F || *final_percent > 100.0F ||
            static_cast<double>(*final_percent) != *experience_percent) {
            experience_percent.reset();
        }
    }
    if (alternate_advancement_percent) {
        const auto final_percent = read_value_at<float>(
            reader,
            progression_cache,
            symbols.alternate_advancement_percent_offset,
            progression_detail);
        if (!final_percent || !std::isfinite(*final_percent) ||
            *final_percent < 0.0F || *final_percent > 100.0F ||
            static_cast<double>(*final_percent) !=
                *alternate_advancement_percent) {
            alternate_advancement_percent.reset();
        }
    }
    if (alternate_advancement_points) {
        const std::size_t points_width =
            symbols.unallocated_alternate_advancement_points_offset != 0U
                ? symbols.unallocated_alternate_advancement_points_bytes
                : sizeof(std::uint32_t);
        const auto final_points = read_unsigned_at(
            reader,
            alternate_advancement_points_base,
            alternate_advancement_points_offset,
            points_width,
            progression_detail);
        if (!final_points || *final_points > 10'000'000U ||
            *final_points != *alternate_advancement_points) {
            alternate_advancement_points.reset();
        }
    }
    return {
        .snapshot = CharacterSnapshot{
            .state = PlayerSnapshotState::in_world,
            .name = *name,
            .health = {.current = health, .maximum = *maximum_health},
            .mana = {.current = *mana, .maximum = *maximum_mana},
            .experience_percent = experience_percent,
            .alternate_advancement_percent =
                alternate_advancement_percent,
            .alternate_advancement_points = alternate_advancement_points,
            .equipment = std::move(equipment),
            .detail = "Live read-only character snapshot",
        },
        .error = CharacterReadError::none,
        .detail = {},
    };
}

}  // namespace plazmic
