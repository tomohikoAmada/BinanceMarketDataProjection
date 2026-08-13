#pragma once

// Production Core replay executor (OD-M5-P6-014 / OD-M5-P6-023 / OD-M5-P6-030).
//
// The timed production path is: preloaded normalized replay operations ->
// production M1 decimal parsing (the actual production host path for
// normalized text input) -> production M3 BookProjection operations -> minimal
// evidence capture. The event-latency bracket executes exactly one production
// event and captures only typed evidence (no hashing inside the bracket);
// checksum folding happens outside any timed bracket.

#include "replay_checksum.hpp"

#include "replay_types.hpp"

#include <binance_market_data/projection/v1/numeric/numeric_spec.hpp>
#include <binance_market_data/projection/v1/order_book/book_level.hpp>
#include <binance_market_data/projection/v1/order_book/level_update.hpp>
#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace bmd_projection::m5::benchmark {

namespace core = binance_market_data::projection::v1;

struct EventEvidence final {
    std::uint64_t kind{};
    std::uint64_t disposition{};
    std::uint64_t status{};
    std::uint64_t update_id{};

    friend constexpr bool operator==(const EventEvidence&, const EventEvidence&) noexcept = default;
};

// Folds one event's evidence into the FNV-1a checksum.
[[nodiscard]] std::uint64_t fold_evidence(std::uint64_t checksum, const EventEvidence& evidence);

// Folds the final projection state (status, sequence, book shape) into the
// checksum. Shared by the Core and Adapter replay executors.
[[nodiscard]] std::uint64_t finalize_projection_checksum(const core::BookProjection& projection,
                                                         std::uint64_t checksum);

class CoreReplayExecutor final {
  public:
    explicit CoreReplayExecutor(const replay::ReplayFixture& fixture);

    // Executes one production event against `projection` and returns the typed
    // evidence. All intermediate storage is preallocated at construction; the
    // measured execution performs no allocation and no hashing.
    [[nodiscard]] EventEvidence execute_event(core::BookProjection& projection,
                                              std::size_t event_index) const;

    // Folds the final projection state (status, sequence, book shape) into the
    // checksum.
    [[nodiscard]] std::uint64_t finalize_checksum(const core::BookProjection& projection,
                                                  std::uint64_t checksum) const;

    // Runs the entire workload against any projection, folding evidence.
    [[nodiscard]] std::uint64_t run(core::BookProjection& projection) const;

    [[nodiscard]] std::size_t event_count() const noexcept;
    [[nodiscard]] core::NumericSpec numeric_spec() const noexcept { return numeric_spec_; }
    [[nodiscard]] core::SequencePolicyKind policy() const noexcept { return policy_; }
    [[nodiscard]] const replay::ReplayFixture& fixture() const noexcept { return fixture_; }

    // Preflight result, computed once outside any measured region.
    [[nodiscard]] std::uint64_t expected_checksum() const noexcept { return expected_checksum_; }
    void set_expected_checksum(std::uint64_t value) noexcept { expected_checksum_ = value; }

  private:
    [[nodiscard]] EventEvidence execute_install(core::BookProjection& projection,
                                                const replay::InstallBaselineOp& operation) const;
    [[nodiscard]] EventEvidence execute_depth_update(core::BookProjection& projection,
                                                     const replay::DepthUpdateOp& operation) const;

    // Parses production levels into the preallocated scratch; returns the
    // first production parse error if any.
    [[nodiscard]] bool parse_levels(const std::vector<replay::LevelInput>& levels,
                                    std::size_t side_offset) const;

    const replay::ReplayFixture& fixture_;
    core::NumericSpec numeric_spec_;
    core::SequencePolicyKind policy_;
    // Preallocated scratch: no allocation inside the measured region.
    mutable std::vector<core::BookLevel> scratch_book_levels_;
    mutable std::vector<core::LevelUpdate> scratch_updates_;
    std::uint64_t expected_checksum_{0};
};

// Canonical workload identity of a replay fixture for metadata purposes:
// workload ID, generator schema/version, seed, canonical log SHA-256
// (generated-workload hash), event count, market, policy, NumericSpec.
[[nodiscard]] std::string replay_fixture_identity(const replay::ReplayFixture& fixture);

} // namespace bmd_projection::m5::benchmark
