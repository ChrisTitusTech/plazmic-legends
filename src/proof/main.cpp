#include "common/sha256.h"
#include "game/client_profile.h"
#include "integration/process_discovery.h"
#include "integration/process_reader.h"
#include "overlay/x11_overlay.h"

#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace {

struct Arguments {
    std::filesystem::path client;
    std::filesystem::path proc_root{"/proc"};
    std::chrono::seconds duration{0};
    bool diagnose_only{false};
};

void usage(std::ostream& output) {
    output
        << "Usage: plazmic-legends-proof --client /path/to/eqgame.exe "
           "[--diagnose-only] [--duration SECONDS] [--proc-root PATH]\n";
}

std::optional<Arguments> parse_arguments(int argc, char** argv) {
    Arguments result;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--diagnose-only") {
            result.diagnose_only = true;
        } else if (argument == "--client" && index + 1 < argc) {
            result.client = argv[++index];
        } else if (argument == "--proc-root" && index + 1 < argc) {
            result.proc_root = argv[++index];
        } else if (argument == "--duration" && index + 1 < argc) {
            const std::string_view value(argv[++index]);
            long seconds = 0;
            const auto parsed = std::from_chars(
                value.data(), value.data() + value.size(), seconds);
            if (parsed.ec != std::errc{} ||
                parsed.ptr != value.data() + value.size() || seconds < 0) {
                return std::nullopt;
            }
            result.duration = std::chrono::seconds(seconds);
        } else {
            return std::nullopt;
        }
    }
    if (result.client.empty() ||
        result.client.filename() != "eqgame.exe") {
        return std::nullopt;
    }
    if (result.proc_root.lexically_normal() !=
            std::filesystem::path{"/proc"} &&
        !result.diagnose_only) {
        return std::nullopt;
    }
    return result;
}

int discovery_exit_code(plazmic::DiscoveryError error) {
    switch (error) {
        case plazmic::DiscoveryError::no_candidate:
            return 10;
        case plazmic::DiscoveryError::ambiguous_candidates:
            return 11;
        case plazmic::DiscoveryError::proc_access:
            return 12;
        case plazmic::DiscoveryError::none:
            return 0;
    }
    return 12;
}

}  // namespace

int main(int argc, char** argv) {
    const auto arguments = parse_arguments(argc, argv);
    if (!arguments) {
        usage(std::cerr);
        return 2;
    }

    std::string digest;
    try {
        digest = plazmic::sha256_file(arguments->client);
    } catch (const std::exception& error) {
        std::cerr << "client error: " << error.what() << '\n';
        return 3;
    }
    const plazmic::ClientProfile* profile =
        plazmic::select_client_profile(digest);
    if (profile == nullptr) {
        std::cerr << "compatibility error: unsupported client SHA-256\n";
        return 4;
    }

    const auto discovery = plazmic::discover_client_process(
        arguments->client, arguments->proc_root);
    if (!discovery) {
        std::cerr << "discovery error: " << discovery.detail << '\n';
        return discovery_exit_code(discovery.error);
    }

    const bool using_live_proc =
        arguments->proc_root.lexically_normal() ==
        std::filesystem::path{"/proc"};
    plazmic::RemotePeIdentity remote_identity{};
    if (using_live_proc) {
        const plazmic::ProcessMemoryReader reader(discovery.process);
        std::string remote_error;
        if (!plazmic::read_remote_pe_identity(
                reader, discovery.process.image_base, remote_identity,
                remote_error)) {
            std::cerr << "access error: " << remote_error << '\n';
            return 13;
        }
        if (remote_identity.machine != profile->machine ||
            remote_identity.timestamp != profile->timestamp ||
            remote_identity.optional_magic != profile->optional_magic ||
            remote_identity.image_base != discovery.process.image_base ||
            remote_identity.image_size != profile->image_size) {
            std::cerr
                << "compatibility error: live PE identity does not match "
                   "the selected profile\n"
                << "actual_machine=0x" << std::hex
                << remote_identity.machine << '\n'
                << "actual_timestamp=0x" << remote_identity.timestamp << '\n'
                << "actual_optional_magic=0x"
                << remote_identity.optional_magic << '\n'
                << "actual_image_base=0x"
                << remote_identity.image_base << '\n'
                << "actual_image_size=0x" << remote_identity.image_size
                << std::dec << '\n';
            return 14;
        }
    }

    std::cout << "profile=" << profile->id << '\n'
              << "client_sha256=" << digest << '\n'
              << "pid=" << discovery.process.pid << '\n'
              << "uid=" << discovery.process.uid << '\n'
              << "image_base=0x" << std::hex << discovery.process.image_base
              << std::dec << '\n'
              << "client_mappings="
              << discovery.process.client_mappings.size() << '\n'
              << "process_mappings=" << discovery.process.mappings.size()
              << '\n'
              << "remote_pe_identity="
              << (using_live_proc ? "verified" : "fixture") << '\n';
    if (using_live_proc) {
        std::cout << "remote_machine=0x" << std::hex
                  << remote_identity.machine << '\n'
                  << "remote_timestamp=0x" << remote_identity.timestamp
                  << '\n'
                  << "remote_image_base=0x"
                  << remote_identity.image_base << '\n'
                  << "remote_image_size=0x" << remote_identity.image_size
                  << std::dec << '\n';
    }

    if (arguments->diagnose_only) {
        return 0;
    }
    const plazmic::OverlayOptions overlay_options{
        .target_pid = discovery.process.pid,
        .profile = std::string(profile->id),
        .duration = arguments->duration,
    };
    return plazmic::run_x11_overlay(overlay_options);
}
