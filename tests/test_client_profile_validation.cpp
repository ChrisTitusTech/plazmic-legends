#include "game/client_profile.h"
#include "game/client_profile_validation.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

}  // namespace

int main() {
    const auto profiles = plazmic::known_client_profiles();
    require(profiles.size() == 5U,
            "known profile registry has an unexpected size");
    require(plazmic::validate_known_client_profiles().empty(),
            "known profile registry failed its contract");

    for (const plazmic::ClientProfile* profile : profiles) {
        require(profile != nullptr, "known profile registry contains null");
        const auto validation =
            plazmic::validate_client_profile(*profile);
        require(validation && validation.identity_available &&
                    validation.player_available &&
                    validation.spawns_available,
                "required profile capability failed validation");
        require(validation.character_available ==
                    profile->character_snapshot_supported,
                "character capability did not match the profile gate");
        require(plazmic::select_client_profile(profile->sha256) == profile,
                "known profile did not select by exact digest");
    }
    require(!plazmic::validate_client_profile(*profiles[0])
                 .progression_available &&
                !plazmic::validate_client_profile(*profiles[1])
                     .progression_available &&
                !plazmic::validate_client_profile(*profiles[2])
                     .progression_available &&
                plazmic::validate_client_profile(*profiles[3])
                    .progression_available &&
                !plazmic::validate_client_profile(*profiles[4])
                     .progression_available,
            "progression capability did not remain exact-profile scoped");
    require(plazmic::select_client_profile(
                "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff") ==
                nullptr,
            "unknown digest selected a compatibility profile");

    plazmic::ClientProfile invalid = *profiles.front();
    invalid.sha256 =
        "97EE793D491930AC97F91E5E26FAC16D84D17FF24AFCD24D5390D256E7045661";
    require(!plazmic::validate_client_profile(invalid),
            "uppercase digest passed the exact identity contract");

    invalid = *profiles.front();
    invalid.game_state.world_data_pointer_rva = invalid.image_size;
    require(!plazmic::validate_client_profile(invalid),
            "out-of-image world RVA passed the profile contract");

    invalid = *profiles.front();
    invalid.spawns.record_bytes = invalid.spawns.level_offset;
    require(!plazmic::validate_client_profile(invalid),
            "truncated spawn record passed the profile contract");

    invalid = *profiles[1];
    invalid.character_snapshot_supported = true;
    require(!plazmic::validate_client_profile(invalid),
            "partial character capability was enabled");

    invalid = *profiles.back();
    invalid.character.progression_cache_rva = 0x100U;
    require(!plazmic::validate_client_profile(invalid),
            "partial progression capability was enabled");

    return 0;
}
