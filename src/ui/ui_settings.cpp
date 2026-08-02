#include "ui/ui_settings.h"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <utility>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

namespace plazmic {
namespace {

constexpr qint64 kMaximumSettingsBytes = 1024 * 1024;
constexpr qsizetype kMaximumClientDirectoryBytes = 4096;

bool is_base64(const QByteArray& value) {
    if (value.size() % 4 != 0) {
        return false;
    }
    return std::ranges::all_of(value, [](char character) {
        const auto byte = static_cast<unsigned char>(character);
        return (byte >= 'A' && byte <= 'Z') ||
               (byte >= 'a' && byte <= 'z') ||
               (byte >= '0' && byte <= '9') || byte == '+' || byte == '/' ||
               byte == '=';
    });
}

std::optional<QByteArray> quoted_value(const QByteArray& line,
                                       const QByteArray& key) {
    const QByteArray prefix = key + " = \"";
    if (!line.startsWith(prefix) || !line.endsWith('"')) {
        return std::nullopt;
    }
    const QByteArray encoded =
        line.mid(prefix.size(), line.size() - prefix.size() - 1);
    if (!is_base64(encoded)) {
        return std::nullopt;
    }
    return QByteArray::fromBase64(encoded);
}

bool valid_client_directory(const QString& directory) {
    if (directory.isEmpty() || !QDir::isAbsolutePath(directory)) {
        return false;
    }
    const QByteArray encoded = directory.toUtf8();
    if (encoded.isEmpty() ||
        encoded.size() > kMaximumClientDirectoryBytes) {
        return false;
    }
    for (QChar character : directory) {
        if (character.isNull() ||
            character.category() == QChar::Other_Control) {
            return false;
        }
    }
    return true;
}

std::optional<QString> toml_string_value(const QByteArray& line,
                                         const QByteArray& key) {
    const QByteArray prefix = key + " = \"";
    if (!line.startsWith(prefix) || !line.endsWith('"')) {
        return std::nullopt;
    }
    const QByteArray encoded =
        line.mid(prefix.size(), line.size() - prefix.size() - 1);
    QByteArray decoded;
    decoded.reserve(encoded.size());
    bool escaped = false;
    for (char character : encoded) {
        if (escaped) {
            if (character != '\\' && character != '"') {
                return std::nullopt;
            }
            decoded += character;
            escaped = false;
            continue;
        }
        if (character == '\\') {
            escaped = true;
            continue;
        }
        const auto byte = static_cast<unsigned char>(character);
        if (byte < 0x20U || byte == 0x7FU) {
            return std::nullopt;
        }
        decoded += character;
    }
    if (escaped || decoded.size() > kMaximumClientDirectoryBytes) {
        return std::nullopt;
    }
    const QString result = QString::fromUtf8(decoded);
    if (result.toUtf8() != decoded || !valid_client_directory(result)) {
        return std::nullopt;
    }
    return QDir::cleanPath(result);
}

QByteArray toml_string(const QString& value) {
    QByteArray encoded;
    const QByteArray bytes = value.toUtf8();
    encoded.reserve(bytes.size() + 2);
    encoded += '"';
    for (char character : bytes) {
        if (character == '\\' || character == '"') {
            encoded += '\\';
        }
        encoded += character;
    }
    encoded += '"';
    return encoded;
}

std::optional<QByteArray> scalar_value(const QByteArray& line,
                                       const QByteArray& key) {
    const QByteArray prefix = key + " = ";
    if (!line.startsWith(prefix)) {
        return std::nullopt;
    }
    const QByteArray value = line.mid(prefix.size()).trimmed();
    if (value.isEmpty()) {
        return std::nullopt;
    }
    return value;
}

std::optional<double> height_range_value(const QByteArray& line,
                                         const QByteArray& key) {
    const auto value = scalar_value(line, key);
    if (!value) {
        return std::nullopt;
    }
    bool valid = false;
    const double number = value->toDouble(&valid);
    if (!valid || !std::isfinite(number) || number < 0.0 ||
        number > 1000.0) {
        return std::nullopt;
    }
    return number;
}

std::optional<int> integer_value(const QByteArray& line,
                                 const QByteArray& key,
                                 int minimum,
                                 int maximum) {
    const auto value = scalar_value(line, key);
    if (!value) {
        return std::nullopt;
    }
    bool valid = false;
    const int number = value->toInt(&valid);
    if (!valid || number < minimum || number > maximum) {
        return std::nullopt;
    }
    return number;
}

}  // namespace

UiSettings::UiSettings(QString path) : path_(std::move(path)) {}

QString UiSettings::default_path() {
    return QDir(QStandardPaths::writableLocation(
                    QStandardPaths::ConfigLocation))
        .filePath("plazmic-legends/config.toml");
}

std::optional<UiState> UiSettings::load() const {
    QFile file(path_);
    if (!file.exists()) {
        return std::nullopt;
    }
    if (!file.open(QIODevice::ReadOnly) || file.size() <= 0 ||
        file.size() > kMaximumSettingsBytes) {
        return std::nullopt;
    }

    const QList<QByteArray> lines = file.readAll().split('\n');
    enum class Section {
        other,
        client,
        window,
        map,
        spawns,
    };
    Section section = Section::other;
    std::optional<QString> client_directory;
    std::optional<QByteArray> geometry;
    std::optional<QByteArray> layout;
    bool height_filter_enabled = true;
    double height_filter_below = 15.0;
    double height_filter_above = 15.0;
    bool player_follow_enabled = false;
    bool named_spawn_labels_visible = false;
    bool player_labels_visible = false;
    bool npc_labels_visible = false;
    bool named_spawns_visible = true;
    bool player_spawns_visible = true;
    bool npc_spawns_visible = true;
    bool other_spawns_visible = true;
    QString spawn_filter;
    int spawn_type_filter = -1;
    int spawn_sort_column = 3;
    bool spawn_sort_descending = false;
    std::array<int, 4> spawn_column_widths{220, 70, 90, 100};
    for (QByteArray line : lines) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }
        if (line.startsWith('[')) {
            if (line == "[client]") {
                section = Section::client;
            } else if (line == "[window]") {
                section = Section::window;
            } else if (line == "[map]") {
                section = Section::map;
            } else if (line == "[spawns]") {
                section = Section::spawns;
            } else {
                section = Section::other;
            }
            continue;
        }
        if (section == Section::client) {
            if (line.startsWith("game_directory")) {
                if (client_directory) {
                    return std::nullopt;
                }
                client_directory =
                    toml_string_value(line, "game_directory");
                if (!client_directory) {
                    return std::nullopt;
                }
            }
        } else if (section == Section::window) {
            if (line.startsWith("geometry")) {
                geometry = quoted_value(line, "geometry");
                if (!geometry) {
                    return std::nullopt;
                }
            } else if (line.startsWith("layout")) {
                layout = quoted_value(line, "layout");
                if (!layout) {
                    return std::nullopt;
                }
            }
        } else if (section == Section::map) {
            if (line.startsWith("height_filter_enabled")) {
                const auto value =
                    scalar_value(line, "height_filter_enabled");
                if (!value || (*value != "true" && *value != "false")) {
                    return std::nullopt;
                }
                height_filter_enabled = *value == "true";
            } else if (line.startsWith("height_filter_below")) {
                const auto value =
                    height_range_value(line, "height_filter_below");
                if (!value) {
                    return std::nullopt;
                }
                height_filter_below = *value;
            } else if (line.startsWith("height_filter_above")) {
                const auto value =
                    height_range_value(line, "height_filter_above");
                if (!value) {
                    return std::nullopt;
                }
                height_filter_above = *value;
            } else if (line.startsWith("player_follow_enabled")) {
                const auto value =
                    scalar_value(line, "player_follow_enabled");
                if (!value || (*value != "true" && *value != "false")) {
                    return std::nullopt;
                }
                player_follow_enabled = *value == "true";
            } else if (line.startsWith("named_spawn_labels_visible")) {
                const auto value = scalar_value(
                    line, "named_spawn_labels_visible");
                if (!value || (*value != "true" && *value != "false")) {
                    return std::nullopt;
                }
                named_spawn_labels_visible = *value == "true";
            } else if (line.startsWith("player_labels_visible")) {
                const auto value =
                    scalar_value(line, "player_labels_visible");
                if (!value || (*value != "true" && *value != "false")) {
                    return std::nullopt;
                }
                player_labels_visible = *value == "true";
            } else if (line.startsWith("npc_labels_visible")) {
                const auto value =
                    scalar_value(line, "npc_labels_visible");
                if (!value || (*value != "true" && *value != "false")) {
                    return std::nullopt;
                }
                npc_labels_visible = *value == "true";
            } else if (line.startsWith("named_spawns_visible")) {
                const auto value =
                    scalar_value(line, "named_spawns_visible");
                if (!value || (*value != "true" && *value != "false")) {
                    return std::nullopt;
                }
                named_spawns_visible = *value == "true";
            } else if (line.startsWith("player_spawns_visible")) {
                const auto value =
                    scalar_value(line, "player_spawns_visible");
                if (!value || (*value != "true" && *value != "false")) {
                    return std::nullopt;
                }
                player_spawns_visible = *value == "true";
            } else if (line.startsWith("npc_spawns_visible")) {
                const auto value =
                    scalar_value(line, "npc_spawns_visible");
                if (!value || (*value != "true" && *value != "false")) {
                    return std::nullopt;
                }
                npc_spawns_visible = *value == "true";
            } else if (line.startsWith("other_spawns_visible")) {
                const auto value =
                    scalar_value(line, "other_spawns_visible");
                if (!value || (*value != "true" && *value != "false")) {
                    return std::nullopt;
                }
                other_spawns_visible = *value == "true";
            }
        } else if (section == Section::spawns) {
            if (line.startsWith("filter")) {
                const auto value = quoted_value(line, "filter");
                if (!value || value->size() > 1024 ||
                    value->contains('\0')) {
                    return std::nullopt;
                }
                spawn_filter = QString::fromUtf8(*value);
                if (spawn_filter.size() > 256) {
                    return std::nullopt;
                }
            } else if (line.startsWith("type")) {
                const auto value =
                    integer_value(line, "type", -1, 2);
                if (!value) {
                    return std::nullopt;
                }
                spawn_type_filter = *value;
            } else if (line.startsWith("sort_column")) {
                const auto value =
                    integer_value(line, "sort_column", 0, 3);
                if (!value) {
                    return std::nullopt;
                }
                spawn_sort_column = *value;
            } else if (line.startsWith("sort_descending")) {
                const auto value =
                    scalar_value(line, "sort_descending");
                if (!value || (*value != "true" && *value != "false")) {
                    return std::nullopt;
                }
                spawn_sort_descending = *value == "true";
            } else if (line.startsWith("column_widths")) {
                const auto value =
                    scalar_value(line, "column_widths");
                if (!value) {
                    return std::nullopt;
                }
                const QList<QByteArray> widths = value->split(',');
                if (widths.size() != 4) {
                    return std::nullopt;
                }
                for (qsizetype index = 0; index < widths.size(); ++index) {
                    bool valid = false;
                    const int width =
                        widths[index].trimmed().toInt(&valid);
                    if (!valid || width < 40 || width > 1000) {
                        return std::nullopt;
                    }
                    spawn_column_widths[
                        static_cast<std::size_t>(index)] = width;
                }
            }
        }
    }

    const bool has_geometry = geometry && !geometry->isEmpty();
    const bool has_layout = layout && !layout->isEmpty();
    if (has_geometry != has_layout ||
        (!has_geometry && !client_directory)) {
        return std::nullopt;
    }
    return UiState{
        .client_directory =
            client_directory.value_or(QString{}),
        .geometry = geometry.value_or(QByteArray{}),
        .layout = layout.value_or(QByteArray{}),
        .height_filter_enabled = height_filter_enabled,
        .height_filter_below = height_filter_below,
        .height_filter_above = height_filter_above,
        .player_follow_enabled = player_follow_enabled,
        .named_spawn_labels_visible = named_spawn_labels_visible,
        .player_labels_visible = player_labels_visible,
        .npc_labels_visible = npc_labels_visible,
        .named_spawns_visible = named_spawns_visible,
        .player_spawns_visible = player_spawns_visible,
        .npc_spawns_visible = npc_spawns_visible,
        .other_spawns_visible = other_spawns_visible,
        .spawn_filter = spawn_filter,
        .spawn_type_filter = spawn_type_filter,
        .spawn_sort_column = spawn_sort_column,
        .spawn_sort_descending = spawn_sort_descending,
        .spawn_column_widths = spawn_column_widths,
    };
}

bool UiSettings::save(const UiState& state) const {
    const bool has_geometry = !state.geometry.isEmpty();
    const bool has_layout = !state.layout.isEmpty();
    if (has_geometry != has_layout ||
        (!has_geometry && state.client_directory.isEmpty()) ||
        (!state.client_directory.isEmpty() &&
         !valid_client_directory(state.client_directory)) ||
        !std::isfinite(state.height_filter_below) ||
        !std::isfinite(state.height_filter_above) ||
        state.height_filter_below < 0.0 ||
        state.height_filter_below > 1000.0 ||
        state.height_filter_above < 0.0 ||
        state.height_filter_above > 1000.0 ||
        state.spawn_filter.size() > 256 ||
        state.spawn_type_filter < -1 ||
        state.spawn_type_filter > 2 ||
        state.spawn_sort_column < 0 ||
        state.spawn_sort_column > 3 ||
        std::ranges::any_of(
            state.spawn_column_widths,
            [](int width) { return width < 40 || width > 1000; })) {
        return false;
    }
    const QFileInfo info(path_);
    if (!QDir().mkpath(info.absolutePath())) {
        return false;
    }
    constexpr QFileDevice::Permissions kDirectoryPermissions =
        QFileDevice::ReadOwner | QFileDevice::WriteOwner |
        QFileDevice::ExeOwner;
    if (!QFile::setPermissions(
            info.absolutePath(), kDirectoryPermissions)) {
        return false;
    }

    QSaveFile file(path_);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    constexpr QFileDevice::Permissions kFilePermissions =
        QFileDevice::ReadOwner | QFileDevice::WriteOwner;
    if (!file.setPermissions(kFilePermissions)) {
        file.cancelWriting();
        return false;
    }
    QByteArray contents;
    contents += "# Plazmic Legends per-user UI state\n";
    if (!state.client_directory.isEmpty()) {
        contents += "[client]\n";
        contents += "game_directory = " +
                    toml_string(
                        QDir::cleanPath(state.client_directory)) +
                    "\n";
    }
    if (has_geometry) {
        if (!state.client_directory.isEmpty()) {
            contents += "\n";
        }
        contents += "[window]\n";
        contents += "geometry = \"" +
                    state.geometry.toBase64() + "\"\n";
        contents += "layout = \"" +
                    state.layout.toBase64() + "\"\n";
    }
    contents += "\n[map]\n";
    contents += QByteArray("height_filter_enabled = ") +
                (state.height_filter_enabled ? "true\n" : "false\n");
    contents += "height_filter_below = " +
                QByteArray::number(state.height_filter_below, 'f', 1) +
                "\n";
    contents += "height_filter_above = " +
                QByteArray::number(state.height_filter_above, 'f', 1) +
                "\n";
    contents += QByteArray("player_follow_enabled = ") +
                (state.player_follow_enabled ? "true\n" : "false\n");
    contents += QByteArray("named_spawn_labels_visible = ") +
                (state.named_spawn_labels_visible ? "true\n" : "false\n");
    contents += QByteArray("player_labels_visible = ") +
                (state.player_labels_visible ? "true\n" : "false\n");
    contents += QByteArray("npc_labels_visible = ") +
                (state.npc_labels_visible ? "true\n" : "false\n");
    contents += QByteArray("named_spawns_visible = ") +
                (state.named_spawns_visible ? "true\n" : "false\n");
    contents += QByteArray("player_spawns_visible = ") +
                (state.player_spawns_visible ? "true\n" : "false\n");
    contents += QByteArray("npc_spawns_visible = ") +
                (state.npc_spawns_visible ? "true\n" : "false\n");
    contents += QByteArray("other_spawns_visible = ") +
                (state.other_spawns_visible ? "true\n" : "false\n");
    contents += "\n[spawns]\n";
    contents += "filter = \"" +
                state.spawn_filter.toUtf8().toBase64() + "\"\n";
    contents += "type = " +
                QByteArray::number(state.spawn_type_filter) + "\n";
    contents += "sort_column = " +
                QByteArray::number(state.spawn_sort_column) + "\n";
    contents += QByteArray("sort_descending = ") +
                (state.spawn_sort_descending ? "true\n" : "false\n");
    contents += "column_widths = ";
    for (std::size_t index = 0;
         index < state.spawn_column_widths.size(); ++index) {
        if (index != 0U) {
            contents += ",";
        }
        contents += QByteArray::number(state.spawn_column_widths[index]);
    }
    contents += "\n";
    if (file.write(contents) != contents.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

}  // namespace plazmic
