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
#include <map>
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
    constexpr int kExpectedUpdateLevelCount = 10;
    const auto wire = wire_support::make_update_wire(wire_support::kM4AdaptDepthUpdateCell);
    const auto wire_bytes = wire.SerializeAsString();
    const std::string expected = "m4_cell_v1\nfamily=AdaptDepthUpdate/Spot\nupdate_level_count=" +
                                 std::to_string(kExpectedUpdateLevelCount) + '\n' + "update_wire_" +
                                 std::to_string(wire_bytes.size()) + ':' + wire_bytes + '\n';
    EXPECT_EQ(wire.bids_size(), kExpectedUpdateLevelCount);
    EXPECT_EQ(wire_support::m4_generated_workload_description("AdaptDepthUpdate/Spot", kDepth),
              expected);
}

// The Limited snapshot identity is checked against a constructed production
// snapshot, not against a duplicated expected depth literal.
TEST(Phase6M4SnapshotFixture, LimitedIdentityMatchesRuntimeDepthLimit) {
    constexpr std::size_t kDepth = 100;
    const auto projection =
        bm::build_synchronized_projection(core::SequencePolicyKind::Spot, kDepth);
    const auto options = wire_support::benchmark_limited_snapshot_options();
    if (!options.depth_limit.has_value()) {
        ADD_FAILURE() << "Limited snapshot options must carry a depth limit";
        return;
    }
    const auto runtime_depth_limit = options.depth_limit.value();

    const auto produced = adapter::make_local_order_book_snapshot(
        projection, wire_support::benchmark_snapshot_context(), options);
    ASSERT_TRUE(std::holds_alternative<core::LocalOrderBookSnapshot>(produced));
    const auto& snapshot = std::get<core::LocalOrderBookSnapshot>(produced);
    ASSERT_TRUE(snapshot.has_depth_limit());
    EXPECT_EQ(snapshot.depth_limit(), runtime_depth_limit.value());

    const auto description = wire_support::m4_generated_workload_description(
        "MakeLocalOrderBookSnapshot/Limited", kDepth);
    EXPECT_NE(description.find("depth_limit=" + std::to_string(snapshot.depth_limit()) + '\n'),
              std::string::npos);
}

// The provenance seam must reject the original failure mode even when a
// runtime caller supplies a value different from the accepted fixture.
TEST(Phase6M4SnapshotFixture, LimitedIdentityCheckRejectsRuntimeDrift) {
    constexpr std::size_t kDepth = 100;
    const auto options = wire_support::benchmark_limited_snapshot_options();
    EXPECT_TRUE(wire_support::benchmark_limited_snapshot_matches_identity(options, kDepth));

    auto drifted = options;
    const auto drifted_limit = adapter::DepthLimit::create(21);
    if (!std::holds_alternative<adapter::DepthLimit>(drifted_limit)) {
        ADD_FAILURE() << "test drift value must be a valid depth limit";
        return;
    }
    drifted.depth_limit = std::get<adapter::DepthLimit>(drifted_limit);
    EXPECT_FALSE(wire_support::benchmark_limited_snapshot_matches_identity(drifted, kDepth));
}

// P6-FINAL-001: the formal canonical CheckedApply sequence parameters must
// explicitly encode the locked successor operation. Asserted against explicit
// literals so the fields cannot silently drift from the timed successor wire.
TEST(Phase6M4CheckedApply, CanonicalSequenceFieldsExposeTheLockedSuccessor) {
    const auto fields = wire_support::checked_apply_canonical_sequence_fields();
    std::map<std::string, std::string> by_key(fields.begin(), fields.end());
    EXPECT_EQ(by_key.at("policy"), "Spot");
    EXPECT_EQ(by_key.at("initial_update_id"), "1000001");
    EXPECT_EQ(by_key.at("first_update_id"), "1000002");
    EXPECT_EQ(by_key.at("final_update_id"), "1000002");
    EXPECT_EQ(by_key.at("previous_final_update_id"), "not_applicable");
    EXPECT_EQ(by_key.at("first_update_id"),
              std::to_string(std::stoull(by_key.at("initial_update_id")) + 1));
    EXPECT_EQ(by_key.at("final_update_id"), by_key.at("first_update_id"));
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

// M5-P7-PRB-002: the accepted Phase-6 M4 snapshot/serialization identities
// describe the shared runtime SnapshotContext fixture; both the Phase-6
// timing benchmark and the Phase-7 M4 allocation executable execute exactly
// this value. Asserted against explicit literals (not another copy of the
// workload-spec string) so any future fixture drift fails this test even if
// the registered identity text stayed unchanged.
TEST(Phase6M4SnapshotFixture, SharedContextIsTheAcceptedPhase6Fixture) {
    const auto context = wire_support::benchmark_snapshot_context();
    EXPECT_EQ(context.identity.symbol, "BTCUSDT");
    EXPECT_EQ(context.identity.policy, core::SequencePolicyKind::Spot);
    EXPECT_EQ(context.producer, "phase6-benchmark");
    EXPECT_EQ(context.producer_version, "1");
    EXPECT_EQ(context.source, adapter::SnapshotOrigin::GatewayLive);
    EXPECT_EQ(context.generated_time_utc_ns, 1'234'567U);
    EXPECT_EQ(context.generated_monotonic_ns, std::optional<std::uint64_t>{654'321});
    EXPECT_FALSE(context.current_gap.has_value());
}

} // namespace
