#include "integration/process_reader.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <limits>
#include <string>

#include <sys/uio.h>

namespace plazmic {
namespace {

template <typename Value>
Value decode_little_endian(const std::byte* bytes) {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < sizeof(Value); ++index) {
        value |= static_cast<std::uint64_t>(
                     std::to_integer<unsigned int>(bytes[index]))
                 << static_cast<unsigned int>(index * 8U);
    }
    return static_cast<Value>(value);
}

bool checked_add(std::uintptr_t base,
                 std::uintptr_t offset,
                 std::uintptr_t& result) {
    if (base > std::numeric_limits<std::uintptr_t>::max() - offset) {
        return false;
    }
    result = base + offset;
    return true;
}

template <std::size_t Size>
bool read_array(const ProcessMemoryReader& reader,
                std::uintptr_t address,
                std::array<std::byte, Size>& bytes,
                std::string& error) {
    const auto result = reader.read_exact(address, bytes);
    if (!result) {
        error = result.detail;
        return false;
    }
    return true;
}

}  // namespace

ProcessMemoryReader::ProcessMemoryReader(const ClientProcess& process)
    : pid_(process.pid), uid_(process.uid), mappings_(process.mappings) {}

bool ProcessMemoryReader::contains_readable_range(std::uintptr_t address,
                                                  std::size_t size) const {
    if (address == 0U || size == 0U ||
        address > std::numeric_limits<std::uintptr_t>::max() - size) {
        return false;
    }
    const std::uintptr_t end = address + size;
    return std::ranges::any_of(
        mappings_, [address, end](const ProcessMapping& mapping) {
            return !mapping.permissions.empty() &&
                   mapping.permissions.front() == 'r' &&
                   address >= mapping.begin && end <= mapping.end;
        });
}

ProcessReadResult ProcessMemoryReader::read_exact(
    std::uintptr_t address,
    std::span<std::byte> output) const {
    if (pid_ <= 0 || address == 0U || output.empty() ||
        output.size() >
            static_cast<std::size_t>(
                std::numeric_limits<ssize_t>::max()) ||
        address > std::numeric_limits<std::uintptr_t>::max() -
                      output.size()) {
        return {
            .error = ProcessReadError::invalid_request,
            .bytes_read = 0,
            .detail = "invalid process-memory read request",
        };
    }
    if (uid_ != getuid()) {
        return {
            .error = ProcessReadError::permission_denied,
            .bytes_read = 0,
            .detail = "process owner does not match the current user",
        };
    }
    if (!contains_readable_range(address, output.size())) {
        return {
            .error = ProcessReadError::outside_readable_mapping,
            .bytes_read = 0,
            .detail = "read falls outside one readable process mapping",
        };
    }

    iovec local{
        .iov_base = output.data(),
        .iov_len = output.size(),
    };
    iovec remote{
        .iov_base = reinterpret_cast<void*>(address),
        .iov_len = output.size(),
    };
    errno = 0;
    const ssize_t bytes = process_vm_readv(pid_, &local, 1, &remote, 1, 0);
    if (bytes < 0) {
        const int read_errno = errno;
        std::ranges::fill(output, std::byte{0});
        ProcessReadError read_error = ProcessReadError::system_error;
        if (read_errno == ESRCH) {
            read_error = ProcessReadError::process_unavailable;
        } else if (read_errno == EPERM || read_errno == EACCES) {
            read_error = ProcessReadError::permission_denied;
        }
        return {
            .error = read_error,
            .bytes_read = 0,
            .detail = "process_vm_readv failed: " +
                      std::string(std::strerror(read_errno)),
        };
    }

    const auto bytes_read = static_cast<std::size_t>(bytes);
    if (bytes_read != output.size()) {
        std::ranges::fill(output, std::byte{0});
        return {
            .error = ProcessReadError::short_read,
            .bytes_read = bytes_read,
            .detail = "process_vm_readv returned a short read",
        };
    }
    return {
        .error = ProcessReadError::none,
        .bytes_read = bytes_read,
        .detail = "exact read completed",
    };
}

bool read_remote_pe_identity(const ProcessMemoryReader& reader,
                             std::uintptr_t mapped_image_base,
                             RemotePeIdentity& identity,
                             std::string& error) {
    identity = {};
    std::array<std::byte, 64> dos_header{};
    if (!read_array(reader, mapped_image_base, dos_header, error)) {
        return false;
    }
    if (dos_header[0] != std::byte{'M'} ||
        dos_header[1] != std::byte{'Z'}) {
        error = "mapped image does not begin with the PE MZ signature";
        return false;
    }

    const std::uint32_t pe_offset =
        decode_little_endian<std::uint32_t>(&dos_header[0x3c]);
    if (pe_offset < dos_header.size() || pe_offset > 0x100000U) {
        error = "mapped PE header offset is outside the accepted range";
        return false;
    }

    std::uintptr_t pe_address = 0;
    if (!checked_add(mapped_image_base, pe_offset, pe_address)) {
        error = "mapped PE header address overflows";
        return false;
    }
    std::array<std::byte, 24> pe_header{};
    if (!read_array(reader, pe_address, pe_header, error)) {
        return false;
    }
    constexpr std::array<std::byte, 4> kPeSignature{
        std::byte{'P'}, std::byte{'E'}, std::byte{0}, std::byte{0}};
    if (!std::equal(kPeSignature.begin(), kPeSignature.end(),
                    pe_header.begin())) {
        error = "mapped image has an invalid PE signature";
        return false;
    }

    const std::uint16_t optional_size =
        decode_little_endian<std::uint16_t>(&pe_header[20]);
    if (optional_size < 60U) {
        error = "mapped PE optional header is too small";
        return false;
    }
    std::uintptr_t optional_address = 0;
    if (!checked_add(pe_address, pe_header.size(), optional_address)) {
        error = "mapped PE optional-header address overflows";
        return false;
    }
    std::array<std::byte, 64> optional_header{};
    if (!read_array(reader, optional_address, optional_header, error)) {
        return false;
    }

    identity = {
        .machine =
            decode_little_endian<std::uint16_t>(&pe_header[4]),
        .timestamp =
            decode_little_endian<std::uint32_t>(&pe_header[8]),
        .optional_magic =
            decode_little_endian<std::uint16_t>(&optional_header[0]),
        .image_base =
            decode_little_endian<std::uint64_t>(&optional_header[24]),
        .image_size =
            decode_little_endian<std::uint32_t>(&optional_header[56]),
    };
    if (identity.optional_magic != 0x20bU) {
        error = "mapped PE optional header is not PE32+";
        return false;
    }
    return true;
}

}  // namespace plazmic
