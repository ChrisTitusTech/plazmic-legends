#include "common/sha256.h"
#include "integration/process_discovery.h"
#include "integration/process_reader.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>

#include <sys/types.h>
#include <unistd.h>

namespace {

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        const auto nonce =
            std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("plazmic-process-test-" + std::to_string(getpid()) + "-" +
                 std::to_string(nonce));
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

  private:
    std::filesystem::path path_;
};

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void write_text(const std::filesystem::path& path,
                const std::string& content) {
    std::ofstream output(path);
    output << content;
    if (!output) {
        throw std::runtime_error("cannot write test fixture");
    }
}

void add_process(const std::filesystem::path& proc_root,
                 int pid,
                 uid_t uid,
                 const std::filesystem::path& mapped_client,
                 std::string command = "eqgame.exe") {
    const auto process = proc_root / std::to_string(pid);
    std::filesystem::create_directories(process);
    write_text(process / "comm", command + "\n");
    write_text(process / "status",
               "Name:\t" + command + "\nUid:\t" + std::to_string(uid) +
                   "\t" + std::to_string(uid) + "\t" +
                   std::to_string(uid) + "\t" + std::to_string(uid) + "\n");
    write_text(process / "maps",
               "140000000-140001000 r--p 00000000 00:00 1 " +
                   mapped_client.string() + "\n"
                   "140001000-140002000 r-xp 00001000 00:00 1 " +
                   mapped_client.string() + "\n");
}

void test_sha256_vector(std::size_t size, const std::string& expected) {
    TemporaryDirectory temporary;
    const auto file = temporary.path() / "vector";
    write_text(file, std::string(size, 'a'));
    require(plazmic::sha256_file(file) == expected,
            "SHA-256 vector mismatch at size " + std::to_string(size));
}

void test_sha256() {
    test_sha256_vector(
        0, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    test_sha256_vector(
        55, "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318");
    test_sha256_vector(
        56, "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a");
    test_sha256_vector(
        63, "7d3e74a05d7db15bce4ad9ec0658ea98e3f06eeecf16b4c6fff2da457ddc2f34");
    test_sha256_vector(
        64, "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb");
    test_sha256_vector(
        65, "635361c48bb9eab14198e76ea8ab7f1a41685d6ad62aa9146d301d4f17eb0ae0");
}

void test_no_candidate() {
    TemporaryDirectory temporary;
    const auto client = temporary.path() / "eqgame.exe";
    write_text(client, "fixture");
    const plazmic::DiscoveryResult empty_result;
    require(!static_cast<bool>(empty_result),
            "default discovery result must be unsuccessful");
    const auto result =
        plazmic::discover_client_process(client, temporary.path(), getuid());
    require(result.error == plazmic::DiscoveryError::no_candidate,
            "empty proc tree must report no candidate");
}

void test_exact_candidate() {
    TemporaryDirectory temporary;
    const auto client = temporary.path() / "eqgame.exe";
    write_text(client, "fixture");
    add_process(temporary.path(), 101, getuid(), client);
    add_process(temporary.path(), 102, getuid(), client, "other.exe");
    add_process(temporary.path(), 103, getuid() + 1U, client);

    const auto result =
        plazmic::discover_client_process(client, temporary.path(), getuid());
    require(static_cast<bool>(result), "one exact candidate must succeed");
    require(result.process.pid == 101, "wrong process selected");
    require(result.process.image_base == 0x140000000U,
            "wrong image base selected");
    require(result.process.client_mappings.size() == 2U,
            "client mappings were not retained");
    require(result.process.mappings.size() == 2U,
            "complete process mappings were not retained");
}

void test_wrong_path_and_ambiguity() {
    TemporaryDirectory temporary;
    const auto client = temporary.path() / "eqgame.exe";
    const auto wrong = temporary.path() / "other" / "eqgame.exe";
    std::filesystem::create_directories(wrong.parent_path());
    write_text(client, "fixture");
    write_text(wrong, "fixture");
    add_process(temporary.path(), 201, getuid(), wrong);

    auto result =
        plazmic::discover_client_process(client, temporary.path(), getuid());
    require(result.error == plazmic::DiscoveryError::no_candidate,
            "same name at wrong path must be rejected");

    add_process(temporary.path(), 202, getuid(), client);
    add_process(temporary.path(), 203, getuid(), client);
    result =
        plazmic::discover_client_process(client, temporary.path(), getuid());
    require(result.error == plazmic::DiscoveryError::ambiguous_candidates,
            "multiple exact candidates must be rejected");
}

void test_deleted_mapping_is_rejected() {
    TemporaryDirectory temporary;
    const auto client = temporary.path() / "eqgame.exe";
    write_text(client, "fixture");
    add_process(temporary.path(), 301, getuid(),
                client.string() + " (deleted)");

    const auto result =
        plazmic::discover_client_process(client, temporary.path(), getuid());
    require(result.error == plazmic::DiscoveryError::no_candidate,
            "deleted executable mapping must be rejected");
}

void test_incomplete_eqgame_metadata_is_access_error() {
    TemporaryDirectory temporary;
    const auto client = temporary.path() / "eqgame.exe";
    write_text(client, "fixture");
    const auto process = temporary.path() / "401";
    std::filesystem::create_directories(process);
    write_text(process / "comm", "eqgame.exe\n");

    const auto result =
        plazmic::discover_client_process(client, temporary.path(), getuid());
    require(result.error == plazmic::DiscoveryError::proc_access,
            "incomplete eqgame metadata must report access failure");
}

void test_process_liveness() {
    require(plazmic::is_process_alive(getpid()),
            "current test process must be alive");
    require(!plazmic::is_process_alive(-1),
            "negative PID must not be alive");
}

plazmic::ClientProcess self_process_for_range(std::uintptr_t begin,
                                              std::size_t size,
                                              std::string permissions =
                                                  "r--p") {
    plazmic::ClientProcess process;
    process.pid = getpid();
    process.uid = getuid();
    process.command = "process_discovery_test";
    process.mappings.push_back({
        .begin = begin,
        .end = begin + size,
        .file_offset = 0,
        .permissions = std::move(permissions),
        .path = {},
    });
    return process;
}

void test_bounded_process_reader() {
    const std::array<std::byte, 8> source{
        std::byte{0x10}, std::byte{0x20}, std::byte{0x30}, std::byte{0x40},
        std::byte{0x50}, std::byte{0x60}, std::byte{0x70}, std::byte{0x80},
    };
    const auto address =
        reinterpret_cast<std::uintptr_t>(source.data());
    const auto process = self_process_for_range(address, source.size());
    const plazmic::ProcessMemoryReader reader(process);

    std::array<std::byte, 8> output{};
    auto result = reader.read_exact(address, output);
    require(static_cast<bool>(result), "bounded self-process read failed");
    require(output == source, "bounded self-process bytes differ");

    result = reader.read_exact(address + source.size() - 1U,
                               std::span<std::byte>(output).first(2));
    require(result.error ==
                plazmic::ProcessReadError::outside_readable_mapping,
            "cross-boundary read must be rejected");

    result = reader.read_exact(address, {});
    require(result.error == plazmic::ProcessReadError::invalid_request,
            "empty read must be rejected");

    result = reader.read_exact(
        std::numeric_limits<std::uintptr_t>::max() - 1U,
        std::span<std::byte>(output).first(2));
    require(result.error == plazmic::ProcessReadError::invalid_request,
            "overflowing read must be rejected");

    const auto unreadable_process =
        self_process_for_range(address, source.size(), "-w-p");
    const plazmic::ProcessMemoryReader unreadable_reader(unreadable_process);
    result = unreadable_reader.read_exact(address, output);
    require(result.error ==
                plazmic::ProcessReadError::outside_readable_mapping,
            "unreadable mapping must be rejected");

    auto wrong_uid_process = process;
    wrong_uid_process.uid = getuid() + 1U;
    const plazmic::ProcessMemoryReader wrong_uid_reader(wrong_uid_process);
    result = wrong_uid_reader.read_exact(address, output);
    require(result.error == plazmic::ProcessReadError::permission_denied,
            "cross-UID read must be rejected");

    auto missing_process = process;
    missing_process.pid = std::numeric_limits<pid_t>::max();
    const plazmic::ProcessMemoryReader missing_reader(missing_process);
    output.fill(std::byte{0xff});
    result = missing_reader.read_exact(address, output);
    require(result.error == plazmic::ProcessReadError::process_unavailable,
            "missing process must report unavailable");
    require(std::ranges::all_of(output, [](std::byte value) {
                return value == std::byte{0};
            }),
            "failed kernel read must clear the destination");
}

template <typename Value, std::size_t Size>
void write_little_endian(std::array<std::byte, Size>& bytes,
                         std::size_t offset,
                         Value value) {
    for (std::size_t index = 0; index < sizeof(Value); ++index) {
        bytes[offset + index] = static_cast<std::byte>(
            value >> static_cast<unsigned int>(index * 8U));
    }
}

void test_remote_pe_identity_reader() {
    std::array<std::byte, 512> image{};
    image[0] = std::byte{'M'};
    image[1] = std::byte{'Z'};
    write_little_endian(image, 0x3c, std::uint32_t{0x80});
    image[0x80] = std::byte{'P'};
    image[0x81] = std::byte{'E'};
    write_little_endian(image, 0x84, std::uint16_t{0x8664});
    write_little_endian(image, 0x88, std::uint32_t{0x6a6a2851});
    write_little_endian(image, 0x94, std::uint16_t{0xf0});
    write_little_endian(image, 0x98, std::uint16_t{0x20b});
    write_little_endian(image, 0xb0, std::uint64_t{0x140000000ULL});
    write_little_endian(image, 0xd0, std::uint32_t{0x16c1000});

    const auto address =
        reinterpret_cast<std::uintptr_t>(image.data());
    const auto process = self_process_for_range(address, image.size());
    const plazmic::ProcessMemoryReader reader(process);
    plazmic::RemotePeIdentity identity{};
    std::string error;
    require(plazmic::read_remote_pe_identity(reader, address, identity, error),
            "remote PE fixture read failed: " + error);
    require(address != identity.image_base,
            "relocated mapping was confused with preferred PE image base");
    require(identity.machine == 0x8664U, "remote PE machine differs");
    require(identity.timestamp == 0x6a6a2851U,
            "remote PE timestamp differs");
    require(identity.optional_magic == 0x20bU,
            "remote PE optional magic differs");
    require(identity.image_base == 0x140000000ULL,
            "remote PE image-base field differs");
    require(identity.image_size == 0x16c1000U,
            "remote PE image size differs");

    image[0x80] = std::byte{'X'};
    error.clear();
    require(!plazmic::read_remote_pe_identity(reader, address, identity, error),
            "invalid PE signature must be rejected");
}

}  // namespace

int main() {
    try {
        test_sha256();
        test_no_candidate();
        test_exact_candidate();
        test_wrong_path_and_ambiguity();
        test_deleted_mapping_is_rejected();
        test_incomplete_eqgame_metadata_is_access_error();
        test_process_liveness();
        test_bounded_process_reader();
        test_remote_pe_identity_reader();
        std::cout << "process discovery tests passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "test failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
