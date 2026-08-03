#pragma once

#include "model/player_snapshot.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace plazmic {

struct VitalSnapshot {
    std::int64_t current{};
    std::optional<std::int64_t> maximum;

    [[nodiscard]] std::optional<double> percentage() const {
        if (!maximum || *maximum <= 0 || current < 0) {
            return std::nullopt;
        }
        return static_cast<double>(current) * 100.0 /
               static_cast<double>(*maximum);
    }
};

struct EquipmentSlotSnapshot {
    std::string slot;
    std::string item;
};

struct CharacterSnapshot {
    PlayerSnapshotState state{PlayerSnapshotState::unavailable};
    std::string name;
    VitalSnapshot health;
    VitalSnapshot mana;
    std::vector<EquipmentSlotSnapshot> equipment;
    std::string detail{"Character information unavailable"};

    [[nodiscard]] bool available() const {
        return state == PlayerSnapshotState::in_world;
    }
};

}  // namespace plazmic
