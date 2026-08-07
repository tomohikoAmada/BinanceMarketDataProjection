#include <binance_market_data/projection_adapter/v1/proto_adapter.hpp>

#include <binance_market_data/common/v1/enums.pb.h>
#include <binance_market_data/market/v1/market_events.pb.h>
#include <binance_market_data/projection/v1/snapshots.pb.h>

#include <gtest/gtest.h>

#include <concepts>
#include <cstdint>
#include <limits>
#include <locale>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {

namespace adapter = binance_market_data::projection_adapter::v1;
namespace core = binance_market_data::projection::v1;
namespace common_wire = binance_market_data::common::v1;
namespace market_wire = binance_market_data::market::v1;

[[nodiscard]] core::DecimalScale scale(std::uint32_t value = 2) {
    return core::DecimalScale::create(value).value();
}

[[nodiscard]] core::NumericSpec spec(std::uint32_t price = 2, std::uint32_t quantity = 3) {
    return {scale(price), scale(quantity)};
}

[[nodiscard]] adapter::ExpectedIdentity spot_identity() {
    return {"BTCUSDT", core::SequencePolicyKind::Spot};
}

[[nodiscard]] adapter::ExpectedIdentity usdm_identity() {
    return {"BTCUSDT", core::SequencePolicyKind::UsdMPerpetual};
}

[[nodiscard]] market_wire::ExchangeDepthSnapshot
baseline_wire(common_wire::Market market = common_wire::MARKET_SPOT) {
    market_wire::ExchangeDepthSnapshot wire;
    wire.set_venue(common_wire::VENUE_BINANCE);
    wire.set_market(market);
    wire.set_symbol("BTCUSDT");
    wire.set_schema_version("exchange-depth-snapshot.v1");
    wire.set_producer("fixture");
    wire.set_producer_version("1.0");
    wire.set_request_id("request-1");
    wire.set_last_update_id(100);
    auto* bid = wire.add_bids();
    bid->set_price("100.00");
    bid->set_quantity("2.500");
    auto* ask = wire.add_asks();
    ask->set_price("101.00");
    ask->set_quantity("3.000");
    return wire;
}

[[nodiscard]] market_wire::DepthUpdate
update_wire(common_wire::Market market = common_wire::MARKET_SPOT) {
    market_wire::DepthUpdate wire;
    auto* metadata = wire.mutable_metadata();
    metadata->set_venue(common_wire::VENUE_BINANCE);
    metadata->set_market(market);
    metadata->set_symbol("BTCUSDT");
    metadata->set_producer("fixture");
    metadata->set_producer_version("1.0");
    metadata->set_connection_id("connection-1");
    metadata->set_stream(common_wire::STREAM_DIFF_DEPTH);
    metadata->set_schema_version("depth-update.v1");
    wire.set_first_update_id(99);
    wire.set_final_update_id(101);
    auto* bid = wire.add_bids();
    bid->set_price("100");
    bid->set_quantity("4.5");
    auto* ask = wire.add_asks();
    ask->set_price("102.00");
    ask->set_quantity("1.2500");
    return wire;
}

[[nodiscard]] adapter::SnapshotContext context() {
    return {
        spot_identity(), "projection-test",     "1.0",       adapter::SnapshotOrigin::GatewayLive,
        123456,          std::uint64_t{654321}, std::nullopt};
}

template <typename Owner>
concept RvalueInstallable = requires(Owner owner, core::BookProjection projection) {
    std::move(owner).install_into(projection);
};

template <typename Owner>
concept RvalueApplicable = requires(Owner owner, core::BookProjection projection) {
    std::move(owner).apply_to(projection);
};

static_assert(!std::is_copy_constructible_v<adapter::AdaptedBookBaseline>);
static_assert(std::is_nothrow_move_constructible_v<adapter::AdaptedBookBaseline>);
static_assert(!std::is_copy_constructible_v<adapter::AdaptedDepthBatch>);
static_assert(std::is_nothrow_move_constructible_v<adapter::AdaptedDepthBatch>);
static_assert(!RvalueInstallable<adapter::AdaptedBookBaseline>);
static_assert(!RvalueApplicable<adapter::AdaptedDepthBatch>);

TEST(DepthLimitTest, EnforcesTheCompleteInt32Domain) {
    EXPECT_TRUE(std::holds_alternative<adapter::AdapterError>(adapter::DepthLimit::create(-1)));
    EXPECT_TRUE(std::holds_alternative<adapter::AdapterError>(adapter::DepthLimit::create(0)));
    EXPECT_EQ(std::get<adapter::DepthLimit>(adapter::DepthLimit::create(1)).value(), 1);
    EXPECT_EQ(std::get<adapter::DepthLimit>(adapter::DepthLimit::create(INT32_MAX)).value(),
              INT32_MAX);
    EXPECT_TRUE(std::holds_alternative<adapter::AdapterError>(
        adapter::DepthLimit::create(static_cast<std::int64_t>(INT32_MAX) + 1)));
}

TEST(InboundSnapshotTest, AdaptsOwnsQualityAndInstallsAfterWireDestruction) {
    auto wire = baseline_wire();
    wire.add_quality_flags(common_wire::QUALITY_FLAG_OUT_OF_ORDER);
    wire.add_quality_flags(common_wire::QUALITY_FLAG_CROSSED_BOOK);
    wire.add_quality_flags(common_wire::QUALITY_FLAG_OUT_OF_ORDER);
    auto result = adapter::adapt_exchange_depth_snapshot(wire, spec(), spot_identity());
    ASSERT_TRUE(std::holds_alternative<adapter::AdaptedBookBaseline>(result));
    auto owner = std::move(std::get<adapter::AdaptedBookBaseline>(result));
    wire.Clear();

    ASSERT_EQ(owner.metadata().observed_quality,
              std::vector<adapter::HostQualityFact>{adapter::HostQualityFact::OutOfOrder});
    core::BookProjection projection{spec(), core::SequencePolicyKind::Spot};
    const auto install = owner.install_into(projection);
    ASSERT_TRUE(std::holds_alternative<core::InstallResult>(install));
    EXPECT_EQ(std::get<core::InstallResult>(install).disposition,
              core::InstallDisposition::Installed);
    EXPECT_EQ(projection.diagnostic_book().best_bid()->price.value(), 10000);
}

TEST(InboundSnapshotTest, RejectsNormalizedDuplicatesAndDoesNotMutateProjection) {
    auto wire = baseline_wire();
    auto* duplicate = wire.add_bids();
    duplicate->set_price("100.000");
    duplicate->set_quantity("1");
    core::BookProjection projection{spec(), core::SequencePolicyKind::Spot};

    const auto result = adapter::adapt_exchange_depth_snapshot(wire, spec(), spot_identity());
    ASSERT_TRUE(std::holds_alternative<adapter::AdapterError>(result));
    EXPECT_EQ(std::get<adapter::AdapterError>(result).code,
              adapter::AdapterErrorCode::InvalidOrdering);
    EXPECT_EQ(projection.status(), core::ProjectionStatus::AwaitingBaseline);
    EXPECT_TRUE(projection.diagnostic_book().empty());
}

TEST(InboundValidationTest, RejectsEnumIdentitySchemaIdentifierAndDecimalFailures) {
    auto wire = baseline_wire();
    wire.set_venue(common_wire::VENUE_UNSPECIFIED);
    auto result = adapter::adapt_exchange_depth_snapshot(wire, spec(), spot_identity());
    EXPECT_EQ(std::get<adapter::AdapterError>(result).code,
              adapter::AdapterErrorCode::UnspecifiedEnum);

    wire = baseline_wire();
    wire.set_symbol("ETHUSDT");
    result = adapter::adapt_exchange_depth_snapshot(wire, spec(), spot_identity());
    EXPECT_EQ(std::get<adapter::AdapterError>(result).code,
              adapter::AdapterErrorCode::IdentityMismatch);

    wire = baseline_wire();
    wire.set_schema_version("wrong");
    result = adapter::adapt_exchange_depth_snapshot(wire, spec(), spot_identity());
    EXPECT_EQ(std::get<adapter::AdapterError>(result).code,
              adapter::AdapterErrorCode::UnsupportedSchemaVersion);

    wire = baseline_wire();
    wire.mutable_bids(0)->set_price("1e2");
    result = adapter::adapt_exchange_depth_snapshot(wire, spec(), spot_identity());
    EXPECT_EQ(std::get<adapter::AdapterError>(result).code,
              adapter::AdapterErrorCode::InvalidDecimal);

    wire = baseline_wire();
    wire.mutable_bids(0)->set_quantity("-1");
    result = adapter::adapt_exchange_depth_snapshot(wire, spec(), spot_identity());
    EXPECT_EQ(std::get<adapter::AdapterError>(result).code,
              adapter::AdapterErrorCode::NegativeQuantity);

    wire = baseline_wire();
    wire.mutable_bids(0)->set_price("100.001");
    result = adapter::adapt_exchange_depth_snapshot(wire, spec(), spot_identity());
    EXPECT_EQ(std::get<adapter::AdapterError>(result).code,
              adapter::AdapterErrorCode::ScaleMismatch);
}

TEST(InboundUpdateTest, PreservesPuPresenceOrderAndM3Results) {
    auto baseline =
        adapter::adapt_exchange_depth_snapshot(baseline_wire(), spec(), spot_identity());
    core::BookProjection projection{spec(), core::SequencePolicyKind::Spot};
    ASSERT_TRUE(std::holds_alternative<core::InstallResult>(
        std::get<adapter::AdaptedBookBaseline>(baseline).install_into(projection)));

    auto wire = update_wire();
    wire.set_previous_final_update_id(77);
    auto result = adapter::adapt_depth_update(wire, spec(), spot_identity());
    ASSERT_TRUE(std::holds_alternative<adapter::AdaptedDepthBatch>(result));
    auto owner = std::move(std::get<adapter::AdaptedDepthBatch>(result));
    wire.Clear();
    const auto applied = owner.apply_to(projection);
    ASSERT_TRUE(std::holds_alternative<core::ApplyResult>(applied));
    EXPECT_EQ(std::get<core::ApplyResult>(applied).disposition, core::ApplyDisposition::Applied);
    EXPECT_EQ(projection.diagnostic_book().best_bid()->quantity.value(), 4500);
    EXPECT_EQ(projection.diagnostic_book().best_ask()->price.value(), 10100);
}

TEST(InboundUpdateTest, RejectsMissingMetadataInvalidRangeAndUnknownQuality) {
    market_wire::DepthUpdate missing;
    auto result = adapter::adapt_depth_update(missing, spec(), spot_identity());
    EXPECT_EQ(std::get<adapter::AdapterError>(result).code,
              adapter::AdapterErrorCode::MissingRequiredField);

    auto wire = update_wire();
    wire.set_first_update_id(2);
    wire.set_final_update_id(1);
    result = adapter::adapt_depth_update(wire, spec(), spot_identity());
    EXPECT_EQ(std::get<adapter::AdapterError>(result).code,
              adapter::AdapterErrorCode::InvalidUpdateRange);

    wire = update_wire();
    wire.mutable_metadata()->mutable_quality_flags()->Add(999);
    result = adapter::adapt_depth_update(wire, spec(), spot_identity());
    EXPECT_EQ(std::get<adapter::AdapterError>(result).code,
              adapter::AdapterErrorCode::UnknownEnumValue);
}

TEST(InboundMatrixTest, AcceptsUsdMIdsPresenceDuplicatesAndExactRescale) {
    auto baseline_result = adapter::adapt_exchange_depth_snapshot(
        baseline_wire(common_wire::MARKET_USD_M_PERPETUAL), spec(), usdm_identity());
    ASSERT_TRUE(std::holds_alternative<adapter::AdaptedBookBaseline>(baseline_result));
    core::BookProjection projection{spec(), core::SequencePolicyKind::UsdMPerpetual};
    EXPECT_EQ(std::get<core::InstallResult>(
                  std::get<adapter::AdaptedBookBaseline>(baseline_result).install_into(projection))
                  .disposition,
              core::InstallDisposition::Installed);

    auto wire = update_wire(common_wire::MARKET_USD_M_PERPETUAL);
    wire.set_first_update_id(100);
    wire.set_final_update_id(100);
    wire.set_previous_final_update_id(std::numeric_limits<std::uint64_t>::max());
    wire.mutable_bids(0)->set_price("100.0000");
    wire.mutable_bids(0)->set_quantity("0");
    wire.add_bids()->CopyFrom(wire.bids(0));
    auto update_result = adapter::adapt_depth_update(wire, spec(), usdm_identity());
    ASSERT_TRUE(std::holds_alternative<adapter::AdaptedDepthBatch>(update_result));
    EXPECT_EQ(std::get<core::ApplyResult>(
                  std::get<adapter::AdaptedDepthBatch>(update_result).apply_to(projection))
                  .disposition,
              core::ApplyDisposition::Applied);

    wire.set_first_update_id(0);
    wire.set_final_update_id(std::numeric_limits<std::uint64_t>::max());
    wire.set_previous_final_update_id(0);
    update_result = adapter::adapt_depth_update(wire, spec(), usdm_identity());
    EXPECT_TRUE(std::holds_alternative<adapter::AdaptedDepthBatch>(update_result));
}

TEST(InboundMatrixTest, RejectsAllIdentityEnumSchemaAndNumericDomains) {
    auto wire = baseline_wire();
    wire.set_venue(static_cast<common_wire::Venue>(999));
    auto result = adapter::adapt_exchange_depth_snapshot(wire, spec(), spot_identity());
    EXPECT_EQ(std::get<adapter::AdapterError>(result).code,
              adapter::AdapterErrorCode::UnsupportedVenue);

    wire = baseline_wire();
    wire.set_market(static_cast<common_wire::Market>(999));
    result = adapter::adapt_exchange_depth_snapshot(wire, spec(), spot_identity());
    EXPECT_EQ(std::get<adapter::AdapterError>(result).code,
              adapter::AdapterErrorCode::UnknownEnumValue);

    wire = baseline_wire();
    wire.set_market(common_wire::MARKET_UNSPECIFIED);
    result = adapter::adapt_exchange_depth_snapshot(wire, spec(), spot_identity());
    EXPECT_EQ(std::get<adapter::AdapterError>(result).code,
              adapter::AdapterErrorCode::UnspecifiedEnum);

    wire = baseline_wire(common_wire::MARKET_USD_M_PERPETUAL);
    result = adapter::adapt_exchange_depth_snapshot(wire, spec(), spot_identity());
    EXPECT_EQ(std::get<adapter::AdapterError>(result).code,
              adapter::AdapterErrorCode::IdentityMismatch);

    for (const std::string symbol : {"", "a", "btc usdt", "BTC_USDT"}) {
        wire = baseline_wire();
        wire.set_symbol(symbol);
        result = adapter::adapt_exchange_depth_snapshot(wire, spec(), spot_identity());
        EXPECT_TRUE(std::holds_alternative<adapter::AdapterError>(result));
    }

    wire = baseline_wire();
    wire.clear_schema_version();
    result = adapter::adapt_exchange_depth_snapshot(wire, spec(), spot_identity());
    EXPECT_EQ(std::get<adapter::AdapterError>(result).code,
              adapter::AdapterErrorCode::UnsupportedSchemaVersion);

    wire = baseline_wire();
    wire.mutable_bids(0)->set_price("-1");
    result = adapter::adapt_exchange_depth_snapshot(wire, spec(), spot_identity());
    EXPECT_EQ(std::get<adapter::AdapterError>(result).code,
              adapter::AdapterErrorCode::NonPositivePrice);

    wire = baseline_wire();
    wire.mutable_bids(0)->set_price("9223372036854775808");
    result = adapter::adapt_exchange_depth_snapshot(wire, spec(), spot_identity());
    EXPECT_EQ(std::get<adapter::AdapterError>(result).code,
              adapter::AdapterErrorCode::NumericOverflow);

    auto update = update_wire();
    update.mutable_metadata()->set_stream(common_wire::STREAM_BOOK_TICKER);
    auto update_result = adapter::adapt_depth_update(update, spec(), spot_identity());
    EXPECT_EQ(std::get<adapter::AdapterError>(update_result).code,
              adapter::AdapterErrorCode::UnexpectedStream);

    update = update_wire();
    update.mutable_metadata()->set_stream(common_wire::STREAM_UNSPECIFIED);
    update_result = adapter::adapt_depth_update(update, spec(), spot_identity());
    EXPECT_EQ(std::get<adapter::AdapterError>(update_result).code,
              adapter::AdapterErrorCode::UnspecifiedEnum);

    wire = baseline_wire();
    wire.add_quality_flags(common_wire::QUALITY_FLAG_UNSPECIFIED);
    result = adapter::adapt_exchange_depth_snapshot(wire, spec(), spot_identity());
    EXPECT_EQ(std::get<adapter::AdapterError>(result).code,
              adapter::AdapterErrorCode::UnspecifiedEnum);

    for (const auto id : {std::uint64_t{0}, std::numeric_limits<std::uint64_t>::max()}) {
        wire = baseline_wire();
        wire.set_last_update_id(id);
        EXPECT_TRUE(std::holds_alternative<adapter::AdaptedBookBaseline>(
            adapter::adapt_exchange_depth_snapshot(wire, spec(), spot_identity())));
    }
}

TEST(InboundMatrixTest, AcceptsEmptyLockedCrossedAndStrictlyOrderedSnapshots) {
    auto empty = baseline_wire();
    empty.clear_bids();
    empty.clear_asks();
    EXPECT_TRUE(std::holds_alternative<adapter::AdaptedBookBaseline>(
        adapter::adapt_exchange_depth_snapshot(empty, spec(), spot_identity())));

    auto locked = baseline_wire();
    locked.mutable_asks(0)->set_price("100.00");
    EXPECT_TRUE(std::holds_alternative<adapter::AdaptedBookBaseline>(
        adapter::adapt_exchange_depth_snapshot(locked, spec(), spot_identity())));

    auto crossed = baseline_wire();
    crossed.mutable_asks(0)->set_price("99.00");
    EXPECT_TRUE(std::holds_alternative<adapter::AdaptedBookBaseline>(
        adapter::adapt_exchange_depth_snapshot(crossed, spec(), spot_identity())));

    auto ordered = baseline_wire();
    auto* second_bid = ordered.add_bids();
    second_bid->set_price("99.00");
    second_bid->set_quantity("1.000");
    auto* second_ask = ordered.add_asks();
    second_ask->set_price("102.00");
    second_ask->set_quantity("1.000");
    EXPECT_TRUE(std::holds_alternative<adapter::AdaptedBookBaseline>(
        adapter::adapt_exchange_depth_snapshot(ordered, spec(), spot_identity())));
    std::swap(*ordered.mutable_bids(0), *ordered.mutable_bids(1));
    const auto rejected = adapter::adapt_exchange_depth_snapshot(ordered, spec(), spot_identity());
    EXPECT_EQ(std::get<adapter::AdapterError>(rejected).code,
              adapter::AdapterErrorCode::InvalidOrdering);
}

TEST(BindingTest, ChecksPriceThenQuantityThenPolicyBeforeMutation) {
    auto baseline =
        adapter::adapt_exchange_depth_snapshot(baseline_wire(), spec(), spot_identity());
    auto owner = std::move(std::get<adapter::AdaptedBookBaseline>(baseline));

    core::BookProjection both_scale_mismatch{spec(3, 4), core::SequencePolicyKind::Spot};
    auto result = owner.install_into(both_scale_mismatch);
    ASSERT_TRUE(std::holds_alternative<adapter::AdapterError>(result));
    EXPECT_EQ(std::get<adapter::AdapterError>(result).field,
              adapter::AdapterField::ProjectionPriceScale);
    EXPECT_EQ(both_scale_mismatch.status(), core::ProjectionStatus::AwaitingBaseline);

    core::BookProjection quantity_mismatch{spec(2, 4), core::SequencePolicyKind::Spot};
    result = owner.install_into(quantity_mismatch);
    EXPECT_EQ(std::get<adapter::AdapterError>(result).field,
              adapter::AdapterField::ProjectionQuantityScale);

    core::BookProjection policy_mismatch{spec(), core::SequencePolicyKind::UsdMPerpetual};
    result = owner.install_into(policy_mismatch);
    EXPECT_EQ(std::get<adapter::AdapterError>(result).code,
              adapter::AdapterErrorCode::ProjectionPolicyMismatch);
    EXPECT_TRUE(policy_mismatch.diagnostic_book().empty());
}

TEST(BindingLifetimeTest, MoveConstructionAssignmentAndOppositePolicyRemainChecked) {
    auto first_result =
        adapter::adapt_exchange_depth_snapshot(baseline_wire(), spec(), spot_identity());
    auto second_result =
        adapter::adapt_exchange_depth_snapshot(baseline_wire(), spec(), spot_identity());
    auto moved = std::move(std::get<adapter::AdaptedBookBaseline>(first_result));
    auto assigned = std::move(std::get<adapter::AdaptedBookBaseline>(second_result));
    assigned = std::move(moved);

    core::BookProjection wrong_policy{spec(), core::SequencePolicyKind::UsdMPerpetual};
    auto result = assigned.install_into(wrong_policy);
    EXPECT_EQ(std::get<adapter::AdapterError>(result).code,
              adapter::AdapterErrorCode::ProjectionPolicyMismatch);
    EXPECT_EQ(wrong_policy.status(), core::ProjectionStatus::AwaitingBaseline);

    auto usd_result = adapter::adapt_exchange_depth_snapshot(
        baseline_wire(common_wire::MARKET_USD_M_PERPETUAL), spec(), usdm_identity());
    auto usd_owner = std::move(std::get<adapter::AdaptedBookBaseline>(usd_result));
    core::BookProjection spot_target{spec(), core::SequencePolicyKind::Spot};
    result = usd_owner.install_into(spot_target);
    EXPECT_EQ(std::get<adapter::AdapterError>(result).code,
              adapter::AdapterErrorCode::ProjectionPolicyMismatch);
}

TEST(M3ResultPropagationTest, PreservesEveryNormalSequencingDisposition) {
    auto update_result = adapter::adapt_depth_update(update_wire(), spec(), spot_identity());
    auto update_owner = std::move(std::get<adapter::AdaptedDepthBatch>(update_result));
    core::BookProjection projection{spec(), core::SequencePolicyKind::Spot};
    EXPECT_EQ(std::get<core::ApplyResult>(update_owner.apply_to(projection)).disposition,
              core::ApplyDisposition::RejectedWrongState);

    auto baseline =
        adapter::adapt_exchange_depth_snapshot(baseline_wire(), spec(), spot_identity());
    static_cast<void>(std::get<adapter::AdaptedBookBaseline>(baseline).install_into(projection));
    EXPECT_EQ(std::get<core::ApplyResult>(update_owner.apply_to(projection)).disposition,
              core::ApplyDisposition::Applied);
    EXPECT_EQ(std::get<core::ApplyResult>(update_owner.apply_to(projection)).disposition,
              core::ApplyDisposition::IgnoredDuplicate);

    auto stale_wire = update_wire();
    stale_wire.set_first_update_id(1);
    stale_wire.set_final_update_id(2);
    auto stale = adapter::adapt_depth_update(stale_wire, spec(), spot_identity());
    EXPECT_EQ(std::get<core::ApplyResult>(
                  std::get<adapter::AdaptedDepthBatch>(stale).apply_to(projection))
                  .disposition,
              core::ApplyDisposition::IgnoredStale);

    auto gap_wire = update_wire();
    gap_wire.set_first_update_id(200);
    gap_wire.set_final_update_id(201);
    auto gap = adapter::adapt_depth_update(gap_wire, spec(), spot_identity());
    EXPECT_EQ(
        std::get<core::ApplyResult>(std::get<adapter::AdaptedDepthBatch>(gap).apply_to(projection))
            .disposition,
        core::ApplyDisposition::GapDetected);
}

TEST(ContractsFixtureRoundTripTest, PreservesKnownSemanticFieldsThroughCoreAndOutput) {
    const auto fixture = baseline_wire().SerializeAsString();
    market_wire::ExchangeDepthSnapshot decoded;
    ASSERT_TRUE(decoded.ParseFromString(fixture));
    auto baseline = adapter::adapt_exchange_depth_snapshot(decoded, spec(), spot_identity());
    ASSERT_TRUE(std::holds_alternative<adapter::AdaptedBookBaseline>(baseline));
    decoded.Clear();

    core::BookProjection projection{spec(), core::SequencePolicyKind::Spot};
    ASSERT_EQ(std::get<core::InstallResult>(
                  std::get<adapter::AdaptedBookBaseline>(baseline).install_into(projection))
                  .disposition,
              core::InstallDisposition::Installed);
    auto bridge = adapter::adapt_depth_update(update_wire(), spec(), spot_identity());
    ASSERT_EQ(std::get<core::ApplyResult>(
                  std::get<adapter::AdaptedDepthBatch>(bridge).apply_to(projection))
                  .disposition,
              core::ApplyDisposition::Applied);
    const auto output = adapter::make_local_order_book_snapshot(projection, context(), {});
    ASSERT_TRUE(std::holds_alternative<core::LocalOrderBookSnapshot>(output));
    const auto& snapshot = std::get<core::LocalOrderBookSnapshot>(output);
    EXPECT_EQ(snapshot.venue(), common_wire::VENUE_BINANCE);
    EXPECT_EQ(snapshot.market(), common_wire::MARKET_SPOT);
    EXPECT_EQ(snapshot.symbol(), "BTCUSDT");
    EXPECT_EQ(snapshot.schema_version(), "local-order-book-snapshot.v1");
    EXPECT_EQ(snapshot.last_update_id(), 101U);
    EXPECT_TRUE(snapshot.synchronized());
    EXPECT_EQ(snapshot.bids(0).price(), "100.00");
    EXPECT_EQ(snapshot.bids(0).quantity(), "4.500");
}

TEST(SnapshotOutputTest, ImplementsAwaitingBaselineBridgeAndSynchronizedRows) {
    core::BookProjection projection{spec(), core::SequencePolicyKind::Spot};
    auto snapshot = adapter::make_local_order_book_snapshot(projection, context(), {});
    ASSERT_TRUE(std::holds_alternative<adapter::AdapterError>(snapshot));
    EXPECT_EQ(std::get<adapter::AdapterError>(snapshot).code,
              adapter::AdapterErrorCode::MissingLastUpdateId);

    auto baseline =
        adapter::adapt_exchange_depth_snapshot(baseline_wire(), spec(), spot_identity());
    ASSERT_TRUE(std::holds_alternative<core::InstallResult>(
        std::get<adapter::AdaptedBookBaseline>(baseline).install_into(projection)));
    snapshot = adapter::make_local_order_book_snapshot(projection, context(), {});
    ASSERT_TRUE(std::holds_alternative<core::LocalOrderBookSnapshot>(snapshot));
    const auto& bridge = std::get<core::LocalOrderBookSnapshot>(snapshot);
    EXPECT_FALSE(bridge.synchronized());
    ASSERT_EQ(bridge.quality_flags_size(), 1);
    EXPECT_EQ(bridge.quality_flags(0), common_wire::QUALITY_FLAG_SNAPSHOT_BRIDGE_PENDING);
    EXPECT_EQ(bridge.bids(0).price(), "100.00");
    EXPECT_EQ(bridge.bids(0).quantity(), "2.500");
    EXPECT_EQ(bridge.source(), common_wire::SNAPSHOT_SOURCE_GATEWAY_LIVE);
    EXPECT_EQ(bridge.producer(), "projection-test");
    EXPECT_EQ(bridge.generated_time_utc_ns(), 123456U);
    ASSERT_TRUE(bridge.has_generated_monotonic_ns());
    EXPECT_EQ(bridge.generated_monotonic_ns(), 654321U);

    auto update = adapter::adapt_depth_update(update_wire(), spec(), spot_identity());
    ASSERT_TRUE(std::holds_alternative<core::ApplyResult>(
        std::get<adapter::AdaptedDepthBatch>(update).apply_to(projection)));
    auto synchronized_context = context();
    synchronized_context.source = adapter::SnapshotOrigin::HistoryReplay;
    adapter::SnapshotOptions options;
    options.depth_limit = std::get<adapter::DepthLimit>(adapter::DepthLimit::create(1));
    options.host_quality_facts = {adapter::HostQualityFact::RecoveredTail,
                                  adapter::HostQualityFact::Duplicate,
                                  adapter::HostQualityFact::Duplicate};
    snapshot = adapter::make_local_order_book_snapshot(projection, synchronized_context, options);
    ASSERT_TRUE(std::holds_alternative<core::LocalOrderBookSnapshot>(snapshot));
    const auto& synchronized = std::get<core::LocalOrderBookSnapshot>(snapshot);
    EXPECT_TRUE(synchronized.synchronized());
    EXPECT_TRUE(synchronized.has_depth_limit());
    EXPECT_EQ(synchronized.depth_limit(), 1);
    EXPECT_EQ(synchronized.bids_size(), 1);
    ASSERT_EQ(synchronized.quality_flags_size(), 2);
    EXPECT_EQ(synchronized.quality_flags(0), common_wire::QUALITY_FLAG_DUPLICATE);
    EXPECT_EQ(synchronized.quality_flags(1), common_wire::QUALITY_FLAG_RECOVERED_TAIL);
    EXPECT_FALSE(synchronized.has_last_gap());
}

TEST(SnapshotOutputTest, EmitsCurrentGapAndCanonicalCoreHostQuality) {
    auto crossed = baseline_wire();
    crossed.mutable_bids(0)->set_price("102.00");
    auto baseline = adapter::adapt_exchange_depth_snapshot(crossed, spec(), spot_identity());
    core::BookProjection projection{spec(), core::SequencePolicyKind::Spot};
    ASSERT_TRUE(std::holds_alternative<core::InstallResult>(
        std::get<adapter::AdaptedBookBaseline>(baseline).install_into(projection)));
    auto bridge = update_wire();
    bridge.clear_bids();
    bridge.clear_asks();
    auto bridge_owner = adapter::adapt_depth_update(bridge, spec(), spot_identity());
    ASSERT_EQ(std::get<core::ApplyResult>(
                  std::get<adapter::AdaptedDepthBatch>(bridge_owner).apply_to(projection))
                  .disposition,
              core::ApplyDisposition::Applied);

    auto gap_wire = update_wire();
    gap_wire.clear_bids();
    gap_wire.clear_asks();
    gap_wire.set_first_update_id(200);
    gap_wire.set_final_update_id(201);
    auto gap_owner = adapter::adapt_depth_update(gap_wire, spec(), spot_identity());
    ASSERT_EQ(std::get<core::ApplyResult>(
                  std::get<adapter::AdaptedDepthBatch>(gap_owner).apply_to(projection))
                  .disposition,
              core::ApplyDisposition::GapDetected);

    auto gap_context = context();
    gap_context.current_gap =
        adapter::CurrentGapContext{999, adapter::GapRecoveryState::ResyncInProgress};
    adapter::SnapshotOptions options;
    options.host_quality_facts = {adapter::HostQualityFact::OutOfOrder,
                                  adapter::HostQualityFact::OrderBookResync};
    const auto snapshot = adapter::make_local_order_book_snapshot(projection, gap_context, options);
    ASSERT_TRUE(std::holds_alternative<core::LocalOrderBookSnapshot>(snapshot));
    const auto& output = std::get<core::LocalOrderBookSnapshot>(snapshot);
    EXPECT_FALSE(output.synchronized());
    ASSERT_TRUE(output.has_last_gap());
    EXPECT_EQ(output.last_gap().previous_sequence(), 101);
    EXPECT_EQ(output.last_gap().next_sequence(), 200);
    EXPECT_EQ(output.last_gap().detected_at_utc_ns(), 999);
    EXPECT_EQ(output.last_gap().reason_code(), common_wire::REASON_CODE_SEQUENCE_GAP_DETECTED);
    EXPECT_EQ(output.last_gap().recovery_state(), common_wire::RESYNC_STATE_RESYNC_IN_PROGRESS);
    ASSERT_EQ(output.quality_flags_size(), 4);
    EXPECT_EQ(output.quality_flags(0), common_wire::QUALITY_FLAG_OUT_OF_ORDER);
    EXPECT_EQ(output.quality_flags(1), common_wire::QUALITY_FLAG_SEQUENCE_GAP);
    EXPECT_EQ(output.quality_flags(2), common_wire::QUALITY_FLAG_ORDERBOOK_RESYNC);
    EXPECT_EQ(output.quality_flags(3), common_wire::QUALITY_FLAG_CROSSED_BOOK);
}

TEST(SnapshotOutputTest, RejectsContradictoryGapAndHostQualityContext) {
    core::BookProjection projection{spec(), core::SequencePolicyKind::Spot};
    auto baseline =
        adapter::adapt_exchange_depth_snapshot(baseline_wire(), spec(), spot_identity());
    ASSERT_TRUE(std::holds_alternative<core::InstallResult>(
        std::get<adapter::AdaptedBookBaseline>(baseline).install_into(projection)));

    auto invalid_context = context();
    invalid_context.current_gap =
        adapter::CurrentGapContext{1, adapter::GapRecoveryState::ResyncRequired};
    auto snapshot = adapter::make_local_order_book_snapshot(projection, invalid_context, {});
    EXPECT_EQ(std::get<adapter::AdapterError>(snapshot).code,
              adapter::AdapterErrorCode::InvalidGapContext);

    adapter::SnapshotOptions invalid_quality;
    invalid_quality.host_quality_facts = {adapter::HostQualityFact::RecoveredTail};
    snapshot = adapter::make_local_order_book_snapshot(projection, context(), invalid_quality);
    EXPECT_EQ(std::get<adapter::AdapterError>(snapshot).code,
              adapter::AdapterErrorCode::InvalidHostQualityCombination);
}

TEST(SnapshotOutputTest, MapsAllFiveGapReasonsAndAllCurrentRecoveryStates) {
    const auto apply_wire = [](core::BookProjection& projection, common_wire::Market market,
                               const adapter::ExpectedIdentity& expected, std::uint64_t first,
                               std::uint64_t final,
                               std::optional<std::uint64_t> previous = std::nullopt) {
        auto wire = update_wire(market);
        wire.clear_bids();
        wire.clear_asks();
        wire.set_first_update_id(first);
        wire.set_final_update_id(final);
        if (previous.has_value()) {
            wire.set_previous_final_update_id(*previous);
        } else {
            wire.clear_previous_final_update_id();
        }
        auto adapted = adapter::adapt_depth_update(wire, spec(), expected);
        return std::get<core::ApplyResult>(
            std::get<adapter::AdaptedDepthBatch>(adapted).apply_to(projection));
    };
    const auto install = [](core::BookProjection& projection, common_wire::Market market,
                            const adapter::ExpectedIdentity& expected) {
        auto adapted =
            adapter::adapt_exchange_depth_snapshot(baseline_wire(market), spec(), expected);
        return std::get<core::InstallResult>(
            std::get<adapter::AdaptedBookBaseline>(adapted).install_into(projection));
    };
    const auto expect_gap = [](const core::BookProjection& projection,
                               const adapter::ExpectedIdentity& expected,
                               core::GapReason expected_reason, std::uint64_t expected_previous,
                               std::uint64_t expected_next) {
        ASSERT_EQ(projection.status(), core::ProjectionStatus::NeedsResync);
        ASSERT_TRUE(projection.last_gap().has_value());
        EXPECT_EQ(projection.last_gap()->reason, expected_reason);
        auto snapshot_context = context();
        snapshot_context.identity = expected;
        snapshot_context.current_gap =
            adapter::CurrentGapContext{444, adapter::GapRecoveryState::ResyncRequired};
        const auto output =
            adapter::make_local_order_book_snapshot(projection, snapshot_context, {});
        ASSERT_TRUE(std::holds_alternative<core::LocalOrderBookSnapshot>(output));
        const auto& gap = std::get<core::LocalOrderBookSnapshot>(output).last_gap();
        EXPECT_EQ(gap.previous_sequence(), expected_previous);
        EXPECT_EQ(gap.next_sequence(), expected_next);
        EXPECT_EQ(gap.reason_code(), common_wire::REASON_CODE_SEQUENCE_GAP_DETECTED);
    };

    core::BookProjection spot_bootstrap{spec(), core::SequencePolicyKind::Spot};
    ASSERT_EQ(install(spot_bootstrap, common_wire::MARKET_SPOT, spot_identity()).disposition,
              core::InstallDisposition::Installed);
    EXPECT_EQ(
        apply_wire(spot_bootstrap, common_wire::MARKET_SPOT, spot_identity(), 102, 102).disposition,
        core::ApplyDisposition::GapDetected);
    expect_gap(spot_bootstrap, spot_identity(), core::GapReason::SpotBootstrapForwardGap, 100, 102);

    core::BookProjection spot_live{spec(), core::SequencePolicyKind::Spot};
    static_cast<void>(install(spot_live, common_wire::MARKET_SPOT, spot_identity()));
    static_cast<void>(apply_wire(spot_live, common_wire::MARKET_SPOT, spot_identity(), 99, 101));
    EXPECT_EQ(
        apply_wire(spot_live, common_wire::MARKET_SPOT, spot_identity(), 103, 103).disposition,
        core::ApplyDisposition::GapDetected);
    expect_gap(spot_live, spot_identity(), core::GapReason::SpotLiveForwardGap, 101, 103);

    core::BookProjection futures_bootstrap{spec(), core::SequencePolicyKind::UsdMPerpetual};
    static_cast<void>(
        install(futures_bootstrap, common_wire::MARKET_USD_M_PERPETUAL, usdm_identity()));
    EXPECT_EQ(apply_wire(futures_bootstrap, common_wire::MARKET_USD_M_PERPETUAL, usdm_identity(),
                         101, 101, 100)
                  .disposition,
              core::ApplyDisposition::GapDetected);
    expect_gap(futures_bootstrap, usdm_identity(), core::GapReason::FuturesBootstrapRangeMiss, 100,
               101);

    core::BookProjection futures_missing{spec(), core::SequencePolicyKind::UsdMPerpetual};
    static_cast<void>(
        install(futures_missing, common_wire::MARKET_USD_M_PERPETUAL, usdm_identity()));
    static_cast<void>(apply_wire(futures_missing, common_wire::MARKET_USD_M_PERPETUAL,
                                 usdm_identity(), 99, 101, 50));
    EXPECT_EQ(
        apply_wire(futures_missing, common_wire::MARKET_USD_M_PERPETUAL, usdm_identity(), 102, 102)
            .disposition,
        core::ApplyDisposition::GapDetected);
    expect_gap(futures_missing, usdm_identity(), core::GapReason::FuturesMissingPreviousFinal, 101,
               102);

    core::BookProjection futures_mismatch{spec(), core::SequencePolicyKind::UsdMPerpetual};
    static_cast<void>(
        install(futures_mismatch, common_wire::MARKET_USD_M_PERPETUAL, usdm_identity()));
    static_cast<void>(apply_wire(futures_mismatch, common_wire::MARKET_USD_M_PERPETUAL,
                                 usdm_identity(), 99, 101, 50));
    EXPECT_EQ(apply_wire(futures_mismatch, common_wire::MARKET_USD_M_PERPETUAL, usdm_identity(),
                         102, 102, 100)
                  .disposition,
              core::ApplyDisposition::GapDetected);
    expect_gap(futures_mismatch, usdm_identity(), core::GapReason::FuturesPreviousFinalMismatch,
               101, 102);

    for (const auto& [host_state, wire_state] :
         std::array<std::pair<adapter::GapRecoveryState, common_wire::ResyncState>, 3>{
             {{adapter::GapRecoveryState::ResyncRequired,
               common_wire::RESYNC_STATE_RESYNC_REQUIRED},
              {adapter::GapRecoveryState::ResyncInProgress,
               common_wire::RESYNC_STATE_RESYNC_IN_PROGRESS},
              {adapter::GapRecoveryState::ResyncFailed,
               common_wire::RESYNC_STATE_RESYNC_FAILED}}}) {
        auto snapshot_context = context();
        snapshot_context.current_gap = adapter::CurrentGapContext{555, host_state};
        const auto output =
            adapter::make_local_order_book_snapshot(spot_live, snapshot_context, {});
        ASSERT_TRUE(std::holds_alternative<core::LocalOrderBookSnapshot>(output));
        EXPECT_EQ(std::get<core::LocalOrderBookSnapshot>(output).last_gap().recovery_state(),
                  wire_state);
    }
}

TEST(SnapshotOutputTest, OmitsHistoricalGapAfterRebaselineRecovery) {
    core::BookProjection projection{spec(), core::SequencePolicyKind::Spot};
    auto baseline =
        adapter::adapt_exchange_depth_snapshot(baseline_wire(), spec(), spot_identity());
    static_cast<void>(std::get<adapter::AdaptedBookBaseline>(baseline).install_into(projection));
    auto bridge = update_wire();
    auto bridge_owner = adapter::adapt_depth_update(bridge, spec(), spot_identity());
    static_cast<void>(std::get<adapter::AdaptedDepthBatch>(bridge_owner).apply_to(projection));
    auto gap_wire = update_wire();
    gap_wire.set_first_update_id(200);
    gap_wire.set_final_update_id(200);
    auto gap_owner = adapter::adapt_depth_update(gap_wire, spec(), spot_identity());
    static_cast<void>(std::get<adapter::AdaptedDepthBatch>(gap_owner).apply_to(projection));
    ASSERT_TRUE(projection.last_gap().has_value());

    auto replacement_wire = baseline_wire();
    replacement_wire.set_last_update_id(300);
    auto replacement =
        adapter::adapt_exchange_depth_snapshot(replacement_wire, spec(), spot_identity());
    EXPECT_EQ(std::get<core::InstallResult>(
                  std::get<adapter::AdaptedBookBaseline>(replacement).install_into(projection))
                  .disposition,
              core::InstallDisposition::Installed);
    auto recovery_wire = update_wire();
    recovery_wire.set_first_update_id(299);
    recovery_wire.set_final_update_id(301);
    auto recovery = adapter::adapt_depth_update(recovery_wire, spec(), spot_identity());
    EXPECT_EQ(std::get<core::ApplyResult>(
                  std::get<adapter::AdaptedDepthBatch>(recovery).apply_to(projection))
                  .disposition,
              core::ApplyDisposition::Applied);
    ASSERT_EQ(projection.status(), core::ProjectionStatus::Synchronized);
    ASSERT_TRUE(projection.last_gap().has_value());

    const auto output = adapter::make_local_order_book_snapshot(projection, context(), {});
    ASSERT_TRUE(std::holds_alternative<core::LocalOrderBookSnapshot>(output));
    EXPECT_FALSE(std::get<core::LocalOrderBookSnapshot>(output).has_last_gap());
}

class CommaDecimalPoint final : public std::numpunct<char> {
  protected:
    [[nodiscard]] char do_decimal_point() const override { return ','; }
};

TEST(SnapshotOutputTest, FixedFormattingIsIndependentOfGlobalLocale) {
    core::BookProjection projection{spec(), core::SequencePolicyKind::Spot};
    auto baseline =
        adapter::adapt_exchange_depth_snapshot(baseline_wire(), spec(), spot_identity());
    static_cast<void>(std::get<adapter::AdaptedBookBaseline>(baseline).install_into(projection));
    const auto prior_locale = std::locale();
    std::locale::global(std::locale{std::locale::classic(), new CommaDecimalPoint});
    const auto output = adapter::make_local_order_book_snapshot(projection, context(), {});
    std::locale::global(prior_locale);
    ASSERT_TRUE(std::holds_alternative<core::LocalOrderBookSnapshot>(output));
    EXPECT_EQ(std::get<core::LocalOrderBookSnapshot>(output).bids(0).price(), "100.00");
}

} // namespace
