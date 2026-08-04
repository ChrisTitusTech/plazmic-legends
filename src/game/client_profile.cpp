#include "game/client_profile.h"

namespace plazmic {
namespace {

constexpr ClientProfile kLegendsReferenceProfile{
    .id = "legends-2026-07-29",
    .sha256 =
        "97ee793d491930ac97f91e5e26fac16d84d17ff24afcd24d5390d256e7045661",
    .machine = 0x8664U,
    .timestamp = 0x6a6a2851U,
    .optional_magic = 0x20bU,
    .image_size = 0x16c1000U,
    .game_state =
        {
            .local_player_pointer_rva = 0x00f07388U,
            .world_data_pointer_rva = 0x00f07378U,
            .player_y_offset = 0x78U,
            .player_x_offset = 0x74U,
            .player_z_offset = 0x7cU,
            .player_heading_offset = 0x94U,
            .player_zone_id_offset = 0x2fcU,
            .zone_table_offset = 0x30U,
            .zone_entry_id_offset = 0x0cU,
            .zone_short_name_offset = 0x10U,
            .zone_short_name_bytes = 64U,
            .zone_id_mask = 0x7fffU,
            .maximum_zone_id = 1000U,
        },
    .spawns =
        {
            .next_offset = 0x08U,
            .previous_offset = 0x10U,
            .name_offset = 0x0b8U,
            .name_bytes = 64U,
            .type_offset = 0x139U,
            .id_offset = 0x178U,
            .level_offset = 0x6b4U,
            .y_offset = 0x78U,
            .x_offset = 0x74U,
            .z_offset = 0x7cU,
            .record_bytes = 0x6b5U,
            .maximum_count = 2048U,
        },
    .character =
        {
            .local_character_pointer_rva = 0x00f074d0U,
            .player_name_offset = 0x0b8U,
            .player_name_bytes = 64U,
            .character_zone_offset = 0x2810U,
            .character_type_descriptor_offset = 0x08U,
            .type_descriptor_displacement_offset = 0x04U,
            .stats_lookup_offset = 0x10U,
            .stats_lookup_key_offset = 0x08U,
            .stats_record_key_offset = 0x00U,
            .stats_record_value_offset = 0x08U,
            .stats_record_next_offset = 0x18U,
            .stats_maximum_records = 64U,
            .stats_current_mana_offset = 0x2764U,
            .stats_current_health_offset = 0x2770U,
            .health_adjustment_offset = 0x28U,
            .player_maximum_mana_offset = 0x2d0U,
            .character_base_bias = 0x08U,
            .profile_manager_offset = 0x08U,
            .profile_manager_current_type_offset = 0x08U,
            .profile_list_type_offset = 0x00U,
            .profile_list_first_profile_offset = 0x08U,
            .profile_list_next_offset = 0x18U,
            .profile_list_maximum_count = 8U,
            .inventory_container_offset = 0x20U,
            .inventory_size_offset = 0x00U,
            .inventory_data_offset = 0x08U,
            .inventory_entry_bytes = 0x10U,
            .inventory_maximum_slots = 36U,
            .item_name_pointer_offset = 0x80U,
            .item_name_bytes = 64U,
        },
};

constexpr ClientProfile kLegendsAugustProfile{
    .id = "legends-2026-08-03",
    .sha256 =
        "f8af4e704746118f8dd94b688e585bc5c37c3d085da620136bcacad5486145ac",
    .machine = 0x8664U,
    .timestamp = 0x6a711da7U,
    .optional_magic = 0x20bU,
    .image_size = 0x16c0000U,
    .game_state =
        {
            .local_player_pointer_rva = 0x00f05ff8U,
            .world_data_pointer_rva = 0x00f05fe8U,
            .player_y_offset = 0x78U,
            .player_x_offset = 0x74U,
            .player_z_offset = 0x7cU,
            .player_heading_offset = 0x94U,
            .player_zone_id_offset = 0x4a0U,
            .zone_table_offset = 0x30U,
            .zone_entry_id_offset = 0x0cU,
            .zone_short_name_offset = 0x10U,
            .zone_short_name_bytes = 64U,
            .zone_id_mask = 0x7fffU,
            .maximum_zone_id = 1000U,
        },
    .spawns =
        {
            .next_offset = 0x08U,
            .previous_offset = 0x10U,
            .name_offset = 0x0b8U,
            .name_bytes = 64U,
            .type_offset = 0x139U,
            .id_offset = 0x178U,
            .level_offset = 0x620U,
            .y_offset = 0x78U,
            .x_offset = 0x74U,
            .z_offset = 0x7cU,
            .record_bytes = 0x621U,
            .maximum_count = 2048U,
        },
    .character =
        {
            .local_character_pointer_rva = 0x00f06140U,
            .player_name_offset = 0x0b8U,
            .player_name_bytes = 64U,
            .character_zone_offset = 0x2810U,
            .character_type_descriptor_offset = 0x08U,
            .type_descriptor_displacement_offset = 0x04U,
            .stats_lookup_offset = 0x10U,
            .stats_lookup_key_offset = 0x08U,
            .stats_record_key_offset = 0x00U,
            .stats_record_value_offset = 0x08U,
            .stats_record_next_offset = 0x18U,
            .stats_maximum_records = 64U,
            .stats_current_mana_offset = 0x2764U,
            .stats_current_health_offset = 0x2770U,
            .health_adjustment_offset = 0x28U,
            .player_maximum_mana_offset = 0x2bcU,
            .character_base_bias = 0x08U,
            .profile_manager_offset = 0x08U,
            .profile_manager_current_type_offset = 0x08U,
            .profile_list_type_offset = 0x00U,
            .profile_list_first_profile_offset = 0x08U,
            .profile_list_next_offset = 0x18U,
            .profile_list_maximum_count = 8U,
            .inventory_container_offset = 0x20U,
            .inventory_size_offset = 0x00U,
            .inventory_data_offset = 0x08U,
            .inventory_entry_bytes = 0x10U,
            .inventory_maximum_slots = 36U,
            .item_name_pointer_offset = 0xa8U,
            .item_name_bytes = 64U,
        },
};

}  // namespace

const ClientProfile& legends_reference_profile() {
    return kLegendsReferenceProfile;
}

const ClientProfile* select_client_profile(std::string_view sha256) {
    if (sha256 == kLegendsReferenceProfile.sha256) {
        return &kLegendsReferenceProfile;
    }
    if (sha256 == kLegendsAugustProfile.sha256) {
        return &kLegendsAugustProfile;
    }
    return nullptr;
}

}  // namespace plazmic
