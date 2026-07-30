#include "ui/ui_settings.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <QFile>
#include <QTemporaryDir>

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
        };
        require(settings.save(expected), "cannot save UI state");
        const auto loaded = settings.load();
        require(loaded.has_value(), "saved UI state did not load");
        require(loaded->geometry == expected.geometry,
                "geometry did not round trip");
        require(loaded->layout == expected.layout,
                "layout did not round trip");

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
