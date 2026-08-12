#include "activity/activity_tracker.h"

#include "game/combat_log_parser.h"

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

#include <sys/stat.h>
#include <sys/resource.h>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::string persisted_source(std::string_view label,
                             std::uint64_t sequence = 0U) {
    return "0123456789ab:" +
           plazmic::CombatHistoryStore::privacy_key(label) + ":" +
           std::to_string(sequence);
}

std::string timestamped(
    std::chrono::system_clock::time_point timestamp,
    std::string_view payload) {
    const std::time_t value =
        std::chrono::system_clock::to_time_t(timestamp);
    std::tm local{};
    require(::localtime_r(&value, &local) != nullptr,
            "dynamic fixture timestamp could not be converted");
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << '[' << std::put_time(&local, "%a %b %d %H:%M:%S %Y")
           << "] " << payload;
    return output.str();
}

}  // namespace

int main() {
    (void)::umask(0077);
    std::filesystem::path directory;
    try {
        constexpr std::string_view kCharacter = "Synthetic Hero";
        const auto experience = plazmic::parse_activity_line(
            "[Mon Aug 03 12:00:00 2026] You gain experience! (0.125%)",
            kCharacter, "synthetic_zone");
        require(experience &&
                    experience->kind ==
                        plazmic::ActivityEventKind::experience &&
                    experience->amount == 0.125 &&
                    experience->zone == "synthetic_zone",
                "experience percentage was not parsed");
        const auto aa = plazmic::parse_activity_line(
            "[Mon Aug 03 12:30:00 2026] You have gained an ability point!  "
            "You now have 42 ability points.",
            kCharacter, "synthetic_zone");
        require(aa &&
                    aa->kind ==
                        plazmic::ActivityEventKind::alternate_advancement &&
                    aa->total == 42U,
                "AA point total was not parsed");
        const auto loot = plazmic::parse_activity_line(
            "[Mon Aug 03 12:45:00 2026] --You have looted Synthetic Gem.--",
            kCharacter, "synthetic_zone");
        require(loot && loot->kind == plazmic::ActivityEventKind::loot &&
                    loot->label == "Synthetic Gem",
                "loot event was not parsed");
        const auto fixture_time = std::chrono::system_clock::time_point{
            std::chrono::seconds(experience->timestamp_unix_seconds)};
        require(!plazmic::parse_activity_line(
                    "[Mon Aug 03 12:00:00 2026] You gain experience! (101%)",
                    kCharacter, "synthetic_zone") &&
                    !plazmic::parse_activity_line(
                        "[Mon Aug 03 12:00:00 2026] You gain experience! "
                        "(1e1%)",
                        kCharacter, "synthetic_zone") &&
                    !plazmic::parse_activity_line(
                        "[Mon Aug 03 12:00:00 2026] You gain experience! "
                        "(1.%)",
                        kCharacter, "synthetic_zone") &&
                    !plazmic::parse_activity_line(
                        "[Mon Aug 03 12:00:00 2026] You receive Synthetic Gem",
                        kCharacter, "synthetic_zone"),
                "ambiguous or invalid activity line was accepted");

        plazmic::ActivityTracker tracker;
        tracker.consume(
            "[Mon Aug 03 12:00:00 2026] You gain experience! (0.125%)",
            kCharacter, "synthetic_zone");
        tracker.consume(
            "[Mon Aug 03 12:30:00 2026] You have gained an ability point!  "
            "You now have 42 ability points.",
            kCharacter, "synthetic_zone");
        tracker.consume(
            "[Mon Aug 03 12:45:00 2026] --You have looted Synthetic Gem.--",
            kCharacter, "synthetic_zone");
        tracker.observe_damage(
            plazmic::DamageEvent{
                .timestamp = fixture_time,
                .attacker = std::string(kCharacter),
                .defender = "Synthetic Target",
                .damage = 500U,
                .kind = plazmic::DamageKind::spell,
                .ability = "Synthetic Burst",
            },
            kCharacter, persisted_source("initial-spell-observation"));
        tracker.observe_damage(
            plazmic::DamageEvent{
                .timestamp = fixture_time,
                .attacker = "Synthetic Stranger",
                .defender = "Synthetic Target",
                .damage = 500U,
                .kind = plazmic::DamageKind::spell,
                .ability = "Unrelated Burst",
            },
            kCharacter, persisted_source("unrelated-spell-observation"));
        tracker.observe_damage(
            plazmic::DamageEvent{
                .timestamp = fixture_time,
                .attacker = std::string(kCharacter),
                .defender = "Synthetic Target",
                .damage = 45U,
                .kind = plazmic::DamageKind::melee,
                .ability = "Kick",
            },
            kCharacter, persisted_source("named-melee-observation"));
        tracker.observe_damage(
            plazmic::DamageEvent{
                .timestamp = fixture_time,
                .attacker = std::string(kCharacter),
                .defender = "Synthetic Target",
                .damage = 120U,
                .kind = plazmic::DamageKind::melee,
                .ability = "Slash",
            },
            kCharacter, persisted_source("auto-attack-observation"));
        plazmic::CharacterSnapshot initial{
            .state = plazmic::PlayerSnapshotState::in_world,
            .name = std::string(kCharacter),
            .health = {},
            .mana = {},
            .equipment = {{.slot = "Head", .item = "Synthetic Cap"}},
            .detail = {},
        };
        tracker.observe_character(initial, "synthetic_zone", fixture_time);
        auto changed = initial;
        changed.equipment.front().item = "Synthetic Crown";
        tracker.observe_character(
            changed, "synthetic_zone",
            std::chrono::system_clock::time_point{
                std::chrono::seconds(aa->timestamp_unix_seconds)});
        const auto analytics = tracker.snapshot(
            std::chrono::system_clock::time_point{
                std::chrono::seconds(loot->timestamp_unix_seconds + 900)});
        require(analytics.events.size() == 4U &&
                    analytics.experience_percent == 0.125 &&
                    analytics.alternate_advancement_points == 42U &&
                    analytics.recent_loot_count == 1U &&
                    analytics.experience_percent_per_hour > 0.0 &&
                    analytics.level_pace_hours &&
                    analytics.next_alternate_advancement_hours &&
                    analytics.abilities.size() == 2U &&
                    std::ranges::find(
                        analytics.abilities, std::string("Kick"),
                        &plazmic::AbilityActivitySnapshot::name) !=
                        analytics.abilities.end() &&
                    std::ranges::find(
                        analytics.abilities, std::string("Slash"),
                        &plazmic::AbilityActivitySnapshot::name) ==
                        analytics.abilities.end() &&
                    analytics.class_activity_summary.find(
                        "1 equipped slot") != std::string::npos &&
                    analytics.recent_celebration &&
                    analytics.recent_celebration->find("Loot") !=
                        std::string::npos,
                "activity analytics were not aggregated deterministically");
        require(analytics.events.back().kind ==
                        plazmic::ActivityEventKind::equipment_change &&
                    analytics.events.back().evidence.find("immutable") !=
                        std::string::npos,
                "equipment change evidence was not recorded");

        const auto rate_now = fixture_time + std::chrono::hours(1);
        plazmic::ActivityTracker independent_rates;
        independent_rates.consume(
            timestamped(rate_now - std::chrono::minutes(59),
                        "You have gained an ability point!  You now have 1 "
                        "ability points."),
            kCharacter, "synthetic_zone");
        independent_rates.consume(
            timestamped(rate_now - std::chrono::minutes(5),
                        "You gain experience! (1.000%)"),
            kCharacter, "synthetic_zone");
        const auto rate_snapshot = independent_rates.snapshot(rate_now);
        require(rate_snapshot.experience_percent_per_hour > 11.9 &&
                    rate_snapshot.experience_percent_per_hour < 12.1 &&
                    rate_snapshot.alternate_advancement_points_per_hour >
                        1.0 &&
                    rate_snapshot.alternate_advancement_points_per_hour <
                        1.1,
                "XP and AA analytics did not use independent rate windows");

        plazmic::ActivityTracker future_events;
        future_events.consume(
            timestamped(rate_now + std::chrono::hours(25),
                        "You gain experience! (1.000%)"),
            kCharacter, "synthetic_zone");
        require(future_events.snapshot(rate_now).events.empty(),
                "future-dated activity survived ingestion pruning");
        future_events.consume(
            timestamped(rate_now + std::chrono::minutes(30),
                        "You gain experience! (1.000%)"),
            kCharacter, "synthetic_zone");
        future_events.consume(
            timestamped(rate_now + std::chrono::minutes(30),
                        "You have gained an ability point!  You now have 2 "
                        "ability points."),
            kCharacter, "synthetic_zone");
        future_events.consume(
            timestamped(rate_now + std::chrono::minutes(30),
                        "--You have looted Future Gem.--"),
            kCharacter, "synthetic_zone");
        const auto near_future_snapshot = future_events.snapshot(rate_now);
        require(near_future_snapshot.events.size() == 3U &&
                    near_future_snapshot.experience_percent_per_hour == 0.0 &&
                    near_future_snapshot
                            .alternate_advancement_points_per_hour == 0.0 &&
                    near_future_snapshot.recent_loot_count == 0U,
                "future-dated activity entered a recent analytics window");
        const std::string replay_line =
            "[Mon Aug 03 12:00:05 2026] You hit Synthetic Target for 325 "
            "points of magic damage by Replay Burst.";
        const auto replay_damage =
            plazmic::parse_damage_line(replay_line, kCharacter);
        require(replay_damage.has_value(),
                "replay ability fixture did not parse");
        plazmic::ActivityTracker replay_tracker;
        const std::string first_source = replay_tracker.consume(
            replay_line, kCharacter, "synthetic_zone", true);
        replay_tracker.observe_damage(
            *replay_damage, kCharacter, first_source);
        replay_tracker.begin_log_stream(replay_line + "\n");
        const std::string replayed_source = replay_tracker.consume(
            replay_line, kCharacter, "synthetic_zone", true);
        replay_tracker.observe_damage(
            *replay_damage, kCharacter, replayed_source);
        const std::string new_generation_source = replay_tracker.consume(
            replay_line, kCharacter, "synthetic_zone", true);
        replay_tracker.observe_damage(
            *replay_damage, kCharacter, new_generation_source);
        replay_tracker.begin_log_stream();
        const std::string no_overlap_source = replay_tracker.consume(
            replay_line, kCharacter, "synthetic_zone", true);
        replay_tracker.observe_damage(
            *replay_damage, kCharacter, no_overlap_source);
        const auto replay_activity = replay_tracker.snapshot(
            replay_damage->timestamp + std::chrono::minutes(1));
        require(replay_activity.abilities.size() == 1U &&
                    replay_activity.abilities.front().damage == 975U &&
                    replay_activity.abilities.front().observations == 3U &&
                    replayed_source == first_source &&
                    new_generation_source != first_source &&
                    no_overlap_source != first_source &&
                    no_overlap_source != new_generation_source,
                "replay matching collided with a new-generation ability");
        plazmic::ActivityTracker occurrence_rollover;
        const std::string repeated_occurrence_line = timestamped(
            replay_damage->timestamp,
            "--You have looted Repeated Occurrence Item.--");
        std::string first_occurrence_source;
        std::string rollover_occurrence_source;
        for (std::uint32_t occurrence = 0U;
             occurrence <=
                 plazmic::ActivityTracker::maximum_occurrences_per_fingerprint;
             ++occurrence) {
            const std::string source = occurrence_rollover.consume(
                repeated_occurrence_line, kCharacter, "synthetic_zone");
            if (occurrence == 0U) {
                first_occurrence_source = source;
            } else if (
                occurrence ==
                plazmic::ActivityTracker::maximum_occurrences_per_fingerprint) {
                rollover_occurrence_source = source;
            }
        }
        require(first_occurrence_source.size() >= 47U &&
                    rollover_occurrence_source.size() >= 47U &&
                    first_occurrence_source.substr(0U, 12U) !=
                        rollover_occurrence_source.substr(0U, 12U) &&
                    rollover_occurrence_source.ends_with(":0"),
                "repeated-line identity exhaustion did not roll generations");
        plazmic::ActivityTracker timestamp_bounded;
        for (std::size_t index = 0U;
             index < plazmic::ActivityTracker::maximum_events; ++index) {
            timestamp_bounded.consume(
                timestamped(
                    replay_damage->timestamp +
                        std::chrono::seconds(index + 1U),
                    "--You have looted Newer Bounded Item " +
                        std::to_string(index) + ".--"),
                kCharacter, "synthetic_zone");
        }
        timestamp_bounded.consume(
            timestamped(replay_damage->timestamp,
                        "--You have looted Older Rewritten Item.--"),
            kCharacter, "synthetic_zone");
        const auto timestamp_bounded_snapshot = timestamp_bounded.snapshot(
            replay_damage->timestamp + std::chrono::minutes(10));
        require(timestamp_bounded_snapshot.events.size() ==
                    plazmic::ActivityTracker::maximum_events &&
                    std::ranges::find(
                        timestamp_bounded_snapshot.events,
                        std::string("Older Rewritten Item"),
                        &plazmic::ActivityEventSnapshot::label) ==
                        timestamp_bounded_snapshot.events.end() &&
                    std::ranges::find(
                        timestamp_bounded_snapshot.events,
                        std::string("Newer Bounded Item 511"),
                        &plazmic::ActivityEventSnapshot::label) !=
                        timestamp_bounded_snapshot.events.end(),
                "event bound retained an older rewritten observation");
        plazmic::ActivityTracker source_bound;
        for (std::size_t index = 0U; index < 4097U; ++index) {
            source_bound.consume(
                timestamped(
                    replay_damage->timestamp,
                    "--You have looted Synthetic Item " +
                        std::to_string(index) + ".--"),
                kCharacter, "synthetic_zone");
        }
        const std::string repeated_after_bound = timestamped(
            replay_damage->timestamp,
            "--You have looted Repeated After Bound.--");
        source_bound.consume(
            repeated_after_bound, kCharacter, "synthetic_zone");
        source_bound.consume(
            repeated_after_bound, kCharacter, "synthetic_zone");
        const auto source_bound_snapshot = source_bound.snapshot(
            replay_damage->timestamp + std::chrono::minutes(1));
        const auto repeated_after_bound_count = std::ranges::count(
            source_bound_snapshot.events,
            std::string("Repeated After Bound"),
            &plazmic::ActivityEventSnapshot::label);
        require(repeated_after_bound_count == 2,
                "source IDs collided after the qualifying-line bound: " +
                    std::to_string(repeated_after_bound_count));
        plazmic::ActivityTracker bounded_replay;
        std::vector<std::string> bounded_lines;
        bounded_lines.reserve(plazmic::ActivityTracker::maximum_events);
        for (std::size_t index = 0U;
             index < plazmic::ActivityTracker::maximum_events; ++index) {
            bounded_lines.push_back(timestamped(
                replay_damage->timestamp,
                "--You have looted Bounded Item " +
                    std::to_string(index) + ".--"));
            bounded_replay.consume(
                bounded_lines.back(), kCharacter, "synthetic_zone");
        }
        std::string bounded_overlap;
        for (const auto& replayed_line : bounded_lines) {
            bounded_overlap += replayed_line + "\n";
        }
        bounded_replay.begin_log_stream(bounded_overlap);
        bounded_replay.consume(
            timestamped(replay_damage->timestamp,
                        "--You have looted Prehistory Item.--"),
            kCharacter, "synthetic_zone");
        for (const auto& replayed_line : bounded_lines) {
            bounded_replay.consume(
                replayed_line, kCharacter, "synthetic_zone");
        }
        const auto bounded_replay_snapshot = bounded_replay.snapshot(
            replay_damage->timestamp + std::chrono::minutes(1));
        require(std::ranges::find(
                    bounded_replay_snapshot.events,
                    std::string("Prehistory Item"),
                    &plazmic::ActivityEventSnapshot::label) !=
                    bounded_replay_snapshot.events.end() &&
                    std::ranges::find(
                        bounded_replay_snapshot.events,
                        std::string("Bounded Item 0"),
                        &plazmic::ActivityEventSnapshot::label) ==
                        bounded_replay_snapshot.events.end(),
                "bounded replay mutated its immutable boundary IDs");
        plazmic::ActivityTracker categorized;
        categorized.observe_damage(
            plazmic::DamageEvent{
                .timestamp = replay_damage->timestamp,
                .attacker = std::string(kCharacter),
                .defender = "Synthetic Target",
                .damage = 100U,
                .kind = plazmic::DamageKind::spell,
                .ability = "Shared Name",
            },
            kCharacter, persisted_source("spell-observation"));
        categorized.observe_damage(
            plazmic::DamageEvent{
                .timestamp = replay_damage->timestamp +
                             std::chrono::seconds(1),
                .attacker = std::string(kCharacter),
                .defender = "Synthetic Target",
                .damage = 200U,
                .kind = plazmic::DamageKind::damage_over_time,
                .ability = "Shared Name",
            },
            kCharacter, persisted_source("dot-observation"));
        const auto categorized_snapshot = categorized.snapshot(
            replay_damage->timestamp + std::chrono::minutes(1));
        require(categorized_snapshot.abilities.size() == 2U &&
                    categorized_snapshot.abilities.front().category !=
                        categorized_snapshot.abilities.back().category,
                "same-name ability categories were combined");
        plazmic::ActivityTracker invalid_source_observation;
        invalid_source_observation.observe_damage(
            *replay_damage, kCharacter, "arbitrary-printable-id");
        require(invalid_source_observation
                    .snapshot(replay_damage->timestamp +
                              std::chrono::minutes(1))
                    .abilities.empty(),
                "invalid source identifier created an ability observation");
        plazmic::ActivityTracker expired_ability;
        expired_ability.observe_damage(
            plazmic::DamageEvent{
                .timestamp = std::chrono::system_clock::now() -
                             plazmic::ActivityTracker::maximum_age -
                             std::chrono::seconds(1),
                .attacker = std::string(kCharacter),
                .defender = "Synthetic Target",
                .damage = 1U,
                .kind = plazmic::DamageKind::spell,
                .ability = "Expired Burst",
            },
            kCharacter, persisted_source("expired-spell-observation"));
        require(expired_ability.snapshot().abilities.empty(),
                "expired ability aggregate was retained");
        plazmic::ActivityTracker slot_changes;
        slot_changes.observe_character(initial, "synthetic_zone");
        auto added_slot = initial;
        added_slot.equipment.push_back({
            .slot = "Primary",
            .item = "Synthetic Blade",
        });
        slot_changes.observe_character(
            added_slot, "synthetic_zone",
            std::chrono::system_clock::time_point{
                std::chrono::seconds(aa->timestamp_unix_seconds)});
        auto removed_slot = added_slot;
        removed_slot.equipment.erase(removed_slot.equipment.begin());
        slot_changes.observe_character(
            removed_slot, "synthetic_zone",
            std::chrono::system_clock::time_point{
                std::chrono::seconds(aa->timestamp_unix_seconds + 1)});
        const auto slot_activity = slot_changes.snapshot(
            std::chrono::system_clock::time_point{
                std::chrono::seconds(loot->timestamp_unix_seconds)});
        require(slot_activity.events.size() == 2U &&
                    slot_activity.events.front().label.find(
                        "Empty -> Synthetic Blade") != std::string::npos &&
                    slot_activity.events.back().label.find(
                        "Synthetic Cap -> Empty") != std::string::npos,
                "added or removed equipment slot was not recorded");
        plazmic::ActivityTracker lifecycle_equipment;
        lifecycle_equipment.observe_character(
            initial, "synthetic_zone", fixture_time);
        lifecycle_equipment.reset_transient_observations();
        lifecycle_equipment.observe_character(
            changed, "synthetic_zone",
            fixture_time + std::chrono::seconds(1));
        require(lifecycle_equipment.snapshot(
                    fixture_time + std::chrono::minutes(1))
                    .events.empty(),
                "lifecycle recovery compared non-consecutive equipment");

        char template_path[] = "/tmp/plazmic-activity-test-XXXXXX";
        const char* created = ::mkdtemp(template_path);
        require(created != nullptr, "temporary directory could not be created");
        directory = created;
        const auto inventory = directory / "Inventory.txt";
        {
            std::ofstream output(inventory, std::ios::binary);
            output << "Location\tName\tID\tCount\tSlots\n"
                   << "Head\tSynthetic Crown\t1001\t1\t0\n"
                   << "Ear\tSynthetic Earring\t1002\t1\t0\n"
                   << "Ear\tSynthetic Earring\t1002\t1\t0\n"
                   << "General1-Slot1\tSynthetic Gem\t1003\t3\t0\n"
                   << "General1-Slot2\t\t0\t0\t0\n"
                   << "\nKeyRing\tName\tID\n"
                   << "Equipment\tSynthetic Key\t1004\n";
        }
        const auto imported =
            plazmic::import_inventory_output(inventory, changed);
        require(imported.available && imported.entries.size() == 5U &&
                    imported.entries[3].quantity == 3U &&
                    imported.imported_equipped_items.size() == 1U &&
                    imported.equipped_not_in_import.empty(),
                "inventory output was not reconciled with equipment");
        {
            std::ofstream output(inventory, std::ios::binary | std::ios::trunc);
            output << "Location\tName\tID\tCount\tSlots\n"
                   << "General1\tName\t1005\t1\t0\n"
                   << "General2\tSynthetic Gem\t1006\t1\t0\n";
        }
        const auto item_named_name =
            plazmic::import_inventory_output(inventory, changed);
        require(item_named_name.available &&
                    item_named_name.entries.size() == 2U &&
                    item_named_name.entries.front().item == "Name",
                "inventory item named Name was mistaken for a header");
        {
            std::ofstream output(inventory, std::ios::binary | std::ios::trunc);
            output << "Location\tName\tID\tCount\tSlots\n"
                   << "General1-Slot1\t\t0\t0\t0\n";
        }
        const auto empty_inventory =
            plazmic::import_inventory_output(inventory, changed);
        require(empty_inventory.available && empty_inventory.entries.empty() &&
                    empty_inventory.equipped_not_in_import.size() == 1U,
                "valid all-empty inventory output was rejected");
        {
            std::ofstream output(inventory, std::ios::binary | std::ios::trunc);
            output << "Head\tSynthetic Crown\t1001\t1\t0\n";
        }
        require(!plazmic::import_inventory_output(inventory, changed).available,
                "headerless inventory output was accepted");
        {
            std::ofstream output(inventory, std::ios::binary | std::ios::trunc);
            output << "Location\tName\tID\tCount\tSlots\n"
                   << "Head\tSynthetic Crown\t1001\t1\t0\textra\n";
        }
        require(!plazmic::import_inventory_output(inventory, changed).available,
                "extended inventory row bypassed the selected header shape");
        {
            std::ofstream output(inventory, std::ios::binary | std::ios::trunc);
            output << "KeyRing\tName\tID\n"
                   << "Equipment\tSynthetic Key\n";
        }
        require(!plazmic::import_inventory_output(inventory, changed).available,
                "truncated key-ring inventory row was accepted");
        {
            std::ofstream output(inventory, std::ios::binary | std::ios::trunc);
            output << "Location\tName\tID\tCount\tSlots\n"
                   << "Head\tSynthetic Crown\t1001\t1\t0\n";
            for (std::size_t row = 0U; row < 4095U; ++row) {
                output << '\n';
            }
        }
        const auto excessive_inventory =
            plazmic::import_inventory_output(inventory, changed);
        require(!excessive_inventory.available &&
                    excessive_inventory.detail.find("row limit") !=
                        std::string::npos,
                "inventory non-item rows bypassed the row limit");
        const auto inventory_target = directory / "inventory-target.txt";
        {
            std::ofstream output(inventory_target, std::ios::binary);
            output << "Location\tName\tID\tCount\tSlots\n"
                   << "General1\tSynthetic Gem\t1007\t1\t0\n";
        }
        const auto inventory_link = directory / "inventory-link.txt";
        std::filesystem::create_symlink(inventory_target, inventory_link);
        require(!plazmic::import_inventory_output(
                     inventory_link, changed).available,
                "inventory import followed a symlink");
        const auto oversized_inventory = directory / "oversized-inventory.txt";
        {
            std::ofstream output(oversized_inventory, std::ios::binary);
            output.seekp(static_cast<std::streamoff>(2U * 1024U * 1024U));
            output.put('x');
        }
        require(!plazmic::import_inventory_output(
                     oversized_inventory, changed).available,
                "inventory import accepted more than 2 MiB");

        const auto state_root = directory / "state";
        const auto persistence_now = std::chrono::system_clock::now();
        const auto persistence_event =
            persistence_now - std::chrono::hours(1);
        const std::string activity_key =
            plazmic::CombatHistoryStore::privacy_key("synthetic activity");
        plazmic::ActivityTracker persisted(state_root);
        require(persisted.select(activity_key),
                "activity partition could not be selected");
        persisted.set_retention_enabled(true);
        const std::string persisted_first_source = persisted.consume(
            timestamped(persistence_event,
                        "You gain experience! (0.125%)"),
            kCharacter, "synthetic_zone");
        const auto persisted_snapshot = persisted.snapshot(persistence_now);
        const auto activity_file =
            state_root / "activity" / (activity_key + ".json");
        const auto activity_directory = activity_file.parent_path();
        struct stat activity_directory_permissions {};
        struct stat activity_permissions {};
        require(persisted_snapshot.persisted &&
                    ::stat(activity_directory.c_str(),
                           &activity_directory_permissions) == 0 &&
                    (activity_directory_permissions.st_mode & 0777) == 0700 &&
                    ::stat(activity_file.c_str(), &activity_permissions) == 0 &&
                    (activity_permissions.st_mode & 0777) == 0600,
                "activity history was not saved owner-only");
        persisted.consume(
            timestamped(persistence_event + std::chrono::seconds(1),
                        "You gain experience! (0.250%)"),
            kCharacter, "synthetic_zone");
        persisted.observe_damage(
            plazmic::DamageEvent{
                .timestamp = persistence_event + std::chrono::seconds(2),
                .attacker = std::string(kCharacter),
                .defender = "Synthetic Target",
                .damage = 45U,
                .kind = plazmic::DamageKind::melee,
                .ability = "Kick",
            },
            kCharacter, persisted_source("persisted-named-melee"));
        require(persisted.snapshot(persistence_now).persisted &&
                    ::stat(activity_directory.c_str(),
                           &activity_directory_permissions) == 0 &&
                    (activity_directory_permissions.st_mode & 0777) == 0700 &&
                    ::stat(activity_file.c_str(), &activity_permissions) == 0 &&
                    (activity_permissions.st_mode & 0777) == 0600,
                "activity replacement weakened owner-only modes");
        plazmic::ActivityTracker restored(state_root);
        restored.set_retention_enabled(true);
        require(restored.select(activity_key) &&
                    restored.snapshot(persistence_now)
                            .events.size() == 2U &&
                    restored.snapshot(persistence_now)
                            .abilities.size() == 1U &&
                    restored.snapshot(persistence_now)
                            .abilities.front().name == "Kick" &&
                    restored.snapshot(persistence_now)
                            .abilities.front().category == "Melee" &&
                    ::stat(activity_directory.c_str(),
                           &activity_directory_permissions) == 0 &&
                    (activity_directory_permissions.st_mode & 0777) == 0700 &&
                    ::stat(activity_file.c_str(), &activity_permissions) == 0 &&
                    (activity_permissions.st_mode & 0777) == 0600,
                "activity history was not restored by privacy key");
        require(::chmod(activity_file.c_str(), 0640) == 0,
                "cannot create unsafe activity-file permission fixture");
        plazmic::ActivityTracker unsafe_file(state_root);
        unsafe_file.set_retention_enabled(true);
        require(!unsafe_file.select(activity_key) &&
                    !unsafe_file.delete_history(activity_key),
                "unsafe activity-file permissions were read or deleted");
        require(::chmod(activity_file.c_str(), 0600) == 0 &&
                    ::chmod(activity_directory.c_str(), 0750) == 0,
                "cannot create unsafe activity-directory permission fixture");
        plazmic::ActivityTracker unsafe_directory(state_root);
        unsafe_directory.set_retention_enabled(true);
        require(!unsafe_directory.select(activity_key),
                "unsafe activity-directory permissions were accepted");
        require(::chmod(activity_directory.c_str(), 0700) == 0,
                "cannot restore activity-directory permissions");
        const std::string restored_source = restored.consume(
            timestamped(persistence_event + std::chrono::seconds(2),
                        "You gain experience! (0.500%)"),
            kCharacter, "synthetic_zone");
        require(persisted_first_source.substr(0U, 12U) !=
                    restored_source.substr(0U, 12U),
                "persisted generation state was reused after restart");
        const std::string long_equipment_key =
            plazmic::CombatHistoryStore::privacy_key("long equipment");
        plazmic::ActivityTracker long_equipment(state_root);
        long_equipment.set_retention_enabled(true);
        require(long_equipment.select(long_equipment_key),
                "long equipment partition could not be selected");
        auto long_before = initial;
        long_before.equipment.front().item = std::string(127U, 'A');
        auto long_after = long_before;
        long_after.equipment.front().item = std::string(127U, 'B');
        long_equipment.observe_character(
            long_before, "synthetic_zone", persistence_event);
        long_equipment.observe_character(
            long_after, "synthetic_zone",
            persistence_event + std::chrono::seconds(1));
        require(long_equipment.snapshot(persistence_now).persisted,
                "bounded long equipment event was not saved");
        plazmic::ActivityTracker long_equipment_restored(state_root);
        long_equipment_restored.set_retention_enabled(true);
        require(long_equipment_restored.select(long_equipment_key) &&
                    long_equipment_restored.snapshot(persistence_now)
                            .events.size() == 1U,
                "bounded long equipment event did not round-trip");
        const std::string long_slot_key =
            plazmic::CombatHistoryStore::privacy_key("long equipment slot");
        plazmic::ActivityTracker long_slot(state_root);
        require(long_slot.select(long_slot_key),
                "long equipment-slot partition could not be selected");
        auto long_slot_before = initial;
        long_slot_before.equipment.front().slot = std::string(300U, 'S');
        long_slot_before.equipment.front().item = "Before";
        auto long_slot_after = long_slot_before;
        long_slot_after.equipment.front().item = "After";
        long_slot.observe_character(
            long_slot_before, "synthetic_zone", persistence_event);
        long_slot.observe_character(
            long_slot_after, "synthetic_zone",
            persistence_event + std::chrono::seconds(1));
        const auto long_slot_snapshot = long_slot.snapshot(persistence_now);
        require(long_slot_snapshot.events.size() == 1U &&
                    long_slot_snapshot.events.front().label.size() <= 256U,
                "equipment label exceeded its bound for a long slot name");
        const std::string replay_boundary_key =
            plazmic::CombatHistoryStore::privacy_key(
                "persisted replay boundary");
        plazmic::ActivityTracker replay_boundary_writer(state_root);
        replay_boundary_writer.set_retention_enabled(true);
        require(replay_boundary_writer.select(replay_boundary_key),
                "persisted replay boundary could not be selected");
        std::vector<std::string> replay_boundary_lines;
        replay_boundary_lines.reserve(600U);
        for (std::size_t index = 0U; index < 600U; ++index) {
            replay_boundary_lines.push_back(timestamped(
                persistence_event,
                "--You have looted Persisted Boundary Item " +
                    std::to_string(index) + ".--"));
            replay_boundary_writer.consume(
                replay_boundary_lines.back(), kCharacter,
                "synthetic_zone");
        }
        require(replay_boundary_writer.snapshot(persistence_now).persisted,
                "persisted replay boundary was not saved");
        plazmic::ActivityTracker replay_boundary_reader(state_root);
        replay_boundary_reader.set_retention_enabled(true);
        require(replay_boundary_reader.select(replay_boundary_key),
                "persisted replay boundary was not restored");
        std::string persisted_overlap;
        for (const auto& replayed_line : replay_boundary_lines) {
            persisted_overlap += replayed_line + "\n";
        }
        replay_boundary_reader.begin_log_stream(persisted_overlap);
        replay_boundary_reader.consume(
            timestamped(persistence_event,
                        "--You have looted Persisted Prehistory Item.--"),
            kCharacter, "synthetic_zone");
        for (const auto& replayed_line : replay_boundary_lines) {
            replay_boundary_reader.consume(
                replayed_line, kCharacter, "synthetic_zone");
        }
        const auto replay_boundary_snapshot =
            replay_boundary_reader.snapshot(persistence_now);
        require(std::ranges::find(
                    replay_boundary_snapshot.events,
                    std::string("Persisted Prehistory Item"),
                    &plazmic::ActivityEventSnapshot::label) !=
                    replay_boundary_snapshot.events.end() &&
                    std::ranges::find(
                        replay_boundary_snapshot.events,
                        std::string("Persisted Boundary Item 88"),
                        &plazmic::ActivityEventSnapshot::label) ==
                        replay_boundary_snapshot.events.end(),
                "persisted replay boundary lost evicted source identities");
        const std::string replay_age_key =
            plazmic::CombatHistoryStore::privacy_key(
                "timestamp bounded replay metadata");
        plazmic::ActivityTracker replay_age(state_root);
        replay_age.set_retention_enabled(true);
        require(replay_age.select(replay_age_key),
                "timestamp replay fixture could not select its partition");
        std::string newest_replay_line;
        for (std::size_t index = 0U; index < 4096U; ++index) {
            newest_replay_line = timestamped(
                persistence_event + std::chrono::minutes(1),
                "--You have looted Newer Replay Item " +
                    std::to_string(index) + ".--");
            replay_age.consume(
                newest_replay_line, kCharacter, "synthetic_zone");
        }
        const std::string older_replay_line = timestamped(
            persistence_event,
            "--You have looted Older Rewritten Replay Item.--");
        replay_age.consume(
            older_replay_line, kCharacter, "synthetic_zone");
        require(replay_age.snapshot(persistence_now).persisted,
                "timestamp replay fixture was not persisted");
        const auto replay_age_file =
            state_root / "activity" / (replay_age_key + ".json");
        std::ifstream replay_age_input(replay_age_file, std::ios::binary);
        const std::string replay_age_bytes(
            std::istreambuf_iterator<char>(replay_age_input), {});
        require(replay_age_bytes.find(
                    plazmic::CombatHistoryStore::privacy_key(
                        older_replay_line)) == std::string::npos &&
                    replay_age_bytes.find(
                        plazmic::CombatHistoryStore::privacy_key(
                            newest_replay_line)) != std::string::npos,
                "replay bound retained older metadata over newer metadata");
        const std::string legacy_key =
            plazmic::CombatHistoryStore::privacy_key(
                "legacy replay migration");
        const auto legacy_file =
            state_root / "activity" / (legacy_key + ".json");
        const std::string legacy_line = timestamped(
            persistence_event, "--You have looted Legacy Gem.--");
        const auto legacy_event = plazmic::parse_activity_line(
            legacy_line, kCharacter, "synthetic_zone");
        require(legacy_event.has_value(),
                "legacy migration fixture did not parse");
        const std::string legacy_source =
            persisted_source(legacy_line);
        {
            std::ofstream output(legacy_file, std::ios::binary);
            output
                << R"({"schema":1,"events":[{"kind":2,"timestamp":")"
                << legacy_event->timestamp_unix_seconds
                << R"(","zone":"synthetic_zone","label":"Legacy Gem","amount":1,"evidence":"Exact local loot line","sourceId":")"
                << legacy_source << R"("}],"abilities":[]})";
        }
        plazmic::ActivityTracker legacy_merge(state_root);
        require(legacy_merge.select(legacy_key),
                "legacy migration partition could not be selected");
        const std::string legacy_session_source = legacy_merge.consume(
            timestamped(persistence_event + std::chrono::seconds(1),
                        "--You have looted Session Gem.--"),
            kCharacter, "synthetic_zone");
        legacy_merge.set_retention_enabled(true);
        legacy_merge.begin_log_stream(legacy_line + "\n");
        legacy_merge.consume(
            legacy_line, kCharacter, "synthetic_zone");
        const auto legacy_merge_snapshot =
            legacy_merge.snapshot(persistence_now);
        require(legacy_merge_snapshot.events.size() == 2U &&
                    std::ranges::count(
                        legacy_merge_snapshot.events,
                        std::string("Legacy Gem"),
                        &plazmic::ActivityEventSnapshot::label) == 1,
                "legacy replay IDs were lost while merging session history");
        std::ifstream migrated_legacy_file(legacy_file, std::ios::binary);
        const std::string migrated_legacy_bytes(
            std::istreambuf_iterator<char>(migrated_legacy_file), {});
        require(legacy_session_source.substr(0U, 12U) !=
                    legacy_source.substr(0U, 12U) &&
                    migrated_legacy_bytes.find("generationCounter") !=
                        std::string::npos,
                "legacy migration reused retained generation state or did "
                "not persist its counter");
        const auto export_file = directory / "activity-export.json";
        require(plazmic::save_activity_export(
                    export_file, analytics),
                "activity export could not be saved");
        std::ifstream exported_activity(export_file, std::ios::binary);
        const std::string exported_bytes(
            std::istreambuf_iterator<char>(exported_activity), {});
        struct stat export_permissions {};
        require(::stat(export_file.c_str(), &export_permissions) == 0 &&
                    (export_permissions.st_mode & 0777) == 0600 &&
                    exported_bytes.find("confidence") != std::string::npos &&
                    exported_bytes.find("unconfirmed") != std::string::npos,
                "activity export lost owner-only mode or confidence evidence");
        const auto existing_export = directory / "existing-activity.json";
        {
            std::ofstream output(existing_export, std::ios::binary);
            output << "old";
        }
        require(::chmod(existing_export.c_str(), 0644) == 0 &&
                    plazmic::save_activity_export(existing_export, analytics),
                "regular activity export could not be overwritten");
        struct stat existing_export_permissions {};
        std::ifstream existing_export_input(existing_export, std::ios::binary);
        const std::string existing_export_bytes{
            std::istreambuf_iterator<char>(existing_export_input), {}};
        require(::stat(existing_export.c_str(),
                       &existing_export_permissions) == 0 &&
                    (existing_export_permissions.st_mode & 0777) == 0600 &&
                    existing_export_bytes.find("events") != std::string::npos,
                "overwritten activity export was not owner-only JSON");
        const auto export_target = directory / "export-target.json";
        {
            std::ofstream output(export_target, std::ios::binary);
            output << "sentinel";
        }
        const auto export_link = directory / "export-link.json";
        std::filesystem::create_symlink(export_target, export_link);
        require(!plazmic::save_activity_export(export_link, analytics),
                "activity export accepted a symlink destination");
        std::ifstream export_target_input(export_target, std::ios::binary);
        require(std::string(std::istreambuf_iterator<char>(export_target_input),
                            {}) == "sentinel",
                "activity export modified a symlink target");
        const auto export_directory = directory / "export-directory.json";
        std::filesystem::create_directory(export_directory);
        require(!plazmic::save_activity_export(export_directory, analytics) &&
                    std::filesystem::is_directory(export_directory),
                "activity export replaced an existing directory");
        const auto export_fifo = directory / "export-fifo.json";
        require(::mkfifo(export_fifo.c_str(), 0600) == 0 &&
                    !plazmic::save_activity_export(export_fifo, analytics),
                "activity export accepted an existing FIFO");
        struct stat export_fifo_status {};
        require(::lstat(export_fifo.c_str(), &export_fifo_status) == 0 &&
                    S_ISFIFO(export_fifo_status.st_mode),
                "activity export replaced an existing FIFO");

        const auto changed_mode_state = directory / "changed-mode-state";
        const std::string changed_mode_key =
            plazmic::CombatHistoryStore::privacy_key("changed mode activity");
        plazmic::ActivityTracker changed_mode(changed_mode_state);
        changed_mode.set_retention_enabled(true);
        require(changed_mode.select(changed_mode_key),
                "changed-mode partition could not be selected");
        changed_mode.consume(
            timestamped(persistence_event,
                        "--You have looted Initial Mode Gem.--"),
            kCharacter, "synthetic_zone");
        require(changed_mode.snapshot(persistence_now).persisted,
                "changed-mode fixture was not persisted");
        const auto changed_mode_directory =
            changed_mode_state / "activity";
        require(::chmod(changed_mode_directory.c_str(), 0750) == 0,
                "cannot change retained activity directory mode");
        changed_mode.consume(
            timestamped(persistence_event + std::chrono::seconds(1),
                        "--You have looted Rejected Mode Gem.--"),
            kCharacter, "synthetic_zone");
        struct stat changed_mode_status {};
        require(!changed_mode.snapshot(persistence_now).persisted &&
                    ::stat(changed_mode_directory.c_str(),
                           &changed_mode_status) == 0 &&
                    (changed_mode_status.st_mode & 0777) == 0750,
                "save repaired an unsafe retained directory instead of "
                "failing closed");
        require(::chmod(changed_mode_directory.c_str(), 0700) == 0,
                "cannot restore changed-mode fixture permissions");

        const auto changed_file_state = directory / "changed-file-state";
        const std::string changed_file_key =
            plazmic::CombatHistoryStore::privacy_key("changed file activity");
        plazmic::ActivityTracker changed_file(changed_file_state);
        changed_file.set_retention_enabled(true);
        require(changed_file.select(changed_file_key),
                "changed-file partition could not be selected");
        changed_file.consume(
            timestamped(persistence_event,
                        "--You have looted Initial File Mode Gem.--"),
            kCharacter, "synthetic_zone");
        require(changed_file.snapshot(persistence_now).persisted,
                "changed-file fixture was not persisted");
        const auto changed_file_path = changed_file_state / "activity" /
                                       (changed_file_key + ".json");
        require(::chmod(changed_file_path.c_str(), 0640) == 0,
                "cannot change retained activity file mode");
        changed_file.consume(
            timestamped(persistence_event + std::chrono::seconds(1),
                        "--You have looted Rejected File Mode Gem.--"),
            kCharacter, "synthetic_zone");
        struct stat changed_file_status {};
        require(!changed_file.snapshot(persistence_now).persisted &&
                    ::stat(changed_file_path.c_str(),
                           &changed_file_status) == 0 &&
                    (changed_file_status.st_mode & 0777) == 0640,
                "save replaced an unsafe retained file instead of failing "
                "closed");

        require(restored.delete_history(activity_key) &&
                    !std::filesystem::exists(activity_file),
                "activity deletion did not remove the selected partition");
        for (const auto& entry :
             std::filesystem::directory_iterator(activity_directory)) {
            require(entry.path().filename().string().find(activity_key) ==
                        std::string::npos,
                    "activity deletion left a partition artifact");
        }

        plazmic::ActivityTracker missing_state_root(
            std::filesystem::path{});
        missing_state_root.set_retention_enabled(true);
        require(!missing_state_root.select(activity_key),
                "empty activity state root did not fail closed");

        const auto missing_delete_state = directory / "missing-delete-state";
        const std::string missing_delete_key =
            plazmic::CombatHistoryStore::privacy_key("missing delete activity");
        plazmic::ActivityTracker missing_delete(missing_delete_state);
        require(missing_delete.select(missing_delete_key),
                "missing-directory deletion fixture could not be selected");
        missing_delete.consume(
            timestamped(persistence_event,
                        "--You have looted Memory Only Gem.--"),
            kCharacter, "synthetic_zone");
        require(missing_delete.snapshot(persistence_now).events.size() == 1U &&
                    missing_delete.delete_history(missing_delete_key) &&
                    missing_delete.snapshot(persistence_now).events.empty(),
                "missing-directory deletion retained in-memory activity");
        missing_delete.set_retention_enabled(true);
        const auto missing_delete_file =
            missing_delete_state / "activity" /
            (missing_delete_key + ".json");
        std::ifstream missing_delete_input(
            missing_delete_file, std::ios::binary);
        const std::string missing_delete_bytes{
            std::istreambuf_iterator<char>(missing_delete_input), {}};
        require(missing_delete.snapshot(persistence_now).events.empty() &&
                    missing_delete_bytes.find("Memory Only Gem") ==
                        std::string::npos,
                "deleted memory-only activity was persisted after opt-in");

        const std::string merge_key =
            plazmic::CombatHistoryStore::privacy_key("merge activity");
        plazmic::ActivityTracker merge_prior(state_root);
        merge_prior.set_retention_enabled(true);
        require(merge_prior.select(merge_key),
                "merge fixture partition could not be selected");
        merge_prior.consume(
            timestamped(persistence_event - std::chrono::minutes(1),
                        "--You have looted Prior Gem.--"),
            kCharacter, "synthetic_zone");
        (void)merge_prior.snapshot(persistence_now);
        plazmic::ActivityTracker merge_session(state_root);
        require(merge_session.select(merge_key),
                "merge session partition could not be selected");
        const std::string repeated_loot = timestamped(
            persistence_event, "--You have looted Repeated Gem.--");
        merge_session.consume(
            repeated_loot, kCharacter, "synthetic_zone");
        merge_session.consume(
            repeated_loot, kCharacter, "synthetic_zone");
        merge_session.set_retention_enabled(true);
        require(merge_session.snapshot(persistence_now).events.size() == 3U,
                "retention enable collapsed genuine repeated events");

        const std::string replay_only_key =
            plazmic::CombatHistoryStore::privacy_key("replay-only activity");
        plazmic::ActivityTracker replay_only(state_root);
        replay_only.set_retention_enabled(true);
        require(replay_only.select(replay_only_key),
                "replay-only partition could not be selected");
        for (std::size_t index = 0U;
             index < plazmic::ActivityTracker::maximum_events; ++index) {
            replay_only.consume(
                timestamped(
                    persistence_now - std::chrono::seconds(
                        static_cast<std::int64_t>(index + 100U)),
                    "--You have looted Replay Fill " +
                        std::to_string(index) + ".--"),
                kCharacter, "synthetic_zone");
        }
        require(replay_only.snapshot(persistence_now).persisted,
                "replay-only fixture was not persisted");
        const std::string rejected_source = replay_only.consume(
            timestamped(
                persistence_now - std::chrono::hours(1),
                "--You have looted Rejected Display Event.--"),
            kCharacter, "synthetic_zone");
        const auto replay_only_snapshot = replay_only.snapshot(persistence_now);
        std::ifstream replay_only_input(
            state_root / "activity" / (replay_only_key + ".json"),
            std::ios::binary);
        const std::string replay_only_bytes{
            std::istreambuf_iterator<char>(replay_only_input), {}};
        require(replay_only_snapshot.persisted &&
                    replay_only_snapshot.events.size() ==
                        plazmic::ActivityTracker::maximum_events &&
                    !rejected_source.empty() &&
                    replay_only_bytes.find(rejected_source) !=
                        std::string::npos,
                "replay-only metadata was reported persisted without a save");

        const std::string ordered_merge_key =
            plazmic::CombatHistoryStore::privacy_key("ordered merge activity");
        plazmic::ActivityTracker ordered_prior(state_root);
        ordered_prior.set_retention_enabled(true);
        require(ordered_prior.select(ordered_merge_key),
                "ordered-merge partition could not be selected");
        for (std::size_t index = 0U;
             index < plazmic::ActivityTracker::maximum_events; ++index) {
            ordered_prior.consume(
                timestamped(
                    persistence_now - std::chrono::seconds(
                        static_cast<std::int64_t>(index + 1U)),
                    "--You have looted Ordered Stored " +
                        std::to_string(index) + ".--"),
                kCharacter, "synthetic_zone");
        }
        require(ordered_prior.snapshot(persistence_now).persisted,
                "ordered-merge fixture was not persisted");
        plazmic::ActivityTracker ordered_session(state_root);
        require(ordered_session.select(ordered_merge_key),
                "ordered-merge session could not select its partition");
        ordered_session.consume(
            timestamped(persistence_now,
                        "--You have looted Ordered Session.--"),
            kCharacter, "synthetic_zone");
        ordered_session.set_retention_enabled(true);
        const auto ordered_snapshot = ordered_session.snapshot(persistence_now);
        require(std::ranges::find(
                    ordered_snapshot.events,
                    std::string("Ordered Stored 0"),
                    &plazmic::ActivityEventSnapshot::label) !=
                    ordered_snapshot.events.end() &&
                    std::ranges::find(
                        ordered_snapshot.events,
                        std::string("Ordered Stored 511"),
                        &plazmic::ActivityEventSnapshot::label) ==
                        ordered_snapshot.events.end() &&
                    std::ranges::find(
                        ordered_snapshot.events,
                        std::string("Ordered Session"),
                        &plazmic::ActivityEventSnapshot::label) !=
                        ordered_snapshot.events.end(),
                "retention merge evicted vector order instead of oldest time");

        const std::string older_merge_key =
            plazmic::CombatHistoryStore::privacy_key("older merge activity");
        plazmic::ActivityTracker older_merge_prior(state_root);
        older_merge_prior.set_retention_enabled(true);
        require(older_merge_prior.select(older_merge_key),
                "older-merge partition could not be selected");
        for (std::size_t index = 0U;
             index < plazmic::ActivityTracker::maximum_events; ++index) {
            older_merge_prior.consume(
                timestamped(
                    persistence_now - std::chrono::seconds(
                        static_cast<std::int64_t>(index + 1U)),
                    "--You have looted Newer Stored " +
                        std::to_string(index) + ".--"),
                kCharacter, "synthetic_zone");
        }
        require(older_merge_prior.snapshot(persistence_now).persisted,
                "older-merge fixture was not persisted");
        plazmic::ActivityTracker older_merge_session(state_root);
        require(older_merge_session.select(older_merge_key),
                "older-merge session could not select its partition");
        older_merge_session.consume(
            timestamped(persistence_now - std::chrono::hours(2),
                        "--You have looted Older Session.--"),
            kCharacter, "synthetic_zone");
        older_merge_session.set_retention_enabled(true);
        const auto older_merge_snapshot =
            older_merge_session.snapshot(persistence_now);
        require(older_merge_snapshot.events.size() ==
                        plazmic::ActivityTracker::maximum_events &&
                    std::ranges::find(
                        older_merge_snapshot.events,
                        std::string("Older Session"),
                        &plazmic::ActivityEventSnapshot::label) ==
                        older_merge_snapshot.events.end(),
                "retention merge displaced newer history with an older event");

        const std::string replay_merge_key =
            plazmic::CombatHistoryStore::privacy_key("replay merge activity");
        const std::string retained_replay_line = timestamped(
            persistence_event, "--You have looted Retained Replay Gem.--");
        plazmic::ActivityTracker replay_merge_prior(state_root);
        replay_merge_prior.set_retention_enabled(true);
        require(replay_merge_prior.select(replay_merge_key),
                "replay merge partition could not be selected");
        replay_merge_prior.consume(
            retained_replay_line, kCharacter, "synthetic_zone");
        (void)replay_merge_prior.snapshot(persistence_now);
        plazmic::ActivityTracker replay_merge_session(state_root);
        require(replay_merge_session.select(replay_merge_key),
                "replay merge session could not select its partition");
        replay_merge_session.consume(
            timestamped(persistence_event - std::chrono::seconds(1),
                        "--You have looted Session Gem.--"),
            kCharacter, "synthetic_zone");
        replay_merge_session.set_retention_enabled(true);
        replay_merge_session.begin_log_stream(retained_replay_line + "\n");
        replay_merge_session.consume(
            retained_replay_line, kCharacter, "synthetic_zone");
        const auto replay_merge_snapshot =
            replay_merge_session.snapshot(persistence_now);
        require(std::ranges::count(
                    replay_merge_snapshot.events,
                    std::string("Retained Replay Gem"),
                    &plazmic::ActivityEventSnapshot::label) == 1,
                "retention merge did not rebuild replay metadata");
        require(std::ranges::find(
                    replay_merge_snapshot.events,
                    std::string("Session Gem"),
                    &plazmic::ActivityEventSnapshot::label) !=
                    replay_merge_snapshot.events.end(),
                "stale replay boundary discarded a verified session event");

        const std::string ability_merge_key =
            plazmic::CombatHistoryStore::privacy_key("ability merge cap");
        plazmic::ActivityTracker ability_merge_prior(state_root);
        ability_merge_prior.set_retention_enabled(true);
        require(ability_merge_prior.select(ability_merge_key),
                "ability merge partition could not be selected");
        for (std::size_t index = 0U;
             index < plazmic::ActivityTracker::maximum_abilities; ++index) {
            ability_merge_prior.observe_damage(
                plazmic::DamageEvent{
                    .timestamp = persistence_event +
                                 std::chrono::seconds(index),
                    .attacker = std::string(kCharacter),
                    .defender = "Synthetic Target",
                    .damage = 1U,
                    .kind = plazmic::DamageKind::spell,
                    .ability = "Stored Ability " + std::to_string(index),
                },
                kCharacter,
                persisted_source("stored-observation-" +
                                     std::to_string(index),
                                 index));
        }
        require(ability_merge_prior.snapshot(persistence_now).persisted,
                "ability merge fixture was not persisted");
        plazmic::ActivityTracker ability_merge_session(state_root);
        require(ability_merge_session.select(ability_merge_key),
                "ability merge session could not select its partition");
        ability_merge_session.observe_damage(
            plazmic::DamageEvent{
                .timestamp = persistence_now - std::chrono::seconds(1),
                .attacker = std::string(kCharacter),
                .defender = "Synthetic Target",
                .damage = 1U,
                .kind = plazmic::DamageKind::spell,
                .ability = "New Session Ability",
            },
            kCharacter, persisted_source("new-session-observation"));
        ability_merge_session.set_retention_enabled(true);
        const auto bounded_ability_merge =
            ability_merge_session.snapshot(persistence_now);
        plazmic::ActivityTracker ability_merge_restored(state_root);
        ability_merge_restored.set_retention_enabled(true);
        require(bounded_ability_merge.persisted &&
                    bounded_ability_merge.abilities.size() ==
                        plazmic::ActivityTracker::maximum_abilities &&
                    ability_merge_restored.select(ability_merge_key) &&
                    ability_merge_restored.snapshot(persistence_now)
                            .abilities.size() ==
                        plazmic::ActivityTracker::maximum_abilities,
                "retention merge exceeded the ability/category pair cap");

        const std::string toggle_key =
            plazmic::CombatHistoryStore::privacy_key("toggle activity");
        plazmic::ActivityTracker toggled(state_root);
        toggled.set_retention_enabled(true);
        require(toggled.select(toggle_key),
                "toggle fixture could not be selected");
        const auto first_observation = persistence_now -
                                       std::chrono::minutes(2);
        toggled.observe_damage(
            plazmic::DamageEvent{
                .timestamp = first_observation,
                .attacker = std::string(kCharacter),
                .defender = "Synthetic Target",
                .damage = 100U,
                .kind = plazmic::DamageKind::spell,
                .ability = "Toggle Burst",
            },
            kCharacter,
            persisted_source("toggle-spell-observation-1"));
        (void)toggled.snapshot(persistence_now);
        toggled.set_retention_enabled(false);
        toggled.observe_damage(
            plazmic::DamageEvent{
                .timestamp = persistence_now - std::chrono::minutes(1),
                .attacker = std::string(kCharacter),
                .defender = "Synthetic Target",
                .damage = 200U,
                .kind = plazmic::DamageKind::spell,
                .ability = "Toggle Burst",
            },
            kCharacter,
            persisted_source("toggle-spell-observation-2"));
        toggled.set_retention_enabled(true);
        const auto toggle_snapshot = toggled.snapshot(persistence_now);
        require(toggle_snapshot.abilities.size() == 1U &&
                    toggle_snapshot.abilities.front().damage == 300U &&
                    toggle_snapshot.abilities.front().observations == 2U,
                "retention toggle duplicated ability aggregates");

        const std::string rewrite_key =
            plazmic::CombatHistoryStore::privacy_key("rewrite activity");
        const auto rewrite_file =
            state_root / "activity" / (rewrite_key + ".json");
        const std::string rewrite_source = persisted_source("rewrite-old");
        const std::string rewrite_retained_source =
            persisted_source("rewrite-retained");
        const auto expired_seconds =
            std::chrono::duration_cast<std::chrono::seconds>(
                (persistence_now - plazmic::ActivityTracker::maximum_age -
                 std::chrono::hours(1))
                    .time_since_epoch())
                .count();
        {
            std::ofstream output(rewrite_file, std::ios::binary);
            output << "{\"schema\":1,\"events\":[{\"kind\":0,"
                      "\"timestamp\":\""
                   << expired_seconds
                   << "\",\"zone\":\"synthetic_zone\","
                      "\"label\":\"Experience gained\","
                      "\"amount\":0.125,\"evidence\":\"Synthetic\","
                      "\"sourceId\":\""
                   << rewrite_source
                   << "\"},{\"kind\":2,\"timestamp\":\""
                   << std::chrono::duration_cast<std::chrono::seconds>(
                          persistence_event.time_since_epoch())
                          .count()
                   << "\",\"zone\":\"synthetic_zone\","
                      "\"label\":\"Retained Item\",\"amount\":1,"
                      "\"evidence\":\"Synthetic\",\"sourceId\":\""
                   << rewrite_retained_source
                   << "\"}],\"abilities\":[],\"replaySources\":[\""
                   << rewrite_source << "\",\"" << rewrite_retained_source
                   << "\"]}";
            require(output.good(),
                    "rewrite-failure fixture could not be written");
        }
        plazmic::ActivityTracker rewrite_failure(state_root);
        require(rewrite_failure.select(rewrite_key),
                "rewrite-failure partition could not be selected");
        rewrite_failure.consume(
            timestamped(persistence_event,
                        "--You have looted Session Item.--"),
            kCharacter, "synthetic_zone");
        struct rlimit previous_file_limit {};
        require(::getrlimit(RLIMIT_FSIZE, &previous_file_limit) == 0,
                "file-size limit could not be read");
        struct sigaction ignored_signal {};
        struct sigaction previous_signal {};
        ignored_signal.sa_handler = SIG_IGN;
        require(::sigemptyset(&ignored_signal.sa_mask) == 0 &&
                    ::sigaction(SIGXFSZ, &ignored_signal,
                                &previous_signal) == 0,
                "file-size signal could not be ignored");
        const struct rlimit blocked_file_limit{
            .rlim_cur = 0,
            .rlim_max = previous_file_limit.rlim_max,
        };
        require(::setrlimit(RLIMIT_FSIZE, &blocked_file_limit) == 0,
                "file writes could not be bounded for rewrite test");
        rewrite_failure.set_retention_enabled(true);
        const auto rewrite_failure_snapshot =
            rewrite_failure.snapshot(persistence_now);
        require(::setrlimit(RLIMIT_FSIZE, &previous_file_limit) == 0 &&
                    ::sigaction(SIGXFSZ, &previous_signal, nullptr) == 0,
                "file-write test limits could not be restored");
        require(rewrite_failure_snapshot.events.size() == 2U &&
                    std::ranges::find(
                        rewrite_failure_snapshot.events,
                        std::string("Session Item"),
                        &plazmic::ActivityEventSnapshot::label) !=
                        rewrite_failure_snapshot.events.end() &&
                    std::ranges::find(
                        rewrite_failure_snapshot.events,
                        std::string("Retained Item"),
                        &plazmic::ActivityEventSnapshot::label) !=
                        rewrite_failure_snapshot.events.end() &&
                    !rewrite_failure_snapshot.persisted,
                "failed load rewrite discarded loaded or session activity");

        const std::string retry_key =
            plazmic::CombatHistoryStore::privacy_key("retry activity");
        plazmic::ActivityTracker retry(state_root);
        require(retry.select(retry_key),
                "retry partition could not be selected");
        retry.set_retention_enabled(true);
        const auto retry_file =
            state_root / "activity" / (retry_key + ".json");
        std::filesystem::remove(retry_file);
        std::filesystem::create_directories(retry_file / "sentinel");
        retry.consume(
            timestamped(persistence_event,
                        "--You have looted Retry Item.--"),
            kCharacter, "synthetic_zone");
        require(!retry.snapshot(persistence_now).persisted,
                "blocked activity save unexpectedly succeeded");
        std::filesystem::remove_all(retry_file);
        require(!retry.snapshot(persistence_now).persisted &&
                    !std::filesystem::exists(retry_file),
                "activity save ignored its retry backoff");
        std::this_thread::sleep_for(
            plazmic::ActivityTracker::persistence_retry_delay +
            std::chrono::milliseconds(50));
        require(retry.snapshot(persistence_now).persisted &&
                    std::filesystem::is_regular_file(retry_file),
                "activity save did not retry after its backoff");

        const std::string blocked_key =
            plazmic::CombatHistoryStore::privacy_key("blocked activity");
        const std::string next_key =
            plazmic::CombatHistoryStore::privacy_key("next activity");
        plazmic::ActivityTracker blocked(state_root);
        require(blocked.select(blocked_key),
                "blocked fixture could not be selected");
        blocked.set_retention_enabled(true);
        const auto blocked_file =
            state_root / "activity" / (blocked_key + ".json");
        std::filesystem::remove(blocked_file);
        std::filesystem::create_directories(blocked_file / "sentinel");
        blocked.consume(
            timestamped(persistence_event,
                        "You gain experience! (0.500%)"),
            kCharacter, "synthetic_zone");
        require(!blocked.select(next_key) &&
                    blocked.snapshot(persistence_now).storage_key == blocked_key &&
                    blocked.snapshot(persistence_now).events.size() == 1U,
                "failed partition switch discarded unsaved activity");

        const std::string deferred_key =
            plazmic::CombatHistoryStore::privacy_key("deferred activity");
        const std::string deferred_next_key =
            plazmic::CombatHistoryStore::privacy_key(
                "deferred next activity");
        plazmic::ActivityTracker deferred(state_root);
        deferred.set_retention_enabled(true);
        require(deferred.select(deferred_key),
                "deferred fixture could not be selected");
        deferred.consume(
            timestamped(persistence_event,
                        "You gain experience! (0.125%)"),
            kCharacter, "synthetic_zone");
        require(deferred.snapshot(persistence_now).persisted,
                "deferred fixture could not be persisted");
        const auto deferred_file =
            state_root / "activity" / (deferred_key + ".json");
        std::filesystem::remove(deferred_file);
        std::filesystem::create_directories(deferred_file / "sentinel");
        deferred.consume(
            timestamped(persistence_event + std::chrono::seconds(1),
                        "You gain experience! (0.250%)"),
            kCharacter, "synthetic_zone");
        require(!deferred.snapshot(persistence_now).persisted,
                "deferred fixture did not retain an unsaved observation");
        deferred.begin_deferred_persistence();
        require(!deferred.select(deferred_next_key) &&
                    deferred.snapshot(persistence_now).storage_key ==
                        deferred_key &&
                    deferred.snapshot(persistence_now).events.size() == 2U,
                "deferred partition switch discarded unsaved activity");
        std::filesystem::remove_all(deferred_file);
        deferred.commit_deferred_persistence(true);
        require(deferred.snapshot(persistence_now).persisted &&
                    deferred.select(deferred_next_key) &&
                    deferred.select(deferred_key) &&
                    deferred.snapshot(persistence_now).events.size() == 2U,
                "deferred activity did not persist and restore after commit");

        const std::string undeletable_key =
            plazmic::CombatHistoryStore::privacy_key("undeletable activity");
        plazmic::ActivityTracker undeletable(state_root);
        require(undeletable.select(undeletable_key),
                "undeletable fixture could not be selected");
        const auto undeletable_file =
            state_root / "activity" / (undeletable_key + ".json");
        std::filesystem::create_directories(undeletable_file / "sentinel");
        undeletable.consume(
            timestamped(persistence_event,
                        "You gain experience! (0.500%)"),
            kCharacter, "synthetic_zone");
        require(!undeletable.delete_history(undeletable_key) &&
                    undeletable.snapshot(persistence_now).events.size() == 1U,
                "failed deletion cleared retained in-memory activity");

        const std::string first_key =
            plazmic::CombatHistoryStore::privacy_key("first activity");
        const std::string second_key =
            plazmic::CombatHistoryStore::privacy_key("second activity");
        plazmic::ActivityTracker targeted_delete(state_root);
        targeted_delete.set_retention_enabled(true);
        require(targeted_delete.select(first_key),
                "first deletion fixture could not be selected");
        targeted_delete.consume(
            timestamped(persistence_event,
                        "You gain experience! (0.125%)"),
            kCharacter, "synthetic_zone");
        (void)targeted_delete.snapshot(persistence_now);
        require(targeted_delete.select(second_key),
                "second deletion fixture could not be selected");
        targeted_delete.consume(
            timestamped(persistence_event + std::chrono::seconds(1),
                        "You gain experience! (0.250%)"),
            kCharacter, "synthetic_zone");
        const auto second_before_delete =
            targeted_delete.snapshot(persistence_now);
        require(targeted_delete.select(first_key) &&
                    targeted_delete.snapshot(persistence_now).events.size() ==
                        1U &&
                    targeted_delete.snapshot(persistence_now)
                            .events.front()
                            .amount == 0.125 &&
                    targeted_delete.select(second_key) &&
                    targeted_delete.snapshot(persistence_now).events ==
                        second_before_delete.events,
                "successful partition switch did not restore prior activity");
        const bool deleted_first =
            targeted_delete.delete_history(first_key);
        const auto second_after_delete =
            targeted_delete.snapshot(persistence_now);
        require(deleted_first &&
                    second_after_delete.storage_key == second_key &&
                    second_after_delete.events ==
                        second_before_delete.events,
                "explicit deletion changed the newly selected partition");

        const std::string expired_key =
            plazmic::CombatHistoryStore::privacy_key("expired activity");
        const std::string sweep_key =
            plazmic::CombatHistoryStore::privacy_key("sweep activity");
        const auto expired_time =
            persistence_now - plazmic::ActivityTracker::maximum_age -
            std::chrono::hours(1);
        plazmic::ActivityTracker expired_partition(state_root);
        expired_partition.set_retention_enabled(true);
        require(expired_partition.select(expired_key),
                "expired partition fixture could not be selected");
        const std::string expired_source = expired_partition.consume(
            timestamped(expired_time,
                        "You gain experience! (0.125%)"),
            kCharacter, "synthetic_zone");
        require(expired_partition.snapshot(expired_time +
                                           std::chrono::minutes(1))
                    .persisted,
                "expired partition fixture was not saved");
        const auto expired_file =
            state_root / "activity" / (expired_key + ".json");
        std::ifstream expired_input(expired_file, std::ios::binary);
        const std::string expired_bytes{
            std::istreambuf_iterator<char>(expired_input), {}};
        const auto opt_out_state = directory / "opt-out-expiry-state";
        const std::string opt_out_key =
            plazmic::CombatHistoryStore::privacy_key("opt out expiry");
        plazmic::ActivityTracker opt_out_writer(opt_out_state);
        opt_out_writer.set_retention_enabled(true);
        require(opt_out_writer.select(opt_out_key),
                "opt-out expiry fixture could not be selected");
        opt_out_writer.consume(
            timestamped(expired_time,
                        "--You have looted Expired Opt Out Gem.--"),
            kCharacter, "synthetic_zone");
        require(opt_out_writer.snapshot(expired_time +
                                        std::chrono::minutes(1))
                    .persisted,
                "opt-out expiry fixture was not persisted");
        plazmic::ActivityTracker opt_out_maintenance(opt_out_state);
        (void)opt_out_maintenance.snapshot(persistence_now);
        std::ifstream opt_out_input(
            opt_out_state / "activity" / (opt_out_key + ".json"),
            std::ios::binary);
        const std::string opt_out_bytes{
            std::istreambuf_iterator<char>(opt_out_input), {}};
        require(opt_out_bytes.find("Expired Opt Out Gem") ==
                    std::string::npos,
                "retention opt-out stopped expiry maintenance");
        const auto batch_root = directory / "batch-state";
        std::filesystem::create_directories(batch_root / "activity");
        std::vector<std::filesystem::path> batch_files;
        for (std::size_t index = 0U;
             index < plazmic::ActivityTracker::maximum_sweep_partitions + 1U;
             ++index) {
            const std::string key = plazmic::CombatHistoryStore::privacy_key(
                "batch activity " + std::to_string(index));
            const auto file = batch_root / "activity" / (key + ".json");
            std::ofstream output(file, std::ios::binary);
            output << expired_bytes;
            require(output.good(), "batch sweep fixture could not be written");
            batch_files.push_back(file);
        }
        plazmic::ActivityTracker batch_sweeper(batch_root);
        batch_sweeper.set_retention_enabled(true);
        require(batch_sweeper.select(
                    plazmic::CombatHistoryStore::privacy_key("batch sweep")),
                "batch sweep partition could not be selected");
        (void)batch_sweeper.snapshot(persistence_now);
        const auto count_unexpired_batch_files = [&batch_files]() {
            return std::ranges::count_if(batch_files, [](const auto& file) {
                std::ifstream input(file, std::ios::binary);
                const std::string bytes{
                    std::istreambuf_iterator<char>(input), {}};
                return bytes.find("Experience gained") != std::string::npos;
            });
        };
        require(count_unexpired_batch_files() == 1,
                "activity sweep did not stop at its per-refresh batch bound");
        (void)batch_sweeper.snapshot(persistence_now +
                                     std::chrono::seconds(2));
        require(count_unexpired_batch_files() == 0,
                "activity sweep did not resume after its batch bound");
        plazmic::ActivityTracker sweeper(state_root);
        sweeper.set_retention_enabled(true);
        require(sweeper.select(sweep_key),
                "activity sweep fixture could not be selected");
        for (std::size_t batch = 0U; batch < 140U; ++batch) {
            (void)sweeper.snapshot(
                persistence_now + std::chrono::seconds(batch * 2U));
        }
        plazmic::ActivityTracker swept(state_root);
        swept.set_retention_enabled(true);
        require(swept.select(expired_key) &&
                    swept.snapshot(persistence_now).events.empty(),
                "inactive activity partition was not expired");
        std::ifstream swept_input(expired_file, std::ios::binary);
        const std::string swept_bytes{
            std::istreambuf_iterator<char>(swept_input), {}};
        require(swept_bytes.find(expired_source) == std::string::npos,
                "expired activity retained offline-guessable replay metadata");

        const std::string incompatible_key =
            plazmic::CombatHistoryStore::privacy_key("future activity");
        const auto incompatible_file =
            state_root / "activity" / (incompatible_key + ".json");
        {
            std::ofstream output(incompatible_file, std::ios::binary);
            output << R"({"schema":2,"events":[],"abilities":[]})";
            require(output.good(),
                    "incompatible fixture could not be written");
        }
        plazmic::ActivityTracker incompatible(state_root);
        incompatible.set_retention_enabled(true);
        require(!incompatible.select(incompatible_key),
                "future activity schema was accepted");
        const auto incompatible_snapshot = incompatible.snapshot();
        require(incompatible_snapshot.storage_key == incompatible_key &&
                    !incompatible_snapshot.persisted &&
                    incompatible.delete_history(incompatible_key) &&
                    !std::filesystem::exists(incompatible_file),
                "incompatible activity partition could not be deleted");

        const std::string invalid_event_key =
            plazmic::CombatHistoryStore::privacy_key("invalid event");
        const auto invalid_event_file =
            state_root / "activity" / (invalid_event_key + ".json");
        {
            std::ofstream output(invalid_event_file, std::ios::binary);
            output << R"({"schema":1,"events":[{"kind":0,"timestamp":"1","zone":"synthetic_zone","label":"Experience gained","amount":101,"evidence":"Synthetic"}],"abilities":[]})";
            require(output.good(),
                    "invalid event fixture could not be written");
        }
        plazmic::ActivityTracker invalid_event(state_root);
        invalid_event.set_retention_enabled(true);
        require(!invalid_event.select(invalid_event_key),
                "invalid restored event fields were accepted");

        const std::string duplicate_source_key =
            plazmic::CombatHistoryStore::privacy_key("duplicate source");
        const auto duplicate_source_file =
            state_root / "activity" / (duplicate_source_key + ".json");
        const auto persistence_seconds =
            std::chrono::duration_cast<std::chrono::seconds>(
                persistence_event.time_since_epoch())
                .count();
        const std::string valid_persisted_source =
            "0123456789ab:0123456789abcdef0123456789abcdef:0";
        {
            std::ofstream output(duplicate_source_file, std::ios::binary);
            const std::string event =
                R"({"kind":2,"timestamp":")" +
                std::to_string(persistence_seconds) +
                R"(","zone":"synthetic_zone","label":"Synthetic Gem","amount":1,"evidence":"Synthetic","sourceId":")" +
                valid_persisted_source + R"("})";
            output << R"({"schema":1,"events":[)" << event << ',' << event
                   << R"(],"abilities":[]})";
            require(output.good(),
                    "duplicate source fixture could not be written");
        }
        plazmic::ActivityTracker duplicate_source(state_root);
        duplicate_source.set_retention_enabled(true);
        require(!duplicate_source.select(duplicate_source_key),
                "duplicate persisted source identifiers were accepted");

        const std::string invalid_source_key =
            plazmic::CombatHistoryStore::privacy_key("invalid source");
        const auto invalid_source_file =
            state_root / "activity" / (invalid_source_key + ".json");
        {
            std::ofstream output(invalid_source_file, std::ios::binary);
            output
                << R"({"schema":1,"events":[{"kind":2,"timestamp":")"
                << persistence_seconds
                << R"(","zone":"synthetic_zone","label":"Synthetic Gem","amount":1,"evidence":"Synthetic","sourceId":"arbitrary-printable-id"}],"abilities":[]})";
            require(output.good(),
                    "invalid source fixture could not be written");
        }
        plazmic::ActivityTracker invalid_source(state_root);
        invalid_source.set_retention_enabled(true);
        require(!invalid_source.select(invalid_source_key),
                "invalid persisted event source identifier was accepted");

        for (const std::string_view ordinal : {"4096", "00"}) {
            const std::string ordinal_key =
                plazmic::CombatHistoryStore::privacy_key(
                    std::string("invalid source ordinal ") +
                    std::string(ordinal));
            const auto ordinal_file =
                state_root / "activity" / (ordinal_key + ".json");
            {
                std::ofstream output(ordinal_file, std::ios::binary);
                output
                    << R"({"schema":1,"events":[{"kind":2,"timestamp":")"
                    << persistence_seconds
                    << R"(","zone":"synthetic_zone","label":"Synthetic Gem","amount":1,"evidence":"Synthetic","sourceId":"0123456789ab:0123456789abcdef0123456789abcdef:)"
                    << ordinal << R"("}],"abilities":[]})";
            }
            plazmic::ActivityTracker invalid_ordinal(state_root);
            invalid_ordinal.set_retention_enabled(true);
            require(!invalid_ordinal.select(ordinal_key),
                    "noncanonical or out-of-range source ordinal was "
                    "accepted");
        }

        const std::string missing_source_key =
            plazmic::CombatHistoryStore::privacy_key("missing source");
        const auto missing_source_file =
            state_root / "activity" / (missing_source_key + ".json");
        {
            std::ofstream output(missing_source_file, std::ios::binary);
            output
                << R"({"schema":1,"events":[{"kind":0,"timestamp":")"
                << persistence_seconds
                << R"(","zone":"synthetic_zone","label":"Experience gained","amount":1,"evidence":"Synthetic"}],"abilities":[]})";
            require(output.good(),
                    "missing source fixture could not be written");
        }
        plazmic::ActivityTracker missing_source(state_root);
        missing_source.set_retention_enabled(true);
        require(!missing_source.select(missing_source_key),
                "missing persisted log source identifier was accepted");

        const std::string invalid_ability_source_key =
            plazmic::CombatHistoryStore::privacy_key(
                "invalid ability source");
        const auto invalid_ability_source_file =
            state_root / "activity" /
            (invalid_ability_source_key + ".json");
        {
            std::ofstream output(invalid_ability_source_file,
                                 std::ios::binary);
            output
                << R"({"schema":1,"events":[],"abilities":[{"name":"Synthetic Burst","category":"Spell","observations":[{"sourceId":"arbitrary-printable-id","damage":"1","timestamp":")"
                << persistence_seconds
                << R"("}]}]})";
            require(output.good(),
                    "invalid ability source fixture could not be written");
        }
        plazmic::ActivityTracker invalid_ability_source(state_root);
        invalid_ability_source.set_retention_enabled(true);
        require(!invalid_ability_source.select(invalid_ability_source_key),
                "invalid persisted ability source identifier was accepted");

        const std::string invalid_ability_category_key =
            plazmic::CombatHistoryStore::privacy_key(
                "invalid ability category");
        const auto invalid_ability_category_file =
            state_root / "activity" /
            (invalid_ability_category_key + ".json");
        {
            std::ofstream output(invalid_ability_category_file,
                                 std::ios::binary);
            output
                << R"({"schema":1,"events":[],"abilities":[{"name":"Synthetic Burst","category":"Class confirmed","observations":[{"sourceId":")"
                << valid_persisted_source
                << R"(","damage":"1","timestamp":")"
                << persistence_seconds
                << R"("}]}]})";
            require(output.good(),
                    "invalid ability category fixture could not be written");
        }
        plazmic::ActivityTracker invalid_ability_category(state_root);
        invalid_ability_category.set_retention_enabled(true);
        require(!invalid_ability_category.select(
                    invalid_ability_category_key),
                "invalid persisted ability category was accepted");

        const auto symlink_state = directory / "symlink-state";
        const auto symlink_target = directory / "symlink-target";
        std::filesystem::create_directories(symlink_state);
        std::filesystem::create_directories(symlink_target);
        std::filesystem::create_directory_symlink(
            symlink_target, symlink_state / "activity");
        const std::string symlink_key =
            plazmic::CombatHistoryStore::privacy_key("symlink activity");
        plazmic::ActivityTracker symlink_save(symlink_state);
        require(symlink_save.select(symlink_key),
                "symlink save fixture could not select its key");
        symlink_save.consume(
            timestamped(persistence_event,
                        "--You have looted Symlink Item.--"),
            kCharacter, "synthetic_zone");
        symlink_save.set_retention_enabled(true);
        require(!symlink_save.snapshot(persistence_now).persisted &&
                    !std::filesystem::exists(
                        symlink_target / (symlink_key + ".json")),
                "activity save followed a symlinked parent directory");
        const auto symlink_delete_target =
            symlink_target / (symlink_key + ".json");
        {
            std::ofstream output(symlink_delete_target, std::ios::binary);
            output << "synthetic";
        }
        require(!symlink_save.delete_history(symlink_key) &&
                    std::filesystem::is_regular_file(symlink_delete_target),
                "activity deletion followed a symlinked parent directory");

        std::filesystem::remove_all(directory);
        directory.clear();
        std::cout << "bounded progression and activity tracker passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        if (!directory.empty()) {
            std::error_code ignored;
            std::filesystem::remove_all(directory, ignored);
        }
        std::cerr << "test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
