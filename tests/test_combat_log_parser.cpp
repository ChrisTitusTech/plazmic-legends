#include "game/combat_log_parser.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include <unistd.h>

namespace {

using namespace std::chrono_literals;

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::filesystem::path fixture_directory() {
    std::string pattern = "/tmp/plazmic-combat-XXXXXX";
    char* created = ::mkdtemp(pattern.data());
    if (created == nullptr) {
        throw std::runtime_error("cannot create combat fixture directory");
    }
    return created;
}

void append(const std::filesystem::path& path, std::string_view value) {
    std::ofstream output(path, std::ios::app | std::ios::binary);
    output << value;
    if (!output) {
        throw std::runtime_error("cannot append combat fixture");
    }
}

}  // namespace

int main() {
    std::filesystem::path directory;
    try {
        constexpr std::string_view kCharacter = "Testhero";
        const auto melee = plazmic::parse_damage_line(
            "[Sun Aug 03 12:00:00 2026] You slash Training Target "
            "for 120 points of damage.",
            kCharacter);
        require(melee && melee->attacker == kCharacter &&
                    melee->defender == "Training Target" &&
                    melee->damage == 120U &&
                    melee->kind == plazmic::DamageKind::melee,
                "melee damage line was not parsed");
        const auto singular = plazmic::parse_damage_line(
            "[Sun Aug 03 12:00:00 2026] You hit Training Target "
            "for 1 point of damage.",
            kCharacter);
        require(singular && singular->damage == 1U,
                "singular point damage line was not parsed");
        const auto defender_with_for = plazmic::parse_damage_line(
            "[Sun Aug 03 12:00:00 2026] You hit Trial for Power "
            "for 120 points of damage.",
            kCharacter);
        require(defender_with_for &&
                    defender_with_for->defender == "Trial for Power" &&
                    defender_with_for->damage == 120U,
                "defender containing the damage delimiter was not parsed");

        const auto spell = plazmic::parse_damage_line(
            "[Sun Aug 03 12:00:01 2026] Testmage hit Training Target "
            "for 300 points of magic damage by Synthetic Bolt.",
            kCharacter);
        require(spell && spell->attacker == "Testmage" &&
                    spell->damage == 300U &&
                    spell->kind == plazmic::DamageKind::spell,
                "spell damage line was not parsed");

        const auto damage_over_time = plazmic::parse_damage_line(
            "[Sun Aug 03 12:00:02 2026] Training Target has taken 80 "
            "damage from your Synthetic Affliction.",
            kCharacter);
        require(damage_over_time &&
                    damage_over_time->attacker == kCharacter &&
                    damage_over_time->damage == 80U &&
                    damage_over_time->kind ==
                        plazmic::DamageKind::damage_over_time,
                "damage-over-time line was not parsed");

        const auto pet = plazmic::parse_damage_line(
            "[Sun Aug 03 12:00:03 2026] Testpet (Owner: Testhero) bites "
            "Training Target for 50 points of damage.",
            kCharacter);
        require(pet && pet->kind == plazmic::DamageKind::pet &&
                    pet->attacker == kCharacter,
                "pet damage line was not classified");
        const auto frenzy = plazmic::parse_damage_line(
            "[Sun Aug 03 12:00:04 2026] You frenzy on Training Target "
            "for 75 points of damage.",
            kCharacter);
        require(frenzy && frenzy->attacker == kCharacter &&
                    frenzy->damage == 75U,
                "frenzy damage line was not parsed");
        const auto possessive_spell = plazmic::parse_damage_line(
            "[Sun Aug 03 12:00:05 2026] Testmage's Synthetic Bolt hits "
            "Training Target for 325 points of magic damage by Synthetic "
            "Bolt.",
            kCharacter);
        require(possessive_spell &&
                    possessive_spell->attacker == "Testmage" &&
                    possessive_spell->damage == 325U &&
                    possessive_spell->kind == plazmic::DamageKind::spell,
                "possessive spell was misattributed or misclassified");
        require(!plazmic::parse_damage_line(
                    "[Sun Aug 03 12:00:06 2026] Testhero says hello.",
                    kCharacter),
                "non-damage line was accepted");
        require(!plazmic::parse_damage_line(
                    "malformed log line", kCharacter),
                "malformed line was accepted");
        require(!plazmic::parse_damage_line(
                    "[Sun Aug 03 12:00:07 2026] Synthetic speaker says, "
                    "'You hit Training Target for 120 points of damage.'",
                    kCharacter),
                "combat-like quoted chat was accepted as damage");
        require(!plazmic::parse_damage_line(
                    "[Sun Aug 03 12:00:08 2026] Synthetic speaker says, "
                    "'Training Target has taken 80 damage from your "
                    "Synthetic Affliction.'",
                    kCharacter),
                "damage-over-time text in chat was accepted as damage");
        require(!plazmic::parse_damage_line(
                    "[Sun Aug 03 12:00:09 2026] Synthetic speaker tells the "
                    "group, 'You hit Training Target for 120 points of "
                    "damage.'",
                    kCharacter),
                "group chat containing combat text was accepted as damage");

        plazmic::CombatAccumulator accumulator;
        require(accumulator.add(*melee, kCharacter),
                "melee event was rejected");
        require(accumulator.add(*spell, kCharacter),
                "spell event was rejected");
        require(accumulator.add(*damage_over_time, kCharacter),
                "damage-over-time event was rejected");
        const auto active = accumulator.snapshot(
            damage_over_time->timestamp + 1s, kCharacter);
        require(active.state == plazmic::CombatEncounterState::active &&
                    active.total_damage == 500U &&
                    active.participants.size() == 2U,
                "active encounter aggregate is incorrect");
        require(active.participants[0].name == "Testmage" &&
                    active.participants[0].damage == 300U &&
                    active.participants[0].percentage == 60.0,
                "participants were not sorted or calculated");
        require(active.active_character_dps > 0.0,
                "active character DPS was not calculated");
        const auto complete = accumulator.snapshot(
            damage_over_time->timestamp + 11s, kCharacter);
        require(complete.state == plazmic::CombatEncounterState::complete,
                "encounter inactivity was not applied");
        require(!accumulator.add({
                    .timestamp = damage_over_time->timestamp + 11s,
                    .attacker = "Testmage",
                    .defender = "Second Training Target",
                    .damage = 900U,
                }, kCharacter),
                "other participant anchored a new encounter");
        const auto retained = accumulator.snapshot(
            damage_over_time->timestamp + 11s, kCharacter);
        require(retained.state ==
                        plazmic::CombatEncounterState::complete &&
                    retained.total_damage == 500U &&
                    retained.target == "Training Target",
                "rejected new anchor cleared the completed encounter");
        require(accumulator.add({
                    .timestamp = damage_over_time->timestamp + 11s,
                    .attacker = std::string(kCharacter),
                    .defender = "Second Training Target",
                    .damage = 25U,
                }, kCharacter),
                "new target after inactivity was rejected");
        const auto next_encounter = accumulator.snapshot(
            damage_over_time->timestamp + 12s, kCharacter);
        require(next_encounter.total_damage == 25U &&
                    next_encounter.target == "Second Training Target",
                "new target after inactivity retained the prior encounter");

        plazmic::CombatAccumulator participant_bound;
        for (std::size_t index = 0U;
             index < plazmic::CombatAccumulator::maximum_participants;
             ++index) {
            const std::string attacker = index == 0U
                ? std::string(kCharacter)
                : "Synthetic " + std::to_string(index);
            require(participant_bound.add({
                        .timestamp = melee->timestamp,
                        .attacker = attacker,
                        .defender = "Training Target",
                        .damage = 1U,
                    }, kCharacter),
                    "participant within the hard bound was rejected");
        }
        require(!participant_bound.add({
                    .timestamp = melee->timestamp,
                    .attacker = "Synthetic overflow",
                    .defender = "Training Target",
                    .damage = 1U,
                }, kCharacter),
                "participant above the hard bound was accepted");

        directory = fixture_directory();
        const auto logs = directory / "Logs";
        std::filesystem::create_directory(logs);
        const auto log = logs / "eqlog_Testhero_synthetic.txt";
        append(log,
               "[Sun Aug 03 12:00:00 2026] You slash Training Target "
               "for 120 points of damage.\n");
        plazmic::CombatLogTailer tailer(false);
        auto refresh = tailer.refresh(
            directory, kCharacter, melee->timestamp + 1s);
        require(refresh.error == plazmic::CombatLogError::none &&
                    refresh.snapshot.total_damage == 120U,
                "initial combat log was not tailed");

        append(log,
               "[Sun Aug 03 12:00:01 2026] Testmage hit Training Target "
               "for 300 points of magic damage by Synthetic Bolt.\n");
        append(log,
               "[Sun Aug 03 12:00:02 2026] Training Target hits You "
               "for 900 points of damage.\n");
        append(log,
               "[Sun Aug 03 12:00:02 2026] Training Target hits Synthetic "
               "Ally for 800 points of damage.\n");
        append(log,
               "[Sun Aug 03 12:00:02 2026] You hit Different Target "
               "for 700 points of damage.\n");
        refresh = tailer.refresh(
            directory, kCharacter, spell->timestamp + 1s);
        require(refresh.snapshot.total_damage == 420U,
                "appended outgoing damage or incoming filter is incorrect");

        append(log,
               "[Sun Aug 03 12:00:02 2026] You hit Training Target ");
        refresh = tailer.refresh(
            directory, kCharacter, spell->timestamp + 2s);
        require(refresh.snapshot.total_damage == 420U,
                "partial line was consumed before its newline");
        append(log, "for 30 points of damage.\n");
        refresh = tailer.refresh(
            directory, kCharacter, spell->timestamp + 3s);
        require(refresh.snapshot.total_damage == 450U,
                "completed partial line was not consumed once");

        {
            std::ofstream truncated(log, std::ios::trunc);
            truncated <<
                "[Sun Aug 03 12:00:03 2026] You hit Second Target "
                "for 25 points of damage.\n";
        }
        refresh = tailer.refresh(
            directory, kCharacter, spell->timestamp + 3s);
        require(refresh.snapshot.total_damage == 25U &&
                    refresh.snapshot.target == "Second Target",
                "truncated log did not start a new encounter");

        const auto rotated = logs / "synthetic-previous.log";
        std::filesystem::rename(log, rotated);
        append(log,
               "[Sun Aug 03 12:00:31 2026] You hit Rotated Target "
               "for 35 points of damage.\n");
        refresh = tailer.refresh(
            directory, kCharacter, spell->timestamp + 32s);
        require(refresh.snapshot.total_damage == 35U &&
                    refresh.snapshot.target == "Rotated Target",
                "replaced combat log was not read from its new inode");

        std::filesystem::remove(log);
        refresh = tailer.refresh(
            directory, kCharacter, spell->timestamp + 33s);
        require(refresh.error == plazmic::CombatLogError::missing &&
                    !refresh.snapshot.available(),
                "missing selected log retained an encounter");
        append(log,
               "[Sun Aug 03 12:00:34 2026] You hit Gap Target "
               "for 45 points of damage.\n");
        refresh = tailer.refresh(
            directory, kCharacter, spell->timestamp + 34s);
        require(refresh.snapshot.total_damage == 45U &&
                    refresh.snapshot.target == "Gap Target",
                "gapped log rotation skipped replacement events");
        {
            std::ofstream empty(log, std::ios::trunc);
        }
        refresh = tailer.refresh(
            directory, kCharacter, spell->timestamp + 35s);
        require(refresh.snapshot.state ==
                    plazmic::CombatEncounterState::idle,
                "empty truncated log republished a stale encounter");

        append(log, std::string(
            plazmic::CombatLogTailer::maximum_line_bytes + 1U, 'x'));
        append(log, "\n");
        refresh = tailer.refresh(
            directory, kCharacter, spell->timestamp + 22s);
        require(refresh.error == plazmic::CombatLogError::unavailable &&
                    !refresh.snapshot.available(),
                "oversized line did not fail closed");
        append(log,
               "[Sun Aug 03 12:00:23 2026] You hit Recovered Target "
               "for 60 points of damage.\n");
        refresh = tailer.refresh(
            directory, kCharacter, spell->timestamp + 24s);
        require(refresh.error == plazmic::CombatLogError::none &&
                    refresh.snapshot.total_damage == 60U,
                "tailer did not recover after an oversized line");

        plazmic::CombatLogTailer from_end;
        refresh = from_end.refresh(
            directory, kCharacter, spell->timestamp + 33s);
        require(refresh.error == plazmic::CombatLogError::none &&
                    refresh.snapshot.state ==
                        plazmic::CombatEncounterState::idle,
                "default tailer consumed historical log content");
        append(log,
               "[Sun Aug 03 12:00:34 2026] You hit Fresh Target "
               "for 40 points of damage.\n");
        refresh = from_end.refresh(
            directory, kCharacter, spell->timestamp + 35s);
        require(refresh.snapshot.total_damage == 40U,
                "default tailer did not consume newly appended content");
        std::filesystem::remove(log);
        refresh = from_end.refresh(
            directory, kCharacter, spell->timestamp + 36s);
        require(refresh.error == plazmic::CombatLogError::missing,
                "default tailer did not observe the rotation gap");
        append(log,
               "[Sun Aug 03 12:00:36 2026] You hit Replacement Target "
               "for 45 points of damage.\n");
        refresh = from_end.refresh(
            directory, kCharacter, spell->timestamp + 37s);
        require(refresh.snapshot.total_damage == 45U &&
                    refresh.snapshot.target == "Replacement Target",
                "default tailer skipped gapped replacement events");

        const auto second = logs / "eqlog_Testhero_other.txt";
        append(second, "synthetic\n");
        refresh = from_end.refresh(directory, kCharacter);
        require(refresh.error == plazmic::CombatLogError::ambiguous,
                "newly ambiguous character logs did not fail closed");
        std::filesystem::remove(second);
        append(log,
               "[Sun Aug 03 12:00:38 2026] You hit Replacement Target "
               "for 5 points of damage.\n");
        refresh = from_end.refresh(
            directory, kCharacter, spell->timestamp + 39s);
        require(refresh.snapshot.total_damage == 50U &&
                    refresh.snapshot.target == "Replacement Target",
                "transient ambiguity replayed or skipped log content");

        std::filesystem::remove_all(directory);
        directory.clear();
        directory = fixture_directory();
        std::filesystem::create_directory(directory / "Logs");
        const auto rewritten_log =
            directory / "Logs" / "eqlog_Testhero_rewritten.txt";
        for (std::size_t index = 0U; index < 10U; ++index) {
            append(rewritten_log,
                   "[Sun Aug 03 12:01:00 2026] You hit Old Target "
                   "for 1 points of damage.\n");
        }
        plazmic::CombatLogTailer rewrite_tailer(false);
        refresh = rewrite_tailer.refresh(
            directory, kCharacter, melee->timestamp + 61s);
        require(refresh.snapshot.total_damage == 10U,
                "initial rewrite fixture was not consumed");
        {
            std::ofstream rewritten(rewritten_log, std::ios::trunc);
            for (std::size_t index = 0U; index < 8U; ++index) {
                rewritten <<
                    "[Sun Aug 03 12:01:01 2026] You hit New Target "
                    "for 2 points of damage.\n";
            }
            for (std::size_t index = 0U; index < 2U; ++index) {
                rewritten <<
                    "[Sun Aug 03 12:01:00 2026] You hit Old Target "
                    "for 1 points of damage.\n";
            }
            rewritten <<
                "[Sun Aug 03 12:01:01 2026] You hit New Target "
                "for 2 points of damage.\n";
        }
        refresh = rewrite_tailer.refresh(
            directory, kCharacter, melee->timestamp + 62s);
        require(refresh.snapshot.total_damage == 2U &&
                    refresh.snapshot.target == "Old Target",
                "same-inode rewrite with a retained tail sample was missed");

        std::filesystem::remove_all(directory);
        directory.clear();
        directory = fixture_directory();
        std::filesystem::create_directory(directory / "Logs");
        const auto large_log =
            directory / "Logs" / "eqlog_Testhero_large.txt";
        {
            std::ofstream output(large_log, std::ios::binary);
            for (std::size_t index = 0U; index < 4096U; ++index) {
                output << "[Sun Aug 03 12:01:00 2026] You hit Synthetic "
                          "Target for 1 points of damage.\n";
            }
        }
        plazmic::CombatLogTailer large_tailer(false);
        std::uint64_t large_total = 0U;
        const auto large_start = std::chrono::steady_clock::now();
        for (std::size_t refresh_count = 0U;
             refresh_count < 4U && large_total < 4096U;
             ++refresh_count) {
            const auto large_refresh = large_tailer.refresh(
                directory, kCharacter, melee->timestamp + 61s);
            require(large_refresh.error == plazmic::CombatLogError::none,
                    "large bounded combat fixture failed to parse");
            large_total = large_refresh.snapshot.total_damage;
        }
        const double large_parse_ms =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - large_start)
                .count();
        require(large_total == 4096U,
                "large bounded combat fixture was not fully consumed");
        require(large_parse_ms < 1000.0,
                "large combat fixture exceeded its refresh budget");

        std::filesystem::remove_all(directory);
        directory.clear();
        std::cout << "bounded local combat parser passed\n";
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
