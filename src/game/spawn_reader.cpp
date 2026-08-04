#include "game/spawn_reader.h"

#include "integration/process_reader.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <span>
#include <utility>
#include <vector>

namespace plazmic {
namespace {

constexpr std::size_t kMaximumProfileRecordBytes = 4096U;
constexpr std::size_t kLinkRecordBytes = 24U;

template <typename Value>
Value decode(const std::span<const std::byte> bytes, std::size_t offset) {
    Value value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

bool field_fits(std::size_t offset,
                std::size_t bytes,
                std::size_t record_bytes) {
    return offset <= record_bytes && bytes <= record_bytes - offset;
}

bool valid_symbols(const SpawnSymbols& symbols) {
    return symbols.name_bytes >= 2U && symbols.name_bytes <= 64U &&
           symbols.maximum_count >= 1U &&
           symbols.maximum_count <= 4096U &&
           symbols.record_bytes >= 24U &&
           symbols.record_bytes <= kMaximumProfileRecordBytes &&
           field_fits(
               symbols.next_offset, sizeof(std::uintptr_t),
               kLinkRecordBytes) &&
           field_fits(
               symbols.previous_offset, sizeof(std::uintptr_t),
               kLinkRecordBytes) &&
           field_fits(
               symbols.name_offset, symbols.name_bytes,
               symbols.record_bytes) &&
           field_fits(
               symbols.type_offset, sizeof(std::uint8_t),
               symbols.record_bytes) &&
           field_fits(
               symbols.id_offset, sizeof(std::uint32_t),
               symbols.record_bytes) &&
           field_fits(
               symbols.level_offset, sizeof(std::uint8_t),
               symbols.record_bytes) &&
           field_fits(
               symbols.y_offset, sizeof(float), symbols.record_bytes) &&
           field_fits(
               symbols.x_offset, sizeof(float), symbols.record_bytes) &&
           field_fits(
               symbols.z_offset, sizeof(float), symbols.record_bytes);
}

SpawnReadResult failure(SpawnReadError error, std::string detail) {
    return {
        .snapshot = std::nullopt,
        .error = error,
        .detail = std::move(detail),
    };
}

std::optional<std::string> decode_name(
    const std::span<const std::byte> record,
    const SpawnSymbols& symbols) {
    const auto begin =
        record.begin() + static_cast<std::ptrdiff_t>(symbols.name_offset);
    const auto end =
        begin + static_cast<std::ptrdiff_t>(symbols.name_bytes);
    const auto terminator = std::find(begin, end, std::byte{0});
    if (terminator == begin || terminator == end) {
        return std::nullopt;
    }
    for (auto current = begin; current != terminator; ++current) {
        const auto character =
            std::to_integer<unsigned int>(*current);
        if (character < 0x20U || character > 0x7eU) {
            return std::nullopt;
        }
    }
    return std::string(
        reinterpret_cast<const char*>(&*begin),
        static_cast<std::size_t>(terminator - begin));
}

std::optional<SpawnType> decode_type(std::uint8_t raw) {
    switch (raw) {
        case 0U:
            return SpawnType::player;
        case 1U:
            return SpawnType::npc;
        case 2U:
            return SpawnType::corpse;
        default:
            return std::nullopt;
    }
}

bool valid_coordinate(float value) {
    return std::isfinite(value) && std::abs(value) <= 1000000.0F;
}

struct Links {
    std::uintptr_t vtable;
    std::uintptr_t next;
    std::uintptr_t previous;
};

std::optional<Links> read_links(const ProcessMemoryReader& reader,
                                std::uintptr_t address,
                                const SpawnSymbols& symbols) {
    std::array<std::byte, kLinkRecordBytes> bytes{};
    const ProcessReadResult result = reader.read_exact(address, bytes);
    if (!result) {
        return std::nullopt;
    }
    const std::span<const std::byte> view(bytes);
    return Links{
        .vtable = decode<std::uintptr_t>(view, 0U),
        .next = decode<std::uintptr_t>(view, symbols.next_offset),
        .previous =
            decode<std::uintptr_t>(view, symbols.previous_offset),
    };
}

std::optional<std::vector<std::uintptr_t>> traverse(
    const ProcessMemoryReader& reader,
    std::uintptr_t root,
    const SpawnSymbols& symbols,
    std::uintptr_t expected_vtable) {
    std::vector<std::uintptr_t> addresses;
    addresses.reserve(
        std::min<std::size_t>(symbols.maximum_count, 256U));
    std::set<std::uintptr_t> visited;
    std::uintptr_t current = root;
    std::uintptr_t previous = 0U;
    while (current != 0U) {
        if (addresses.size() >= symbols.maximum_count ||
            !visited.insert(current).second) {
            return std::nullopt;
        }
        const auto links = read_links(reader, current, symbols);
        if (!links || links->vtable != expected_vtable ||
            links->previous != previous) {
            return std::nullopt;
        }
        addresses.push_back(current);
        previous = current;
        current = links->next;
    }
    return addresses;
}

std::optional<std::uintptr_t> find_root(
    const ProcessMemoryReader& reader,
    std::uintptr_t anchor,
    const SpawnSymbols& symbols,
    std::uintptr_t expected_vtable) {
    std::set<std::uintptr_t> visited;
    std::uintptr_t current = anchor;
    for (std::size_t count = 0U; count < symbols.maximum_count; ++count) {
        if (!visited.insert(current).second) {
            return std::nullopt;
        }
        const auto links = read_links(reader, current, symbols);
        if (!links || links->vtable != expected_vtable) {
            return std::nullopt;
        }
        if (links->previous == 0U) {
            return current;
        }
        const auto previous =
            read_links(reader, links->previous, symbols);
        if (!previous || previous->vtable != expected_vtable ||
            previous->next != current) {
            return std::nullopt;
        }
        current = links->previous;
    }
    return std::nullopt;
}

}  // namespace

SpawnReadResult read_spawn_collection(
    const ClientProcess& process,
    std::uintptr_t anchor,
    const SpawnSymbols& symbols,
    const PlayerSnapshot& player) {
    if (process.pid <= 0 || anchor == 0U || !player.available() ||
        !valid_symbols(symbols)) {
        return failure(
            SpawnReadError::invalid_profile,
            "spawn profile or live anchor is incomplete");
    }

    const ProcessMemoryReader reader(process);
    const auto anchor_links = read_links(reader, anchor, symbols);
    if (!anchor_links) {
        return failure(
            SpawnReadError::read_failed,
            "cannot read the spawn collection anchor");
    }
    if (anchor_links->vtable == 0U) {
        return failure(
            SpawnReadError::invalid_collection,
            "spawn anchor failed exact-profile validation");
    }
    const auto root = find_root(
        reader, anchor, symbols, anchor_links->vtable);
    if (!root) {
        return failure(
            SpawnReadError::inconsistent_collection,
            "spawn reverse links did not resolve a bounded root");
    }

    const auto addresses =
        traverse(reader, *root, symbols, anchor_links->vtable);
    if (!addresses || addresses->empty() ||
        std::find(addresses->begin(), addresses->end(), anchor) ==
            addresses->end()) {
        return failure(
            SpawnReadError::inconsistent_collection,
            "spawn links changed, lost the anchor, or exceeded the bound");
    }

    std::vector<SpawnSnapshot> snapshots;
    snapshots.reserve(addresses->size());
    std::set<std::uint32_t> ids;
    unsigned int player_level = 0U;
    std::vector<std::byte> record(symbols.record_bytes);
    for (const std::uintptr_t address : *addresses) {
        const ProcessReadResult read_result =
            reader.read_exact(address, record);
        if (!read_result) {
            return failure(
                SpawnReadError::inconsistent_collection,
                "spawn record changed during the staged read");
        }
        const std::span<const std::byte> view(record);
        if (decode<std::uintptr_t>(view, 0U) != anchor_links->vtable) {
            return failure(
                SpawnReadError::inconsistent_collection,
                "spawn record identity changed during the staged read");
        }
        const auto name = decode_name(view, symbols);
        const auto type =
            decode_type(decode<std::uint8_t>(view, symbols.type_offset));
        const std::uint32_t id =
            decode<std::uint32_t>(view, symbols.id_offset);
        const std::uint8_t level =
            decode<std::uint8_t>(view, symbols.level_offset);
        const float y = decode<float>(view, symbols.y_offset);
        const float x = decode<float>(view, symbols.x_offset);
        const float z = decode<float>(view, symbols.z_offset);
        if (!name || !type || id == 0U || level == 0U ||
            !valid_coordinate(x) || !valid_coordinate(y) ||
            !valid_coordinate(z)) {
            return failure(
                SpawnReadError::invalid_collection,
                "spawn record failed bounded field validation");
        }
        if (!ids.insert(id).second) {
            return failure(
                SpawnReadError::invalid_collection,
                "spawn collection contains a duplicate stable ID");
        }
        if (address == anchor) {
            player_level = static_cast<unsigned int>(level);
        }
        const double delta_x =
            static_cast<double>(x) - player.x;
        const double delta_y =
            static_cast<double>(y) - player.y;
        snapshots.push_back({
            .id = id,
            .type = *type,
            .name = *name,
            .level = static_cast<unsigned int>(level),
            .x = static_cast<double>(x),
            .y = static_cast<double>(y),
            .z = static_cast<double>(z),
            .distance = std::hypot(delta_x, delta_y),
        });
    }

    const auto confirmation =
        traverse(reader, *root, symbols, anchor_links->vtable);
    if (!confirmation || *confirmation != *addresses) {
        return failure(
            SpawnReadError::inconsistent_collection,
            "spawn links changed during the staged read");
    }
    if (player_level == 0U) {
        return failure(
            SpawnReadError::inconsistent_collection,
            "spawn collection lost the local-player anchor level");
    }

    return {
        .snapshot =
            SpawnCollectionSnapshot{
                .state = PlayerSnapshotState::in_world,
                .zone = player.zone,
                .player_level = player_level,
                .spawns = std::move(snapshots),
                .detail = "Live read-only spawn snapshot",
            },
        .error = SpawnReadError::none,
        .detail = {},
    };
}

}  // namespace plazmic
