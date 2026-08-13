#include "ui/character_profile_exporter.h"

#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

plazmic::CharacterSnapshot available_snapshot() {
    return {
        .state = plazmic::PlayerSnapshotState::in_world,
        .name = "Synthetic Character",
        .health = {},
        .mana = {},
        .alternate_advancement_percent = std::nullopt,
        .alternate_advancement_points = std::nullopt,
        .equipment = {},
        .detail = "Synthetic character snapshot",
    };
}

QJsonObject parse_export(const plazmic::CharacterProfileExport& result) {
    QJsonParseError error;
    const QJsonDocument document =
        QJsonDocument::fromJson(result.contents, &error);
    require(error.error == QJsonParseError::NoError && document.isObject(),
            "inventory export is not valid JSON");
    return document.object();
}

}  // namespace

int main() {
    try {
        constexpr std::array<std::pair<const char*, const char*>, 23>
            slot_mappings{
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

        plazmic::CharacterSnapshot complete = available_snapshot();
        complete.name = "\xc3\x89lan O\xe2\x80\x99Rinn & Co.";
        for (std::size_t index = 0U; index < slot_mappings.size(); ++index) {
            complete.equipment.push_back({
                .slot = slot_mappings[index].first,
                .item = "Synthetic Item " + std::to_string(index + 1U),
            });
        }
        complete.equipment[13].item = "King's & Queen\xe2\x80\x99s Blade +12";
        complete.equipment[14].item =
            "Deterioriated Ancient Faydark Longbow";

        const plazmic::CharacterProfileExport exported =
            plazmic::build_character_profile_export(complete);
        require(static_cast<bool>(exported), exported.detail.toStdString());
        require(exported.equipped_items == slot_mappings.size(),
                "equipped item count is incorrect");
        require(
            exported.suggested_file_name ==
                "elan-orinn-and-co-eq-legends-tools-character-sheet.json",
            "suggested profile-backup file name is incorrect: " +
                exported.suggested_file_name.toStdString());
        const QJsonObject root = parse_export(exported);
        require(root.value("version").toInt() == 1 &&
                    root.value("source").toString() ==
                        "Plazmic Legends Inventory Export" &&
                    root.value("name").toString() ==
                        QString::fromUtf8(complete.name) &&
                    !root.value("exportedAt").toString().isEmpty(),
                "profile-backup metadata is incomplete");
        require(!root.contains("race") && !root.contains("classes") &&
                    !root.contains("favoredStats") &&
                    !root.contains("aaRanks") &&
                    !root.contains("exaltations"),
                "inventory-only export invented unavailable profile fields");
        const QJsonObject equipped = root.value("equipped").toObject();
        const QJsonObject upgrades = root.value("slotUpgrades").toObject();
        for (std::size_t index = 0U; index < slot_mappings.size(); ++index) {
            const QString site_slot = slot_mappings[index].second;
            const QString expected =
                QString("item:synthetic-item-%1").arg(index + 1U);
            if (index == 13U) {
                require(equipped.value(site_slot).toString() ==
                            "item:kings-and-queens-blade" &&
                            upgrades.value(site_slot).toInt() == 10,
                        "item punctuation or upgrade suffix was not normalized");
            } else if (index == 14U) {
                require(
                    equipped.value(site_slot).toString() ==
                        "item:deteriorated-ancient-faydark-longbow",
                    "EQ Legends Tools compatibility alias was not applied");
            } else {
                require(equipped.value(site_slot).toString() == expected &&
                            upgrades.value(site_slot).toInt() == 0,
                        "equipment slot mapping is incorrect");
            }
        }
        require(
            equipped.size() == static_cast<qsizetype>(slot_mappings.size()),
            "profile backup contains unexpected equipment entries");

        plazmic::CharacterSnapshot empty = available_snapshot();
        const auto empty_export =
            plazmic::build_character_profile_export(empty);
        require(empty_export && empty_export.equipped_items == 0U &&
                    parse_export(empty_export)
                        .value("equipped")
                        .toObject()
                        .isEmpty() &&
                    parse_export(empty_export)
                        .value("slotUpgrades")
                        .toObject()
                        .isEmpty(),
                "an available empty inventory did not export cleanly");

        plazmic::CharacterSnapshot unavailable = available_snapshot();
        unavailable.state = plazmic::PlayerSnapshotState::zoning;
        require(!plazmic::build_character_profile_export(unavailable),
                "an unavailable snapshot was exported");
        require(
            !plazmic::character_profile_export_is_current(complete,
                                                          unavailable),
            "a snapshot invalidated during destination selection stayed current");

        plazmic::CharacterSnapshot changed_character = complete;
        changed_character.name = "Different Synthetic Character";
        require(
            !plazmic::character_profile_export_is_current(
                complete, changed_character),
            "a character change during destination selection stayed current");

        plazmic::CharacterSnapshot refreshed_inventory = complete;
        refreshed_inventory.equipment.clear();
        require(
            plazmic::character_profile_export_is_current(
                complete, refreshed_inventory),
            "a current snapshot for the same character was rejected");

        plazmic::CharacterSnapshot duplicate = available_snapshot();
        duplicate.equipment = {
            {.slot = "Head", .item = "One"},
            {.slot = "Head", .item = "Two"},
        };
        require(!plazmic::build_character_profile_export(duplicate),
                "duplicate equipment slots were exported");

        plazmic::CharacterSnapshot unknown = available_snapshot();
        unknown.equipment = {{.slot = "Unknown", .item = "Item"}};
        require(!plazmic::build_character_profile_export(unknown),
                "an unknown equipment slot was exported");

        plazmic::CharacterSnapshot invalid = available_snapshot();
        invalid.equipment = {{.slot = "Head", .item = "Bad\nItem"}};
        require(!plazmic::build_character_profile_export(invalid),
                "an invalid item name was exported");

        QTemporaryDir directory;
        require(directory.isValid(), "cannot create export test directory");
        const QString output_path = directory.filePath("profile.json");
        const auto saved = plazmic::save_character_profile_export(
            output_path, complete);
        require(saved && saved.equipped_items == slot_mappings.size(),
                saved.detail.toStdString());
        QFile output(output_path);
        require(output.open(QIODevice::ReadOnly),
                "saved inventory backup cannot be opened");
        QJsonParseError saved_error;
        const QJsonDocument saved_document =
            QJsonDocument::fromJson(output.readAll(), &saved_error);
        const QJsonObject saved_root = saved_document.object();
        require(saved_error.error == QJsonParseError::NoError &&
                    saved_document.isObject() &&
                    saved_root.value("source").toString() ==
                        "Plazmic Legends Inventory Export" &&
                    saved_root.value("name").toString() ==
                        QString::fromUtf8(complete.name) &&
                    saved_root.value("equipped").toObject() == equipped &&
                    saved_root.value("slotUpgrades").toObject() == upgrades,
                "saved inventory backup lost its profile content");
        const QFileDevice::Permissions permissions =
            QFileInfo(output_path).permissions();
        require(
            (permissions & (QFileDevice::ReadOwner |
                            QFileDevice::WriteOwner)) ==
                    (QFileDevice::ReadOwner | QFileDevice::WriteOwner) &&
                (permissions &
                 (QFileDevice::ReadGroup | QFileDevice::WriteGroup |
                  QFileDevice::ExeGroup | QFileDevice::ReadOther |
                  QFileDevice::WriteOther | QFileDevice::ExeOther)) == 0,
            "saved inventory backup is not owner-only");

        const QString replacement_path =
            directory.filePath("existing-profile.json");
        QFile replacement(replacement_path);
        require(replacement.open(QIODevice::WriteOnly) &&
                    replacement.write("old") == 3,
                "cannot create existing export target");
        replacement.close();
        require(QFile::setPermissions(
                    replacement_path,
                    QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                        QFileDevice::ReadGroup | QFileDevice::ReadOther),
                "cannot make existing export target permissive");
        require(static_cast<bool>(
                    plazmic::save_character_profile_export(
                        replacement_path, complete)),
                "cannot atomically replace an existing export target");
        QFile replaced(replacement_path);
        require(replaced.open(QIODevice::ReadOnly),
                "cannot open replaced export target");
        QJsonParseError replaced_error;
        const QJsonDocument replaced_document =
            QJsonDocument::fromJson(replaced.readAll(), &replaced_error);
        const QJsonObject replaced_root = replaced_document.object();
        require(replaced_error.error == QJsonParseError::NoError &&
                    replaced_document.isObject() &&
                    replaced_root.value("equipped").toObject() == equipped &&
                    replaced_root.value("slotUpgrades").toObject() == upgrades,
                "existing export target was not replaced with profile content");
        require(
            (QFileInfo(replacement_path).permissions() &
             (QFileDevice::ReadGroup | QFileDevice::WriteGroup |
              QFileDevice::ExeGroup | QFileDevice::ReadOther |
              QFileDevice::WriteOther | QFileDevice::ExeOther)) == 0,
            "replaced inventory backup retained shared permissions");

        std::cout << "EQ Legends Tools inventory profile export passed\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
