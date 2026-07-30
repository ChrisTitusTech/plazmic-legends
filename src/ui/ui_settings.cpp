#include "ui/ui_settings.h"

#include <algorithm>
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
    bool in_window_section = false;
    std::optional<QByteArray> geometry;
    std::optional<QByteArray> layout;
    for (QByteArray line : lines) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }
        if (line.startsWith('[')) {
            in_window_section = line == "[window]";
            continue;
        }
        if (!in_window_section) {
            continue;
        }
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
    }

    if (!geometry || geometry->isEmpty() || !layout || layout->isEmpty()) {
        return std::nullopt;
    }
    return UiState{
        .geometry = *geometry,
        .layout = *layout,
    };
}

bool UiSettings::save(const UiState& state) const {
    if (state.geometry.isEmpty() || state.layout.isEmpty()) {
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
    if (file.write(contents) != contents.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

}  // namespace plazmic
