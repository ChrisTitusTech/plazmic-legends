#include "game/game_state_reader.h"

#include "common/sha256.h"
#include "game/spawn_reader.h"
#include "integration/process_reader.h"
#include "map/map_parser.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <exception>
#include <limits>
#include <span>
#include <string_view>
#include <utility>

namespace plazmic {
namespace {

template <typename Value>
std::optional<Value> read_value(const ProcessMemoryReader& reader,
                                std::uintptr_t address,
                                std::string& detail) {
    std::array<std::byte, sizeof(Value)> bytes{};
    const ProcessReadResult result = reader.read_exact(address, bytes);
    if (!result) {
        detail = result.detail;
        return std::nullopt;
    }
    Value value{};
    std::memcpy(&value, bytes.data(), sizeof(value));
    return value;
}

bool checked_add(std::uintptr_t base,
                 std::size_t offset,
                 std::uintptr_t& result) {
    if (offset > std::numeric_limits<std::uintptr_t>::max() - base) {
        return false;
    }
    result = base + offset;
    return true;
}

bool checked_index(std::uintptr_t base,
                   std::size_t offset,
                   std::uint32_t index,
                   std::uintptr_t& result) {
    constexpr std::size_t kPointerBytes = sizeof(std::uintptr_t);
    if (index >
        (std::numeric_limits<std::size_t>::max() - offset) / kPointerBytes) {
        return false;
    }
    return checked_add(
        base, offset + static_cast<std::size_t>(index) * kPointerBytes,
        result);
}

GameStateReadResult failure(GameStateReadError error, std::string detail) {
    return {
        .snapshot = std::nullopt,
        .spawns = std::nullopt,
        .error = error,
        .detail = std::move(detail),
    };
}

std::optional<std::string> read_zone_name(
    const ProcessMemoryReader& reader,
    std::uintptr_t zone_entry,
    const GameStateSymbols& symbols,
    std::string& detail) {
    if (symbols.zone_short_name_bytes == 0U ||
        symbols.zone_short_name_bytes > 65U) {
        detail = "profile zone-name bound is invalid";
        return std::nullopt;
    }
    std::uintptr_t name_address = 0;
    if (!checked_add(
            zone_entry, symbols.zone_short_name_offset, name_address)) {
        detail = "zone-name address overflows";
        return std::nullopt;
    }

    std::array<std::byte, 65> bytes{};
    const auto output =
        std::span<std::byte>(bytes).first(symbols.zone_short_name_bytes);
    const ProcessReadResult result = reader.read_exact(name_address, output);
    if (!result) {
        detail = result.detail;
        return std::nullopt;
    }
    const auto terminator = std::find(bytes.begin(), bytes.begin() +
        static_cast<std::ptrdiff_t>(symbols.zone_short_name_bytes),
        std::byte{0});
    if (terminator == bytes.begin() ||
        terminator == bytes.begin() +
            static_cast<std::ptrdiff_t>(symbols.zone_short_name_bytes)) {
        detail = "zone short name is empty or unterminated";
        return std::nullopt;
    }
    const std::string name(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::size_t>(terminator - bytes.begin()));
    if (!valid_zone_short_name(name)) {
        detail = "zone short name failed bounded path validation";
        return std::nullopt;
    }
    return name;
}

bool valid_player_value(double value) {
    return std::isfinite(value) && std::abs(value) <= 1000000.0;
}

}  // namespace

GameStateReadResult read_game_state(
    const ClientProcess& process,
    const GameStateSymbols& symbols,
    const SpawnSymbols& spawn_symbols) {
    if (process.pid <= 0 || process.image_base == 0U ||
        symbols.local_player_pointer_rva == 0U ||
        symbols.world_data_pointer_rva == 0U ||
        symbols.zone_id_mask == 0U ||
        symbols.maximum_zone_id == 0U) {
        return failure(
            GameStateReadError::invalid_profile,
            "game-state profile is incomplete");
    }

    const ProcessMemoryReader reader(process);
    std::string detail;
    std::uintptr_t local_pointer_address = 0;
    std::uintptr_t world_pointer_address = 0;
    if (!checked_add(
            process.image_base, symbols.local_player_pointer_rva,
            local_pointer_address) ||
        !checked_add(
            process.image_base, symbols.world_data_pointer_rva,
            world_pointer_address)) {
        return failure(
            GameStateReadError::invalid_profile,
            "profile global address overflows");
    }

    const auto local_player =
        read_value<std::uintptr_t>(reader, local_pointer_address, detail);
    if (!local_player) {
        return failure(GameStateReadError::read_failed, std::move(detail));
    }
    if (*local_player == 0U) {
        return failure(
            GameStateReadError::not_in_world,
            "local player is unavailable");
    }
    const auto world_data =
        read_value<std::uintptr_t>(reader, world_pointer_address, detail);
    if (!world_data) {
        return failure(GameStateReadError::read_failed, std::move(detail));
    }
    if (*world_data == 0U) {
        return failure(
            GameStateReadError::not_in_world,
            "world data is unavailable");
    }

    auto player_address = [local_player](std::size_t offset) {
        std::uintptr_t result = 0;
        return checked_add(*local_player, offset, result)
                   ? std::optional<std::uintptr_t>(result)
                   : std::nullopt;
    };
    const auto zone_id_address =
        player_address(symbols.player_zone_id_offset);
    const auto y_address = player_address(symbols.player_y_offset);
    const auto x_address = player_address(symbols.player_x_offset);
    const auto z_address = player_address(symbols.player_z_offset);
    const auto heading_address =
        player_address(symbols.player_heading_offset);
    if (!zone_id_address || !y_address || !x_address || !z_address ||
        !heading_address) {
        return failure(
            GameStateReadError::invalid_pointer,
            "player field address overflows");
    }

    const auto raw_zone =
        read_value<std::uint32_t>(reader, *zone_id_address, detail);
    const auto y = read_value<float>(reader, *y_address, detail);
    const auto x = read_value<float>(reader, *x_address, detail);
    const auto z = read_value<float>(reader, *z_address, detail);
    const auto heading =
        read_value<float>(reader, *heading_address, detail);
    if (!raw_zone || !y || !x || !z || !heading) {
        return failure(GameStateReadError::read_failed, std::move(detail));
    }

    const std::uint32_t zone_id = *raw_zone & symbols.zone_id_mask;
    if (zone_id == 0U || zone_id > symbols.maximum_zone_id) {
        return failure(
            GameStateReadError::not_in_world,
            "player zone ID is outside the in-world range");
    }
    std::uintptr_t zone_pointer_address = 0;
    if (!checked_index(
            *world_data, symbols.zone_table_offset, zone_id,
            zone_pointer_address)) {
        return failure(
            GameStateReadError::invalid_pointer,
            "zone-table address overflows");
    }
    const auto zone_entry =
        read_value<std::uintptr_t>(reader, zone_pointer_address, detail);
    if (!zone_entry) {
        return failure(GameStateReadError::read_failed, std::move(detail));
    }
    if (*zone_entry == 0U) {
        return failure(
            GameStateReadError::invalid_zone,
            "zone table has no entry for the live zone ID");
    }
    std::uintptr_t entry_id_address = 0;
    if (!checked_add(
            *zone_entry, symbols.zone_entry_id_offset, entry_id_address)) {
        return failure(
            GameStateReadError::invalid_pointer,
            "zone-entry ID address overflows");
    }
    const auto entry_id =
        read_value<std::uint32_t>(reader, entry_id_address, detail);
    if (!entry_id || *entry_id != zone_id) {
        return failure(
            GameStateReadError::invalid_zone,
            "zone-table entry does not match the live zone ID");
    }
    const auto zone =
        read_zone_name(reader, *zone_entry, symbols, detail);
    if (!zone) {
        return failure(GameStateReadError::invalid_zone, std::move(detail));
    }

    if (!valid_player_value(*x) || !valid_player_value(*y) ||
        !valid_player_value(*z) || !std::isfinite(*heading) ||
        *heading < 0.0F || *heading >= 512.0F) {
        return failure(
            GameStateReadError::invalid_player,
            "player coordinates or heading are outside profile bounds");
    }

    PlayerSnapshot player{
        .state = PlayerSnapshotState::in_world,
        .zone = *zone,
        .x = static_cast<double>(*x),
        .y = static_cast<double>(*y),
        .z = static_cast<double>(*z),
        .heading_degrees =
            static_cast<double>(*heading) * (360.0 / 512.0),
        .detail = "Live read-only player snapshot",
    };
    SpawnReadResult spawn_result = read_spawn_collection(
        process, *local_player, spawn_symbols, player);
    if (!spawn_result) {
        GameStateReadError error = GameStateReadError::invalid_spawns;
        if (spawn_result.error ==
            SpawnReadError::inconsistent_collection) {
            error = GameStateReadError::inconsistent_snapshot;
        } else if (spawn_result.error == SpawnReadError::read_failed) {
            error = GameStateReadError::read_failed;
        } else if (spawn_result.error ==
                   SpawnReadError::invalid_profile) {
            error = GameStateReadError::invalid_profile;
        }
        return failure(error, std::move(spawn_result.detail));
    }

    const auto final_local =
        read_value<std::uintptr_t>(reader, local_pointer_address, detail);
    const auto final_zone =
        read_value<std::uint32_t>(reader, *zone_id_address, detail);
    if (!final_local || !final_zone) {
        return failure(GameStateReadError::read_failed, std::move(detail));
    }
    if (*final_local != *local_player ||
        (*final_zone & symbols.zone_id_mask) != zone_id) {
        return failure(
            GameStateReadError::inconsistent_snapshot,
            "player or zone changed during the snapshot read");
    }

    return {
        .snapshot = std::move(player),
        .spawns = std::move(spawn_result.snapshot),
        .error = GameStateReadError::none,
        .detail = {},
    };
}

LiveGameStateProbe::LiveGameStateProbe(
    std::filesystem::path client,
    const ClientProfile* profile)
    : client_(std::move(client)), profile_(profile) {
    if (profile_ != nullptr) {
        file_monitor_.emplace(client_, std::string(profile_->sha256));
    }
}

GameStateReadResult LiveGameStateProbe::refresh() {
    if (profile_ == nullptr) {
        return failure(
            GameStateReadError::invalid_profile,
            "client has no exact compatibility profile");
    }
    const ClientFileCheck file = file_monitor_->check();
    if (!file) {
        process_.reset();
        return failure(
            GameStateReadError::invalid_profile, file.detail);
    }

    const auto now = std::chrono::steady_clock::now();
    if (process_ && !is_process_alive(process_->pid)) {
        process_.reset();
        next_discovery_check_ = now;
    }

    constexpr auto kDiscoveryInterval = std::chrono::seconds(1);
    if (now >= next_discovery_check_) {
        next_discovery_check_ = now + kDiscoveryInterval;
        DiscoveryResult discovery = discover_client_process(client_);
        if (!discovery) {
            last_discovery_detail_ = discovery.detail;
            process_.reset();
            return failure(
                GameStateReadError::process_unavailable,
                discovery.detail);
        }

        const bool new_process =
            !process_ || process_->pid != discovery.process.pid;
        if (new_process) {
            std::string digest;
            try {
                digest = sha256_file(client_);
            } catch (const std::exception& error) {
                process_.reset();
                last_discovery_detail_ =
                    "cannot revalidate client file: " +
                    std::string(error.what());
                return failure(
                    GameStateReadError::invalid_profile,
                    last_discovery_detail_);
            }
            if (digest != profile_->sha256) {
                process_.reset();
                last_discovery_detail_ =
                    "client file no longer matches the exact profile";
                return failure(
                    GameStateReadError::invalid_profile,
                    last_discovery_detail_);
            }

            const ProcessMemoryReader reader(discovery.process);
            RemotePeIdentity identity{};
            std::string identity_error;
            if (!read_remote_pe_identity(
                    reader, discovery.process.image_base, identity,
                    identity_error)) {
                process_.reset();
                last_discovery_detail_ =
                    "cannot validate live client identity: " +
                    identity_error;
                return failure(
                    GameStateReadError::read_failed,
                    last_discovery_detail_);
            }
            if (identity.machine != profile_->machine ||
                identity.timestamp != profile_->timestamp ||
                identity.optional_magic != profile_->optional_magic ||
                identity.image_base != discovery.process.image_base ||
                identity.image_size != profile_->image_size) {
                process_.reset();
                last_discovery_detail_ =
                    "live client identity does not match the exact profile";
                return failure(
                    GameStateReadError::invalid_profile,
                    last_discovery_detail_);
            }
        }
        process_ = std::move(discovery.process);
        last_discovery_detail_.clear();
    }

    if (!process_) {
        return failure(
            GameStateReadError::process_unavailable,
            last_discovery_detail_);
    }

    GameStateReadResult result =
        read_game_state(
            *process_, profile_->game_state, profile_->spawns);
    if (result.error == GameStateReadError::read_failed) {
        process_.reset();
        next_discovery_check_ = now + kDiscoveryInterval;
        last_discovery_detail_ =
            result.detail.empty()
                ? "client memory read failed"
                : result.detail;
    }
    return result;
}

}  // namespace plazmic
