#pragma once

// Lean M4 cell-identity declarations shared by the M4 workload-spec
// registration, the Phase-6 M4 benchmarks, and the Phase-7 M4 measurement
// executable. This header deliberately does NOT include the Protobuf adapter
// header: workload identity description must not depend on wire types. The
// definitions live in adapter_wire_support.cpp (which owns the protobuf
// dependency). adapter_wire_support.hpp aggregates this header for the
// wire-building consumers.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bmd_projection::m5::benchmark::adapter_support {

inline constexpr std::size_t kM4UpdateLevelCount = 10;

// One authoritative Spot DepthUpdate successor-range description shared by the
// M4 update-boundary families. The actual timed wire and the canonical
// generated-workload identity both derive from the same cell description so
// the two cannot silently drift (OD-M5-P6-021/023).
struct M4SpotDepthUpdateCell final {
    std::uint64_t first_update_id{};
    std::uint64_t final_update_id{};
    std::optional<std::uint64_t> previous_final_update_id{std::nullopt};
};

// AdaptDepthUpdate/Spot adapts the range shape at 1'000'001 (no Core apply).
inline constexpr M4SpotDepthUpdateCell kM4AdaptDepthUpdateCell{1'000'001, 1'000'001, std::nullopt};

// CheckedApply executes the true successor [1'000'002, 1'000'002] against a
// prepared projection synchronized at 1'000'001.
inline constexpr M4SpotDepthUpdateCell kM4CheckedApplyCell{1'000'002, 1'000'002, std::nullopt};

// The prepared synchronized state against which CheckedApply executes
// (build_synchronized_projection leaves the projection at base + 1).
inline constexpr std::uint64_t kM4CheckedApplyPreparedUpdateId = 1'000'001;

// Formal canonical sequence parameters of the locked CheckedApply successor
// cell. Derived from the same authoritative cell used to build the timed wire,
// so the registered canonical workload spec cannot silently drift from
// execution (OD-M5-P6-021/023/028; P6-FINAL-001).
[[nodiscard]] std::vector<std::pair<std::string, std::string>>
checked_apply_canonical_sequence_fields();

// Canonical M4 generated-workload description (m4_cell_v1) from which the
// generated-workload SHA-256 is derived. The update-boundary families build
// their update wire from the authoritative cell descriptions above.
[[nodiscard]] std::string m4_generated_workload_description(std::string_view family,
                                                            std::size_t depth);

} // namespace bmd_projection::m5::benchmark::adapter_support
