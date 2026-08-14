// Adapter-conditional Phase-6 benchmark support tests: AdapterWireReplay
// preconstruction and final-state validation (OD-M5-P6-015/024).
// Compiled only when the ProtoAdapter is enabled.

#include "adapter_benchmark_test_support.hpp"

#include "adapter_wire_support.hpp"
#include "book_state.hpp"
#include "canonical_text.hpp"

#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>
#include <binance_market_data/projection_adapter/v1/proto_adapter.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <variant>

namespace {

namespace bm = bmd_projection::m5::benchmark;
namespace test_support = bmd_projection::m5::benchmark::test_support;
namespace wire_support = bmd_projection::m5::benchmark::adapter_support;
namespace adapter = binance_market_data::projection_adapter::v1;
namespace core = binance_market_data::projection::v1;

struct CheckedApplyObservation final {
    core::ApplyDisposition disposition{};
    std::optional<core::UpdateId> last_update_id_after;
};

// Independent execution of the actual CheckedApply successor event against a
// freshly prepared synchronized Spot projection. The wire ids are explicit
// literals, not the producer cell, so the observation cannot drift with it.
[[nodiscard]] CheckedApplyObservation checked_apply_observation(std::size_t depth) {
    auto projection = bm::build_synchronized_projection(core::SequencePolicyKind::Spot, depth);
    const auto identity = wire_support::benchmark_wire_identity();
    const auto wire = wire_support::make_update_wire(1'000'002, 1'000'002, std::nullopt);
    const auto adapted =
        adapter::adapt_depth_update(wire, identity.numeric_spec, identity.expected);
    if (!std::holds_alternative<adapter::AdaptedDepthBatch>(adapted)) {
        std::abort();
    }
    const auto& owner = std::get<adapter::AdaptedDepthBatch>(adapted);
    const auto applied = owner.apply_to(projection);
    if (!std::holds_alternative<core::ApplyResult>(applied)) {
        std::abort();
    }
    const auto& result = std::get<core::ApplyResult>(applied);
    return {result.disposition, result.last_update_id_after};
}

// Independent canonical description of that exact successor event, built from
// explicit literal fields rather than the producer description.
[[nodiscard]] std::string checked_apply_expected_description(std::size_t depth) {
    const auto wire_bytes =
        wire_support::make_update_wire(1'000'002, 1'000'002, std::nullopt).SerializeAsString();
    return "m4_cell_v1\nfamily=CheckedApply\ndepth=" + std::to_string(depth) + '\n' +
           "update_wire_" + std::to_string(wire_bytes.size()) + ':' + wire_bytes + '\n' +
           "initial_bids=" + bm::describe_levels(bm::build_bid_levels(depth)) + '\n' +
           "initial_asks=" + bm::describe_levels(bm::build_ask_levels(depth)) + '\n' +
           "initial_update_id=1000001\npolicy=Spot\n";
}

// Independent regression (REQ-002): the M4 CheckedApply generated-workload
// identity must describe the actual timed successor event. The prepared
// projection is synchronized at 1'000'001; the timed event is U = u =
// 1'000'002. The expected canonical description is constructed from explicit
// literals, not from the producer, so the historical defect (a 1'000'001
// provenance wire) fails this test.
TEST(Phase6M4CheckedApply, ProvenanceDescribesTheActualSuccessorWorkload) {
    constexpr std::size_t kDepth = 8;
    constexpr std::uint64_t kPreparedUpdateId = 1'000'001;
    constexpr std::uint64_t kSuccessorUpdateId = 1'000'002;

    auto projection = bm::build_synchronized_projection(core::SequencePolicyKind::Spot, kDepth);
    EXPECT_EQ(projection.status(), core::ProjectionStatus::Synchronized);
    EXPECT_EQ(projection.last_update_id(), core::UpdateId{kPreparedUpdateId});

    const auto observation = checked_apply_observation(kDepth);
    EXPECT_EQ(observation.disposition, core::ApplyDisposition::Applied);
    EXPECT_EQ(observation.last_update_id_after, core::UpdateId{kSuccessorUpdateId});

    const auto produced = wire_support::m4_generated_workload_description("CheckedApply", kDepth);
    const auto expected = checked_apply_expected_description(kDepth);
    EXPECT_EQ(produced, expected);
    EXPECT_EQ(std::get<std::string>(bmd_projection::m5::replay::sha256_hex(produced)),
              std::get<std::string>(bmd_projection::m5::replay::sha256_hex(expected)));
}

// The AdaptDepthUpdate/Spot cell adapts the range at 1'000'001 without a Core
// apply; its identity must stay bound to that exact wire.
TEST(Phase6M4AdaptDepthUpdate, ProvenanceBoundToTheAdaptedWire) {
    constexpr std::size_t kDepth = 8;
    const auto wire = wire_support::make_update_wire(wire_support::kM4AdaptDepthUpdateCell);
    const auto wire_bytes = wire.SerializeAsString();
    const std::string expected =
        "m4_cell_v1\nfamily=AdaptDepthUpdate/Spot\ndepth=" + std::to_string(kDepth) + '\n' +
        "update_wire_" + std::to_string(wire_bytes.size()) + ':' + wire_bytes + '\n';
    EXPECT_EQ(wire_support::m4_generated_workload_description("AdaptDepthUpdate/Spot", kDepth),
              expected);
}

TEST(Phase6AdapterReplay, PreconstructionCoversFullWorkloadAndChecksumIsStable) {
    const auto observation = test_support::observe_spot_replay_repeat();
    EXPECT_TRUE(observation.full_preconstruction);
    EXPECT_TRUE(observation.first_baseline_adapts);
    EXPECT_TRUE(observation.rebaseline_uses_adapter_wire);
    EXPECT_TRUE(observation.corrupt_rebaseline_is_rejected);
    EXPECT_EQ(observation.event_count, 2'048U);
    EXPECT_EQ(observation.first_checksum, observation.second_checksum);
    EXPECT_TRUE(observation.first_synchronized);
    EXPECT_TRUE(observation.second_synchronized);
}

TEST(Phase6AdapterReplay, UsdMWorkloadProducesStableDistinctChecksum) {
    const auto observation = test_support::observe_spot_usdm_replays();
    EXPECT_NE(observation.spot_checksum, observation.usdm_checksum);
    EXPECT_TRUE(observation.usdm_synchronized);
}

} // namespace
