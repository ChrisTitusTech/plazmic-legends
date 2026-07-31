#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace plazmic {

enum class ClientSelectionSource {
    none,
    command_line,
    environment,
    settings,
    scan,
};

struct ClientSelectionRequest {
    std::optional<std::filesystem::path> command_line_client;
    std::optional<std::filesystem::path> environment_directory;
    std::optional<std::filesystem::path> saved_directory;
    std::filesystem::path home_directory;
};

struct ClientSelection {
    std::filesystem::path client;
    std::filesystem::path game_directory;
    ClientSelectionSource source{ClientSelectionSource::none};
    bool should_persist{false};
    bool scanned{false};
    std::string detail;

    [[nodiscard]] explicit operator bool() const {
        return !client.empty();
    }
};

[[nodiscard]] std::vector<std::filesystem::path>
discover_legends_directories(const std::filesystem::path& home_directory);

[[nodiscard]] ClientSelection select_client(
    const ClientSelectionRequest& request);

}  // namespace plazmic
