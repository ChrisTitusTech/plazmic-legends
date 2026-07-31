#include "launcher/client_selection.h"

#include <algorithm>
#include <chrono>
#include <deque>
#include <ranges>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace plazmic {
namespace {

constexpr std::size_t kMaximumVisitedDirectories = 25000;
constexpr std::size_t kMaximumSearchDepth = 10;
constexpr std::size_t kMaximumWineUsers = 64;
constexpr auto kMaximumSearchDuration = std::chrono::seconds(1);

const std::filesystem::path kDaybreakRelativePath =
    std::filesystem::path("Daybreak Game Company") /
    "Installed Games" / "EverQuest Legends";

std::filesystem::path normalized_absolute(
    const std::filesystem::path& path) {
    if (path.empty()) {
        return {};
    }
    std::error_code error;
    std::filesystem::path result =
        std::filesystem::absolute(path, error);
    if (error) {
        result = path;
    }
    return result.lexically_normal();
}

bool is_eqgame(const std::filesystem::path& client) {
    if (client.filename() != "eqgame.exe") {
        return false;
    }
    std::error_code error;
    return std::filesystem::is_regular_file(client, error) && !error;
}

std::filesystem::path client_in(
    const std::filesystem::path& directory) {
    if (directory.empty()) {
        return {};
    }
    return normalized_absolute(directory) / "eqgame.exe";
}

void add_candidate(
    const std::filesystem::path& directory,
    std::vector<std::filesystem::path>& candidates,
    std::unordered_set<std::string>& candidate_keys) {
    const std::filesystem::path client = directory / "eqgame.exe";
    if (!is_eqgame(client)) {
        return;
    }
    std::error_code error;
    std::filesystem::path normalized =
        std::filesystem::weakly_canonical(directory, error);
    if (error) {
        normalized = normalized_absolute(directory);
    }
    const std::string key = normalized.native();
    if (candidate_keys.insert(key).second) {
        candidates.push_back(std::move(normalized));
    }
}

void check_drive_c(
    const std::filesystem::path& drive_c,
    std::vector<std::filesystem::path>& candidates,
    std::unordered_set<std::string>& candidate_keys,
    std::chrono::steady_clock::time_point deadline) {
    const std::filesystem::path users = drive_c / "users";
    std::error_code error;
    std::filesystem::directory_iterator iterator(
        users,
        std::filesystem::directory_options::skip_permission_denied,
        error);
    const std::filesystem::directory_iterator end;
    std::size_t checked_users = 0;
    while (!error && iterator != end &&
           checked_users < kMaximumWineUsers &&
           candidates.size() < 2U &&
           std::chrono::steady_clock::now() < deadline) {
        ++checked_users;
        std::error_code type_error;
        if (iterator->is_directory(type_error) && !type_error) {
            add_candidate(
                iterator->path() / kDaybreakRelativePath,
                candidates,
                candidate_keys);
        }
        iterator.increment(error);
    }
}

bool should_prune(const std::filesystem::path& directory) {
    const std::filesystem::path name = directory.filename();
    return name == ".cache" || name == ".git" ||
           name == "node_modules" || name == "build" ||
           name == "target" || name == "Trash";
}

struct PendingDirectory {
    std::filesystem::path path;
    std::size_t depth;
};

ClientSelection from_directory(
    const std::filesystem::path& directory,
    ClientSelectionSource source,
    bool should_persist,
    bool scanned) {
    const std::filesystem::path normalized =
        normalized_absolute(directory);
    return {
        .client = normalized / "eqgame.exe",
        .game_directory = normalized,
        .source = source,
        .should_persist = should_persist,
        .scanned = scanned,
        .detail = {},
    };
}

}  // namespace

std::vector<std::filesystem::path> discover_legends_directories(
    const std::filesystem::path& home_directory) {
    std::vector<std::filesystem::path> candidates;
    if (home_directory.empty()) {
        return candidates;
    }

    const std::filesystem::path home =
        normalized_absolute(home_directory);
    const std::vector<std::filesystem::path> search_roots{
        home / "games",
        home / "Games",
        home / ".wine",
        home / ".local/share/bottles/bottles",
        home / ".local/share/Steam/steamapps/compatdata",
        home / ".steam/steam/steamapps/compatdata",
        home / ".var/app/com.usebottles.bottles/data/bottles/bottles",
        home /
            ".var/app/com.valvesoftware.Steam/data/Steam/steamapps/compatdata",
        home / ".local/share/lutris",
        home / ".var/app/net.lutris.Lutris/data/lutris",
        home,
    };

    std::deque<PendingDirectory> pending;
    for (const auto& root : search_roots) {
        pending.push_back({.path = root, .depth = 0});
    }
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> candidate_keys;
    std::size_t visited_count = 0;
    const auto deadline =
        std::chrono::steady_clock::now() + kMaximumSearchDuration;

    while (!pending.empty() &&
           visited_count < kMaximumVisitedDirectories &&
           std::chrono::steady_clock::now() < deadline &&
           candidates.size() < 2U) {
        PendingDirectory current = std::move(pending.front());
        pending.pop_front();
        const std::filesystem::path normalized =
            normalized_absolute(current.path);
        if (!visited.insert(normalized.native()).second) {
            continue;
        }
        ++visited_count;

        std::error_code type_error;
        if (!std::filesystem::is_directory(normalized, type_error) ||
            type_error) {
            continue;
        }
        if (normalized.filename() == "drive_c") {
            check_drive_c(
                normalized, candidates, candidate_keys, deadline);
            continue;
        }
        if (normalized.filename() == "Daybreak Game Company") {
            add_candidate(
                normalized / "Installed Games" / "EverQuest Legends",
                candidates,
                candidate_keys);
            continue;
        }
        if (current.depth >= kMaximumSearchDepth ||
            should_prune(normalized)) {
            continue;
        }

        std::error_code iterator_error;
        std::filesystem::directory_iterator iterator(
            normalized,
            std::filesystem::directory_options::skip_permission_denied,
            iterator_error);
        const std::filesystem::directory_iterator end;
        while (!iterator_error && iterator != end &&
               candidates.size() < 2U &&
               std::chrono::steady_clock::now() < deadline &&
               visited_count + pending.size() <
                   kMaximumVisitedDirectories) {
            std::error_code entry_error;
            if (iterator->is_directory(entry_error) && !entry_error) {
                const std::filesystem::path child = iterator->path();
                if (child.filename() == "drive_c") {
                    check_drive_c(
                        child, candidates, candidate_keys, deadline);
                } else if (child.filename() ==
                           "Daybreak Game Company") {
                    add_candidate(
                        child / "Installed Games" /
                            "EverQuest Legends",
                        candidates,
                        candidate_keys);
                } else {
                    std::error_code symlink_error;
                    const bool is_symlink =
                        iterator->is_symlink(symlink_error);
                    if (!symlink_error && !is_symlink) {
                        pending.push_back({
                            .path = child,
                            .depth = current.depth + 1U,
                        });
                    }
                }
            }
            iterator.increment(iterator_error);
        }
    }

    std::ranges::sort(candidates);
    return candidates;
}

ClientSelection select_client(const ClientSelectionRequest& request) {
    if (request.command_line_client &&
        !request.command_line_client->empty()) {
        const std::filesystem::path client =
            normalized_absolute(*request.command_line_client);
        return {
            .client = client,
            .game_directory = client.parent_path(),
            .source = ClientSelectionSource::command_line,
            .should_persist = is_eqgame(client),
            .scanned = false,
            .detail = {},
        };
    }

    const std::filesystem::path environment_client =
        request.environment_directory
            ? client_in(*request.environment_directory)
            : std::filesystem::path{};
    if (is_eqgame(environment_client)) {
        return from_directory(
            environment_client.parent_path(),
            ClientSelectionSource::environment,
            true,
            false);
    }

    const std::filesystem::path saved_client =
        request.saved_directory
            ? client_in(*request.saved_directory)
            : std::filesystem::path{};
    if (is_eqgame(saved_client)) {
        return from_directory(
            saved_client.parent_path(),
            ClientSelectionSource::settings,
            false,
            false);
    }

    const std::vector<std::filesystem::path> discovered =
        discover_legends_directories(request.home_directory);
    if (discovered.size() == 1U) {
        return from_directory(
            discovered.front(),
            ClientSelectionSource::scan,
            true,
            true);
    }
    if (discovered.size() > 1U) {
        return {
            .client = {},
            .game_directory = {},
            .source = ClientSelectionSource::none,
            .should_persist = false,
            .scanned = true,
            .detail =
                "Multiple Legends installations found; set "
                "[client].game_directory",
        };
    }

    if (!environment_client.empty()) {
        return {
            .client = environment_client,
            .game_directory = environment_client.parent_path(),
            .source = ClientSelectionSource::environment,
            .should_persist = false,
            .scanned = true,
            .detail = {},
        };
    }
    if (!saved_client.empty()) {
        return {
            .client = saved_client,
            .game_directory = saved_client.parent_path(),
            .source = ClientSelectionSource::settings,
            .should_persist = false,
            .scanned = true,
            .detail = {},
        };
    }
    return {
        .client = {},
        .game_directory = {},
        .source = ClientSelectionSource::none,
        .should_persist = false,
        .scanned = true,
        .detail =
            "Set --client, EQ_LEGENDS_DIR, or "
            "[client].game_directory",
    };
}

}  // namespace plazmic
