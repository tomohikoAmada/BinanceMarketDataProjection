#pragma once

// Deterministic replay result consumption (OD-M5-P6-045): FNV-1a 64 over the
// per-event disposition/status/sequence evidence and the final projection
// state. The methodology version is recorded in the metadata wrapper so a
// validator can confirm the checksum contract without recomputing it.

#include <cstdint>
#include <string_view>

namespace bmd_projection::m5::benchmark {

inline constexpr std::string_view kReplayChecksumMethodology = "M5_PHASE6_REPLAY_CHECKSUM_V1";
inline constexpr std::uint64_t kReplayChecksumSeed = 14'695'981'039'346'656'037ULL;

// FNV-1a 64 over the little-endian byte representation of `value`.
[[nodiscard]] std::uint64_t replay_checksum_append(std::uint64_t state, std::uint64_t value);

} // namespace bmd_projection::m5::benchmark
