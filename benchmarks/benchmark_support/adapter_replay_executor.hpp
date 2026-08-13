#pragma once

// Production adapter replay executor (OD-M5-P6-015/024): preconstructed valid
// wire -> production M4 adaptation -> checked production M3 invocation ->
// minimal result/checksum consumption. Excluded from the timed region: wire
// construction, fixture parsing, file I/O, hashing, generator work, reference
// model, ReplayDriver, OperationObservation, checkpoint comparison, and
// diagnostic formatting. Snapshot production is included without protobuf
// serialization (serialization is measured separately in
// M4/SerializeSnapshot).

#include "adapter_wire_support.hpp"
#include "replay_checksum.hpp"
#include "replay_types.hpp"

#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace bmd_projection::m5::benchmark {

class AdapterReplayExecutor final {
  public:
    AdapterReplayExecutor(const replay::ReplayFixture& fixture,
                          std::vector<adapter_support::PreconstructedEntry> entries);

    [[nodiscard]] std::uint64_t execute_event(core::BookProjection& projection,
                                              std::size_t event_index,
                                              std::uint64_t checksum) const;

    [[nodiscard]] static std::uint64_t finalize_checksum(const core::BookProjection& projection,
                                                         std::uint64_t checksum);

    [[nodiscard]] std::uint64_t run(core::BookProjection& projection) const;

    [[nodiscard]] std::size_t event_count() const noexcept { return entries_.size(); }
    [[nodiscard]] const replay::ReplayFixture& fixture() const noexcept { return *fixture_; }
    [[nodiscard]] core::NumericSpec numeric_spec() const noexcept { return numeric_spec_; }
    [[nodiscard]] core::SequencePolicyKind policy() const noexcept { return policy_; }

    [[nodiscard]] std::uint64_t expected_checksum() const noexcept { return expected_checksum_; }
    void set_expected_checksum(std::uint64_t value) noexcept { expected_checksum_ = value; }

  private:
    const replay::ReplayFixture* fixture_;
    std::vector<adapter_support::PreconstructedEntry> entries_{};
    core::NumericSpec numeric_spec_;
    core::SequencePolicyKind policy_;
    std::uint64_t expected_checksum_{0};
};

} // namespace bmd_projection::m5::benchmark
