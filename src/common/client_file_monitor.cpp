#include "common/client_file_monitor.h"

#include "common/sha256.h"

#include <exception>
#include <system_error>
#include <utility>

namespace plazmic {

ClientFileMonitor::ClientFileMonitor(
    std::filesystem::path client,
    std::string expected_sha256,
    std::chrono::steady_clock::duration rehash_interval)
    : client_(std::move(client)),
      expected_sha256_(std::move(expected_sha256)),
      metadata_(read_metadata()),
      rehash_interval_(rehash_interval),
      next_hash_check_(
          std::chrono::steady_clock::now() + rehash_interval_) {}

std::optional<ClientFileMonitor::Metadata>
ClientFileMonitor::read_metadata() const {
    if (client_.empty()) {
        return std::nullopt;
    }
    std::error_code error;
    const std::uintmax_t size =
        std::filesystem::file_size(client_, error);
    if (error) {
        return std::nullopt;
    }
    const auto modified =
        std::filesystem::last_write_time(client_, error);
    if (error) {
        return std::nullopt;
    }
    return Metadata{
        .size = size,
        .modified = modified,
    };
}

ClientFileCheck ClientFileMonitor::check() {
    if (failed_) {
        return *failed_;
    }
    const auto current = read_metadata();
    if (!current) {
        failed_ = ClientFileCheck{
            .state = ClientFileState::unavailable,
            .detail =
                "Client executable is no longer available; restart after "
                "restoring the configured file",
        };
        return *failed_;
    }
    const auto now = std::chrono::steady_clock::now();
    if (metadata_ && *metadata_ == *current &&
        now < next_hash_check_) {
        return {
            .state = ClientFileState::exact,
            .detail = "Client executable metadata is unchanged",
        };
    }

    std::string digest;
    try {
        digest = sha256_file(client_);
    } catch (const std::exception&) {
        failed_ = ClientFileCheck{
            .state = ClientFileState::unavailable,
            .detail =
                "Client executable cannot be revalidated; restart after "
                "restoring the configured file",
        };
        return *failed_;
    }
    if (digest != expected_sha256_) {
        failed_ = ClientFileCheck{
            .state = ClientFileState::changed,
            .detail =
                "Client executable changed; create and validate a new "
                "compatibility profile before relaunching",
        };
        return *failed_;
    }
    metadata_ = current;
    next_hash_check_ = now + rehash_interval_;
    return {
        .state = ClientFileState::exact,
        .detail = "Client executable still matches the exact profile",
    };
}

}  // namespace plazmic
