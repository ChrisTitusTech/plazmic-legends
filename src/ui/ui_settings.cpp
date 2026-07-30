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

bool is_base64(const QByteArray& value) {
    if (value.isEmpty() || value.size() % 4 != 0) {
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
        window,
        map,
    };
    Section section = Section::other;
    std::optional<QByteArray> geometry;
    std::optional<QByteArray> layout;
    bool height_filter_enabled = true;
    double height_filter_below = 15.0;
    double height_filter_above = 15.0;
    bool player_follow_enabled = false;
    for (QByteArray line : lines) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }
        if (line.startsWith('[')) {
            if (line == "[window]") {
                section = Section::window;
            } else if (line == "[map]") {
                section = Section::map;
            } else {
                section = Section::other;
            }
            continue;
        }
        if (section == Section::window) {
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
            }
        }
    }

    if (!geometry || geometry->isEmpty() || !layout || layout->isEmpty()) {
        return std::nullopt;
    }
    return UiState{
        .geometry = *geometry,
        .layout = *layout,
        .height_filter_enabled = height_filter_enabled,
        .height_filter_below = height_filter_below,
        .height_filter_above = height_filter_above,
        .player_follow_enabled = player_follow_enabled,
    };
}

bool UiSettings::save(const UiState& state) const {
    if (state.geometry.isEmpty() || state.layout.isEmpty() ||
        !std::isfinite(state.height_filter_below) ||
        !std::isfinite(state.height_filter_above) ||
        state.height_filter_below < 0.0 ||
        state.height_filter_below > 1000.0 ||
        state.height_filter_above < 0.0 ||
        state.height_filter_above > 1000.0) {
        return false;
    }
    const QFileInfo info(path_);
    if (!QDir().mkpath(info.absolutePath())) {
        return false;
    }

    QSaveFile file(path_);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    QByteArray contents;
    contents += "# Plazmic Legends per-user UI state\n";
    contents += "[window]\n";
    contents += "geometry = \"" + state.geometry.toBase64() + "\"\n";
    contents += "layout = \"" + state.layout.toBase64() + "\"\n";
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
    if (file.write(contents) != contents.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

}  // namespace plazmic
