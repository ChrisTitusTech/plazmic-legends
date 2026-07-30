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

struct ClientProfile {
    std::string_view id;
    std::string_view sha256;
    std::uint16_t machine;
    std::uint32_t timestamp;
    std::uint16_t optional_magic;
    std::uint32_t image_size;
    GameStateSymbols game_state;
};

[[nodiscard]] const ClientProfile& legends_reference_profile();
[[nodiscard]] const ClientProfile* select_client_profile(
    std::string_view sha256);

}  // namespace plazmic
