#pragma once

// First-divergence comparison and layer attribution for OperationObservation.
//
// The comparison order is fixed by the M5 design:
//   1. successful/error decimal parse evidence
//   2. operation-result kind
//   3. operation-result value/error fields
//   4. post-operation SemanticCheckpoint
//   5. snapshot semantic observation
// Comparison stops at the first mismatch. Attribution names the earliest layer
// whose independently observable semantic result differs: R1 (decimal), R2 (book
// content), R3 (sequence/lifecycle), R4 (adapter/snapshot), D (composition).

#include "operation_observation.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bmd_projection::m5::oracle {

struct Divergence final {
    Divergence() = default;
    Divergence(std::size_t event_index_value, replay::EventKind event_kind_value, Layer layer_value,
               DivergenceCategory category_value, std::string detail_value,
               std::string production_value_text, std::string reference_value_text,
               std::string fixture_identity_value, std::string source_line_value)
        : event_index{event_index_value}, event_kind{event_kind_value}, layer{layer_value},
          category{category_value}, detail{std::move(detail_value)},
          production_value{std::move(production_value_text)},
          reference_value{std::move(reference_value_text)},
          fixture_identity{std::move(fixture_identity_value)},
          source_line{std::move(source_line_value)} {}

    std::size_t event_index{};
    replay::EventKind event_kind{};
    Layer layer{};
    DivergenceCategory category{};
    std::string detail;
    std::string production_value;
    std::string reference_value;
    std::string fixture_identity;
    std::string source_line;
    std::size_t source_line_number{};
    std::string fixture_id;
    std::string replay_log_sha256;
    std::string production_checkpoint;
    std::string reference_checkpoint;
    std::uint32_t price_scale{};
    std::uint32_t quantity_scale{};
    std::string policy;
    std::string market;
    std::string symbol;
    std::vector<std::pair<std::string, std::string>> provenance;

    friend bool operator==(const Divergence&, const Divergence&) = default;
};

[[nodiscard]] std::string_view to_text(Layer layer) noexcept;
[[nodiscard]] std::string_view to_text(DivergenceCategory category) noexcept;
[[nodiscard]] std::string_view to_text(CanonicalDisposition disposition) noexcept;
[[nodiscard]] std::string_view to_text(CanonicalStatus status) noexcept;
[[nodiscard]] std::string_view to_text(CanonicalGapReason reason) noexcept;
[[nodiscard]] std::string_view to_text(CanonicalPolicy policy) noexcept;
[[nodiscard]] std::string_view to_text(CanonicalVenue venue) noexcept;
[[nodiscard]] std::string_view to_text(CanonicalMarket market) noexcept;
[[nodiscard]] std::string_view to_text(CanonicalDecimalError error) noexcept;
[[nodiscard]] std::string_view to_text(CanonicalBookSide side) noexcept;
[[nodiscard]] std::string_view to_text(CanonicalDecimalRole role) noexcept;
[[nodiscard]] std::string_view to_text(CanonicalAdapterCode code) noexcept;
[[nodiscard]] std::string_view to_text(CanonicalAdapterField field) noexcept;
[[nodiscard]] std::string_view to_text(CanonicalQualityFlag flag) noexcept;
[[nodiscard]] std::string_view to_text(CanonicalSnapshotSource source) noexcept;
[[nodiscard]] std::string_view to_text(CanonicalResyncState state) noexcept;
[[nodiscard]] std::string_view to_text(CanonicalReasonCode reason) noexcept;
[[nodiscard]] std::string_view to_text(replay::EventKind kind) noexcept;

// Deterministic canonical text used in divergence diagnostics. Not a digest input.
[[nodiscard]] std::string to_canonical_text(const OperationResult& result);
[[nodiscard]] std::string
to_canonical_text(const std::vector<CanonicalDecimalObservation>& observations);
[[nodiscard]] std::string to_canonical_text(const SemanticCheckpoint& checkpoint);
[[nodiscard]] std::string to_canonical_text(const SnapshotOutcome& snapshot);

// Adds fixture/source/checkpoint context after the fixed semantic comparator has
// selected the first mismatch. This does not participate in comparison.
void enrich_divergence(Divergence& divergence, const replay::ReplayFixture& fixture,
                       const replay::SourceLocation& source, const OperationObservation* production,
                       const OperationObservation* reference);

// Stable failure-only diagnostic text. It is deliberately not an
// OperationObservation stream and is never hashed as a semantic result.
[[nodiscard]] std::string render_divergence(const Divergence& divergence);

// Compares production and reference observations in the fixed order and returns the
// first divergence, or std::nullopt when they agree.
[[nodiscard]] std::optional<Divergence> compare_observations(const OperationObservation& production,
                                                             const OperationObservation& reference,
                                                             std::string_view fixture_identity,
                                                             const replay::SourceLocation& source);

} // namespace bmd_projection::m5::oracle
