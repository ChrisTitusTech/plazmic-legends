#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace plazmic {

struct GameStateSymbols {
    std::uintptr_t local_player_pointer_rva;
    std::uintptr_t world_data_pointer_rva;
    std::size_t player_y_offset;
    std::size_t player_x_offset;
    std::size_t player_z_offset;
    std::size_t player_heading_offset;
    std::size_t player_zone_id_offset;
    std::size_t zone_table_offset;
    std::size_t zone_entry_id_offset;
    std::size_t zone_short_name_offset;
    std::size_t zone_short_name_bytes;
    std::uint32_t zone_id_mask;
    std::uint32_t maximum_zone_id;
};

struct SpawnSymbols {
    std::size_t next_offset;
    std::size_t previous_offset;
    std::size_t name_offset;
    std::size_t name_bytes;
    std::size_t type_offset;
    std::size_t id_offset;
    std::size_t level_offset;
    std::size_t y_offset;
    std::size_t x_offset;
    std::size_t z_offset;
    std::size_t record_bytes;
    std::size_t maximum_count;
};

struct CharacterSymbols {
    std::uintptr_t local_character_pointer_rva;
    std::size_t player_name_offset;
    std::size_t player_name_bytes;
    std::size_t character_zone_offset;
    std::size_t character_type_descriptor_offset;
    std::size_t type_descriptor_displacement_offset;
    std::size_t stats_lookup_offset;
    std::size_t stats_lookup_key_offset;
    std::size_t stats_record_key_offset;
    std::size_t stats_record_value_offset;
    std::size_t stats_record_next_offset;
    std::size_t stats_maximum_records;
    std::size_t stats_current_mana_offset;
    std::size_t stats_current_health_offset;
    std::size_t health_adjustment_offset;
    std::size_t player_maximum_mana_offset;
    std::size_t character_base_bias;
    std::size_t profile_manager_offset;
    std::size_t profile_manager_current_type_offset;
    std::size_t profile_list_type_offset;
    std::size_t profile_list_first_profile_offset;
    std::size_t profile_list_next_offset;
    std::size_t profile_list_maximum_count;
    std::size_t inventory_container_offset;
    std::size_t inventory_size_offset;
    std::size_t inventory_data_offset;
    std::size_t inventory_entry_bytes;
    std::size_t inventory_maximum_slots;
    std::size_t item_name_pointer_offset;
    std::size_t item_name_bytes;
};

struct ClientProfile {
    std::string_view id;
    std::string_view sha256;
    std::uint16_t machine;
    std::uint32_t timestamp;
    std::uint16_t optional_magic;
    std::uint32_t image_size;
    GameStateSymbols game_state;
    SpawnSymbols spawns;
    CharacterSymbols character;
};

[[nodiscard]] const ClientProfile& legends_reference_profile();
[[nodiscard]] const ClientProfile* select_client_profile(
    std::string_view sha256);

}  // namespace plazmic
