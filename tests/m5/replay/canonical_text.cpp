#include "canonical_text.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <utility>

namespace bmd_projection::m5::replay {
namespace {

[[nodiscard]] ParseError error(ErrorCategory category, std::size_t line, std::size_t event,
                               std::size_t token, std::string message) {
    return {category, line, event, token, std::move(message)};
}

[[nodiscard]] bool valid_utf8(std::string_view bytes) {
    std::size_t index = 0;
    while (index < bytes.size()) {
        const auto first = static_cast<unsigned char>(bytes[index]);
        std::size_t length = 0;
        std::uint32_t code_point = 0;
        if (first <= 0x7fU) {
            ++index;
            continue;
        }
        if (first >= 0xc2U && first <= 0xdfU) {
            length = 2;
            code_point = first & 0x1fU;
        } else if (first >= 0xe0U && first <= 0xefU) {
            length = 3;
            code_point = first & 0x0fU;
        } else if (first >= 0xf0U && first <= 0xf4U) {
            length = 4;
            code_point = first & 0x07U;
        } else {
            return false;
        }
        if (index + length > bytes.size()) {
            return false;
        }
        for (std::size_t offset = 1; offset < length; ++offset) {
            const auto continuation = static_cast<unsigned char>(bytes[index + offset]);
            if ((continuation & 0xc0U) != 0x80U) {
                return false;
            }
            code_point = (code_point << 6U) | (continuation & 0x3fU);
        }
        if ((length == 3 && code_point < 0x800U) || (length == 4 && code_point < 0x10000U) ||
            code_point > 0x10ffffU || (code_point >= 0xd800U && code_point <= 0xdfffU)) {
            return false;
        }
        index += length;
    }
    return true;
}

[[nodiscard]] bool is_forbidden_control(unsigned char value) {
    return value == 0 || value == '\t' || value == '\r' || value == '\n';
}

[[nodiscard]] std::string hex_digest(const std::array<std::uint32_t, 8>& state) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const auto word : state) {
        output << std::setw(8) << word;
    }
    return output.str();
}

} // namespace

Result<std::monostate> validate_canonical_bytes(std::string_view bytes) {
    if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xefU &&
        static_cast<unsigned char>(bytes[1]) == 0xbbU &&
        static_cast<unsigned char>(bytes[2]) == 0xbfU) {
        return error(ErrorCategory::InvalidCanonicalBytes, 1, 0, 0, "UTF-8 BOM is forbidden");
    }
    if (!valid_utf8(bytes)) {
        return error(ErrorCategory::InvalidCanonicalBytes, 0, 0, 0, "invalid UTF-8");
    }
    if (bytes.empty() || bytes.back() != '\n') {
        return error(ErrorCategory::InvalidCanonicalBytes, 0, 0, 0,
                     "canonical text requires a final LF");
    }

    std::size_t line_number = 1;
    std::size_t line_start = 0;
    while (line_start < bytes.size()) {
        const auto line_end = bytes.find('\n', line_start);
        const auto end = line_end == std::string_view::npos ? bytes.size() : line_end;
        const auto line = bytes.substr(line_start, end - line_start);
        if (line.empty()) {
            return error(ErrorCategory::InvalidCanonicalBytes, line_number, 0, 0,
                         "blank lines are forbidden");
        }
        if (line.front() == ' ' || line.back() == ' ') {
            return error(ErrorCategory::InvalidCanonicalBytes, line_number, 0, 0,
                         "leading and trailing whitespace are forbidden");
        }
        if (line.find("  ") != std::string_view::npos) {
            return error(ErrorCategory::InvalidCanonicalBytes, line_number, 0, 0,
                         "token separator must be exactly one ASCII space");
        }
        for (const auto character : line) {
            if (is_forbidden_control(static_cast<unsigned char>(character))) {
                return error(ErrorCategory::InvalidCanonicalBytes, line_number, 0, 0,
                             "tabs, carriage returns, and control whitespace are forbidden");
            }
        }
        if (line_end == std::string_view::npos) {
            break;
        }
        line_start = line_end + 1;
        ++line_number;
    }
    return std::monostate{};
}

Result<std::string> sha256_hex(std::string_view bytes) {
    // This is the SHA-256 algorithm from FIPS 180-4, kept test-only so Core has no crypto
    // dependency.
    constexpr std::array<std::uint32_t, 64> round_constants = {
        0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U,
        0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU,
        0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU,
        0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
        0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
        0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
        0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U,
        0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
        0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U,
        0xc67178f2U,
    };
    std::array<std::uint32_t, 8> state = {0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
                                          0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
    std::string padded(bytes);
    padded.push_back(static_cast<char>(0x80));
    while ((padded.size() % 64U) != 56U) {
        padded.push_back('\0');
    }
    const auto bit_count = static_cast<std::uint64_t>(bytes.size()) * 8U;
    for (int shift = 56; shift >= 0; shift -= 8) {
        padded.push_back(static_cast<char>((bit_count >> shift) & 0xffU));
    }

    for (std::size_t block = 0; block < padded.size(); block += 64) {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t index = 0; index < 16; ++index) {
            const auto offset = block + index * 4;
            words.at(index) =
                (static_cast<std::uint32_t>(static_cast<unsigned char>(padded[offset])) << 24U) |
                (static_cast<std::uint32_t>(static_cast<unsigned char>(padded[offset + 1]))
                 << 16U) |
                (static_cast<std::uint32_t>(static_cast<unsigned char>(padded[offset + 2])) << 8U) |
                static_cast<std::uint32_t>(static_cast<unsigned char>(padded[offset + 3]));
        }
        for (std::size_t index = 16; index < words.size(); ++index) {
            const auto sigma0 = std::rotr(words.at(index - 15), 7U) ^
                                std::rotr(words.at(index - 15), 18U) ^ (words.at(index - 15) >> 3U);
            const auto sigma1 = std::rotr(words.at(index - 2), 17U) ^
                                std::rotr(words.at(index - 2), 19U) ^ (words.at(index - 2) >> 10U);
            words.at(index) = words.at(index - 16) + sigma0 + words.at(index - 7) + sigma1;
        }
        auto a = state[0];
        auto b = state[1];
        auto c = state[2];
        auto d = state[3];
        auto e = state[4];
        auto f = state[5];
        auto g = state[6];
        auto h = state[7];
        for (std::size_t index = 0; index < words.size(); ++index) {
            const auto sigma1 = std::rotr(e, 6U) ^ std::rotr(e, 11U) ^ std::rotr(e, 25U);
            const auto choice = (e & f) ^ ((~e) & g);
            const auto temp1 = h + sigma1 + choice + round_constants.at(index) + words.at(index);
            const auto sigma0 = std::rotr(a, 2U) ^ std::rotr(a, 13U) ^ std::rotr(a, 22U);
            const auto majority = (a & b) ^ (a & c) ^ (b & c);
            const auto temp2 = sigma0 + majority;
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
    return hex_digest(state);
}

bool is_canonical_integer(std::string_view token) {
    if (token.empty() || (token.size() > 1 && token.front() == '0')) {
        return false;
    }
    return std::ranges::all_of(
        token, [](const char character) { return character >= '0' && character <= '9'; });
}

Result<std::uint64_t> parse_uint64(std::string_view token, std::size_t line, std::size_t event,
                                   std::size_t field) {
    if (!is_canonical_integer(token)) {
        return error(ErrorCategory::ReplaySyntax, line, event, field,
                     "unsigned integer is not canonically spelled");
    }
    std::uint64_t value{};
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size()) {
        return error(ErrorCategory::ReplaySyntax, line, event, field,
                     "unsigned integer is out of range");
    }
    return value;
}

Result<std::uint32_t> parse_uint32(std::string_view token, std::size_t line, std::size_t event,
                                   std::size_t field) {
    const auto parsed = parse_uint64(token, line, event, field);
    if (std::holds_alternative<ParseError>(parsed)) {
        return std::get<ParseError>(parsed);
    }
    const auto value = std::get<std::uint64_t>(parsed);
    if (value > UINT32_MAX) {
        return error(ErrorCategory::ReplaySyntax, line, event, field,
                     "unsigned integer exceeds uint32 range");
    }
    return static_cast<std::uint32_t>(value);
}

} // namespace bmd_projection::m5::replay
