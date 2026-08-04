#pragma once

#include <optional>

#include <QString>
#include <QStringList>

namespace plazmic {

struct UiBundleContents {
    QString directory;
    QString resolution;
    QString skin_directory;
    QString global_ini;
    QStringList layout_inis;
    QStringList character_inis;
};

struct UiBundleInspection {
    std::optional<UiBundleContents> bundle;
    QString detail;
};

struct UiInstallTargets {
    QStringList layout_inis;
    QStringList character_inis;
};

struct UiInstallTargetInspection {
    std::optional<UiInstallTargets> targets;
    QString detail;
};

struct UiFileInstallRequest {
    QString bundle_directory;
    QString game_directory;
    QString source_layout_ini;
    QString target_layout_ini;
    QString source_character_ini;
    QString target_character_ini;
    bool install_global_ini{true};
};

struct UiFileInstallResult {
    bool installed{false};
    QString detail;
    QString backup_directory;
};

[[nodiscard]] UiBundleInspection inspect_ui_bundle(
    const QString& directory);
[[nodiscard]] UiInstallTargetInspection inspect_ui_install_targets(
    const QString& game_directory);
[[nodiscard]] UiFileInstallResult install_ui_bundle(
    const UiFileInstallRequest& request);

}  // namespace plazmic
