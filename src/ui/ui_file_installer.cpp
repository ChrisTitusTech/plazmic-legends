#include "ui/ui_file_installer.h"

#include <algorithm>
#include <array>
#include <optional>
#include <ranges>

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QTemporaryDir>

namespace plazmic {
namespace {

constexpr qsizetype kMaximumBundleFiles = 10'000;
constexpr qsizetype kMaximumBundleEntries = 20'000;
constexpr qint64 kMaximumBundleFileBytes = 64 * 1024 * 1024;
constexpr qint64 kMaximumBundleBytes = 512 * 1024 * 1024;
constexpr auto kLiveLayoutFileName = "UI_plazmic_1440p.ini";

QString canonical_directory(const QString& path) {
    return QDir(path).canonicalPath();
}

bool owner_only_directory(const QString& path) {
    return QFile::setPermissions(
        path, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                  QFileDevice::ExeOwner);
}

bool owner_only_file(const QString& path) {
    return QFile::setPermissions(
        path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

bool regular_file_without_symlink(const QString& path) {
    const QFileInfo info(path);
    return info.isFile() && !info.isSymLink();
}

QStringList ini_files(const QString& directory) {
    QDir dir(directory);
    QStringList files;
    for (const QFileInfo& info : dir.entryInfoList(
             {"*.ini"}, QDir::Files | QDir::NoSymLinks, QDir::Name)) {
        files.push_back(info.absoluteFilePath());
    }
    return files;
}

void prioritize_file(QStringList& files, const QString& file_name) {
    const auto match = std::ranges::find_if(
        files, [&file_name](const QString& path) {
            return QFileInfo(path).fileName().compare(
                       file_name, Qt::CaseInsensitive) == 0;
        });
    if (match == files.end() || match == files.begin()) {
        return;
    }
    const QString preferred = *match;
    files.erase(match);
    files.prepend(preferred);
}

bool character_ini(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) ||
        file.size() > kMaximumBundleFileBytes) {
        return false;
    }
    while (!file.atEnd()) {
        const QByteArray line = file.readLine().trimmed();
        if (line == "[HotButtons]" || line == "[ADDITIONALFILTERS]") {
            return true;
        }
    }
    return false;
}

QString relative_bundle_path(const QString& root, const QString& path) {
    return "./" + QDir(root).relativeFilePath(path);
}

bool safe_relative_path(const QString& path) {
    if (!path.startsWith("./") || path.contains('\\')) {
        return false;
    }
    const QString cleaned = QDir::cleanPath(path.mid(2));
    return !cleaned.isEmpty() && cleaned != "." &&
           !cleaned.startsWith("../") && !QDir::isAbsolutePath(cleaned);
}

std::optional<QByteArray> file_sha256(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) ||
        file.size() < 0 || file.size() > kMaximumBundleFileBytes) {
        return std::nullopt;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        return std::nullopt;
    }
    return hash.result().toHex();
}

QString verify_bundle_inventory(const QString& root) {
    const QString manifest_path = root + "/SHA256SUMS";
    QFile manifest(manifest_path);
    if (!manifest.open(QIODevice::ReadOnly) ||
        manifest.size() > 2 * 1024 * 1024) {
        return "SHA256SUMS is missing or oversized.";
    }

    QMap<QString, QByteArray> expected;
    const QRegularExpression line_pattern(
        "^([0-9a-f]{64})  (\\./[^\\r\\n]+)$");
    while (!manifest.atEnd()) {
        const QString line = QString::fromUtf8(manifest.readLine()).trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const QRegularExpressionMatch match = line_pattern.match(line);
        if (!match.hasMatch() || !safe_relative_path(match.captured(2)) ||
            expected.contains(match.captured(2))) {
            return "SHA256SUMS contains an invalid or duplicate path.";
        }
        expected.insert(
            match.captured(2), match.captured(1).toLatin1());
    }

    QMap<QString, QByteArray> actual;
    qsizetype entry_count = 0;
    qint64 total_bytes = 0;
    QDirIterator iterator(
        root, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        const QFileInfo info = iterator.fileInfo();
        if (++entry_count > kMaximumBundleEntries) {
            return "The bundle exceeds its entry limit.";
        }
        if (info.isSymLink()) {
            return "The bundle contains a symbolic link.";
        }
        if (!info.isFile() || info.fileName() == "SHA256SUMS") {
            continue;
        }
        if (actual.size() >= kMaximumBundleFiles ||
            info.size() < 0 || info.size() > kMaximumBundleFileBytes ||
            total_bytes > kMaximumBundleBytes - info.size()) {
            return "The bundle exceeds its file or byte limit.";
        }
        total_bytes += info.size();
        const auto digest = file_sha256(path);
        if (!digest) {
            return "A bundle file could not be hashed.";
        }
        actual.insert(relative_bundle_path(root, path), *digest);
    }
    if (actual != expected) {
        return "The bundle does not match SHA256SUMS.";
    }
    return {};
}

bool path_in_list(const QString& path, const QStringList& list) {
    const QString canonical = QFileInfo(path).canonicalFilePath();
    return !canonical.isEmpty() && std::ranges::any_of(
        list, [&canonical](const QString& candidate) {
            return QFileInfo(candidate).canonicalFilePath() == canonical;
        });
}

bool copy_file_atomically(const QString& source, const QString& destination) {
    QFile input(source);
    if (!input.open(QIODevice::ReadOnly) ||
        input.size() < 0 || input.size() > kMaximumBundleFileBytes) {
        return false;
    }
    QSaveFile output(destination);
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly) ||
        !output.setPermissions(
            QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        return false;
    }
    std::array<char, 64 * 1024> buffer{};
    while (!input.atEnd()) {
        const qint64 count = input.read(buffer.data(), buffer.size());
        if (count <= 0 || output.write(buffer.data(), count) != count) {
            output.cancelWriting();
            return false;
        }
    }
    return output.commit();
}

bool copy_tree(const QString& source, const QString& destination) {
    if (!QDir().mkpath(destination) || !owner_only_directory(destination)) {
        return false;
    }
    QDirIterator iterator(
        source, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories);
    qsizetype file_count = 0;
    qint64 total_bytes = 0;
    while (iterator.hasNext()) {
        const QString source_path = iterator.next();
        const QFileInfo info = iterator.fileInfo();
        if (info.isSymLink()) {
            return false;
        }
        const QString relative = QDir(source).relativeFilePath(source_path);
        const QString target_path = QDir(destination).filePath(relative);
        if (info.isDir()) {
            if (!QDir().mkpath(target_path) ||
                !owner_only_directory(target_path)) {
                return false;
            }
            continue;
        }
        if (!info.isFile() || ++file_count > kMaximumBundleFiles ||
            info.size() < 0 || info.size() > kMaximumBundleFileBytes ||
            total_bytes > kMaximumBundleBytes - info.size()) {
            return false;
        }
        total_bytes += info.size();
        if (!QDir().mkpath(QFileInfo(target_path).absolutePath()) ||
            !QFile::copy(source_path, target_path) ||
            !owner_only_file(target_path)) {
            return false;
        }
    }
    return true;
}

QString unique_backup_directory(const QString& game_directory) {
    const QString timestamp =
        QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss");
    const QString base =
        QDir(game_directory).filePath("plazmic-ui-backup-" + timestamp);
    for (int suffix = 0; suffix < 100; ++suffix) {
        const QString candidate = suffix == 0
                                      ? base
                                      : base + "-" + QString::number(suffix);
        if (!QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

}  // namespace

UiBundleInspection inspect_ui_bundle(const QString& directory) {
    const QString root = canonical_directory(directory);
    if (root.isEmpty()) {
        return {.bundle = std::nullopt,
                .detail = "Select an extracted Plazmic UI bundle directory."};
    }
    const QString inventory_error = verify_bundle_inventory(root);
    if (!inventory_error.isEmpty()) {
        return {.bundle = std::nullopt, .detail = inventory_error};
    }

    const QFileInfo manifest_info(root + "/bundle.ini");
    if (!regular_file_without_symlink(manifest_info.absoluteFilePath()) ||
        manifest_info.size() > 64 * 1024) {
        return {.bundle = std::nullopt,
                .detail = "bundle.ini is missing or oversized."};
    }
    QSettings manifest(root + "/bundle.ini", QSettings::IniFormat);
    manifest.beginGroup("plazmic_ui_bundle");
    const int format = manifest.value("format").toInt();
    const QString resolution = manifest.value("resolution").toString();
    const QString skin = manifest.value("skin").toString();
    manifest.endGroup();
    static const QRegularExpression resolution_pattern("^[1-9][0-9]*x[1-9][0-9]*$");
    if (manifest.status() != QSettings::NoError || format != 1 ||
        !resolution_pattern.match(resolution).hasMatch() ||
        skin != "plazmic-ui") {
        return {.bundle = std::nullopt,
                .detail = "bundle.ini is unsupported or malformed."};
    }

    const QString skin_directory = root + "/uifiles/plazmic-ui";
    const QString global_ini = root + "/ini/eqclient.ini";
    QStringList layouts = ini_files(root + "/ini/layouts");
    prioritize_file(layouts, kLiveLayoutFileName);
    const QStringList characters = ini_files(root + "/ini/characters");
    if (!regular_file_without_symlink(skin_directory + "/EQUI.xml") ||
        !regular_file_without_symlink(global_ini) || layouts.isEmpty() ||
        characters.isEmpty() ||
        !std::ranges::all_of(characters, character_ini)) {
        return {.bundle = std::nullopt,
                .detail = "The bundle skin or INI profiles are incomplete."};
    }
    return {
        .bundle = UiBundleContents{
            .directory = root,
            .resolution = resolution,
            .skin_directory = skin_directory,
            .global_ini = global_ini,
            .layout_inis = layouts,
            .character_inis = characters,
        },
        .detail = "Private UI bundle validated.",
    };
}

UiInstallTargetInspection inspect_ui_install_targets(
    const QString& game_directory) {
    const QString root = canonical_directory(game_directory);
    const QFileInfo uifiles_info(root + "/uifiles");
    if (root.isEmpty() ||
        !regular_file_without_symlink(root + "/eqgame.exe") ||
        !uifiles_info.isDir() || uifiles_info.isSymLink()) {
        return {.targets = std::nullopt,
                .detail = "The selected Legends installation is unavailable."};
    }
    QDir dir(root);
    QStringList layouts;
    QStringList characters;
    for (const QFileInfo& info : dir.entryInfoList(
             {"*.ini"}, QDir::Files | QDir::NoSymLinks, QDir::Name)) {
        if (info.fileName().startsWith("UI_") &&
            info.fileName().endsWith(".ini", Qt::CaseInsensitive) &&
            info.fileName().compare(
                kLiveLayoutFileName, Qt::CaseInsensitive) != 0) {
            layouts.push_back(info.absoluteFilePath());
        } else if (character_ini(info.absoluteFilePath())) {
            characters.push_back(info.absoluteFilePath());
        }
    }
    if (layouts.isEmpty() || characters.isEmpty()) {
        return {.targets = std::nullopt,
                .detail = "No target layout or character INIs were found."};
    }
    return {
        .targets = UiInstallTargets{
            .layout_inis = layouts,
            .character_inis = characters,
        },
        .detail = "Target INI profiles discovered.",
    };
}

UiFileInstallResult install_ui_bundle(const UiFileInstallRequest& request) {
    const UiBundleInspection inspected_bundle =
        inspect_ui_bundle(request.bundle_directory);
    const UiInstallTargetInspection inspected_targets =
        inspect_ui_install_targets(request.game_directory);
    if (!inspected_bundle.bundle) {
        return {.installed = false,
                .detail = inspected_bundle.detail,
                .backup_directory = {}};
    }
    if (!inspected_targets.targets) {
        return {.installed = false,
                .detail = inspected_targets.detail,
                .backup_directory = {}};
    }
    const UiBundleContents& bundle = *inspected_bundle.bundle;
    const UiInstallTargets& targets = *inspected_targets.targets;
    if (!path_in_list(request.source_layout_ini, bundle.layout_inis) ||
        !path_in_list(request.target_layout_ini, targets.layout_inis) ||
        !path_in_list(request.source_character_ini, bundle.character_inis) ||
        !path_in_list(request.target_character_ini, targets.character_inis)) {
        return {
            .installed = false,
            .detail = "An INI selection is outside the validated bundle or game directory.",
            .backup_directory = {},
        };
    }

    const QString game_root = canonical_directory(request.game_directory);
    const QString live_layout = game_root + "/" + kLiveLayoutFileName;
    const QFileInfo live_layout_info(live_layout);
    const bool live_layout_existed =
        live_layout_info.exists() || live_layout_info.isSymLink();
    if (live_layout_existed && !regular_file_without_symlink(live_layout)) {
        return {
            .installed = false,
            .detail = "The live layout source is not a regular file.",
            .backup_directory = {},
        };
    }
    const QString backup = unique_backup_directory(game_root);
    if (backup.isEmpty() || !QDir().mkpath(backup + "/ini") ||
        !owner_only_directory(backup) ||
        !owner_only_directory(backup + "/ini")) {
        return {
            .installed = false,
            .detail = "A private rollback directory could not be created.",
            .backup_directory = {},
        };
    }

    const QString target_layout =
        QFileInfo(request.target_layout_ini).canonicalFilePath();
    const QString target_character =
        QFileInfo(request.target_character_ini).canonicalFilePath();
    const QString target_global = game_root + "/eqclient.ini";
    const QString backup_layout =
        backup + "/ini/" + QFileInfo(target_layout).fileName();
    const QString backup_character =
        backup + "/ini/" + QFileInfo(target_character).fileName();
    const QString backup_global = backup + "/ini/eqclient.ini";
    const QString backup_live_layout = backup + "/ini/" + kLiveLayoutFileName;
    if (!QFile::copy(target_layout, backup_layout) ||
        !QFile::copy(target_character, backup_character) ||
        (live_layout_existed &&
         !QFile::copy(live_layout, backup_live_layout)) ||
        (request.install_global_ini &&
         (!regular_file_without_symlink(target_global) ||
          !QFile::copy(target_global, backup_global))) ||
        !owner_only_file(backup_layout) ||
        !owner_only_file(backup_character) ||
        (live_layout_existed && !owner_only_file(backup_live_layout)) ||
        (request.install_global_ini && !owner_only_file(backup_global))) {
        return {.detail = "The selected INIs could not be backed up.",
                .backup_directory = backup};
    }

    const QString uifiles = game_root + "/uifiles";
    QTemporaryDir staging(uifiles + "/.plazmic-ui-install-XXXXXX");
    if (!staging.isValid() ||
        !copy_tree(bundle.skin_directory, staging.path())) {
        return {.detail = "The skin could not be staged.",
                .backup_directory = backup};
    }

    const QString target_skin = uifiles + "/plazmic-ui";
    const QString backup_skin = backup + "/uifiles/plazmic-ui";
    bool old_skin_moved = false;
    if (QFileInfo::exists(target_skin)) {
        const QString backup_uifiles = QFileInfo(backup_skin).absolutePath();
        if (!QDir().mkpath(backup_uifiles) ||
            !owner_only_directory(backup_uifiles) ||
            !QDir().rename(target_skin, backup_skin)) {
            return {.detail = "The installed skin could not be moved to rollback storage.",
                    .backup_directory = backup};
        }
        old_skin_moved = true;
    }
    if (!QDir().rename(staging.path(), target_skin)) {
        if (old_skin_moved) {
            (void)QDir().rename(backup_skin, target_skin);
        }
        return {.detail = "The staged skin could not be activated.",
                .backup_directory = backup};
    }

    const bool inis_installed =
        copy_file_atomically(request.source_layout_ini, target_layout) &&
        copy_file_atomically(request.source_layout_ini, live_layout) &&
        copy_file_atomically(request.source_character_ini, target_character) &&
        (!request.install_global_ini ||
         copy_file_atomically(bundle.global_ini, target_global));
    if (!inis_installed) {
        bool rollback_succeeded =
            copy_file_atomically(backup_layout, target_layout) &&
            copy_file_atomically(backup_character, target_character);
        if (request.install_global_ini) {
            rollback_succeeded =
                copy_file_atomically(backup_global, target_global) &&
                rollback_succeeded;
        }
        if (live_layout_existed) {
            rollback_succeeded =
                copy_file_atomically(backup_live_layout, live_layout) &&
                rollback_succeeded;
        } else if (QFileInfo::exists(live_layout)) {
            rollback_succeeded =
                QFile::remove(live_layout) && rollback_succeeded;
        }
        QDir failed_skin(target_skin);
        rollback_succeeded = failed_skin.removeRecursively() &&
                             rollback_succeeded;
        if (old_skin_moved) {
            rollback_succeeded = QDir().rename(backup_skin, target_skin) &&
                                 rollback_succeeded;
        }
        return {.detail = rollback_succeeded
                              ? "INI installation failed; all targets were restored."
                              : "Critical: installation and rollback both failed. Restore files from the reported rollback directory before starting EverQuest.",
                .backup_directory = backup};
    }

    return {
        .installed = true,
        .detail = "Plazmic UI, live layout source, and selected INI profiles installed.",
        .backup_directory = backup,
    };
}

}  // namespace plazmic
