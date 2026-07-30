#pragma once

#include <string>

namespace plazmic {

enum class PlayerSnapshotState {
    unavailable,
    in_world,
};

struct PlayerSnapshot {
    PlayerSnapshotState state{PlayerSnapshotState::unavailable};
    std::string zone;
    double x{};
    double y{};
    double z{};
    double heading_degrees{};
    std::string detail;

    [[nodiscard]] bool available() const {
        return state == PlayerSnapshotState::in_world;
    }
};

}  // namespace plazmic
