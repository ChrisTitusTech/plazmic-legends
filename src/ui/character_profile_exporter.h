#pragma once

#include "model/character_snapshot.h"

#include <cstddef>

#include <QByteArray>
#include <QString>

namespace plazmic {

struct CharacterProfileExport {
    QByteArray contents;
    QString suggested_file_name;
    std::size_t equipped_items{};
    QString detail;

    [[nodiscard]] explicit operator bool() const {
        return !contents.isEmpty();
    }
};

struct CharacterProfileSaveResult {
    bool saved{};
    std::size_t equipped_items{};
    QString detail;

    [[nodiscard]] explicit operator bool() const { return saved; }
};

[[nodiscard]] CharacterProfileExport build_character_profile_export(
    const CharacterSnapshot& snapshot);

[[nodiscard]] bool character_profile_export_is_current(
    const CharacterSnapshot& initial_snapshot,
    const CharacterSnapshot& current_snapshot);

[[nodiscard]] CharacterProfileSaveResult save_character_profile_export(
    const QString& path,
    const CharacterSnapshot& snapshot);

}  // namespace plazmic
