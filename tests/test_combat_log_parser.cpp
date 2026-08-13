#include "game/combat_log_parser.h"
#include "game/combat_history_store.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>
#include <sys/stat.h>

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
        require(plazmic::CombatHistoryStore::privacy_key(
                    "synthetic identity") ==
                    "4e7f1d22ba8ba92aed00e367fbc78a04",
                "privacy key is not the documented truncated unkeyed SHA-256");
        const auto melee = plazmic::parse_damage_line(
            "[Sun Aug 03 12:00:00 2026] You slash Training Target "
            "for 120 points of damage.",
            kCharacter);
        require(melee && melee->attacker == kCharacter &&
                    melee->defender == "Training Target" &&
                    melee->damage == 120U &&
                    melee->kind == plazmic::DamageKind::melee,
                "melee damage line was not parsed");
        const auto kick = plazmic::parse_damage_line(
            "[Sun Aug 03 12:00:00 2026] You kick Training Target "
            "for 45 points of damage.",
            kCharacter);
        require(kick && plazmic::is_activity_ability(*kick) &&
                    !plazmic::is_activity_ability(*melee),
                "named melee skill was not separated from auto-attacks");
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
                    spell->kind == plazmic::DamageKind::spell &&
                    spell->ability == "Synthetic Bolt",
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
        const auto healing = plazmic::parse_healing_line(
            "[Sun Aug 03 12:00:02 2026] You healed Synthetic Ally "
            "for 240 points.",
            kCharacter);
        require(healing && healing->healer == kCharacter &&
                    healing->target == "Synthetic Ally" &&
                    healing->healing == 240U,
                "outgoing healing line was not parsed");
        const auto healed_by = plazmic::parse_healing_line(
            "[Sun Aug 03 12:00:02 2026] Testhero has been healed by "
            "Testcleric for 180 points.",
            kCharacter);
        require(healed_by && healed_by->healer == "Testcleric" &&
                    healed_by->target == kCharacter &&
                    healed_by->healing == 180U,
                "healed-by line was not parsed");
        const auto hit_points = plazmic::parse_healing_line(
            "[Sun Aug 03 12:00:02 2026] You healed Synthetic Ally "
            "for 240 hit points by Synthetic Mending.",
            kCharacter);
        require(hit_points && hit_points->healing == 240U,
                "hit-points healing line was not parsed");
        const auto have_healed = plazmic::parse_healing_line(
            "[Sun Aug 03 12:00:02 2026] You have healed Synthetic Ally "
            "for 240 hit points by Synthetic Mending.",
            kCharacter);
        require(have_healed && have_healed->healer == kCharacter &&
                    have_healed->target == "Synthetic Ally",
                "have-healed line was misattributed");
        const auto parenthesized = plazmic::parse_healing_line(
            "[Sun Aug 03 12:00:02 2026] Synthetic Cleric healed "
            "Synthetic Ally over time for 190 (240) hit points by "
            "Synthetic Renewal.",
            kCharacter);
        require(parenthesized &&
                    parenthesized->healer == "Synthetic Cleric" &&
                    parenthesized->target == "Synthetic Ally" &&
                    parenthesized->healing == 190U,
                "parenthesized over-time healing line was not parsed");
        const auto zero_secondary = plazmic::parse_healing_line(
            "[Sun Aug 03 12:00:02 2026] You healed Synthetic Ally "
            "for 190 (0) hit points by Synthetic Renewal.",
            kCharacter);
        require(zero_secondary && zero_secondary->healing == 190U,
                "zero parenthesized healing value was rejected");
        require(!plazmic::parse_healing_line(
                    "[Sun Aug 03 12:00:02 2026] You have been healed over "
                    "time for 190 (240) hit points by Synthetic Renewal.",
                    kCharacter) &&
                    !plazmic::parse_healing_line(
                        "[Sun Aug 03 12:00:02 2026] Testhero has been healed "
                        "for 180 hit points by Synthetic Renewal.",
                        kCharacter),
                "passive healing without a healer identity was misattributed");
        const auto self_heal = plazmic::parse_healing_line(
            "[Sun Aug 03 12:00:02 2026] You healed yourself "
            "for 125 hit points by Synthetic Renewal.",
            kCharacter);
        require(self_heal && self_heal->healer == kCharacter &&
                    self_heal->target == kCharacter &&
                    self_heal->healing == 125U,
                "yourself healing target was not normalized");
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
        require(accumulator.add(*healing), "healing event was rejected");
        const auto active = accumulator.snapshot(
            damage_over_time->timestamp + 1s, kCharacter);
        require(active.state == plazmic::CombatEncounterState::active &&
                    active.total_damage == 500U &&
                    active.total_healing == 240U &&
                    active.participants.size() == 2U &&
                    active.healers.size() == 1U &&
                    !active.timeline.empty(),
                "active encounter aggregate is incorrect");
        require(active.participants[0].name == "Testmage" &&
                    active.participants[0].damage == 300U &&
                    active.participants[0].percentage == 60.0,
                "participants were not sorted or calculated");
        require(active.active_character_dps > 0.0,
                "active character DPS was not calculated");
        require(active.participants[1].melee_damage == 120U &&
                    active.participants[1].damage_over_time == 80U,
                "damage-kind drill-down was not retained");
        require(active.participants[0].abilities.size() == 1U &&
                    active.participants[0].abilities.front().name ==
                        "Synthetic Bolt" &&
                    active.participants[0].abilities.front().category ==
                        "Spell" &&
                    active.participants[1].abilities.size() == 2U,
                "attack and spell identity drill-down was not retained");
        plazmic::CombatAccumulator healer_only;
        require(healer_only.add(*healing, kCharacter, "healing_zone"),
                "healing could not establish an encounter");
        const auto healing_activity = healer_only.snapshot(
            healing->timestamp + 1s, kCharacter);
        require(healing_activity.state ==
                        plazmic::CombatEncounterState::active &&
                    healing_activity.total_damage == 0U &&
                    healing_activity.total_healing == 240U &&
                    healing_activity.healers.size() == 1U &&
                    healing_activity.zone == "healing_zone",
                "pure-healer encounter aggregate is incorrect");
        plazmic::CombatAccumulator unrelated_healing;
        const auto unrelated_event = plazmic::HealingEvent{
            .timestamp = healing->timestamp,
            .healer = "Synthetic Other Healer",
            .target = "Synthetic Other Target",
            .healing = 50U,
        };
        require(!unrelated_healing.add(
                    unrelated_event, kCharacter, "healing_zone"),
                "unrelated healing opened an encounter");
        require(unrelated_healing.add(*healing, kCharacter, "healing_zone"),
                "selected-character healing encounter was rejected");
        auto concurrent_unrelated = unrelated_event;
        concurrent_unrelated.timestamp += 1s;
        require(!unrelated_healing.add(
                    concurrent_unrelated, kCharacter, "healing_zone") &&
                    unrelated_healing.snapshot(
                        healing->timestamp + 2s, kCharacter)
                            .total_healing == 240U,
                "unrelated healing contaminated an active encounter");
        auto later_unrelated = unrelated_event;
        later_unrelated.timestamp += 11s;
        require(!unrelated_healing.add(
                    later_unrelated, kCharacter, "healing_zone"),
                "unrelated healing opened an encounter after rollover");
        const auto rolled_healing = unrelated_healing.take_completed();
        require(rolled_healing && rolled_healing->total_healing == 240U &&
                    rolled_healing->healers.size() == 1U,
                "rejected healing rollover lost the completed encounter");
        const auto complete = accumulator.snapshot(
            damage_over_time->timestamp + 11s, kCharacter);
        require(complete.state == plazmic::CombatEncounterState::complete,
                "encounter inactivity was not applied");
        require(!accumulator.add({
                    .timestamp = damage_over_time->timestamp + 11s,
                    .attacker = "Testmage",
                    .defender = "Second Training Target",
                    .damage = 900U,
                    .ability = "Synthetic Bolt",
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
                    .ability = "Hit",
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
                        .ability = "Hit",
                    }, kCharacter),
                    "participant within the hard bound was rejected");
        }
        require(!participant_bound.add({
                    .timestamp = melee->timestamp,
                    .attacker = "Synthetic overflow",
                    .defender = "Training Target",
                    .damage = 1U,
                    .ability = "Hit",
                }, kCharacter),
                "participant above the hard bound was accepted");

        plazmic::CombatAccumulator aggregate_bound;
        for (std::size_t index = 0U; index < 1000U; ++index) {
            require(aggregate_bound.add({
                        .timestamp = melee->timestamp,
                        .attacker = std::string(kCharacter),
                        .defender = "Aggregate Target",
                        .damage = 1'000'000'000'000ULL,
                        .ability = "Synthetic Aggregate",
                    }, kCharacter),
                    "aggregate value within the persistence bound was rejected");
        }
        require(!aggregate_bound.add({
                    .timestamp = melee->timestamp,
                    .attacker = std::string(kCharacter),
                    .defender = "Aggregate Target",
                    .damage = 1U,
                    .ability = "Synthetic Aggregate",
                }, kCharacter),
                "aggregate value above the persistence bound was accepted");

        directory = fixture_directory();
        const auto logs = directory / "Logs";
        std::filesystem::create_directory(logs);
        const auto log = logs / "eqlog_Testhero_synthetic.txt";
        append(log,
               "[Sun Aug 03 12:00:00 2026] You slash Training Target "
               "for 120 points of damage.\n");
        const auto incompatible_activity_game =
            directory / "incompatible-activity-game";
        std::filesystem::create_directories(
            incompatible_activity_game / "Logs");
        append(incompatible_activity_game / "Logs" /
                   "eqlog_Testhero_synthetic.txt",
               "[Sun Aug 03 12:00:00 2026] You slash Training Target "
               "for 120 points of damage.\n"
               "[Sun Aug 03 12:00:01 2026] You gain experience! "
               "(0.125%)\n");
        const auto incompatible_activity_state =
            directory / "incompatible-activity-state";
        const auto incompatible_activity_key =
            plazmic::CombatHistoryStore::privacy_key(
                "testhero\neqlog_testhero_synthetic.txt");
        std::filesystem::create_directories(
            incompatible_activity_state / "activity");
        append(incompatible_activity_state / "activity" /
                   (incompatible_activity_key + ".json"),
               R"({"schema":2,"events":[],"abilities":[]})");
        plazmic::CombatLogTailer incompatible_activity_tailer(
            false, incompatible_activity_state, false);
        incompatible_activity_tailer.set_activity_history_enabled(true);
        const auto incompatible_activity_refresh =
            incompatible_activity_tailer.refresh(
                incompatible_activity_game, kCharacter, "synthetic_zone",
                melee->timestamp + 1s);
        require(incompatible_activity_refresh.error ==
                        plazmic::CombatLogError::none &&
                    incompatible_activity_refresh.snapshot.encounter
                            .total_damage == 120U &&
                    !incompatible_activity_refresh.activity.available &&
                    incompatible_activity_refresh.activity.events.empty() &&
                    !incompatible_activity_refresh.activity.persisted &&
                    incompatible_activity_refresh.activity.detail.find(
                        "unsupported") != std::string::npos,
                "activity schema failure did not fail closed independently");
        const auto deferred_game = directory / "deferred-persistence-game";
        const auto deferred_state = directory / "deferred-persistence-state";
        std::filesystem::create_directories(deferred_game / "Logs");
        append(deferred_game / "Logs" / "eqlog_Deferred_synthetic.txt",
               "[Sun Aug 03 12:00:01 2026] You gain experience! "
               "(0.125%)\n");
        const auto deferred_key =
            plazmic::CombatHistoryStore::privacy_key(
                "deferred\neqlog_deferred_synthetic.txt");
        plazmic::CombatLogTailer deferred_tailer(
            false, deferred_state, true);
        deferred_tailer.set_activity_history_enabled(true);
        deferred_tailer.begin_deferred_persistence();
        const auto deferred_refresh = deferred_tailer.refresh(
            deferred_game, "Deferred", "synthetic_zone",
            melee->timestamp + 1s);
        require(deferred_refresh.error == plazmic::CombatLogError::none &&
                    deferred_refresh.activity.events.size() == 1U &&
                    !std::filesystem::exists(
                        deferred_state / "activity" /
                        (deferred_key + ".json")),
                "deferred refresh wrote speculative activity history");
        deferred_tailer.commit_deferred_persistence(false, true);
        require(std::filesystem::is_regular_file(
                    deferred_state / "activity" /
                    (deferred_key + ".json")),
                "committed deferred activity was not persisted");
        const auto deferred_opt_out_state =
            directory / "deferred-opt-out-state";
        plazmic::CombatLogTailer deferred_opt_out_tailer(
            false, deferred_opt_out_state, true);
        deferred_opt_out_tailer.set_activity_history_enabled(true);
        deferred_opt_out_tailer.begin_deferred_persistence();
        const auto deferred_opt_out_refresh =
            deferred_opt_out_tailer.refresh(
                deferred_game, "Deferred", "synthetic_zone",
                melee->timestamp + 1s);
        deferred_opt_out_tailer.commit_deferred_persistence(false, false);
        require(deferred_opt_out_refresh.error ==
                        plazmic::CombatLogError::none &&
                    !std::filesystem::exists(
                        deferred_opt_out_state / "activity" /
                        (deferred_key + ".json")),
                "deferred activity persisted after commit-time opt-out");
        const auto canonical_state = directory / "canonical-activity-state";
        const auto server_one_game = directory / "server-one-game";
        const auto server_one_case_game = directory / "server-one-case-game";
        const auto server_two_game = directory / "server-two-game";
        const auto invalid_server_game = directory / "invalid-server-game";
        std::filesystem::create_directories(server_one_game / "Logs");
        std::filesystem::create_directories(server_one_case_game / "Logs");
        std::filesystem::create_directories(server_two_game / "Logs");
        std::filesystem::create_directories(invalid_server_game / "Logs");
        append(server_one_game / "Logs" /
                   "eqlog_Testhero_ServerOne.txt", "");
        append(server_one_case_game / "Logs" /
                   "EQLOG_TESTHERO_SERVERONE.TXT", "");
        append(server_two_game / "Logs" /
                   "eqlog_Testhero_ServerTwo.txt", "");
        append(invalid_server_game / "Logs" /
                   "eqlog_Testhero_not a server!.txt", "");
        plazmic::CombatLogTailer server_one_tailer(
            true, canonical_state, false);
        plazmic::CombatLogTailer server_one_case_tailer(
            true, canonical_state, false);
        plazmic::CombatLogTailer server_two_tailer(
            true, canonical_state, false);
        const auto server_one_refresh = server_one_tailer.refresh(
            server_one_game, kCharacter, "synthetic_zone", melee->timestamp);
        const auto server_one_case_refresh = server_one_case_tailer.refresh(
            server_one_case_game, "TESTHERO", "synthetic_zone",
            melee->timestamp);
        const auto server_two_refresh = server_two_tailer.refresh(
            server_two_game, kCharacter, "synthetic_zone", melee->timestamp);
        plazmic::CombatLogTailer invalid_server_tailer(
            true, canonical_state, false);
        const auto invalid_server_refresh = invalid_server_tailer.refresh(
            invalid_server_game, kCharacter, "synthetic_zone",
            melee->timestamp);
        const std::string server_one_key =
            plazmic::CombatHistoryStore::privacy_key(
                "testhero\neqlog_testhero_serverone.txt");
        const std::string server_two_key =
            plazmic::CombatHistoryStore::privacy_key(
                "testhero\neqlog_testhero_servertwo.txt");
        require(server_one_refresh.error == plazmic::CombatLogError::none &&
                    server_one_case_refresh.error ==
                        plazmic::CombatLogError::none &&
                    server_two_refresh.error == plazmic::CombatLogError::none &&
                    server_one_refresh.activity.storage_key == server_one_key &&
                    server_one_case_refresh.activity.storage_key ==
                        server_one_key &&
                    server_two_refresh.activity.storage_key == server_two_key &&
                    server_one_key != server_two_key &&
                    invalid_server_refresh.error ==
                        plazmic::CombatLogError::missing,
                "activity partition key ignored case or server filename");
        plazmic::CombatLogTailer tailer(
            false, directory / "state", true);
        tailer.set_activity_history_enabled(true);
        auto refresh = tailer.refresh(
            directory, kCharacter, "synthetic_zone",
            melee->timestamp + 1s);
        require(refresh.error == plazmic::CombatLogError::none &&
                    refresh.snapshot.encounter.total_damage == 120U,
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
            directory, kCharacter, "synthetic_zone",
            spell->timestamp + 1s);
        require(refresh.snapshot.encounter.total_damage == 420U,
                "appended outgoing damage or incoming filter is incorrect");

        append(log,
               "[Sun Aug 03 12:00:02 2026] You hit Training Target ");
        refresh = tailer.refresh(
            directory, kCharacter, "synthetic_zone",
            spell->timestamp + 2s);
        require(refresh.snapshot.encounter.total_damage == 420U,
                "partial line was consumed before its newline");
        append(log, "for 30 points of damage.\n");
        refresh = tailer.refresh(
            directory, kCharacter, "synthetic_zone",
            spell->timestamp + 3s);
        require(refresh.snapshot.encounter.total_damage == 450U,
                "completed partial line was not consumed once");
        refresh = tailer.refresh(
            directory, kCharacter, "synthetic_zone",
            spell->timestamp + 24s);
        require(refresh.snapshot.history.size() == 1U &&
                    refresh.snapshot.history.front().total_damage == 450U &&
                    refresh.snapshot.history_retention_enabled &&
                    refresh.snapshot.zone_encounters == 1U &&
                    refresh.snapshot.zone_damage == 450U,
                "completed encounter was not retained in zone history");
        append(log,
               "[Sun Aug 03 12:00:21 2026] You gain experience! (0.125%)\n"
               "[Sun Aug 03 12:00:22 2026] You have gained an ability point!  "
               "You now have 42 ability points.\n"
               "[Sun Aug 03 12:00:23 2026] --You have looted Synthetic Gem.--\n");
        refresh = tailer.refresh(
            directory, kCharacter, "synthetic_zone",
            spell->timestamp + 24s);
        require(refresh.activity.events.size() == 3U &&
                    refresh.activity.retention_enabled &&
                    refresh.activity.persisted &&
                    refresh.activity.experience_percent == 0.125 &&
                    refresh.activity.alternate_advancement_points == 42U &&
                    refresh.activity.recent_loot_count == 1U,
                "log tailer did not publish or persist activity analytics");
        const std::string activity_key =
            plazmic::CombatHistoryStore::privacy_key(
                "testhero\neqlog_testhero_synthetic.txt");
        std::ifstream activity_input(
            directory / "state" / "activity" / (activity_key + ".json"),
            std::ios::binary);
        require(activity_input.is_open(),
                "persisted activity partition could not be opened");
        const std::string activity_bytes{
            std::istreambuf_iterator<char>(activity_input), {}};
        require(!activity_bytes.empty() &&
                    activity_bytes.find(
                        plazmic::CombatHistoryStore::privacy_key(
                            "[Sun Aug 03 12:00:01 2026] Testmage hit Training "
                            "Target for 300 points of magic damage by Synthetic "
                        "Bolt.")) == std::string::npos,
                "another attacker's ability polluted activity replay metadata");
        const auto combat_file =
            directory / "state" / "combat" / (activity_key + ".json");
        const auto activity_file =
            directory / "state" / "activity" / (activity_key + ".json");
        struct stat combat_before {};
        struct stat activity_before {};
        require(::stat(combat_file.c_str(), &combat_before) == 0 &&
                    ::stat(activity_file.c_str(), &activity_before) == 0,
                "retained state could not be inspected before deferral");
        tailer.begin_deferred_persistence();
        tailer.commit_deferred_persistence(true, true);
        struct stat combat_after {};
        struct stat activity_after {};
        require(::stat(combat_file.c_str(), &combat_after) == 0 &&
                    ::stat(activity_file.c_str(), &activity_after) == 0 &&
                    combat_before.st_dev == combat_after.st_dev &&
                    combat_before.st_ino == combat_after.st_ino &&
                    activity_before.st_dev == activity_after.st_dev &&
                    activity_before.st_ino == activity_after.st_ino,
                "unchanged deferred commit rewrote retained state");
        plazmic::CombatLogTailer restored_activity(
            true, directory / "state", false);
        restored_activity.set_activity_history_enabled(true);
        const auto restored_activity_refresh = restored_activity.refresh(
            directory, kCharacter, "synthetic_zone",
            spell->timestamp + 24s);
        require(restored_activity_refresh.activity.events.size() == 3U,
                "activity partition did not restore through the log tailer");
        const auto restart_game = directory / "restart-activity-game";
        const auto restart_state = directory / "restart-activity-state";
        const auto restart_log =
            restart_game / "Logs" / "eqlog_Restart_synthetic.txt";
        std::filesystem::create_directories(restart_log.parent_path());
        const std::string restart_line =
            "[Sun Aug 03 12:00:23 2026] --You have looted Restart Gem.--\n";
        append(restart_log, restart_line);
        plazmic::CombatLogTailer restart_writer(false, restart_state, false);
        restart_writer.set_activity_history_enabled(true);
        require(restart_writer.refresh(
                    restart_game, "Restart", "synthetic_zone",
                    spell->timestamp + 24s)
                        .activity.events.size() == 1U,
                "restart activity fixture was not persisted");
        plazmic::CombatLogTailer restart_reader(true, restart_state, false);
        restart_reader.set_activity_history_enabled(true);
        const auto restart_initial = restart_reader.refresh(
            restart_game, "Restart", "synthetic_zone",
            spell->timestamp + 24s);
        append(restart_log, restart_line);
        const auto restored_fresh_stream = restart_reader.refresh(
            restart_game, "Restart", "synthetic_zone",
            spell->timestamp + 25s);
        require(restart_initial.activity.events.size() == 1U &&
                    restored_fresh_stream.activity.events.size() == 2U &&
                    std::ranges::count(
                        restored_fresh_stream.activity.events,
                        std::string("Restart Gem"),
                        &plazmic::ActivityEventSnapshot::label) == 2,
                "fresh tailer did not start at end or treat appended matching "
                "activity as a new stream observation");
        const auto transition_game = directory / "transition-game";
        const auto transition_logs = transition_game / "Logs";
        const auto transition_state = directory / "transition-state";
        std::filesystem::create_directories(transition_logs);
        const auto transition_log =
            transition_logs / "eqlog_Testhero_synthetic.txt";
        append(transition_log,
               "[Sun Aug 03 12:01:00 2026] You gain experience! (0.250%)\n");
        plazmic::CombatLogTailer transition_tailer(
            false, transition_state, false);
        transition_tailer.set_activity_history_enabled(true);
        auto transition_refresh = transition_tailer.refresh(
            transition_game, kCharacter, "synthetic_zone",
            spell->timestamp + 61s);
        require(transition_refresh.error == plazmic::CombatLogError::none &&
                    transition_refresh.activity.persisted &&
                    transition_refresh.activity.events.size() == 1U,
                "activity transition fixture was not persisted");
        const auto transition_key =
            plazmic::CombatHistoryStore::privacy_key(
                "testhero\neqlog_testhero_synthetic.txt");
        const auto transition_file = transition_state / "activity" /
                                     (transition_key + ".json");
        std::filesystem::remove(transition_file);
        std::filesystem::create_directories(transition_file / "sentinel");
        append(transition_log,
               "[Sun Aug 03 12:01:01 2026] You gain experience! (0.500%)\n");
        transition_refresh = transition_tailer.refresh(
            transition_game, kCharacter, "synthetic_zone",
            spell->timestamp + 62s);
        require(transition_refresh.error == plazmic::CombatLogError::none &&
                    !transition_refresh.activity.persisted &&
                    transition_refresh.activity.events.size() == 2U,
                "activity transition fixture did not retain an unsaved event");
        const auto transition_other_log =
            transition_logs / "eqlog_Other_synthetic.txt";
        append(transition_other_log,
               "[Sun Aug 03 12:01:02 2026] You gain experience! (0.125%)\n");
        const auto blocked_transition = transition_tailer.refresh(
            transition_game, "Other", "other_zone",
            spell->timestamp + 63s);
        require(blocked_transition.error ==
                    plazmic::CombatLogError::unavailable &&
                    !blocked_transition.activity.available &&
                    blocked_transition.activity.storage_key.empty() &&
                    blocked_transition.snapshot.encounter.detail.find(
                        "activity history") != std::string::npos,
                "failed activity transition was not reported safely");
        std::filesystem::remove_all(transition_file);
        std::this_thread::sleep_for(
            plazmic::ActivityTracker::persistence_retry_delay + 50ms);
        const auto retried_transition = transition_tailer.refresh(
            transition_game, kCharacter, "synthetic_zone",
            spell->timestamp + 64s);
        require(retried_transition.error == plazmic::CombatLogError::none &&
                    retried_transition.activity.persisted &&
                    retried_transition.activity.events.size() == 2U,
                "unsaved activity was not preserved for retry: error=" +
                    std::to_string(static_cast<int>(retried_transition.error)) +
                    " persisted=" +
                    std::to_string(retried_transition.activity.persisted) +
                    " events=" +
                    std::to_string(retried_transition.activity.events.size()) +
                    " detail=" + retried_transition.activity.detail);
        const auto off_state = directory / "off-state";
        plazmic::CombatLogTailer memory_only(false, off_state);
        const auto memory_only_refresh = memory_only.refresh(
            directory, kCharacter, "synthetic_zone",
            spell->timestamp + 24s);
        require(!memory_only_refresh.snapshot.history.empty() &&
                    !memory_only_refresh.snapshot.history_retention_enabled &&
                    !std::filesystem::exists(off_state),
                "combat history was persisted without explicit opt-in");
        const auto stable_history_path = directory / "state" / "combat" /
            (plazmic::CombatHistoryStore::privacy_key(
                 "testhero\neqlog_testhero_synthetic.txt") +
             ".json");
        struct stat stable_before {};
        require(::stat(stable_history_path.c_str(), &stable_before) == 0,
                "cannot inspect completed history file");
        refresh = tailer.refresh(
            directory, kCharacter, "synthetic_zone",
            spell->timestamp + 24s);
        struct stat stable_after {};
        require(::stat(stable_history_path.c_str(), &stable_after) == 0 &&
                    stable_after.st_ino == stable_before.st_ino,
                "unchanged completed history was rewritten while idle");
        plazmic::CharacterSnapshot equipment_before{
            .state = plazmic::PlayerSnapshotState::in_world,
            .name = std::string(kCharacter),
            .health = {},
            .mana = {},
            .alternate_advancement_percent = std::nullopt,
            .alternate_advancement_points = std::nullopt,
            .equipment = {{.slot = "Head", .item = "Synthetic Cap"}},
            .detail = {},
        };
        tailer.observe_character(
            equipment_before, "synthetic_zone", spell->timestamp + 24s);
        const auto missing_log = logs / "temporarily-missing.txt";
        std::filesystem::rename(log, missing_log);
        const auto missing_refresh = tailer.refresh(
            directory, kCharacter, "synthetic_zone",
            spell->timestamp + 24s);
        require(missing_refresh.error == plazmic::CombatLogError::missing &&
                    missing_refresh.snapshot.history.size() == 1U &&
                    missing_refresh.snapshot.zone_damage == 450U &&
                    !missing_refresh.activity.available &&
                    missing_refresh.activity.storage_key.empty() &&
                    missing_refresh.activity.detail.find("unavailable") !=
                        std::string::npos,
                "transient log failure hid history or exposed activity");
        std::filesystem::rename(missing_log, log);
        append(log,
               "[Sun Aug 03 12:00:02 2026] You hit Training Target "
               "for 5 points of damage.\n");
        refresh = tailer.refresh(
            directory, kCharacter, "synthetic_zone",
            spell->timestamp + 25s);
        auto equipment_after = equipment_before;
        equipment_after.equipment.front().item = "Synthetic Crown";
        tailer.observe_character(
            equipment_after, "synthetic_zone", spell->timestamp + 25s);
        const auto activity_after_gap = tailer.activity_snapshot(
            spell->timestamp + 25s);
        require(refresh.snapshot.history.size() == 1U &&
                    refresh.snapshot.history.front().total_damage == 455U &&
                    refresh.snapshot.zone_damage == 455U &&
                    std::ranges::find(
                        activity_after_gap.events,
                        plazmic::ActivityEventKind::equipment_change,
                        &plazmic::ActivityEventSnapshot::kind) ==
                        activity_after_gap.events.end(),
                "late line or failed-log equipment baseline was mishandled");
        plazmic::CombatLogTailer restored(
            true, directory / "state", true);
        const auto restored_refresh = restored.refresh(
            directory, kCharacter, "synthetic_zone",
            spell->timestamp + 25s);
        require(restored_refresh.snapshot.history.size() == 1U &&
                    restored_refresh.snapshot.history.front().target ==
                        "Training Target" &&
                    restored_refresh.snapshot.history.front().total_damage ==
                        455U,
                "per-character history was not restored after restart");
        const auto other_log = logs / "eqlog_Other_synthetic.txt";
        append(other_log,
               "[Sun Aug 03 12:00:22 2026] You hit Other Target "
               "for 1 point of damage.\n");
        const auto switched_away = tailer.refresh(
            directory, "Other", "other_zone", spell->timestamp + 22s);
        require(switched_away.snapshot.history.empty(),
                "character switch exposed another character's history");
        const auto switched_back = tailer.refresh(
            directory, kCharacter, "synthetic_zone",
            spell->timestamp + 23s);
        require(switched_back.snapshot.history.size() == 1U &&
                    switched_back.snapshot.history.front().target ==
                        "Training Target",
                "character history was not restored after switch-back");
        std::filesystem::remove(other_log);
        const auto history_directory = directory / "state" / "combat";
        require(std::filesystem::is_directory(history_directory),
                "privacy-keyed history file was not created");
        const std::filesystem::path history_file =
            history_directory /
            (plazmic::CombatHistoryStore::privacy_key(
                 "testhero\neqlog_testhero_synthetic.txt") +
             ".json");
        struct stat history_status {};
        require(::stat(history_file.c_str(), &history_status) == 0 &&
                    (history_status.st_mode & 0077) == 0,
                "retained combat history was not owner-only");

        {
            std::ofstream truncated(log, std::ios::trunc);
            truncated <<
                "[Sun Aug 03 12:00:21 2026] You gain experience! (0.125%)\n"
                "[Sun Aug 03 12:00:03 2026] You hit Second Target "
                "for 25 points of damage.\n";
        }
        refresh = tailer.refresh(
            directory, kCharacter, spell->timestamp + 3s);
        require(refresh.snapshot.encounter.total_damage == 25U &&
                    refresh.activity.events.size() == 4U &&
                    refresh.activity.experience_percent == 0.250 &&
                    refresh.snapshot.encounter.target == "Second Target",
                "truncated log replay result: damage=" +
                    std::to_string(refresh.snapshot.encounter.total_damage) +
                    " events=" +
                    std::to_string(refresh.activity.events.size()) +
                    " xp=" +
                    std::to_string(refresh.activity.experience_percent) +
                    " target=" + refresh.snapshot.encounter.target);

        {
            std::ofstream truncated_with_new_repeat(log, std::ios::trunc);
            truncated_with_new_repeat <<
                "[Sun Aug 03 12:00:21 2026] You gain experience! (0.125%)\n"
                "[Sun Aug 03 12:00:21 2026] You gain experience! (0.125%)\n"
                "[Sun Aug 03 12:00:04 2026] You hit Third Target "
                "for 30 points of damage.\n";
        }
        refresh = tailer.refresh(
            directory, kCharacter, spell->timestamp + 4s);
        require(refresh.snapshot.encounter.total_damage == 30U &&
                    refresh.activity.events.size() == 5U &&
                    refresh.activity.experience_percent == 0.375,
                "truncation suppressed a new identical activity after overlap");

        const auto rotated = logs / "synthetic-previous.log";
        std::filesystem::rename(log, rotated);
        append(log,
               "[Sun Aug 03 12:00:21 2026] You gain experience! (0.125%)\n"
               "[Sun Aug 03 12:00:21 2026] You gain experience! (0.125%)\n"
               "[Sun Aug 03 12:00:21 2026] You gain experience! (0.125%)\n"
               "[Sun Aug 03 12:00:31 2026] You hit Rotated Target "
               "for 35 points of damage.\n");
        refresh = tailer.refresh(
            directory, kCharacter, spell->timestamp + 32s);
        require(refresh.snapshot.encounter.total_damage == 35U &&
                    refresh.snapshot.encounter.target == "Rotated Target" &&
                    refresh.activity.events.size() == 8U,
                "inode replacement suppressed a new identical activity");

        std::filesystem::remove(log);
        refresh = tailer.refresh(
            directory, kCharacter, spell->timestamp + 33s);
        require(refresh.error == plazmic::CombatLogError::missing &&
                    !refresh.snapshot.encounter.available(),
                "missing selected log retained an encounter");
        append(log,
               "[Sun Aug 03 12:00:23 2026] --You have looted Synthetic Gem.--\n"
               "[Sun Aug 03 12:00:23 2026] --You have looted Synthetic Gem.--\n"
               "[Sun Aug 03 12:00:34 2026] You hit Gap Target "
               "for 45 points of damage.\n");
        refresh = tailer.refresh(
            directory, kCharacter, spell->timestamp + 34s);
        require(refresh.snapshot.encounter.total_damage == 45U &&
                    refresh.snapshot.encounter.target == "Gap Target" &&
                    refresh.activity.events.size() == 10U,
                "gapped recreation suppressed a new identical activity");
        {
            std::ofstream empty(log, std::ios::trunc);
        }
        refresh = tailer.refresh(
            directory, kCharacter, spell->timestamp + 35s);
        require(refresh.snapshot.encounter.state ==
                    plazmic::CombatEncounterState::idle,
                "empty truncated log republished a stale encounter");

        append(log,
               "[Sun Aug 03 12:00:36 2026] You hit Recovered Target "
               "for 10 points of damage.\n");
        refresh = tailer.refresh(
            directory, kCharacter, spell->timestamp + 36s);
        require(refresh.snapshot.encounter.total_damage == 10U,
                "pre-oversize encounter fixture was not active");
        append(log, std::string(
            plazmic::CombatLogTailer::maximum_line_bytes + 1U, 'x'));
        append(log, "\n");
        refresh = tailer.refresh(
            directory, kCharacter, spell->timestamp + 37s);
        require(refresh.error == plazmic::CombatLogError::unavailable &&
                    !refresh.snapshot.encounter.available(),
                "oversized line did not fail closed");
        append(log,
               "[Sun Aug 03 12:00:37 2026] You hit Recovered Target "
               "for 60 points of damage.\n");
        refresh = tailer.refresh(
            directory, kCharacter, spell->timestamp + 38s);
        require(refresh.error == plazmic::CombatLogError::none &&
                    refresh.snapshot.encounter.total_damage == 70U,
                "oversized line split the active encounter");

        plazmic::CombatLogTailer from_end(true, directory / "state");
        refresh = from_end.refresh(
            directory, kCharacter, spell->timestamp + 33s);
        require(refresh.error == plazmic::CombatLogError::none &&
                    refresh.snapshot.encounter.state ==
                        plazmic::CombatEncounterState::idle,
                "default tailer consumed historical log content");
        append(log,
               "[Sun Aug 03 12:00:34 2026] You hit Fresh Target "
               "for 40 points of damage.\n");
        refresh = from_end.refresh(
            directory, kCharacter, spell->timestamp + 35s);
        require(refresh.snapshot.encounter.total_damage == 40U,
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
        require(refresh.snapshot.encounter.total_damage == 45U &&
                    refresh.snapshot.encounter.target == "Replacement Target",
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
        require(refresh.snapshot.encounter.total_damage == 50U &&
                    refresh.snapshot.encounter.target == "Replacement Target",
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
        plazmic::CombatLogTailer rewrite_tailer(
            false, directory / "state");
        refresh = rewrite_tailer.refresh(
            directory, kCharacter, melee->timestamp + 61s);
        require(refresh.snapshot.encounter.total_damage == 10U,
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
        require(refresh.snapshot.encounter.total_damage == 2U &&
                    refresh.snapshot.encounter.target == "Old Target",
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
        plazmic::CombatLogTailer large_tailer(false, directory / "state");
        large_tailer.set_activity_history_enabled(true);
        std::uint64_t large_total = 0U;
        const auto large_start = std::chrono::steady_clock::now();
        for (std::size_t refresh_count = 0U;
             refresh_count < 4U && large_total < 4096U;
             ++refresh_count) {
            const auto large_refresh = large_tailer.refresh(
                directory, kCharacter, melee->timestamp + 61s);
            require(large_refresh.error == plazmic::CombatLogError::none,
                    "large bounded combat fixture failed to parse");
            large_total = large_refresh.snapshot.encounter.total_damage;
        }
        const double large_parse_ms =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - large_start)
                .count();
        require(large_total == 4096U,
                "large bounded combat fixture was not fully consumed");
        require(large_parse_ms < 1000.0,
                "large combat fixture exceeded its refresh budget");
        append(large_log,
               "[Sun Aug 03 12:01:02 2026] You gain experience! (0.125%)\n");
        auto large_activity_refresh = large_tailer.refresh(
            directory, kCharacter, melee->timestamp + 63s);
        require(large_activity_refresh.activity.events.size() == 1U &&
                    large_activity_refresh.activity.experience_percent ==
                        0.125,
                "activity beyond the replay prefix was not consumed");
        const auto large_other_log =
            directory / "Logs" / "eqlog_Other_large.txt";
        append(large_other_log,
               "[Sun Aug 03 12:01:02 2026] You gain experience! (0.250%)\n");
        (void)large_tailer.refresh(
            directory, "Other", "other_zone", melee->timestamp + 63s);
        large_activity_refresh = large_tailer.refresh(
            directory, kCharacter, "synthetic_zone",
            melee->timestamp + 64s);
        require(large_activity_refresh.activity.events.size() == 1U &&
                    large_activity_refresh.activity.experience_percent ==
                        0.125,
                "character switch replayed activity beyond the verified "
                "prefix");

        std::filesystem::remove_all(directory);
        directory.clear();
        directory = fixture_directory();
        std::filesystem::create_directory(directory / "Logs");
        const auto lifecycle_log =
            directory / "Logs" / "eqlog_Testhero_lifecycle.txt";
        append(lifecycle_log, "");
        plazmic::CombatLogTailer lifecycle_tailer(
            true, directory / "state", true);
        refresh = lifecycle_tailer.refresh(
            directory, kCharacter, "first_zone", melee->timestamp);
        append(lifecycle_log,
               "[Sun Aug 03 12:00:00 2026] You hit Lifecycle Target "
               "for 77 points of damage.\n");
        refresh = lifecycle_tailer.refresh(
            directory, kCharacter, "first_zone", melee->timestamp + 1s);
        require(refresh.snapshot.encounter.state ==
                        plazmic::CombatEncounterState::active &&
                    refresh.snapshot.encounter.total_damage == 77U,
                "lifecycle fixture did not create an active encounter");
        lifecycle_tailer.clear();
        plazmic::CombatLogTailer lifecycle_restored(
            true, directory / "state", true);
        refresh = lifecycle_restored.refresh(
            directory, kCharacter, "second_zone", melee->timestamp + 2s);
        require(refresh.snapshot.history.size() == 1U &&
                    refresh.snapshot.history.front().total_damage == 77U &&
                    refresh.snapshot.history.front().zone == "first_zone",
                "lifecycle reset did not persist the active encounter");

        const auto memory_log =
            directory / "Logs" / "eqlog_Memoryhero_lifecycle.txt";
        append(memory_log, "");
        plazmic::CombatLogTailer memory_tailer(
            true, directory / "memory-state", false);
        (void)memory_tailer.refresh(
            directory, "Memoryhero", "first_zone", melee->timestamp);
        append(memory_log,
               "[Sun Aug 03 12:00:00 2026] You hit Memory Target "
               "for 88 points of damage.\n");
        refresh = memory_tailer.refresh(
            directory, "Memoryhero", "first_zone", melee->timestamp + 20s);
        require(refresh.snapshot.history.size() == 1U,
                "memory-only lifecycle fixture did not retain an encounter");
        memory_tailer.clear();
        refresh = memory_tailer.refresh(
            directory, "Memoryhero", "second_zone", melee->timestamp + 21s);
        require(refresh.snapshot.history.size() == 1U &&
                    refresh.snapshot.history.front().total_damage == 88U,
                "zoning discarded memory-only encounter history");
        const auto other_memory_log =
            directory / "Logs" / "eqlog_Othermemory_lifecycle.txt";
        append(other_memory_log, "");
        refresh = memory_tailer.refresh(
            directory, "Othermemory", "second_zone", melee->timestamp + 22s);
        require(refresh.snapshot.history.empty(),
                "character switch exposed another character's memory history");

        const auto detail_rewritten_log =
            directory / "Logs" / "eqlog_Rewritehero_lifecycle.txt";
        append(detail_rewritten_log,
               "[Sun Aug 03 12:00:00 2026] You hit Original Target "
               "for 33 points of damage.\n");
        plazmic::CombatLogTailer rewritten_tailer(
            false, directory / "rewrite-state", false);
        refresh = rewritten_tailer.refresh(
            directory, "Rewritehero", "rewrite_zone", melee->timestamp + 20s);
        require(refresh.snapshot.history.size() == 1U &&
                    refresh.snapshot.history.front().target ==
                        "Original Target",
                "rewritten-log fixture did not retain its first encounter");
        {
            std::ofstream replacement(detail_rewritten_log,
                                      std::ios::binary | std::ios::trunc);
            replacement <<
                "[Sun Aug 03 12:00:00 2026] You hit Replacement Target "
                "for 33 points of damage.\n";
        }
        refresh = rewritten_tailer.refresh(
            directory, "Rewritehero", "rewrite_zone", melee->timestamp + 20s);
        require(refresh.snapshot.history.size() == 1U &&
                    refresh.snapshot.history.front().target ==
                        "Replacement Target",
                "same-total rewritten encounter retained stale details");

        plazmic::CombatHistoryStore bounded_store(directory / "bounded-state");
        const auto now = std::chrono::system_clock::now();
        const auto now_seconds =
            std::chrono::duration_cast<std::chrono::seconds>(
                now.time_since_epoch())
                .count();
        std::vector<plazmic::CombatEncounterSnapshot> oversized_history;
        for (std::size_t encounter_index = 0U; encounter_index < 50U;
             ++encounter_index) {
            plazmic::CombatEncounterSnapshot encounter;
            encounter.state = plazmic::CombatEncounterState::complete;
            encounter.target = "Synthetic bounded target";
            encounter.zone = "synthetic_bounded_zone";
            encounter.started_unix_seconds =
                now_seconds - static_cast<std::int64_t>(50U - encounter_index);
            encounter.total_damage = 256U;
            encounter.duration_seconds = 1.0;
            for (std::size_t participant_index = 0U;
                 participant_index < 256U; ++participant_index) {
                encounter.participants.push_back({
                    .name = "Synthetic participant " +
                            std::to_string(participant_index),
                    .damage = 1U,
                    .hits = 1U,
                    .dps = 1.0,
                    .percentage = 1.0,
                    .active_seconds = 1.0,
                    .melee_damage = 1U,
                    .abilities = {},
                });
            }
            for (std::uint32_t second = 0U; second < 600U; ++second) {
                encounter.timeline.push_back({
                    .elapsed_seconds = second,
                    .damage = 1U,
                });
            }
            oversized_history.push_back(std::move(encounter));
        }
        const std::string bounded_key =
            plazmic::CombatHistoryStore::privacy_key("bounded synthetic");
        require(bounded_store.save(bounded_key, oversized_history),
                "byte-bounded history save failed instead of pruning");
        const auto bounded_history = bounded_store.load(bounded_key);
        require(!bounded_history.empty() && bounded_history.size() < 50U &&
                    bounded_history.back().started_unix_seconds ==
                        oversized_history.back().started_unix_seconds,
                "history byte pruning did not retain the newest encounters");
        const auto bounded_file = directory / "bounded-state" / "combat" /
                                  (bounded_key + ".json");
        require(std::filesystem::file_size(bounded_file) <=
                    plazmic::CombatHistoryStore::maximum_file_bytes,
                "history file exceeded its byte bound");

        auto single_oversized = oversized_history.back();
        single_oversized.participants.clear();
        for (std::size_t participant = 0U; participant < 256U;
             ++participant) {
            plazmic::CombatParticipantSnapshot value{
                .name = std::string(120U, 'p') +
                        std::to_string(participant),
                .damage = 128U,
                .hits = 128U,
                .dps = 128.0,
                .percentage = 1.0,
                .active_seconds = 1.0,
                .melee_damage = 128U,
                .abilities = {},
            };
            for (std::size_t ability = 0U; ability < 128U; ++ability) {
                value.abilities.push_back({
                    .name = std::string(120U, 'a') +
                            std::to_string(ability),
                    .category = "Melee",
                    .damage = 1U,
                    .hits = 1U,
                });
            }
            single_oversized.participants.push_back(std::move(value));
        }
        const std::string summary_key =
            plazmic::CombatHistoryStore::privacy_key("summary synthetic");
        std::vector<plazmic::CombatEncounterSnapshot> memory_bounded{
            single_oversized};
        require(bounded_store.bound(memory_bounded) &&
                    memory_bounded.size() == 1U &&
                    memory_bounded.front().participants.empty(),
                "memory-only history did not apply the byte bound");
        std::vector<plazmic::CombatEncounterSnapshot> summary_history{
            single_oversized};
        require(bounded_store.save(summary_key, summary_history) &&
                    summary_history.size() == 1U &&
                    summary_history.front().participants.empty() &&
                    summary_history.front().total_damage ==
                        single_oversized.total_damage,
                "oversized newest encounter was not retained as a summary");
        const auto loaded_summary = bounded_store.load(summary_key);
        require(loaded_summary.size() == 1U &&
                    loaded_summary.front().participants.empty() &&
                    loaded_summary.front().total_damage ==
                        single_oversized.total_damage,
                "bounded encounter summary was not persisted");

        auto invalid_aggregate = single_oversized;
        invalid_aggregate.total_damage =
            plazmic::CombatHistoryStore::maximum_aggregate + 1U;
        std::vector<plazmic::CombatEncounterSnapshot> invalid_history{
            invalid_aggregate};
        const std::string invalid_key =
            plazmic::CombatHistoryStore::privacy_key("invalid aggregate");
        require(!bounded_store.save(invalid_key, invalid_history) &&
                    !std::filesystem::exists(
                        directory / "bounded-state" / "combat" /
                        (invalid_key + ".json")),
                "history store wrote an aggregate it cannot reload");

        auto expired = oversized_history.back();
        expired.started_unix_seconds =
            std::chrono::duration_cast<std::chrono::seconds>(
                (now - plazmic::CombatHistoryStore::maximum_age - 1s)
                    .time_since_epoch())
                .count();
        auto fresh = oversized_history.back();
        fresh.started_unix_seconds = now_seconds;
        const std::string age_key =
            plazmic::CombatHistoryStore::privacy_key("aged synthetic");
        std::vector<plazmic::CombatEncounterSnapshot> aged_history{
            expired, fresh};
        require(bounded_store.save(age_key, aged_history),
                "age-bounded history save failed");
        require(aged_history.size() == 1U &&
                    aged_history.front().started_unix_seconds == now_seconds,
                "expired encounter was not pruned from memory");
        const auto age_history = bounded_store.load(age_key);
        require(age_history.size() == 1U &&
                    age_history.front().started_unix_seconds == now_seconds,
                "expired encounter was retained past the age bound");

        const auto expiring_log =
            directory / "Logs" / "eqlog_Expiring_synthetic.txt";
        append(expiring_log, "");
        const auto expiring_state = directory / "expiring-state";
        plazmic::CombatHistoryStore expiring_store(expiring_state);
        const std::string expiring_key =
            plazmic::CombatHistoryStore::privacy_key(
                "expiring\neqlog_expiring_synthetic.txt");
        auto expiring_encounter = fresh;
        const auto expiring_now = std::chrono::system_clock::now();
        const auto expiring_now_seconds =
            std::chrono::duration_cast<std::chrono::seconds>(
                expiring_now.time_since_epoch())
                .count();
        expiring_encounter.started_unix_seconds =
            expiring_now_seconds -
            std::chrono::duration_cast<std::chrono::seconds>(
                plazmic::CombatHistoryStore::maximum_age)
                .count() +
            1;
        std::vector<plazmic::CombatEncounterSnapshot> expiring_history{
            expiring_encounter};
        require(expiring_store.save(expiring_key, expiring_history),
                "near-expiry history fixture could not be saved");
        auto inactive_expiring = expiring_encounter;
        inactive_expiring.target = "Inactive expiring target";
        std::vector<plazmic::CombatEncounterSnapshot> inactive_history{
            inactive_expiring};
        const std::string inactive_key =
            plazmic::CombatHistoryStore::privacy_key("inactive expiring");
        require(expiring_store.save(inactive_key, inactive_history),
                "inactive expiry fixture could not be saved");
        plazmic::CombatLogTailer expiring_tailer(
            true, expiring_state, true);
        auto expiring_refresh = expiring_tailer.refresh(
            directory, "Expiring", "expiring_zone", expiring_now);
        require(expiring_refresh.snapshot.history.size() == 1U,
                "near-expiry history fixture was not loaded");
        std::this_thread::sleep_for(2100ms);
        expiring_refresh = expiring_tailer.refresh(
            directory, "Expiring", "expiring_zone", expiring_now + 3s);
        require(expiring_refresh.snapshot.history.empty() &&
                    expiring_store.load(expiring_key).empty() &&
                    expiring_store.load(inactive_key).empty() &&
                    expiring_store.prune_expired().healthy,
                "running tailer retained active or inactive history beyond "
                "the age limit");

        const auto unavailable_activity_state =
            directory / "unavailable-activity-state";
        const std::string unavailable_activity_key =
            plazmic::CombatHistoryStore::privacy_key(
                "inactive activity awaiting expiry");
        plazmic::ActivityTracker unavailable_activity_fixture(
            unavailable_activity_state);
        unavailable_activity_fixture.set_retention_enabled(true);
        require(unavailable_activity_fixture.select(
                    unavailable_activity_key),
                "unavailable-log activity fixture could not be selected");
        unavailable_activity_fixture.consume(
            "[Sun Aug 03 12:00:05 2026] You gain experience! (0.500%)",
            "Unavailableactivity", "synthetic_zone");
        require(unavailable_activity_fixture
                    .snapshot(melee->timestamp + 1min)
                    .persisted,
                "unavailable-log activity fixture could not be saved");
        plazmic::CombatLogTailer unavailable_activity_tailer(
            true, unavailable_activity_state, false);
        unavailable_activity_tailer.set_activity_history_enabled(true);
        const auto unavailable_activity_refresh =
            unavailable_activity_tailer.refresh(
                directory / "missing-game-directory",
                "Unavailableactivity", "synthetic_zone",
                melee->timestamp +
                    plazmic::ActivityTracker::maximum_age + 1h);
        plazmic::ActivityTracker unavailable_activity_inspector(
            unavailable_activity_state);
        unavailable_activity_inspector.set_retention_enabled(true);
        require(unavailable_activity_refresh.error ==
                    plazmic::CombatLogError::missing &&
                    unavailable_activity_inspector.select(
                        unavailable_activity_key) &&
                    unavailable_activity_inspector
                        .snapshot(melee->timestamp +
                                  plazmic::ActivityTracker::maximum_age + 1h)
                        .events.empty(),
                "unavailable combat log skipped activity expiry maintenance");

        const auto future_log =
            directory / "Logs" / "eqlog_Future_synthetic.txt";
        append(future_log, "");
        const auto future_state = directory / "future-state";
        const std::string future_key =
            plazmic::CombatHistoryStore::privacy_key(
                "future\neqlog_future_synthetic.txt");
        const auto future_file =
            future_state / "combat" / (future_key + ".json");
        std::filesystem::create_directories(future_file.parent_path());
        const std::string future_contents =
            R"({"schema":2,"encounters":[]})";
        append(future_file, future_contents);
        plazmic::CombatLogTailer future_tailer(
            true, future_state, true);
        const auto future_refresh = future_tailer.refresh(
            directory, "Future", "future_zone", melee->timestamp);
        std::ifstream future_input(future_file, std::ios::binary);
        const std::string future_after{
            std::istreambuf_iterator<char>(future_input),
            std::istreambuf_iterator<char>()};
        require(future_refresh.error ==
                        plazmic::CombatLogError::unavailable &&
                    future_refresh.snapshot.encounter.detail.find(
                        "combat history") != std::string::npos &&
                    future_after == future_contents,
                "unsupported history schema was not preserved");

        const auto atomic_state = directory / "atomic-state";
        const auto atomic_first_log =
            directory / "Logs" / "eqlog_Atomicfirst_synthetic.txt";
        const auto atomic_bad_log =
            directory / "Logs" / "eqlog_Atomicbad_synthetic.txt";
        append(atomic_first_log, "");
        append(atomic_bad_log, "");
        const std::string atomic_first_key =
            plazmic::CombatHistoryStore::privacy_key(
                "atomicfirst\neqlog_atomicfirst_synthetic.txt");
        const std::string atomic_bad_key =
            plazmic::CombatHistoryStore::privacy_key(
                "atomicbad\neqlog_atomicbad_synthetic.txt");
        const auto atomic_bad_file = atomic_state / "combat" /
                                     (atomic_bad_key + ".json");
        std::filesystem::create_directories(atomic_bad_file.parent_path());
        append(atomic_bad_file, R"({"schema":2,"encounters":[]})");
        plazmic::CombatLogTailer atomic_tailer(true, atomic_state, true);
        atomic_tailer.set_activity_history_enabled(true);
        const auto atomic_first = atomic_tailer.refresh(
            directory, "Atomicfirst", "atomic_zone", melee->timestamp);
        const auto atomic_failed = atomic_tailer.refresh(
            directory, "Atomicbad", "atomic_zone", melee->timestamp);
        require(atomic_first.error == plazmic::CombatLogError::none &&
                    atomic_first.activity.storage_key == atomic_first_key &&
                    atomic_failed.error ==
                        plazmic::CombatLogError::unavailable &&
                    atomic_failed.snapshot.encounter.detail.find(
                        "combat history") != std::string::npos &&
                    atomic_failed.activity.storage_key.empty(),
                "combat load failure partially switched activity history");

        const std::string malformed_key =
            plazmic::CombatHistoryStore::privacy_key("malformed synthetic");
        const auto malformed_file =
            future_state / "combat" / (malformed_key + ".json");
        append(malformed_file,
               R"({"schema":1,"encounters":{"unexpected":true}})");
        require(!plazmic::CombatHistoryStore(future_state)
                     .load_checked(malformed_key),
                "non-array encounter collection was accepted");

        const auto blocked_state = directory / "blocked-state";
        append(blocked_state, "not a directory");
        const auto blocked_log =
            directory / "Logs" / "eqlog_Blocked_synthetic.txt";
        append(blocked_log,
               "[Sun Aug 03 12:00:00 2026] You hit Blocked Target "
               "for 10 points of damage.\n");
        plazmic::CombatLogTailer blocked_tailer(
            false, blocked_state, true);
        blocked_tailer.set_activity_history_enabled(true);
        const auto blocked_refresh = blocked_tailer.refresh(
            directory, "Blocked", "blocked_zone", melee->timestamp + 20s);
        require(!blocked_refresh.snapshot.history_persisted &&
                    !blocked_refresh.snapshot.history_detail.empty(),
                "history persistence failure was not surfaced");
        const auto blocked_other_log =
            directory / "Logs" / "eqlog_Otherblocked_synthetic.txt";
        append(blocked_other_log, "");
        const auto blocked_switch = blocked_tailer.refresh(
            directory, "Otherblocked", "blocked_zone",
            melee->timestamp + 20s);
        require(blocked_switch.error == plazmic::CombatLogError::unavailable &&
                    !blocked_switch.snapshot.history_persisted &&
                    blocked_switch.snapshot.encounter.detail.find(
                        "awaiting persistence") != std::string::npos &&
                    blocked_switch.activity.storage_key.empty(),
                "combat save failure partially switched local history");
        std::filesystem::remove(blocked_state);
        std::this_thread::sleep_for(
            plazmic::CombatLogTailer::history_retry_delay + 50ms);
        const auto recovered_refresh = blocked_tailer.refresh(
            directory, "Otherblocked", "blocked_zone",
            melee->timestamp + 21s);
        std::error_code combat_directory_error;
        const std::filesystem::directory_iterator combat_entries(
            blocked_state / "combat", combat_directory_error);
        require(recovered_refresh.snapshot.history_persisted &&
                    recovered_refresh.snapshot.history_detail.empty() &&
                    std::filesystem::is_directory(blocked_state) &&
                    !combat_directory_error &&
                    combat_entries !=
                        std::filesystem::directory_iterator{},
                "history persistence did not recover after a transient "
                "failure");

        std::filesystem::remove_all(directory);
        directory.clear();
        directory = fixture_directory();
        std::filesystem::create_directory(directory / "Logs");
        plazmic::CombatLogTailer evicted_path_tailer(
            true, directory / "evicted-state");
        std::vector<std::string> evicted_characters;
        std::vector<std::filesystem::path> evicted_logs;
        for (std::size_t index = 0U; index < 10U; ++index) {
            evicted_characters.push_back("Evict" + std::to_string(index));
            evicted_logs.push_back(
                directory / "Logs" /
                ("eqlog_" + evicted_characters.back() + "_synthetic.txt"));
            append(evicted_logs.back(), "");
            const auto initial = evicted_path_tailer.refresh(
                directory, evicted_characters.back(), "synthetic_zone",
                melee->timestamp);
            require(initial.error == plazmic::CombatLogError::none,
                    "evicted-path fixture could not select its initial log");
        }
        for (const auto& evicted_log : evicted_logs) {
            append(evicted_log,
                   "[Sun Aug 03 12:02:00 2026] You gain experience! "
                   "(0.125%)\n");
        }
        for (std::size_t index = 0U; index < evicted_logs.size(); ++index) {
            const auto returned = evicted_path_tailer.refresh(
                directory, evicted_characters[index], "synthetic_zone",
                melee->timestamp + 121s);
            require(returned.error == plazmic::CombatLogError::none &&
                        returned.activity.events.size() == 1U,
                    "returning evicted log path skipped newly available "
                    "activity");
        }

        std::filesystem::remove_all(directory);
        directory.clear();
        directory = fixture_directory();
        std::filesystem::create_directory(directory / "Logs");
        const auto flush_log =
            directory / "Logs" / "eqlog_Flushhero_synthetic.txt";
        append(flush_log,
               "[Sun Aug 03 12:03:00 2026] You gain experience! (0.125%)\n");
        append(flush_log, std::string(
            plazmic::CombatLogTailer::maximum_line_bytes + 1U, 'x'));
        append(flush_log, "\n");
        const auto flush_state = directory / "flush-state";
        plazmic::CombatLogTailer flush_tailer(false, flush_state);
        flush_tailer.set_activity_history_enabled(true);
        const auto failed_batch = flush_tailer.refresh(
            directory, "Flushhero", "synthetic_zone",
            melee->timestamp + 181s);
        require(failed_batch.error == plazmic::CombatLogError::unavailable,
                "flush fixture did not fail after a valid activity line");
        flush_tailer.clear();
        const std::string flush_key =
            plazmic::CombatHistoryStore::privacy_key(
                "flushhero\neqlog_flushhero_synthetic.txt");
        plazmic::ActivityTracker flush_inspector(flush_state);
        flush_inspector.set_retention_enabled(true);
        require(flush_inspector.select(flush_key) &&
                    flush_inspector.snapshot(melee->timestamp + 182s)
                            .events.size() == 1U,
                "lifecycle clear lost dirty retained activity after a "
                "partial failing refresh");

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
