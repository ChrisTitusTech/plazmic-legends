#include "launcher/client_status.h"
#include "ui/main_window.h"
#include "ui/system_theme.h"
#include "ui/x11_window_class.h"

#include <chrono>
#include <filesystem>
#include <memory>

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QProcessEnvironment>
#include <QTimer>

namespace {

std::filesystem::path selected_client(const QCommandLineParser& parser) {
    if (parser.isSet("client")) {
        return parser.value("client").toStdString();
    }
    const QString directory =
        QProcessEnvironment::systemEnvironment().value("EQ_LEGENDS_DIR");
    if (directory.isEmpty()) {
        return {};
    }
    return QDir(directory).filePath("eqgame.exe").toStdString();
}

}  // namespace

int main(int argc, char** argv) {
    QApplication::setApplicationName("plazmic-legends");
    QApplication::setApplicationDisplayName("Plazmic Legends");
    QApplication::setDesktopFileName("plazmic-legends");
    QApplication application(argc, argv);
    application.setQuitOnLastWindowClosed(true);
    plazmic::SystemTheme system_theme(
        application,
        plazmic::dwm_theme_path_for_session(
            QProcessEnvironment::systemEnvironment()));
    system_theme.start();
    const auto* class_filter =
        plazmic::install_x11_window_class_filter(application);
    if (!class_filter->available()) {
        return 20;
    }

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Read-only EverQuest Legends companion window");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({
        "client",
        "Path to the installed Legends eqgame.exe.",
        "path",
    });
    parser.addOption({
        "duration",
        "Exit automatically after the specified number of seconds.",
        "seconds",
        "0",
    });
    parser.addOption({
        "reset-layout",
        "Ignore saved window geometry and dock state for this run.",
    });
    parser.addOption({
        "settings-file",
        "Override the per-user settings path for testing.",
        "path",
    });
    parser.process(application);

    bool duration_valid = false;
    const int duration_seconds =
        parser.value("duration").toInt(&duration_valid);
    if (!duration_valid || duration_seconds < 0) {
        parser.showHelp(2);
    }

    auto probe =
        std::make_unique<plazmic::ClientStatusProbe>(selected_client(parser));
    const QString settings_path = parser.isSet("settings-file")
                                      ? parser.value("settings-file")
                                      : plazmic::UiSettings::default_path();
    plazmic::MainWindow window(
        probe->refresh(), settings_path, parser.isSet("reset-layout"));
    window.show();

    QTimer refresh_timer;
    QObject::connect(&refresh_timer, &QTimer::timeout, &window,
                     [&window, &probe]() {
                         window.update_snapshot(probe->refresh());
                     });
    refresh_timer.start(std::chrono::seconds(1));

    if (duration_seconds > 0) {
        QTimer::singleShot(std::chrono::seconds(duration_seconds),
                           &window, &QWidget::close);
    }
    return application.exec();
}
