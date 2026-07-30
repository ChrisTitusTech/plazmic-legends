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
