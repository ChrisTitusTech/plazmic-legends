#include "game/client_profile_validation.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <string_view>

namespace plazmic {
namespace {

constexpr std::size_t kMaximumNameBytes = 65U;
constexpr std::size_t kMaximumSpawnCount = 2048U;
constexpr std::size_t kMaximumEquipmentSlots = 36U;
constexpr std::size_t kMaximumCharacterProfileFieldOffset = 64U * 1024U;

void reject(bool condition,
            std::string_view detail,
            ClientProfileValidation& result) {
    if (condition) {
        result.errors.emplace_back(detail);
    }
}

bool lowercase_sha256(std::string_view digest) {
    return digest.size() == 64U &&
           std::ranges::all_of(digest, [](unsigned char value) {
               return std::isdigit(value) != 0 ||
                      (value >= static_cast<unsigned char>('a') &&
                       value <= static_cast<unsigned char>('f'));
           });
}

bool field_fits(std::size_t offset,
                std::size_t width,
                std::size_t record_bytes) {
    return width <= record_bytes && offset <= record_bytes - width;
}

bool image_rva(std::uintptr_t rva, std::uint32_t image_size) {
    return rva != 0U && rva < static_cast<std::uintptr_t>(image_size);
}

bool image_range(std::uintptr_t rva,
                 std::size_t width,
                 std::uint32_t image_size) {
    return rva != 0U && width <= image_size &&
           rva <= static_cast<std::uintptr_t>(image_size - width);
}

bool image_field_range(std::uintptr_t base_rva,
                       std::size_t offset,
                       std::size_t width,
                       std::uint32_t image_size) {
    if (base_rva == 0U ||
        offset > std::numeric_limits<std::uintptr_t>::max() - base_rva) {
        return false;
    }
    return image_range(base_rva + offset, width, image_size);
}

}  // namespace

ClientProfileValidation validate_client_profile(
    const ClientProfile& profile) {
    ClientProfileValidation result;

    reject(profile.id.empty(), "profile ID is empty", result);
    reject(!lowercase_sha256(profile.sha256),
           "profile SHA-256 is not lowercase hexadecimal", result);
    reject(profile.machine == 0U, "profile machine is zero", result);
    reject(profile.timestamp == 0U, "profile timestamp is zero", result);
    reject(profile.optional_magic == 0U,
           "profile optional-header magic is zero", result);
    reject(profile.image_size == 0U, "profile image size is zero", result);
    result.identity_available = result.errors.empty();

    const GameStateSymbols& game = profile.game_state;
    const bool valid_player =
        image_rva(game.local_player_pointer_rva, profile.image_size) &&
        image_rva(game.world_data_pointer_rva, profile.image_size) &&
        game.zone_short_name_bytes > 0U &&
        game.zone_short_name_bytes <= kMaximumNameBytes &&
        game.zone_id_mask != 0U && game.maximum_zone_id > 0U &&
        game.maximum_zone_id <= game.zone_id_mask;
    reject(!valid_player, "player or zone profile is incomplete", result);
    result.player_available = valid_player;

    const SpawnSymbols& spawns = profile.spawns;
    const bool valid_spawns =
        spawns.record_bytes > 0U && spawns.maximum_count > 0U &&
        spawns.maximum_count <= kMaximumSpawnCount &&
        spawns.name_bytes > 0U && spawns.name_bytes <= kMaximumNameBytes &&
        field_fits(spawns.next_offset, sizeof(std::uintptr_t),
                   spawns.record_bytes) &&
        field_fits(spawns.previous_offset, sizeof(std::uintptr_t),
                   spawns.record_bytes) &&
        field_fits(spawns.name_offset, spawns.name_bytes,
                   spawns.record_bytes) &&
        field_fits(spawns.type_offset, sizeof(std::uint8_t),
                   spawns.record_bytes) &&
        field_fits(spawns.id_offset, sizeof(std::uint32_t),
                   spawns.record_bytes) &&
        field_fits(spawns.level_offset, sizeof(std::uint8_t),
                   spawns.record_bytes) &&
        field_fits(spawns.y_offset, sizeof(float), spawns.record_bytes) &&
        field_fits(spawns.x_offset, sizeof(float), spawns.record_bytes) &&
        field_fits(spawns.z_offset, sizeof(float), spawns.record_bytes);
    reject(!valid_spawns, "spawn profile is incomplete or unbounded", result);
    result.spawns_available = valid_spawns;

    const CharacterSymbols& character = profile.character;
    const bool valid_character =
        image_rva(character.local_character_pointer_rva,
                  profile.image_size) &&
        character.player_name_bytes > 0U &&
        character.player_name_bytes <= kMaximumNameBytes &&
        character.stats_maximum_records > 0U &&
        character.profile_list_maximum_count > 0U &&
        character.inventory_entry_bytes > 0U &&
        character.inventory_maximum_slots > 0U &&
        character.inventory_maximum_slots <= kMaximumEquipmentSlots &&
        character.item_name_bytes > 0U &&
        character.item_name_bytes <= kMaximumNameBytes &&
        character.stats_current_mana_offset != 0U &&
        character.stats_current_health_offset != 0U &&
        character.player_maximum_health_offset != 0U &&
        character.player_maximum_mana_offset != 0U;
    reject(profile.character_snapshot_supported && !valid_character,
           "enabled character profile is incomplete", result);
    result.character_available =
        profile.character_snapshot_supported && valid_character;

    reject(character.experience_percent_rva != 0U &&
               (!profile.character_snapshot_supported ||
                !image_range(character.experience_percent_rva,
                             sizeof(float),
                             profile.image_size)),
           "experience profile lacks character support or is out of image",
           result);
    result.experience_available =
        profile.character_snapshot_supported &&
        image_range(character.experience_percent_rva,
                    sizeof(float),
                    profile.image_size);

    const bool any_progression = character.progression_cache_rva != 0U ||
                                 character.alternate_advancement_percent_offset !=
                                     0U ||
                                 character.alternate_advancement_points_offset !=
                                     0U ||
                                 character
                                         .unallocated_alternate_advancement_points_offset !=
                                     0U;
    const bool cache_points =
        character.alternate_advancement_points_offset != 0U &&
        image_field_range(character.progression_cache_rva,
                          character.alternate_advancement_points_offset,
                          sizeof(std::uint32_t),
                          profile.image_size);
    const bool unallocated_points =
        character.unallocated_alternate_advancement_points_offset != 0U &&
        character.unallocated_alternate_advancement_points_offset <=
            kMaximumCharacterProfileFieldOffset - sizeof(std::uint32_t);
    const bool complete_progression =
        character.alternate_advancement_percent_offset != 0U &&
        image_field_range(character.progression_cache_rva,
                          character.alternate_advancement_percent_offset,
                          sizeof(float),
                          profile.image_size) &&
        cache_points != unallocated_points;
    reject(any_progression &&
               (!profile.character_snapshot_supported ||
                !complete_progression),
           "progression profile is partial or lacks character support",
           result);
    result.progression_available =
        profile.character_snapshot_supported && complete_progression;

    return result;
}

std::vector<std::string> validate_known_client_profiles() {
    std::vector<std::string> errors;
    std::set<std::string_view> ids;
    std::set<std::string_view> digests;
    for (const ClientProfile* profile : known_client_profiles()) {
        if (profile == nullptr) {
            errors.emplace_back("known profile registry contains null");
            continue;
        }
        const ClientProfileValidation validation =
            validate_client_profile(*profile);
        for (const std::string& detail : validation.errors) {
            errors.emplace_back(std::string(profile->id) + ": " + detail);
        }
        if (!ids.insert(profile->id).second) {
            errors.emplace_back(std::string(profile->id) +
                                ": duplicate profile ID");
        }
        if (!digests.insert(profile->sha256).second) {
            errors.emplace_back(std::string(profile->id) +
                                ": duplicate profile SHA-256");
        }
        if (select_client_profile(profile->sha256) != profile) {
            errors.emplace_back(std::string(profile->id) +
                                ": registry selection is inconsistent");
        }
    }
    return errors;
}

}  // namespace plazmic
