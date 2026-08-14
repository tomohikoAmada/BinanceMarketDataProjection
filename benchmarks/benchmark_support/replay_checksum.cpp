#include "replay_checksum.hpp"

#include <cstdint>

namespace bmd_projection::m5::benchmark {

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
std::uint64_t replay_checksum_append(std::uint64_t state, std::uint64_t value) {
    for (int shift = 0; shift < 8; ++shift) {
        state ^= static_cast<std::uint8_t>(value >> (shift * 8));
        state *= 1'099'511'628'211ULL;
    }
    return state;
}

} // namespace bmd_projection::m5::benchmark
