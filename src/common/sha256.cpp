#include "common/sha256.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace plazmic {
namespace {

using Block = std::array<std::uint8_t, 64>;
using State = std::array<std::uint32_t, 8>;

constexpr std::array<std::uint32_t, 64> kRoundConstants{
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU,
    0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U,
    0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U,
    0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U,
    0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
    0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
    0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U,
    0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U, 0x1e376c08U,
    0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU,
    0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
};

constexpr State kInitialState{
    0x6a09e667U,
    0xbb67ae85U,
    0x3c6ef372U,
    0xa54ff53aU,
    0x510e527fU,
    0x9b05688cU,
    0x1f83d9abU,
    0x5be0cd19U,
};

void transform(State& state, const Block& block) {
    std::array<std::uint32_t, 64> words{};
    for (std::size_t index = 0; index < 16; ++index) {
        const std::size_t offset = index * 4;
        words[index] =
            (static_cast<std::uint32_t>(block[offset]) << 24U) |
            (static_cast<std::uint32_t>(block[offset + 1]) << 16U) |
            (static_cast<std::uint32_t>(block[offset + 2]) << 8U) |
            static_cast<std::uint32_t>(block[offset + 3]);
    }
    for (std::size_t index = 16; index < words.size(); ++index) {
        const std::uint32_t s0 =
            std::rotr(words[index - 15], 7) ^
            std::rotr(words[index - 15], 18) ^
            (words[index - 15] >> 3U);
        const std::uint32_t s1 =
            std::rotr(words[index - 2], 17) ^
            std::rotr(words[index - 2], 19) ^
            (words[index - 2] >> 10U);
        words[index] =
            words[index - 16] + s0 + words[index - 7] + s1;
    }

    std::uint32_t a = state[0];
    std::uint32_t b = state[1];
    std::uint32_t c = state[2];
    std::uint32_t d = state[3];
    std::uint32_t e = state[4];
    std::uint32_t f = state[5];
    std::uint32_t g = state[6];
    std::uint32_t h = state[7];

    for (std::size_t index = 0; index < words.size(); ++index) {
        const std::uint32_t sigma1 =
            std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
        const std::uint32_t choose = (e & f) ^ ((~e) & g);
        const std::uint32_t temp1 =
            h + sigma1 + choose + kRoundConstants[index] + words[index];
        const std::uint32_t sigma0 =
            std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
        const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t temp2 = sigma0 + majority;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

}  // namespace

std::string sha256_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open client executable: " +
                                 path.string());
    }

    State state = kInitialState;
    Block block{};
    std::uint64_t byte_length = 0;
    std::size_t partial_size = 0;
    while (true) {
        block.fill(0U);
        input.read(reinterpret_cast<char*>(block.data()),
                   static_cast<std::streamsize>(block.size()));
        const std::streamsize count = input.gcount();
        if (count < 0) {
            throw std::runtime_error(
                "invalid read length for client executable: " +
                path.string());
        }
        const auto block_size = static_cast<std::size_t>(count);
        if (byte_length >
            std::numeric_limits<std::uint64_t>::max() - block_size) {
            throw std::runtime_error("client executable is too large to hash");
        }
        byte_length += static_cast<std::uint64_t>(block_size);
        if (block_size == block.size()) {
            transform(state, block);
            continue;
        }
        partial_size = block_size;
        if (!input.eof()) {
            throw std::runtime_error(
                "failed while reading client executable: " + path.string());
        }
        break;
    }
    if (byte_length > std::numeric_limits<std::uint64_t>::max() / 8U) {
        throw std::runtime_error("client executable is too large to hash");
    }

    block[partial_size] = 0x80U;
    if (partial_size >= 56U) {
        transform(state, block);
        block.fill(0U);
    }
    const std::uint64_t bit_length = byte_length * 8U;
    for (std::size_t index = 0; index < 8U; ++index) {
        const unsigned int shift =
            static_cast<unsigned int>((7U - index) * 8U);
        block[56U + index] =
            static_cast<std::uint8_t>(bit_length >> shift);
    }
    transform(state, block);

    std::ostringstream result;
    result << std::hex << std::setfill('0');
    for (const std::uint32_t value : state) {
        result << std::setw(8) << value;
    }
    return result.str();
}

}  // namespace plazmic
