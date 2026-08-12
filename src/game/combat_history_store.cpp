#include "game/combat_history_store.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <ranges>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QString>

namespace plazmic {
namespace {

constexpr int kSchemaVersion = 1;
constexpr std::size_t kMaximumNameBytes = 128U;
constexpr std::size_t kMaximumZoneBytes = 128U;
constexpr std::size_t kMaximumParticipants = 256U;
constexpr std::size_t kMaximumAbilities = 128U;
constexpr std::size_t kMaximumTimelinePoints = 600U;
constexpr std::size_t kMaximumHistoryFiles = 4096U;
constexpr auto kMaximumFutureSkew = std::chrono::hours(24);

std::int64_t unix_seconds(std::chrono::system_clock::time_point value) {
    return std::chrono::duration_cast<std::chrono::seconds>(
               value.time_since_epoch())
        .count();
}

bool retained_by_age(const CombatEncounterSnapshot& encounter,
                     std::chrono::system_clock::time_point now) {
    const std::int64_t oldest = unix_seconds(now - CombatHistoryStore::maximum_age);
    const std::int64_t newest = unix_seconds(now + kMaximumFutureSkew);
    return encounter.started_unix_seconds >= oldest &&
           encounter.started_unix_seconds <= newest;
}

bool valid_text(const std::string& value, std::size_t maximum) {
    return !value.empty() && value.size() <= maximum &&
           std::ranges::all_of(value, [](unsigned char byte) {
               return byte >= 0x20U && byte <= 0x7eU;
           });
}

bool valid_number(double value) {
    return std::isfinite(value) && value >= 0.0 && value <= 1.0e15;
}

QJsonObject participant_json(const CombatParticipantSnapshot& value) {
    QJsonArray abilities;
    for (const auto& ability : value.abilities) {
        abilities.append(QJsonObject{
            {"name", QString::fromStdString(ability.name)},
            {"category", QString::fromStdString(ability.category)},
            {"damage", static_cast<qint64>(ability.damage)},
            {"hits", static_cast<qint64>(ability.hits)},
        });
    }
    return {
        {"name", QString::fromStdString(value.name)},
        {"damage", static_cast<qint64>(value.damage)},
        {"hits", static_cast<qint64>(value.hits)},
        {"dps", value.dps},
        {"percentage", value.percentage},
        {"activeSeconds", value.active_seconds},
        {"melee", static_cast<qint64>(value.melee_damage)},
        {"spell", static_cast<qint64>(value.spell_damage)},
        {"dot", static_cast<qint64>(value.damage_over_time)},
        {"pet", static_cast<qint64>(value.pet_damage)},
        {"abilities", abilities},
    };
}

QJsonObject healer_json(const CombatHealerSnapshot& value) {
    return {
        {"name", QString::fromStdString(value.name)},
        {"healing", static_cast<qint64>(value.healing)},
        {"casts", static_cast<qint64>(value.casts)},
        {"hps", value.hps},
        {"percentage", value.percentage},
    };
}

QJsonObject encounter_json(const CombatEncounterSnapshot& value) {
    QJsonArray participants;
    for (const auto& participant : value.participants) {
        participants.append(participant_json(participant));
    }
    QJsonArray healers;
    for (const auto& healer : value.healers) {
        healers.append(healer_json(healer));
    }
    QJsonArray timeline;
    for (const auto& point : value.timeline) {
        timeline.append(QJsonObject{
            {"second", static_cast<qint64>(point.elapsed_seconds)},
            {"damage", static_cast<qint64>(point.damage)},
            {"healing", static_cast<qint64>(point.healing)},
        });
    }
    return {
        {"target", QString::fromStdString(value.target)},
        {"zone", QString::fromStdString(value.zone)},
        {"started", static_cast<qint64>(value.started_unix_seconds)},
        {"damage", static_cast<qint64>(value.total_damage)},
        {"healing", static_cast<qint64>(value.total_healing)},
        {"duration", value.duration_seconds},
        {"characterDps", value.active_character_dps},
        {"participants", participants},
        {"healers", healers},
        {"timeline", timeline},
    };
}

QByteArray history_bytes(
    const std::vector<CombatEncounterSnapshot>& history) {
    QJsonArray encounters;
    for (const auto& encounter : history) {
        encounters.append(encounter_json(encounter));
    }
    return QJsonDocument(QJsonObject{
        {"schema", kSchemaVersion},
        {"encounters", encounters},
    }).toJson(QJsonDocument::Compact);
}

std::optional<std::uint64_t> unsigned_value(const QJsonObject& object,
                                             const char* key) {
    const QJsonValue value = object.value(key);
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const double number = value.toDouble(-1.0);
    if (!std::isfinite(number) || number < 0.0 ||
        number > static_cast<double>(CombatHistoryStore::maximum_aggregate) ||
        std::floor(number) != number) {
        return std::nullopt;
    }
    return static_cast<std::uint64_t>(number);
}

std::optional<CombatParticipantSnapshot> parse_participant(
    const QJsonValue& value) {
    if (!value.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = value.toObject();
    const std::string name = object.value("name").toString().toStdString();
    const auto damage = unsigned_value(object, "damage");
    const auto hits = unsigned_value(object, "hits");
    const auto melee = unsigned_value(object, "melee");
    const auto spell = unsigned_value(object, "spell");
    const auto dot = unsigned_value(object, "dot");
    const auto pet = unsigned_value(object, "pet");
    const double dps = object.value("dps").toDouble(-1.0);
    const double percentage = object.value("percentage").toDouble(-1.0);
    const double active = object.value("activeSeconds").toDouble(-1.0);
    if (!valid_text(name, kMaximumNameBytes) || !damage || !hits ||
        !melee || !spell || !dot || !pet ||
        *hits > std::numeric_limits<std::uint32_t>::max() ||
        !valid_number(dps) || !valid_number(percentage) ||
        !valid_number(active)) {
        return std::nullopt;
    }
    CombatParticipantSnapshot result{
        .name = name,
        .damage = *damage,
        .hits = static_cast<std::uint32_t>(*hits),
        .dps = dps,
        .percentage = percentage,
        .active_seconds = active,
        .melee_damage = *melee,
        .spell_damage = *spell,
        .damage_over_time = *dot,
        .pet_damage = *pet,
        .abilities = {},
    };
    const QJsonValue abilities_value = object.value("abilities");
    if (!abilities_value.isArray()) {
        return std::nullopt;
    }
    const QJsonArray abilities = abilities_value.toArray();
    if (abilities.size() > static_cast<qsizetype>(kMaximumAbilities)) {
        return std::nullopt;
    }
    for (const auto& value : abilities) {
        if (!value.isObject()) {
            return std::nullopt;
        }
        const QJsonObject ability = value.toObject();
        const std::string ability_name =
            ability.value("name").toString().toStdString();
        const std::string category =
            ability.value("category").toString().toStdString();
        const auto ability_damage = unsigned_value(ability, "damage");
        const auto ability_hits = unsigned_value(ability, "hits");
        if (!valid_text(ability_name, kMaximumNameBytes) ||
            !valid_text(category, 16U) || !ability_damage || !ability_hits ||
            *ability_hits > std::numeric_limits<std::uint32_t>::max()) {
            return std::nullopt;
        }
        result.abilities.push_back({
            .name = ability_name,
            .category = category,
            .damage = *ability_damage,
            .hits = static_cast<std::uint32_t>(*ability_hits),
        });
    }
    return result;
}

std::optional<CombatHealerSnapshot> parse_healer(const QJsonValue& value) {
    if (!value.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = value.toObject();
    const std::string name = object.value("name").toString().toStdString();
    const auto healing = unsigned_value(object, "healing");
    const auto casts = unsigned_value(object, "casts");
    const double hps = object.value("hps").toDouble(-1.0);
    const double percentage = object.value("percentage").toDouble(-1.0);
    if (!valid_text(name, kMaximumNameBytes) || !healing || !casts ||
        *casts > std::numeric_limits<std::uint32_t>::max() ||
        !valid_number(hps) || !valid_number(percentage)) {
        return std::nullopt;
    }
    return CombatHealerSnapshot{
        .name = name,
        .healing = *healing,
        .casts = static_cast<std::uint32_t>(*casts),
        .hps = hps,
        .percentage = percentage,
    };
}

std::optional<CombatEncounterSnapshot> parse_encounter(
    const QJsonValue& value) {
    if (!value.isObject()) {
        return std::nullopt;
    }
    const QJsonObject object = value.toObject();
    CombatEncounterSnapshot result;
    result.state = CombatEncounterState::complete;
    result.target = object.value("target").toString().toStdString();
    result.zone = object.value("zone").toString().toStdString();
    result.started_unix_seconds =
        object.value("started").toInteger(std::numeric_limits<qint64>::min());
    const auto damage = unsigned_value(object, "damage");
    const auto healing = unsigned_value(object, "healing");
    result.duration_seconds = object.value("duration").toDouble(-1.0);
    result.active_character_dps =
        object.value("characterDps").toDouble(-1.0);
    if (!valid_text(result.target, kMaximumNameBytes) ||
        !valid_text(result.zone, kMaximumZoneBytes) || !damage || !healing ||
        result.started_unix_seconds <= 0 ||
        !valid_number(result.duration_seconds) ||
        !valid_number(result.active_character_dps)) {
        return std::nullopt;
    }
    result.total_damage = *damage;
    result.total_healing = *healing;
    const QJsonValue participants_value = object.value("participants");
    const QJsonValue healers_value = object.value("healers");
    const QJsonValue timeline_value = object.value("timeline");
    if (!participants_value.isArray() || !healers_value.isArray() ||
        !timeline_value.isArray()) {
        return std::nullopt;
    }
    const QJsonArray participants = participants_value.toArray();
    const QJsonArray healers = healers_value.toArray();
    const QJsonArray timeline = timeline_value.toArray();
    std::size_t total_abilities = 0U;
    if (participants.size() > static_cast<qsizetype>(kMaximumParticipants) ||
        healers.size() > static_cast<qsizetype>(kMaximumParticipants) ||
        timeline.size() > static_cast<qsizetype>(kMaximumTimelinePoints)) {
        return std::nullopt;
    }
    for (const auto& participant : participants) {
        auto parsed = parse_participant(participant);
        if (!parsed) {
            return std::nullopt;
        }
        if (parsed->abilities.size() >
            CombatHistoryStore::maximum_total_abilities - total_abilities) {
            return std::nullopt;
        }
        total_abilities += parsed->abilities.size();
        result.participants.push_back(std::move(*parsed));
    }
    for (const auto& healer : healers) {
        auto parsed = parse_healer(healer);
        if (!parsed) {
            return std::nullopt;
        }
        result.healers.push_back(std::move(*parsed));
    }
    for (const auto& point_value : timeline) {
        if (!point_value.isObject()) {
            return std::nullopt;
        }
        const QJsonObject point = point_value.toObject();
        const auto second = unsigned_value(point, "second");
        const auto point_damage = unsigned_value(point, "damage");
        const auto point_healing = unsigned_value(point, "healing");
        if (!second || !point_damage || !point_healing ||
            *second > kMaximumTimelinePoints) {
            return std::nullopt;
        }
        result.timeline.push_back({
            .elapsed_seconds = static_cast<std::uint32_t>(*second),
            .damage = *point_damage,
            .healing = *point_healing,
        });
    }
    result.detail = "Retained encounter";
    return result;
}

bool aggregates_within_bounds(const CombatEncounterSnapshot& encounter) {
    const auto within = [](std::uint64_t value) {
        return value <= CombatHistoryStore::maximum_aggregate;
    };
    if (!within(encounter.total_damage) ||
        !within(encounter.total_healing)) {
        return false;
    }
    std::size_t total_abilities = 0U;
    for (const auto& participant : encounter.participants) {
        if (participant.abilities.size() >
            CombatHistoryStore::maximum_total_abilities - total_abilities) {
            return false;
        }
        total_abilities += participant.abilities.size();
        if (!within(participant.damage) ||
            !within(participant.melee_damage) ||
            !within(participant.spell_damage) ||
            !within(participant.damage_over_time) ||
            !within(participant.pet_damage) ||
            std::ranges::any_of(participant.abilities,
                                [&within](const auto& ability) {
                                    return !within(ability.damage);
                                })) {
            return false;
        }
    }
    if (std::ranges::any_of(encounter.healers, [&within](const auto& healer) {
            return !within(healer.healing);
        }) ||
        std::ranges::any_of(encounter.timeline, [&within](const auto& point) {
            return !within(point.damage) || !within(point.healing);
        })) {
        return false;
    }
    return true;
}

}  // namespace

CombatHistoryStore::CombatHistoryStore(std::filesystem::path state_root)
    : state_root_(std::move(state_root)) {}

std::filesystem::path CombatHistoryStore::default_state_root() {
    if (const char* state = std::getenv("XDG_STATE_HOME");
        state != nullptr && *state != '\0') {
        return std::filesystem::path(state) / "plazmic-legends";
    }
    if (const char* home = std::getenv("HOME");
        home != nullptr && *home != '\0') {
        return std::filesystem::path(home) / ".local/state/plazmic-legends";
    }
    return {};
}

std::string CombatHistoryStore::privacy_key(std::string_view identity) {
    const QByteArray hash = QCryptographicHash::hash(
        QByteArray(identity.data(), static_cast<qsizetype>(identity.size())),
        QCryptographicHash::Sha256);
    return hash.toHex().left(32).toStdString();
}

std::filesystem::path CombatHistoryStore::path_for(
    std::string_view local_character_key) const {
    if (state_root_.empty() || local_character_key.size() != 32U ||
        !std::ranges::all_of(local_character_key, [](unsigned char byte) {
            return (byte >= '0' && byte <= '9') ||
                   (byte >= 'a' && byte <= 'f');
        })) {
        return {};
    }
    return state_root_ / "combat" /
           (std::string(local_character_key) + ".json");
}

std::vector<CombatEncounterSnapshot> CombatHistoryStore::load(
    std::string_view local_character_key) const {
    const auto loaded = load_checked(local_character_key);
    return loaded ? loaded->history
                  : std::vector<CombatEncounterSnapshot>{};
}

std::optional<CombatHistoryLoad>
CombatHistoryStore::load_checked(
    std::string_view local_character_key) const {
    const std::filesystem::path path = path_for(local_character_key);
    if (path.empty()) {
        return std::nullopt;
    }
    QFile file(QString::fromStdString(path.string()));
    if (!file.exists()) {
        return CombatHistoryLoad{};
    }
    const QFileInfo information(file);
    if (!information.isFile() || information.isSymLink()) {
        return std::nullopt;
    }
    if (!file.open(QIODevice::ReadOnly) || file.size() <= 0 ||
        file.size() > static_cast<qint64>(maximum_file_bytes)) {
        return std::nullopt;
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return std::nullopt;
    }
    const QJsonObject root = document.object();
    const QJsonValue encounters_value = root.value("encounters");
    if (root.value("schema").toInt(-1) != kSchemaVersion ||
        !encounters_value.isArray()) {
        return std::nullopt;
    }
    const QJsonArray encounters = encounters_value.toArray();
    if (encounters.size() > static_cast<qsizetype>(maximum_encounters)) {
        return std::nullopt;
    }
    const auto now = std::chrono::system_clock::now();
    std::vector<CombatEncounterSnapshot> result;
    result.reserve(static_cast<std::size_t>(encounters.size()));
    for (const auto& value : encounters) {
        auto encounter = parse_encounter(value);
        if (!encounter) {
            return std::nullopt;
        }
        if (retained_by_age(*encounter, now)) {
            result.push_back(std::move(*encounter));
        }
    }
    const bool needs_rewrite =
        result.size() != static_cast<std::size_t>(encounters.size());
    const bool persisted = !needs_rewrite || save(local_character_key, result);
    return CombatHistoryLoad{
        .history = std::move(result),
        .persisted = persisted,
    };
}

bool CombatHistoryStore::bound(
    std::vector<CombatEncounterSnapshot>& history) const {
    const auto now = std::chrono::system_clock::now();
    std::vector<CombatEncounterSnapshot> bounded_history;
    bounded_history.reserve(std::min(history.size(), maximum_encounters));
    for (auto iterator = history.rbegin(); iterator != history.rend() &&
                                             bounded_history.size() < maximum_encounters;
         ++iterator) {
        if (retained_by_age(*iterator, now)) {
            bounded_history.push_back(*iterator);
        }
    }
    std::ranges::reverse(bounded_history);
    for (auto& encounter : bounded_history) {
        std::size_t total_abilities = 0U;
        for (const auto& participant : encounter.participants) {
            total_abilities += participant.abilities.size();
            if (total_abilities > maximum_total_abilities) {
                encounter.participants.clear();
                break;
            }
        }
    }
    if (std::ranges::any_of(bounded_history, [](const auto& encounter) {
            return !aggregates_within_bounds(encounter) ||
                   !parse_encounter(encounter_json(encounter)).has_value();
        })) {
        return false;
    }
    QByteArray bytes = history_bytes(bounded_history);
    while (bytes.size() > static_cast<qsizetype>(maximum_file_bytes) &&
           bounded_history.size() > 1U) {
        bounded_history.erase(bounded_history.begin());
        bytes = history_bytes(bounded_history);
    }
    if (bytes.size() > static_cast<qsizetype>(maximum_file_bytes) &&
        !bounded_history.empty()) {
        bounded_history.front().participants.clear();
        bounded_history.front().healers.clear();
        bounded_history.front().timeline.clear();
        bytes = history_bytes(bounded_history);
    }
    if (bytes.isEmpty() ||
        bytes.size() > static_cast<qsizetype>(maximum_file_bytes)) {
        return false;
    }
    history = std::move(bounded_history);
    return true;
}

bool CombatHistoryStore::save(
    std::string_view local_character_key,
    std::vector<CombatEncounterSnapshot>& history) const {
    const std::filesystem::path path = path_for(local_character_key);
    if (path.empty() || !bound(history)) {
        return false;
    }
    const QByteArray bytes = history_bytes(history);
    const QString directory =
        QString::fromStdString(path.parent_path().string());
    if (!QDir().mkpath(directory)) {
        return false;
    }
    if (!QFile::setPermissions(directory, QFileDevice::ReadOwner |
                                             QFileDevice::WriteOwner |
                                             QFileDevice::ExeOwner)) {
        return false;
    }
    QSaveFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) {
        file.cancelWriting();
        return false;
    }
    if (!file.setPermissions(QFileDevice::ReadOwner |
                             QFileDevice::WriteOwner)) {
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        return false;
    }
    return true;
}

CombatHistoryPrune CombatHistoryStore::prune_expired() const {
    const QString directory = QString::fromStdString(
        (state_root_ / "combat").string());
    QDir combat_directory(directory);
    if (!combat_directory.exists()) {
        return {};
    }
    const QFileInfoList files = combat_directory.entryInfoList(
        {"*.json"}, QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    if (files.size() > static_cast<qsizetype>(kMaximumHistoryFiles)) {
        return {
            .healthy = false,
            .next_expiration = std::nullopt,
        };
    }
    CombatHistoryPrune result;
    for (const QFileInfo& file : files) {
        if (file.isSymLink() || file.completeBaseName().size() != 32) {
            continue;
        }
        const std::string key = file.completeBaseName().toStdString();
        if (!std::ranges::all_of(key, [](unsigned char byte) {
                return std::isxdigit(byte) != 0;
            })) {
            continue;
        }
        const auto loaded = load_checked(key);
        if (loaded && !loaded->persisted) {
            result.healthy = false;
        }
        if (!loaded) {
            continue;
        }
        for (const auto& encounter : loaded->history) {
            const auto expiration =
                std::chrono::system_clock::time_point{
                    std::chrono::seconds(encounter.started_unix_seconds)} +
                maximum_age + std::chrono::seconds(1);
            if (!result.next_expiration ||
                expiration < *result.next_expiration) {
                result.next_expiration = expiration;
            }
        }
    }
    return result;
}

}  // namespace plazmic
