#include "ui/character_profile_exporter.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

#include <QDateTime>
#include <QFile>
#include <QFileDevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>

namespace plazmic {
namespace {

constexpr std::size_t kMaximumSnapshotTextBytes = 128U;

constexpr std::array<std::pair<std::string_view, std::string_view>, 23>
    kEquipmentSlotMappings{
        std::pair{"Charm", "ANY 1"},
        std::pair{"Left Ear", "EAR 1"},
        std::pair{"Head", "HEAD"},
        std::pair{"Face", "FACE"},
        std::pair{"Right Ear", "EAR 2"},
        std::pair{"Neck", "NECK"},
        std::pair{"Shoulders", "SHOULDERS"},
        std::pair{"Arms", "ARMS"},
        std::pair{"Back", "BACK"},
        std::pair{"Left Wrist", "WRIST 1"},
        std::pair{"Right Wrist", "WRIST 2"},
        std::pair{"Range", "RANGE"},
        std::pair{"Hands", "HANDS"},
        std::pair{"Primary", "PRIMARY"},
        std::pair{"Secondary", "SECONDARY"},
        std::pair{"Left Finger", "FINGER 1"},
        std::pair{"Right Finger", "FINGER 2"},
        std::pair{"Chest", "CHEST"},
        std::pair{"Legs", "LEGS"},
        std::pair{"Feet", "FEET"},
        std::pair{"Waist", "WAIST"},
        std::pair{"Power Source", "ANY 2"},
        std::pair{"Ammo", "AMMO"},
    };

QString checked_utf8(std::string_view value) {
    if (value.size() > kMaximumSnapshotTextBytes) {
        return {};
    }
    const QString decoded = QString::fromUtf8(
        value.data(), static_cast<qsizetype>(value.size()));
    if (decoded.toUtf8() != QByteArray(value.data(),
                                       static_cast<qsizetype>(value.size()))) {
        return {};
    }
    for (const QChar character : decoded) {
        if (!character.isPrint()) {
            return {};
        }
    }
    return decoded.trimmed();
}

QString tools_slot(std::string_view slot) {
    const auto match = std::ranges::find_if(
        kEquipmentSlotMappings,
        [slot](const auto& mapping) { return mapping.first == slot; });
    if (match == kEquipmentSlotMappings.end()) {
        return {};
    }
    return QString::fromLatin1(
        match->second.data(), static_cast<qsizetype>(match->second.size()));
}

QString tools_item_slug(const QString& item_name) {
    const QString decomposed = item_name.simplified().normalized(
        QString::NormalizationForm_KD);
    QString normalized;
    normalized.reserve(decomposed.size());
    for (const QChar character : decomposed) {
        const auto code = character.unicode();
        if ((code >= 0x0300U && code <= 0x036fU) ||
            character == '\'' || character == '`' || code == 0x2018U ||
            code == 0x2019U) {
            continue;
        }
        if (character == '&') {
            normalized.append(QStringLiteral(" and "));
        } else {
            normalized.append(character);
        }
    }
    normalized.replace(
        QRegularExpression(QStringLiteral("[^a-zA-Z0-9]+")), "-");
    normalized.remove(QRegularExpression(QStringLiteral("^-+|-+$")));
    normalized = normalized.toLower();
    if (normalized == QStringLiteral("deterioriated-ancient-faydark-longbow")) {
        return QStringLiteral("deteriorated-ancient-faydark-longbow");
    }
    return normalized;
}

std::pair<QString, int> split_item_upgrade(const QString& item_name) {
    static const QRegularExpression upgrade_pattern(
        QStringLiteral("^(.*?)\\s+\\+(\\d+)\\s*$"));
    const QRegularExpressionMatch match = upgrade_pattern.match(item_name);
    if (!match.hasMatch()) {
        return {item_name, 0};
    }
    bool valid = false;
    const int parsed = match.captured(2).toInt(&valid);
    return {
        match.captured(1).trimmed(),
        valid ? std::clamp(parsed, 0, 10) : 0,
    };
}

CharacterProfileExport failure(const QString& detail) {
    return {
        .contents = {},
        .suggested_file_name = {},
        .equipped_items = 0U,
        .detail = detail,
    };
}

}  // namespace

CharacterProfileExport build_character_profile_export(
    const CharacterSnapshot& snapshot) {
    if (!snapshot.available()) {
        return failure("Character information is unavailable.");
    }
    if (snapshot.equipment.size() > kEquipmentSlotMappings.size()) {
        return failure("The character snapshot has too many equipment slots.");
    }

    const QString character_name = checked_utf8(snapshot.name);
    if (character_name.isEmpty()) {
        return failure("The character snapshot has an invalid name.");
    }

    QJsonObject equipped;
    QJsonObject slot_upgrades;
    QSet<QString> seen_slots;
    std::size_t equipped_items = 0U;
    for (const EquipmentSlotSnapshot& equipment : snapshot.equipment) {
        const QString site_slot = tools_slot(equipment.slot);
        if (site_slot.isEmpty()) {
            return failure("The character snapshot has an unknown equipment slot.");
        }
        if (seen_slots.contains(site_slot)) {
            return failure("The character snapshot has a duplicate equipment slot.");
        }
        seen_slots.insert(site_slot);
        if (equipment.item.empty()) {
            continue;
        }

        const QString item = checked_utf8(equipment.item);
        if (item.isEmpty()) {
            return failure("The character snapshot has an invalid item name.");
        }
        const auto [base_name, upgrade] = split_item_upgrade(item);
        const QString slug = tools_item_slug(base_name);
        if (slug.isEmpty()) {
            return failure("The character snapshot has an invalid item name.");
        }
        equipped.insert(site_slot, QStringLiteral("item:") + slug);
        slot_upgrades.insert(site_slot, upgrade);
        ++equipped_items;
    }

    QJsonObject profile;
    profile.insert("version", 1);
    profile.insert(
        "exportedAt",
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    profile.insert("source", "Plazmic Legends Inventory Export");
    profile.insert("name", character_name);
    profile.insert("slotUpgrades", slot_upgrades);
    profile.insert("equipped", equipped);

    QString file_slug = tools_item_slug(character_name);
    if (file_slug.isEmpty()) {
        file_slug = QStringLiteral("character");
    }
    return {
        .contents = QJsonDocument(profile).toJson(QJsonDocument::Indented),
        .suggested_file_name =
            file_slug + QStringLiteral(
                            "-eq-legends-tools-character-sheet.json"),
        .equipped_items = equipped_items,
        .detail = "Inventory profile backup is ready.",
    };
}

bool character_profile_export_is_current(
    const CharacterSnapshot& initial_snapshot,
    const CharacterSnapshot& current_snapshot) {
    if (!initial_snapshot.available() || !current_snapshot.available()) {
        return false;
    }
    const QString initial_name = checked_utf8(initial_snapshot.name);
    const QString current_name = checked_utf8(current_snapshot.name);
    return !initial_name.isEmpty() && initial_name == current_name;
}

CharacterProfileSaveResult save_character_profile_export(
    const QString& path,
    const CharacterSnapshot& snapshot) {
    if (path.isEmpty()) {
        return {
            .saved = false,
            .equipped_items = 0U,
            .detail = "No export file was selected.",
        };
    }
    const CharacterProfileExport profile =
        build_character_profile_export(snapshot);
    if (!profile) {
        return {
            .saved = false,
            .equipped_items = 0U,
            .detail = profile.detail,
        };
    }

    constexpr QFileDevice::Permissions owner_only =
        QFileDevice::ReadOwner | QFileDevice::WriteOwner;
    QSaveFile output(path);
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly) ||
        !output.setPermissions(owner_only) ||
        output.write(profile.contents) != profile.contents.size() ||
        !output.commit()) {
        output.cancelWriting();
        return {
            .saved = false,
            .equipped_items = 0U,
            .detail = "The inventory profile backup could not be saved.",
        };
    }
    constexpr QFileDevice::Permissions shared_permissions =
        QFileDevice::ReadGroup | QFileDevice::WriteGroup |
        QFileDevice::ExeGroup | QFileDevice::ReadOther |
        QFileDevice::WriteOther | QFileDevice::ExeOther;
    QFileDevice::Permissions verified_permissions = QFile::permissions(path);
    if ((verified_permissions & owner_only) != owner_only ||
        (verified_permissions & shared_permissions) != 0) {
        QFile::setPermissions(path, owner_only);
        verified_permissions = QFile::permissions(path);
    }
    if ((verified_permissions & owner_only) != owner_only ||
        (verified_permissions & shared_permissions) != 0) {
        const bool removed = QFile::remove(path);
        return {
            .saved = false,
            .equipped_items = profile.equipped_items,
            .detail =
                removed
                    ? "Owner-only permissions could not be enforced; the "
                      "inventory profile backup was removed."
                    : "Owner-only permissions could not be enforced and the "
                      "inventory profile backup could not be removed.",
        };
    }
    return {
        .saved = true,
        .equipped_items = profile.equipped_items,
        .detail = "Inventory profile backup saved.",
    };
}

}  // namespace plazmic
