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
            .client_directory =
                directory.filePath("EverQuest Legends"),
            .geometry = QByteArray("geometry-bytes"),
            .layout = QByteArray("layout-bytes"),
            .height_filter_enabled = false,
            .height_filter_below = 12.5,
            .height_filter_above = 47.5,
            .player_follow_enabled = true,
            .named_spawn_labels_visible = true,
            .player_labels_visible = true,
            .npc_labels_visible = false,
            .named_spawns_visible = false,
            .player_spawns_visible = true,
            .npc_spawns_visible = false,
            .other_spawns_visible = false,
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
        require(loaded->client_directory ==
                    expected.client_directory,
                "client directory did not round trip");
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
                    loaded->named_spawn_labels_visible ==
                        expected.named_spawn_labels_visible &&
                    loaded->player_labels_visible ==
                        expected.player_labels_visible &&
                    loaded->npc_labels_visible ==
                        expected.npc_labels_visible &&
                    loaded->named_spawns_visible ==
                        expected.named_spawns_visible &&
                    loaded->player_spawns_visible ==
                        expected.player_spawns_visible &&
                    loaded->npc_spawns_visible ==
                        expected.npc_spawns_visible &&
                    loaded->other_spawns_visible ==
                        expected.other_spawns_visible &&
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

        QFile readable(path);
        require(readable.open(QIODevice::ReadOnly),
                "cannot inspect saved settings");
        const QByteArray readable_contents = readable.readAll();
        require(
            readable_contents.contains("[client]\n") &&
                readable_contents.contains(
                    "game_directory = \"" +
                    expected.client_directory.toUtf8() + "\"\n"),
            "client directory was not stored as readable TOML");
        readable.close();

        const QString partial_path =
            directory.filePath("partial/config.toml");
        const plazmic::UiSettings partial_settings(partial_path);
        const QString quoted_directory =
            directory.filePath("EverQuest \"Legends\"\\prefix");
        plazmic::UiState partial_state;
        partial_state.client_directory = quoted_directory;
        require(
            partial_settings.save(partial_state),
            "cannot save client-only settings");
        const auto partial = partial_settings.load();
        require(
            partial &&
                partial->client_directory == quoted_directory &&
                partial->geometry.isEmpty() &&
                partial->layout.isEmpty(),
            "client-only settings or TOML escaping did not round trip");

        plazmic::UiState relative_state;
        relative_state.client_directory =
            "relative/EverQuest Legends";
        require(
            !partial_settings.save(relative_state),
            "relative client directory unexpectedly saved");

        const QString legacy_path =
            directory.filePath("legacy/config.toml");
        plazmic::UiState legacy_state = expected;
        legacy_state.client_directory.clear();
        const plazmic::UiSettings legacy_settings(legacy_path);
        require(legacy_settings.save(legacy_state),
                "cannot save legacy-compatible UI state");
        const auto legacy = legacy_settings.load();
        require(legacy && legacy->client_directory.isEmpty(),
                "settings without [client] did not remain compatible");

        QFile malformed(partial_path);
        require(malformed.open(
                    QIODevice::WriteOnly | QIODevice::Truncate),
                "cannot open malformed client fixture");
        require(
            malformed.write(
                "[client]\n"
                "game_directory = \"/tmp/invalid\\q\"\n") > 0,
            "cannot write malformed client fixture");
        malformed.close();
        require(!partial_settings.load(),
                "malformed TOML escape unexpectedly loaded");

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
