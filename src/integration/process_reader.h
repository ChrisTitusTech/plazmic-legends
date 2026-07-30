#pragma once

#include "integration/process_discovery.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <sys/types.h>

namespace plazmic {

enum class ProcessReadError {
    none,
    invalid_request,
    outside_readable_mapping,
    process_unavailable,
    permission_denied,
    system_error,
    short_read,
};

struct ProcessReadResult {
    ProcessReadError error{ProcessReadError::invalid_request};
    std::size_t bytes_read{};
    std::string detail;

    [[nodiscard]] explicit operator bool() const {
        return error == ProcessReadError::none;
    }
};

class ProcessMemoryReader {
  public:
    explicit ProcessMemoryReader(const ClientProcess& process);

    [[nodiscard]] ProcessReadResult read_exact(
        std::uintptr_t address,
        std::span<std::byte> output) const;

    [[nodiscard]] pid_t pid() const { return pid_; }

  private:
    [[nodiscard]] bool contains_readable_range(std::uintptr_t address,
                                               std::size_t size) const;

    pid_t pid_{};
    uid_t uid_{};
    std::vector<ProcessMapping> mappings_;
};

struct RemotePeIdentity {
    std::uint16_t machine{};
    std::uint32_t timestamp{};
    std::uint16_t optional_magic{};
    std::uint64_t image_base{};
    std::uint32_t image_size{};
};

[[nodiscard]] bool read_remote_pe_identity(
    const ProcessMemoryReader& reader,
    std::uintptr_t mapped_image_base,
    RemotePeIdentity& identity,
    std::string& error);

}  // namespace plazmic
