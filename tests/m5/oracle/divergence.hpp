#pragma once

// First-divergence comparison and layer attribution for OperationObservation.
//
// The comparison order is fixed by the M5 design:
//   1. operation-result kind
//   2. operation-result value/error fields
//   3. post-operation SemanticCheckpoint
//   4. snapshot semantic observation
// Comparison stops at the first mismatch. Attribution names the earliest layer
// whose independently observable semantic result differs: R1 (decimal), R2 (book
// content), R3 (sequence/lifecycle), R4 (adapter/snapshot), D (composition).

#include "operation_observation.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace bmd_projection::m5::oracle {

struct Divergence final {
    std::size_t event_index{};
    replay::EventKind event_kind{};
    Layer layer{};
    DivergenceCategory category{};
    std::string detail;
    std::string production_value;
    std::string reference_value;
    std::string fixture_identity;
    std::string source_line;

    friend bool operator==(const Divergence&, const Divergence&) = default;
};

[[nodiscard]] std::string_view to_text(Layer layer) noexcept;
[[nodiscard]] std::string_view to_text(DivergenceCategory category) noexcept;
[[nodiscard]] std::string_view to_text(CanonicalDisposition disposition) noexcept;
[[nodiscard]] std::string_view to_text(CanonicalStatus status) noexcept;
[[nodiscard]] std::string_view to_text(CanonicalGapReason reason) noexcept;
[[nodiscard]] std::string_view to_text(CanonicalPolicy policy) noexcept;
[[nodiscard]] std::string_view to_text(CanonicalDecimalError error) noexcept;
[[nodiscard]] std::string_view to_text(CanonicalAdapterCode code) noexcept;
[[nodiscard]] std::string_view to_text(CanonicalAdapterField field) noexcept;
[[nodiscard]] std::string_view to_text(CanonicalQualityFlag flag) noexcept;
[[nodiscard]] std::string_view to_text(CanonicalSnapshotSource source) noexcept;
[[nodiscard]] std::string_view to_text(CanonicalResyncState state) noexcept;
[[nodiscard]] std::string_view to_text(CanonicalReasonCode reason) noexcept;
[[nodiscard]] std::string_view to_text(replay::EventKind kind) noexcept;

// Deterministic canonical text used in divergence diagnostics. Not a digest input.
[[nodiscard]] std::string to_canonical_text(const OperationResult& result);
[[nodiscard]] std::string to_canonical_text(const SemanticCheckpoint& checkpoint);
[[nodiscard]] std::string to_canonical_text(const SnapshotOutcome& snapshot);

// Compares production and reference observations in the fixed order and returns the
// first divergence, or std::nullopt when they agree.
[[nodiscard]] std::optional<Divergence> compare_observations(const OperationObservation& production,
                                                             const OperationObservation& reference,
                                                             std::string_view fixture_identity,
                                                             const replay::SourceLocation& source);

} // namespace bmd_projection::m5::oracle
