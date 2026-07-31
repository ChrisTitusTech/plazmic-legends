#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>

namespace plazmic {

enum class ClientFileState {
    exact,
    changed,
    unavailable,
};

struct ClientFileCheck {
    ClientFileState state{ClientFileState::unavailable};
    std::string detail;

    [[nodiscard]] explicit operator bool() const {
        return state == ClientFileState::exact;
    }
};

class ClientFileMonitor {
  public:
    ClientFileMonitor(std::filesystem::path client,
                      std::string expected_sha256,
                      std::chrono::steady_clock::duration
                          rehash_interval = std::chrono::minutes(1));

    [[nodiscard]] ClientFileCheck check();

  private:
    struct Metadata {
        std::uintmax_t size{};
        std::filesystem::file_time_type modified;

        bool operator==(const Metadata&) const = default;
    };

    [[nodiscard]] std::optional<Metadata> read_metadata() const;

    std::filesystem::path client_;
    std::string expected_sha256_;
    std::optional<Metadata> metadata_;
    std::optional<ClientFileCheck> failed_;
    std::chrono::steady_clock::duration rehash_interval_;
    std::chrono::steady_clock::time_point next_hash_check_;
};

}  // namespace plazmic
