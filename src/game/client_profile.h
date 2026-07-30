#pragma once

#include <cstdint>
#include <string_view>

namespace plazmic {

struct ClientProfile {
    std::string_view id;
    std::string_view sha256;
    std::uint16_t machine;
    std::uint32_t timestamp;
    std::uint16_t optional_magic;
    std::uint32_t image_size;
};

[[nodiscard]] const ClientProfile& legends_reference_profile();
[[nodiscard]] const ClientProfile* select_client_profile(
    std::string_view sha256);

}  // namespace plazmic
