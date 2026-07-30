#include "game/client_profile.h"
#include "launcher/client_status.h"
#include "model/status_snapshot.h"

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

class TemporaryFile {
  public:
    TemporaryFile() {
        std::string pattern =
            (std::filesystem::temp_directory_path() /
             "plazmic-unsupported-XXXXXX")
                .string();
        const int descriptor = mkstemp(pattern.data());
        require(descriptor >= 0, "cannot create unsupported client fixture");
        close(descriptor);
        path_ = pattern;
        std::ofstream output(path_, std::ios::binary);
        output << "not a supported client";
        require(output.good(), "cannot write unsupported client fixture");
    }

    ~TemporaryFile() {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

  private:
    std::filesystem::path path_;
};

}  // namespace

int main() {
    try {
        const auto& profile = plazmic::legends_reference_profile();
        require(plazmic::select_client_profile(profile.sha256) == &profile,
                "reference profile did not select by exact digest");
        require(plazmic::select_client_profile("changed") == nullptr,
                "unknown digest unexpectedly selected");

        const plazmic::ClientStatusProbe unconfigured(
            std::filesystem::path{});
        const auto unconfigured_status = unconfigured.refresh();
        require(
            unconfigured_status.compatibility ==
                plazmic::CompatibilityState::not_configured,
            "empty path did not report not configured");
        require(unconfigured_status.process ==
                    plazmic::ProcessState::unavailable,
                "empty path unexpectedly attempted discovery");

        const TemporaryFile unsupported_file;
        const plazmic::ClientStatusProbe unsupported(unsupported_file.path());
        const auto unsupported_status = unsupported.refresh();
        require(unsupported_status.compatibility ==
                    plazmic::CompatibilityState::unsupported,
                "changed digest did not fail closed");
        require(unsupported_status.process ==
                    plazmic::ProcessState::unavailable,
                "unsupported client unexpectedly attempted discovery");
        require(!unsupported_status.pid,
                "unsupported client unexpectedly returned a PID");

        require(plazmic::compatibility_label(
                    plazmic::CompatibilityState::supported) == "Supported",
                "compatibility label mismatch");
        require(plazmic::process_label(plazmic::ProcessState::running) ==
                    "Running",
                "process label mismatch");
        std::cout << "client profile and status boundary passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
