#include "ui/system_theme.h"

#include <utility>

#include <QApplication>
#include <QColor>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QStyleHints>
#include <QVariant>

namespace plazmic {
namespace {

constexpr auto kPortalService = "org.freedesktop.portal.Desktop";
constexpr auto kPortalPath = "/org/freedesktop/portal/desktop";
constexpr auto kPortalInterface = "org.freedesktop.portal.Settings";
constexpr auto kAppearanceNamespace = "org.freedesktop.appearance";
constexpr auto kColorSchemeKey = "color-scheme";
constexpr int kPortalTimeoutMilliseconds = 250;
constexpr int kRefreshIntervalMilliseconds = 1500;
constexpr qint64 kMaximumThemeBytes = 1024 * 1024;

std::optional<QString> assignment_value(const QString& line,
                                        const QString& key) {
    const qsizetype equals = line.indexOf('=');
    if (equals < 0 || line.left(equals).trimmed() != key) {
        return std::nullopt;
    }
    QString value = line.mid(equals + 1).trimmed();
    const qsizetype comment = value.indexOf('#');
    if (comment >= 0) {
        value = value.left(comment).trimmed();
    }
    if (value.size() >= 2 && value.front() == '"' &&
        value.back() == '"') {
        value = value.mid(1, value.size() - 2);
    }
    return value;
}

std::optional<SystemColorMode> portal_color_mode() {
    QDBusInterface portal(
        kPortalService, kPortalPath, kPortalInterface,
        QDBusConnection::sessionBus());
    if (!portal.isValid()) {
        return std::nullopt;
    }
    portal.setTimeout(kPortalTimeoutMilliseconds);
    const QDBusMessage reply =
        portal.call("ReadOne", kAppearanceNamespace, kColorSchemeKey);
    if (reply.type() != QDBusMessage::ReplyMessage ||
        reply.arguments().isEmpty()) {
        return std::nullopt;
    }

    QVariant value = reply.arguments().front();
    if (value.canConvert<QDBusVariant>()) {
        value = value.value<QDBusVariant>().variant();
    }
    bool valid = false;
    const std::uint32_t portal_value = value.toUInt(&valid);
    if (!valid) {
        return std::nullopt;
    }
    return color_mode_from_portal(portal_value);
}

SystemColorMode palette_color_mode(const QPalette& palette) {
    const int window_lightness =
        palette.color(QPalette::Active, QPalette::Window).lightness();
    const int text_lightness =
        palette.color(QPalette::Active, QPalette::WindowText).lightness();
    return window_lightness < text_lightness ? SystemColorMode::dark
                                              : SystemColorMode::light;
}

void set_disabled_colors(QPalette& palette,
                         const QColor& window_text,
                         const QColor& button_text,
                         const QColor& text) {
    palette.setColor(QPalette::Disabled, QPalette::WindowText, window_text);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, button_text);
    palette.setColor(QPalette::Disabled, QPalette::Text, text);
}

QPalette light_palette() {
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#f2f2f2"));
    palette.setColor(QPalette::WindowText, QColor("#202020"));
    palette.setColor(QPalette::Base, QColor("#ffffff"));
    palette.setColor(QPalette::AlternateBase, QColor("#e9e9e9"));
    palette.setColor(QPalette::ToolTipBase, QColor("#ffffdc"));
    palette.setColor(QPalette::ToolTipText, QColor("#202020"));
    palette.setColor(QPalette::Text, QColor("#202020"));
    palette.setColor(QPalette::Button, QColor("#e6e6e6"));
    palette.setColor(QPalette::ButtonText, QColor("#202020"));
    palette.setColor(QPalette::BrightText, QColor("#b00020"));
    palette.setColor(QPalette::Link, QColor("#0057b8"));
    palette.setColor(QPalette::Highlight, QColor("#2d7dd2"));
    palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    palette.setColor(QPalette::PlaceholderText, QColor("#707070"));
    set_disabled_colors(
        palette, QColor("#707070"), QColor("#707070"), QColor("#808080"));
    return palette;
}

QPalette dark_palette() {
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#2b2f36"));
    palette.setColor(QPalette::WindowText, QColor("#eceff4"));
    palette.setColor(QPalette::Base, QColor("#1f2329"));
    palette.setColor(QPalette::AlternateBase, QColor("#343a43"));
    palette.setColor(QPalette::ToolTipBase, QColor("#343a43"));
    palette.setColor(QPalette::ToolTipText, QColor("#eceff4"));
    palette.setColor(QPalette::Text, QColor("#eceff4"));
    palette.setColor(QPalette::Button, QColor("#3a404a"));
    palette.setColor(QPalette::ButtonText, QColor("#eceff4"));
    palette.setColor(QPalette::BrightText, QColor("#ff6b6b"));
    palette.setColor(QPalette::Link, QColor("#88c0d0"));
    palette.setColor(QPalette::Highlight, QColor("#5e81ac"));
    palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    palette.setColor(QPalette::PlaceholderText, QColor("#a7adb7"));
    set_disabled_colors(
        palette, QColor("#7d8490"), QColor("#7d8490"), QColor("#777e89"));
    return palette;
}

}  // namespace

std::optional<SystemColorMode> color_mode_from_portal(
    std::uint32_t value) {
    switch (value) {
        case 1U:
            return SystemColorMode::dark;
        case 2U:
            return SystemColorMode::light;
        case 0U:
        default:
            return std::nullopt;
    }
}

QString default_dwm_theme_path() {
    return QDir(QStandardPaths::writableLocation(
                    QStandardPaths::ConfigLocation))
        .filePath("dwm-titus/themes.toml");
}

QString dwm_theme_path_for_session(
    const QProcessEnvironment& environment) {
    const auto is_dwm = [](const QString& value) {
        return value.compare("dwm", Qt::CaseInsensitive) == 0;
    };
    if (is_dwm(environment.value("DESKTOP_SESSION")) ||
        is_dwm(environment.value("XDG_SESSION_DESKTOP"))) {
        return default_dwm_theme_path();
    }

    const QStringList current_desktops =
        environment.value("XDG_CURRENT_DESKTOP").split(
            ':', Qt::SkipEmptyParts);
    for (const QString& desktop : current_desktops) {
        if (is_dwm(desktop)) {
            return default_dwm_theme_path();
        }
    }
    return {};
}

std::optional<SystemColorMode> read_dwm_color_mode(
    const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() <= 0 ||
        file.size() > kMaximumThemeBytes) {
        return std::nullopt;
    }

    const QList<QByteArray> raw_lines = file.readAll().split('\n');
    QString active_theme;
    QString section;
    for (const QByteArray& raw_line : raw_lines) {
        const QString line = QString::fromUtf8(raw_line).trimmed();
        if (line.startsWith('[') && line.endsWith(']')) {
            section = line.mid(1, line.size() - 2).trimmed();
            continue;
        }
        if (section == "active") {
            if (const auto value = assignment_value(line, "theme")) {
                active_theme = *value;
                break;
            }
        }
    }
    if (active_theme.isEmpty()) {
        return std::nullopt;
    }

    section.clear();
    const QString target_section = "theme." + active_theme;
    for (const QByteArray& raw_line : raw_lines) {
        const QString line = QString::fromUtf8(raw_line).trimmed();
        if (line.startsWith('[') && line.endsWith(']')) {
            section = line.mid(1, line.size() - 2).trimmed();
            continue;
        }
        if (section == target_section) {
            if (const auto value = assignment_value(line, "dark_mode")) {
                if (*value == "true") {
                    return SystemColorMode::dark;
                }
                if (*value == "false") {
                    return SystemColorMode::light;
                }
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

QPalette palette_for_color_mode(SystemColorMode mode) {
    return mode == SystemColorMode::dark ? dark_palette() : light_palette();
}

void apply_color_mode(QApplication& application, SystemColorMode mode) {
    application.setPalette(palette_for_color_mode(mode));
    application.setProperty(
        "plazmic-color-mode",
        mode == SystemColorMode::dark ? "dark" : "light");
}

SystemTheme::SystemTheme(QApplication& application, QString dwm_theme_path)
    : QObject(&application),
      application_(application),
      dwm_theme_path_(std::move(dwm_theme_path)) {
    refresh_timer_.setInterval(kRefreshIntervalMilliseconds);
    QObject::connect(&refresh_timer_, &QTimer::timeout, this,
                     [this]() { refresh(); });
    QObject::connect(
        application_.styleHints(), &QStyleHints::colorSchemeChanged, this,
        [this](Qt::ColorScheme) { refresh(); });
}

void SystemTheme::start() {
    refresh();
    refresh_timer_.start();
}

void SystemTheme::refresh() {
    const SystemColorMode detected = detect().value_or(
        palette_color_mode(application_.palette()));
    if (applied_mode_ == detected) {
        return;
    }
    apply_color_mode(application_, detected);
    applied_mode_ = detected;
}

std::optional<SystemColorMode> SystemTheme::detect() const {
    if (const auto dwm_mode = read_dwm_color_mode(dwm_theme_path_)) {
        return dwm_mode;
    }
    if (const auto portal_mode = portal_color_mode()) {
        return portal_mode;
    }

    switch (application_.styleHints()->colorScheme()) {
        case Qt::ColorScheme::Dark:
            return SystemColorMode::dark;
        case Qt::ColorScheme::Light:
            return SystemColorMode::light;
        case Qt::ColorScheme::Unknown:
            return std::nullopt;
    }
    return std::nullopt;
}

}  // namespace plazmic
