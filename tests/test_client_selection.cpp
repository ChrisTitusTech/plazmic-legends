#include "launcher/client_selection.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include <unistd.h>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class TemporaryHome {
  public:
    TemporaryHome() {
        char pattern[] = "/tmp/plazmic-client-selection.XXXXXXXX";
        const char* created = ::mkdtemp(pattern);
        if (created == nullptr) {
            throw std::runtime_error("cannot create temporary home");
        }
        path_ = created;
    }

    ~TemporaryHome() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TemporaryHome(const TemporaryHome&) = delete;
    TemporaryHome& operator=(const TemporaryHome&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

std::filesystem::path create_client(
    const std::filesystem::path& game_directory) {
    std::filesystem::create_directories(game_directory);
    const std::filesystem::path client =
        game_directory / "eqgame.exe";
    std::ofstream stream(client, std::ios::binary);
    stream << "synthetic client fixture";
    stream.close();
    return client;
}

}  // namespace

int main() {
    try {
        TemporaryHome home;
        const std::filesystem::path explicit_directory =
            home.path() / "explicit";
        const std::filesystem::path explicit_client =
            create_client(explicit_directory);
        const std::filesystem::path environment_directory =
            home.path() / "environment";
        create_client(environment_directory);
        const std::filesystem::path saved_directory =
            home.path() / "saved";
        create_client(saved_directory);

        const plazmic::ClientSelection explicit_selection =
            plazmic::select_client({
                .command_line_client = explicit_client,
                .environment_directory = environment_directory,
                .saved_directory = saved_directory,
                .home_directory = home.path(),
            });
        require(
            explicit_selection.source ==
                    plazmic::ClientSelectionSource::command_line &&
                explicit_selection.client == explicit_client &&
                explicit_selection.should_persist &&
                !explicit_selection.scanned,
            "command-line client did not take priority");

        const plazmic::ClientSelection environment_selection =
            plazmic::select_client({
                .command_line_client = std::nullopt,
                .environment_directory = environment_directory,
                .saved_directory = saved_directory,
                .home_directory = home.path(),
            });
        require(
            environment_selection.source ==
                    plazmic::ClientSelectionSource::environment &&
                environment_selection.game_directory ==
                    environment_directory &&
                environment_selection.should_persist &&
                !environment_selection.scanned,
            "valid EQ_LEGENDS_DIR unexpectedly scanned or lost priority");

        const plazmic::ClientSelection saved_selection =
            plazmic::select_client({
                .command_line_client = std::nullopt,
                .environment_directory = home.path() / "missing",
                .saved_directory = saved_directory,
                .home_directory = home.path(),
            });
        require(
            saved_selection.source ==
                    plazmic::ClientSelectionSource::settings &&
                saved_selection.game_directory == saved_directory &&
                !saved_selection.should_persist &&
                !saved_selection.scanned,
            "valid saved directory unexpectedly triggered a scan");

        TemporaryHome scan_home;
        const std::filesystem::path scanned_directory =
            scan_home.path() / "games/everquest/prefix/drive_c/users/Public" /
            "Daybreak Game Company/Installed Games/EverQuest Legends";
        create_client(scanned_directory);
        const plazmic::ClientSelection scanned_selection =
            plazmic::select_client({
                .command_line_client = std::nullopt,
                .environment_directory = std::nullopt,
                .saved_directory = std::nullopt,
                .home_directory = scan_home.path(),
            });
        require(
            scanned_selection.source ==
                    plazmic::ClientSelectionSource::scan &&
                scanned_selection.game_directory ==
                    std::filesystem::weakly_canonical(
                        scanned_directory) &&
                scanned_selection.should_persist &&
                scanned_selection.scanned,
            "bounded home scan did not find the Legends directory");

        TemporaryHome nested_home;
        const std::filesystem::path nested_directory =
            nested_home.path() / "custom/prefix/users/Public" /
            "Daybreak Game Company/Installed Games/EverQuest Legends";
        create_client(nested_directory);
        const auto nested =
            plazmic::discover_legends_directories(nested_home.path());
        require(
            nested.size() == 1U &&
                nested.front() ==
                    std::filesystem::weakly_canonical(nested_directory),
            "general user-directory scan missed the exact folder structure");

        TemporaryHome ambiguous_home;
        create_client(
            ambiguous_home.path() / "games/one/drive_c/users/Public" /
            "Daybreak Game Company/Installed Games/EverQuest Legends");
        create_client(
            ambiguous_home.path() / "Games/two/drive_c/users/Public" /
            "Daybreak Game Company/Installed Games/EverQuest Legends");
        const plazmic::ClientSelection ambiguous =
            plazmic::select_client({
                .command_line_client = std::nullopt,
                .environment_directory = std::nullopt,
                .saved_directory = std::nullopt,
                .home_directory = ambiguous_home.path(),
            });
        require(
            !ambiguous &&
                ambiguous.detail.find("Multiple") != std::string::npos,
            "ambiguous discovered installations did not fail closed");

        TemporaryHome missing_home;
        const std::filesystem::path missing_environment =
            missing_home.path() / "configured-but-missing";
        const plazmic::ClientSelection missing =
            plazmic::select_client({
                .command_line_client = std::nullopt,
                .environment_directory = missing_environment,
                .saved_directory = std::nullopt,
                .home_directory = missing_home.path(),
            });
        require(
            missing.source ==
                    plazmic::ClientSelectionSource::environment &&
                missing.game_directory ==
                    std::filesystem::absolute(missing_environment) &&
                !missing.should_persist && missing.scanned,
            "invalid environment directory was persisted or not scanned");

        std::cout
            << "client selection precedence and bounded discovery passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
