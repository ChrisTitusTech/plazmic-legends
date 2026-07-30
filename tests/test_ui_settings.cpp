#include "ui/ui_settings.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <QFile>
#include <QTemporaryDir>

#include <sys/stat.h>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int main() {
    try {
        QTemporaryDir directory;
        require(directory.isValid(), "cannot create settings directory");
        const QString path = directory.filePath("nested/config.toml");
        const plazmic::UiSettings settings(path);
        require(!settings.load(), "missing settings unexpectedly loaded");

        const plazmic::UiState expected{
            .geometry = QByteArray("geometry-bytes"),
            .layout = QByteArray("layout-bytes"),
            .height_filter_enabled = false,
            .height_filter_below = 12.5,
            .height_filter_above = 47.5,
            .player_follow_enabled = true,
            .spawn_filter = "guard",
            .spawn_type_filter = 1,
            .spawn_sort_column = 0,
            .spawn_sort_descending = true,
            .spawn_column_widths = {260, 80, 100, 120},
        };
        require(settings.save(expected), "cannot save UI state");
        struct stat saved_permissions {};
        require(::stat(path.toStdString().c_str(), &saved_permissions) == 0 &&
                    (saved_permissions.st_mode & 0777) == 0600,
                "saved UI state permissions are not owner-only");
        const auto loaded = settings.load();
        require(loaded.has_value(), "saved UI state did not load");
        require(loaded->geometry == expected.geometry,
                "geometry did not round trip");
        require(loaded->layout == expected.layout,
                "layout did not round trip");
        require(loaded->height_filter_enabled ==
                    expected.height_filter_enabled &&
                    loaded->height_filter_below ==
                        expected.height_filter_below &&
                    loaded->height_filter_above ==
                        expected.height_filter_above &&
                    loaded->player_follow_enabled ==
                        expected.player_follow_enabled &&
                    loaded->spawn_filter == expected.spawn_filter &&
                    loaded->spawn_type_filter ==
                        expected.spawn_type_filter &&
                    loaded->spawn_sort_column ==
                        expected.spawn_sort_column &&
                    loaded->spawn_sort_descending ==
                        expected.spawn_sort_descending &&
                    loaded->spawn_column_widths ==
                        expected.spawn_column_widths,
                "map settings did not round trip");

        QFile corrupt(path);
        require(corrupt.open(QIODevice::WriteOnly | QIODevice::Truncate),
                "cannot open corrupt settings fixture");
        require(corrupt.write(
                    "[window]\ngeometry = \"%%%\"\nlayout = \"%%%\"\n") > 0,
                "cannot write corrupt settings fixture");
        corrupt.close();
        require(!settings.load(), "corrupt base64 unexpectedly loaded");

        QFile oversized(path);
        require(oversized.open(QIODevice::WriteOnly | QIODevice::Truncate),
                "cannot open oversized settings fixture");
        require(oversized.write(QByteArray(1024 * 1024 + 1, 'A')) > 0,
                "cannot write oversized settings fixture");
        oversized.close();
        require(!settings.load(), "oversized settings unexpectedly loaded");

        std::cout << "UI settings persistence and rejection paths passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
