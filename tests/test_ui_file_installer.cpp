#include "ui/ui_file_installer.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void write_file(const QString& path, const QByteArray& contents) {
    require(QDir().mkpath(QFileInfo(path).absolutePath()),
            "cannot create fixture directory");
    QFile file(path);
    require(file.open(QIODevice::WriteOnly | QIODevice::Truncate),
            "cannot create fixture file");
    require(file.write(contents) == contents.size(),
            "cannot write fixture file");
}

QByteArray digest(const QString& path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "cannot hash fixture file");
    QCryptographicHash hash(QCryptographicHash::Sha256);
    require(hash.addData(&file), "cannot read fixture hash input");
    return hash.result().toHex();
}

void write_inventory(const QString& root) {
    QStringList paths;
    QDirIterator iterator(
        root, QDir::Files | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        if (QFileInfo(path).fileName() != "SHA256SUMS") {
            paths.push_back(path);
        }
    }
    paths.sort();
    QByteArray manifest;
    for (const QString& path : paths) {
        manifest += digest(path) + "  ./" +
                    QDir(root).relativeFilePath(path).toUtf8() + "\n";
    }
    write_file(root + "/SHA256SUMS", manifest);
}

QByteArray read_file(const QString& path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "cannot read result file");
    return file.readAll();
}

}  // namespace

int main() {
    try {
        QTemporaryDir directory;
        require(directory.isValid(), "cannot create test directory");
        const QString bundle = directory.filePath("private bundle");
        write_file(
            bundle + "/bundle.ini",
            "[plazmic_ui_bundle]\nformat=1\nresolution=2560x1440\n"
            "skin=plazmic-ui\n");
        write_file(bundle + "/uifiles/plazmic-ui/EQUI.xml", "<XML />\n");
        write_file(bundle + "/uifiles/plazmic-ui/asset.tga", "new-skin");
        write_file(bundle + "/ini/eqclient.ini", "[ChatFilters]\nMode=new\n");
        const QString source_layout = bundle + "/ini/layouts/UI_source.ini";
        const QString cohesive_layout =
            bundle + "/ini/layouts/UI_plazmic_1440p.ini";
        const QString source_character =
            bundle + "/ini/characters/source_server.ini";
        write_file(source_layout, "[Main]\nXPos=1440\n");
        write_file(cohesive_layout, "[Main]\nXPos=cohesive\n");
        write_file(
            source_character,
            "[HotButtons]\nPage=new\n[ADDITIONALFILTERS]\nMode=new\n");
        write_inventory(bundle);

        const plazmic::UiBundleInspection inspected =
            plazmic::inspect_ui_bundle(bundle);
        require(inspected.bundle.has_value(),
                "valid bundle was rejected");
        require(inspected.bundle->resolution == "2560x1440" &&
                    inspected.bundle->layout_inis.size() == 2 &&
                    QFileInfo(inspected.bundle->layout_inis.front())
                            .fileName() == "UI_plazmic_1440p.ini" &&
                    inspected.bundle->character_inis.size() == 1,
                "bundle contents or preferred layout were not discovered");

        const QString game = directory.filePath("EverQuest Legends");
        write_file(game + "/eqgame.exe", "synthetic");
        write_file(game + "/uifiles/default_modern/EQUI.xml", "<XML />\n");
        write_file(game + "/uifiles/plazmic-ui/EQUI.xml", "<old />\n");
        write_file(game + "/uifiles/plazmic-ui/asset.tga", "old-skin");
        const QString target_layout = game + "/UI_target_server.ini";
        const QString live_layout = game + "/UI_plazmic_1440p.ini";
        const QString target_character = game + "/target_server.ini";
        write_file(target_layout, "[Main]\nXPos=old\n");
        write_file(
            target_character,
            "[HotButtons]\nPage=old\n[ADDITIONALFILTERS]\nMode=old\n");
        write_file(game + "/eqclient.ini", "[ChatFilters]\nMode=old\n");

        const plazmic::UiInstallTargetInspection targets =
            plazmic::inspect_ui_install_targets(game);
        require(targets.targets.has_value() &&
                    targets.targets->layout_inis.size() == 1 &&
                    targets.targets->character_inis.size() == 1,
                "target INIs were not discovered");

        const QString outside = directory.filePath("outside.ini");
        write_file(outside, "[Main]\nXPos=outside\n");
        const plazmic::UiFileInstallResult outside_selection =
            plazmic::install_ui_bundle({
                .bundle_directory = bundle,
                .game_directory = game,
                .source_layout_ini = source_layout,
                .target_layout_ini = outside,
                .source_character_ini = source_character,
                .target_character_ini = target_character,
                .install_global_ini = false,
            });
        require(!outside_selection.installed &&
                    outside_selection.backup_directory.isEmpty(),
                "out-of-root target selection was accepted");

        const plazmic::UiFileInstallResult installed =
            plazmic::install_ui_bundle({
                .bundle_directory = bundle,
                .game_directory = game,
                .source_layout_ini = source_layout,
                .target_layout_ini = target_layout,
                .source_character_ini = source_character,
                .target_character_ini = target_character,
                .install_global_ini = true,
            });
        require(installed.installed, "valid bundle did not install");
        require(read_file(target_layout).contains("XPos=1440") &&
                    read_file(live_layout).contains("XPos=1440") &&
                    read_file(target_character).contains("Page=new") &&
                    read_file(game + "/eqclient.ini").contains("Mode=new") &&
                    read_file(game + "/uifiles/plazmic-ui/asset.tga") ==
                        "new-skin",
                "installed files do not match the bundle");
        require(
            read_file(installed.backup_directory +
                      "/ini/UI_target_server.ini")
                    .contains("XPos=old") &&
                read_file(installed.backup_directory +
                          "/ini/target_server.ini")
                    .contains("Page=old") &&
                !QFileInfo::exists(installed.backup_directory +
                                   "/ini/UI_plazmic_1440p.ini") &&
                read_file(installed.backup_directory +
                          "/uifiles/plazmic-ui/asset.tga") == "old-skin",
            "rollback files were not preserved");

        write_file(live_layout, "[Main]\nXPos=previous-live\n");
        const plazmic::UiInstallTargetInspection repeated_targets =
            plazmic::inspect_ui_install_targets(game);
        require(repeated_targets.targets.has_value() &&
                    repeated_targets.targets->layout_inis.size() == 1,
                "reserved live layout was offered as a target");
        const plazmic::UiFileInstallResult repeated_install =
            plazmic::install_ui_bundle({
                .bundle_directory = bundle,
                .game_directory = game,
                .source_layout_ini = source_layout,
                .target_layout_ini = target_layout,
                .source_character_ini = source_character,
                .target_character_ini = target_character,
                .install_global_ini = false,
            });
        require(repeated_install.installed &&
                    read_file(repeated_install.backup_directory +
                              "/ini/UI_plazmic_1440p.ini")
                        .contains("XPos=previous-live") &&
                    read_file(live_layout).contains("XPos=1440"),
                "existing live layout was not backed up and replaced");

        require(QFile::remove(live_layout) &&
                    QFile::link(outside, live_layout),
                "cannot create live-layout symlink fixture");
        const plazmic::UiFileInstallResult symlinked_live_layout =
            plazmic::install_ui_bundle({
                .bundle_directory = bundle,
                .game_directory = game,
                .source_layout_ini = source_layout,
                .target_layout_ini = target_layout,
                .source_character_ini = source_character,
                .target_character_ini = target_character,
                .install_global_ini = false,
            });
        require(!symlinked_live_layout.installed &&
                    symlinked_live_layout.backup_directory.isEmpty() &&
                    read_file(outside).contains("XPos=outside"),
                "symlinked live layout source was accepted");

        write_file(bundle + "/ini/eqclient.ini", "tampered\n");
        require(!plazmic::inspect_ui_bundle(bundle).bundle,
                "tampered bundle passed its SHA-256 inventory");

        std::cout << "private UI bundle validation, selection, install, and rollback passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
