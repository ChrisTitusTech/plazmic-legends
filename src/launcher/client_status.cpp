#include "launcher/client_status.h"

#include "common/sha256.h"
#include "game/client_profile_validation.h"
#include "integration/process_discovery.h"

#include <exception>
#include <utility>

namespace plazmic {

ClientStatusProbe::ClientStatusProbe(std::filesystem::path client)
    : client_(std::move(client)) {
    if (client_.empty()) {
        identity_status_ = {
            .compatibility = CompatibilityState::not_configured,
            .process = ProcessState::unavailable,
            .profile = "none",
            .detail =
                "Set --client, EQ_LEGENDS_DIR, or "
                "[client].game_directory",
            .pid = std::nullopt,
        };
        return;
    }

    try {
        digest_ = sha256_file(client_);
    } catch (const std::exception& error) {
        identity_status_ = {
            .compatibility = CompatibilityState::client_error,
            .process = ProcessState::unavailable,
            .profile = "none",
            .detail = error.what(),
            .pid = std::nullopt,
        };
        return;
    }

    profile_ = select_client_profile(digest_);
    if (profile_ == nullptr) {
        identity_status_ = {
            .compatibility = CompatibilityState::unsupported,
            .process = ProcessState::unavailable,
            .profile = "none",
            .detail = "Client SHA-256 does not match a supported profile",
            .pid = std::nullopt,
        };
        return;
    }
    if (!validate_known_client_profiles().empty() ||
        !validate_client_profile(*profile_)) {
        profile_ = nullptr;
        identity_status_ = {
            .compatibility = CompatibilityState::unsupported,
            .process = ProcessState::unavailable,
            .profile = "none",
            .detail = "Compiled compatibility profile failed validation",
            .pid = std::nullopt,
        };
        return;
    }
    file_monitor_.emplace(client_, digest_);

    identity_status_ = {
        .compatibility = CompatibilityState::supported,
        .process = ProcessState::not_running,
        .profile = std::string(profile_->id),
        .detail = "Supported client is not running",
        .pid = std::nullopt,
    };
}

StatusSnapshot ClientStatusProbe::refresh() {
    if (profile_ == nullptr) {
        return identity_status_;
    }
    const ClientFileCheck file = file_monitor_->check();
    if (!file) {
        return {
            .compatibility =
                file.state == ClientFileState::changed
                    ? CompatibilityState::unsupported
                    : CompatibilityState::client_error,
            .process = ProcessState::unavailable,
            .profile = "none",
            .detail = file.detail,
            .pid = std::nullopt,
        };
    }

    const auto discovery = discover_client_process(client_);
    if (discovery) {
        return {
            .compatibility = CompatibilityState::supported,
            .process = ProcessState::running,
            .profile = std::string(profile_->id),
            .detail = "Exact Wine-hosted client selected",
            .pid = discovery.process.pid,
        };
    }

    StatusSnapshot result = identity_status_;
    result.detail = discovery.detail;
    switch (discovery.error) {
        case DiscoveryError::no_candidate:
            result.process = ProcessState::not_running;
            break;
        case DiscoveryError::ambiguous_candidates:
            result.process = ProcessState::ambiguous;
            break;
        case DiscoveryError::proc_access:
            result.process = ProcessState::access_error;
            break;
        case DiscoveryError::none:
            result.process = ProcessState::access_error;
            break;
    }
    return result;
}

}  // namespace plazmic
