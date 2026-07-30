#include "ui/system_theme.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QProcessEnvironment>
#include <QTemporaryDir>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool is_dark(const QPalette& palette) {
    return palette.color(QPalette::Window).lightness() <
           palette.color(QPalette::WindowText).lightness();
}

}  // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);

    try {
        require(plazmic::color_mode_from_portal(1U) ==
                    plazmic::SystemColorMode::dark,
                "portal prefer-dark value was not recognized");
        require(plazmic::color_mode_from_portal(2U) ==
                    plazmic::SystemColorMode::light,
                "portal prefer-light value was not recognized");
        require(!plazmic::color_mode_from_portal(0U),
                "portal no-preference value unexpectedly selected a mode");
        require(!plazmic::color_mode_from_portal(99U),
                "unknown portal value unexpectedly selected a mode");

        QProcessEnvironment other_session;
        other_session.insert("DESKTOP_SESSION", "gnome");
        require(plazmic::dwm_theme_path_for_session(other_session).isEmpty(),
                "non-DWM session unexpectedly selected the DWM theme file");
        QProcessEnvironment dwm_session;
        dwm_session.insert("XDG_SESSION_DESKTOP", "dwm");
        require(!plazmic::dwm_theme_path_for_session(dwm_session).isEmpty(),
                "DWM session did not select its theme file");

        QTemporaryDir directory;
        require(directory.isValid(), "cannot create theme test directory");
        const QString theme_path = directory.filePath("themes.toml");
        QFile theme_file(theme_path);
        require(theme_file.open(QIODevice::WriteOnly),
                "cannot open theme fixture");
        require(theme_file.write(
                    "[active]\n"
                    "theme = \"test-dark\"\n"
                    "[theme.test-dark]\n"
                    "dark_mode = true\n"
                    "[theme.test-light]\n"
                    "dark_mode = false\n") > 0,
                "cannot write dark theme fixture");
        theme_file.close();
        require(plazmic::read_dwm_color_mode(theme_path) ==
                    plazmic::SystemColorMode::dark,
                "DWM dark theme was not recognized");

        const QPalette dark = plazmic::palette_for_color_mode(
            plazmic::SystemColorMode::dark);
        const QPalette light = plazmic::palette_for_color_mode(
            plazmic::SystemColorMode::light);
        require(is_dark(dark), "dark palette has light contrast");
        require(!is_dark(light), "light palette has dark contrast");
        require(dark.color(QPalette::Base) != light.color(QPalette::Base),
                "dark and light base colors are identical");

        plazmic::apply_color_mode(
            application, plazmic::SystemColorMode::dark);
        require(application.property("plazmic-color-mode").toString() ==
                    "dark",
                "dark mode property was not applied");
        require(is_dark(application.palette()),
                "application did not receive the dark palette");

        plazmic::apply_color_mode(
            application, plazmic::SystemColorMode::light);
        require(application.property("plazmic-color-mode").toString() ==
                    "light",
                "light mode property was not applied");
        require(!is_dark(application.palette()),
                "application did not receive the light palette");

        plazmic::SystemTheme system_theme(application, theme_path);
        system_theme.start();
        require(system_theme.mode() == plazmic::SystemColorMode::dark,
                "system theme did not apply dark mode");

        require(theme_file.open(
                    QIODevice::WriteOnly | QIODevice::Truncate),
                "cannot reopen theme fixture");
        require(theme_file.write(
                    "[active]\n"
                    "theme = \"test-light\"\n"
                    "[theme.test-dark]\n"
                    "dark_mode = true\n"
                    "[theme.test-light]\n"
                    "dark_mode = false\n") > 0,
                "cannot write light theme fixture");
        theme_file.close();
        system_theme.refresh();
        require(system_theme.mode() == plazmic::SystemColorMode::light,
                "system theme did not apply light mode");
        require(!is_dark(application.palette()),
                "live refresh did not apply the light palette");

        plazmic::SystemTheme fallback_theme(application, {});
        fallback_theme.start();
        require(fallback_theme.mode().has_value(),
                "empty DWM path did not use a desktop fallback");

        std::cout << "system theme mapping, palettes, and refresh passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
