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
};

}  // namespace

const ClientProfile& legends_reference_profile() {
    return kLegendsReferenceProfile;
}

const ClientProfile* select_client_profile(std::string_view sha256) {
    if (sha256 == kLegendsReferenceProfile.sha256) {
        return &kLegendsReferenceProfile;
    }
    return nullptr;
}

}  // namespace plazmic
