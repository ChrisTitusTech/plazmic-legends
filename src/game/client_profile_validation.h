#pragma once

#include "game/client_profile.h"

#include <string>
#include <vector>

namespace plazmic {

struct ClientProfileValidation {
    bool identity_available{false};
    bool player_available{false};
    bool spawns_available{false};
    bool character_available{false};
    bool progression_available{false};
    std::vector<std::string> errors;

    [[nodiscard]] explicit operator bool() const {
        return errors.empty();
    }
};

[[nodiscard]] ClientProfileValidation validate_client_profile(
    const ClientProfile& profile);

[[nodiscard]] std::vector<std::string> validate_known_client_profiles();

}  // namespace plazmic
