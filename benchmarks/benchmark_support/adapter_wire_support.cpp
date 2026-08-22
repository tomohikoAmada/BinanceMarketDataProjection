#include "adapter_wire_support.hpp"

#include <binance_market_data/common/v1/enums.pb.h>
#include <binance_market_data/common/v1/metadata.pb.h>
#include <binance_market_data/projection/v1/numeric/decimal_format.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace bmd_projection::m5::benchmark::adapter_support {
namespace {

namespace common_wire = binance_market_data::common::v1;
namespace replay = bmd_projection::m5::replay;

constexpr std::string_view kBenchmarkProducer{"phase6-benchmark"};
constexpr std::string_view kBenchmarkProducerVersion{"1"};

[[nodiscard]] core::DecimalScale required_scale(std::uint32_t value) {
    const auto scale = core::DecimalScale::create(value);
    if (!scale.has_value()) {
        std::abort();
    }
    return *scale;
}

[[nodiscard]] core::SequencePolicyKind core_policy(replay::SequencePolicy policy) noexcept {
    return policy == replay::SequencePolicy::Spot ? core::SequencePolicyKind::Spot
                                                  : core::SequencePolicyKind::UsdMPerpetual;
}

[[nodiscard]] common_wire::Market wire_market(replay::Market market) noexcept {
    return market == replay::Market::Spot ? common_wire::MARKET_SPOT
                                          : common_wire::MARKET_USD_M_PERPETUAL;
}

[[nodiscard]] common_wire::QualityFlag wire_quality(replay::HostQualityFact fact) noexcept {
    switch (fact) {
    case replay::HostQualityFact::Duplicate:
        return common_wire::QUALITY_FLAG_DUPLICATE;
    case replay::HostQualityFact::OutOfOrder:
        return common_wire::QUALITY_FLAG_OUT_OF_ORDER;
    case replay::HostQualityFact::OrderBookResync:
        return common_wire::QUALITY_FLAG_ORDERBOOK_RESYNC;
    case replay::HostQualityFact::SnapshotTooOld:
        return common_wire::QUALITY_FLAG_SNAPSHOT_TOO_OLD;
    case replay::HostQualityFact::BootstrapBufferOverflow:
        return common_wire::QUALITY_FLAG_BOOTSTRAP_BUFFER_OVERFLOW;
    case replay::HostQualityFact::RecoveredTail:
        return common_wire::QUALITY_FLAG_RECOVERED_TAIL;
    case replay::HostQualityFact::MalformedPayload:
        return common_wire::QUALITY_FLAG_MALFORMED_PAYLOAD;
    case replay::HostQualityFact::ExchangeTimeMissing:
        return common_wire::QUALITY_FLAG_EXCHANGE_TIME_MISSING;
    case replay::HostQualityFact::ReceiveClockDiscontinuity:
        return common_wire::QUALITY_FLAG_RECEIVE_CLOCK_DISCONTINUITY;
    case replay::HostQualityFact::SlowConsumerGap:
        return common_wire::QUALITY_FLAG_SLOW_CONSUMER_GAP;
    case replay::HostQualityFact::ProducerRestart:
        return common_wire::QUALITY_FLAG_PRODUCER_RESTART;
    case replay::HostQualityFact::Overlap:
        return common_wire::QUALITY_FLAG_OVERLAP;
    case replay::HostQualityFact::IdentityConflict:
        return common_wire::QUALITY_FLAG_IDENTITY_CONFLICT;
    }
    return common_wire::QUALITY_FLAG_UNSPECIFIED;
}

[[nodiscard]] adapter::HostQualityFact host_fact(replay::HostQualityFact fact) noexcept {
    switch (fact) {
    case replay::HostQualityFact::Duplicate:
        return adapter::HostQualityFact::Duplicate;
    case replay::HostQualityFact::OutOfOrder:
        return adapter::HostQualityFact::OutOfOrder;
    case replay::HostQualityFact::OrderBookResync:
        return adapter::HostQualityFact::OrderBookResync;
    case replay::HostQualityFact::SnapshotTooOld:
        return adapter::HostQualityFact::SnapshotTooOld;
    case replay::HostQualityFact::BootstrapBufferOverflow:
        return adapter::HostQualityFact::BootstrapBufferOverflow;
    case replay::HostQualityFact::RecoveredTail:
        return adapter::HostQualityFact::RecoveredTail;
    case replay::HostQualityFact::MalformedPayload:
        return adapter::HostQualityFact::MalformedPayload;
    case replay::HostQualityFact::ExchangeTimeMissing:
        return adapter::HostQualityFact::ExchangeTimeMissing;
    case replay::HostQualityFact::ReceiveClockDiscontinuity:
        return adapter::HostQualityFact::ReceiveClockDiscontinuity;
    case replay::HostQualityFact::SlowConsumerGap:
        return adapter::HostQualityFact::SlowConsumerGap;
    case replay::HostQualityFact::ProducerRestart:
        return adapter::HostQualityFact::ProducerRestart;
    case replay::HostQualityFact::Overlap:
        return adapter::HostQualityFact::Overlap;
    case replay::HostQualityFact::IdentityConflict:
        return adapter::HostQualityFact::IdentityConflict;
    }
    return adapter::HostQualityFact::Duplicate;
}

[[nodiscard]] adapter::GapRecoveryState recovery_state(replay::GapRecoveryState state) noexcept {
    switch (state) {
    case replay::GapRecoveryState::Synchronized:
        return adapter::GapRecoveryState::Synchronized;
    case replay::GapRecoveryState::ResyncRequired:
        return adapter::GapRecoveryState::ResyncRequired;
    case replay::GapRecoveryState::ResyncInProgress:
        return adapter::GapRecoveryState::ResyncInProgress;
    case replay::GapRecoveryState::Recovered:
        return adapter::GapRecoveryState::Recovered;
    case replay::GapRecoveryState::ResyncFailed:
        return adapter::GapRecoveryState::ResyncFailed;
    }
    return adapter::GapRecoveryState::Synchronized;
}

[[nodiscard]] adapter::SnapshotOrigin snapshot_origin(replay::SnapshotOrigin origin) noexcept {
    switch (origin) {
    case replay::SnapshotOrigin::GatewayLive:
        return adapter::SnapshotOrigin::GatewayLive;
    case replay::SnapshotOrigin::RecorderReplay:
        return adapter::SnapshotOrigin::RecorderReplay;
    case replay::SnapshotOrigin::HistoryReplay:
        return adapter::SnapshotOrigin::HistoryReplay;
    }
    return adapter::SnapshotOrigin::GatewayLive;
}

[[nodiscard]] std::string format_price_text(std::int64_t units) {
    const auto result = core::format_price(price_units(units), benchmark_numeric_spec().price_scale,
                                           kBenchmarkPriceScale);
    if (!std::holds_alternative<std::string>(result)) {
        std::abort();
    }
    return std::get<std::string>(result);
}

[[nodiscard]] std::string format_quantity_text(std::int64_t units) {
    const auto result = core::format_quantity(
        quantity_units(units), benchmark_numeric_spec().quantity_scale, kBenchmarkQuantityScale);
    if (!std::holds_alternative<std::string>(result)) {
        std::abort();
    }
    return std::get<std::string>(result);
}

void apply_pending_quality(market_wire::ExchangeDepthSnapshot& wire,
                           const std::vector<replay::HostQualityFact>& facts) {
    for (const auto fact : facts) {
        wire.add_quality_flags(wire_quality(fact));
    }
}

void apply_pending_quality(market_wire::DepthUpdate& wire,
                           const std::vector<replay::HostQualityFact>& facts) {
    for (const auto fact : facts) {
        wire.mutable_metadata()->add_quality_flags(wire_quality(fact));
    }
}

struct EntryBuilder final {
    core::NumericSpec conversion_spec;
    adapter::ExpectedIdentity expected;
    common_wire::Market market;
    std::string symbol;
    std::vector<replay::HostQualityFact>* pending_metadata;

    [[nodiscard]] PreconstructedEntry base(PreconstructedKind kind) const {
        PreconstructedEntry entry;
        entry.kind = kind;
        entry.conversion_spec = conversion_spec;
        entry.expected = expected;
        return entry;
    }

    [[nodiscard]] PreconstructedEntry operator()(const replay::InstallBaselineOp& install) {
        auto entry = base(PreconstructedKind::Baseline);
        auto& wire = entry.baseline_wire;
        wire.set_venue(common_wire::VENUE_BINANCE);
        wire.set_market(market);
        wire.set_symbol(symbol);
        wire.set_schema_version("exchange-depth-snapshot.v1");
        wire.set_producer(std::string{kBenchmarkProducer});
        wire.set_producer_version(std::string{kBenchmarkProducerVersion});
        wire.set_request_id("phase6-baseline-" + std::to_string(install.source.event_index));
        apply_pending_quality(wire, *pending_metadata);
        pending_metadata->clear();
        wire.set_last_update_id(install.last_update_id);
        for (const auto& level : install.bids) {
            auto* wire_level = wire.add_bids();
            wire_level->set_price(level.price);
            wire_level->set_quantity(level.quantity);
        }
        for (const auto& level : install.asks) {
            auto* wire_level = wire.add_asks();
            wire_level->set_price(level.price);
            wire_level->set_quantity(level.quantity);
        }
        return entry;
    }

    [[nodiscard]] PreconstructedEntry operator()(const replay::DepthUpdateOp& update) {
        auto entry = base(PreconstructedKind::Update);
        auto& wire = entry.update_wire;
        auto* metadata = wire.mutable_metadata();
        metadata->set_venue(common_wire::VENUE_BINANCE);
        metadata->set_market(market);
        metadata->set_symbol(symbol);
        metadata->set_producer(std::string{kBenchmarkProducer});
        metadata->set_producer_version(std::string{kBenchmarkProducerVersion});
        metadata->set_connection_id("phase6-connection-" +
                                    std::to_string(update.source.event_index));
        metadata->set_stream(common_wire::STREAM_DIFF_DEPTH);
        metadata->set_schema_version("depth-update.v1");
        apply_pending_quality(wire, *pending_metadata);
        pending_metadata->clear();
        wire.set_first_update_id(update.first_update_id);
        wire.set_final_update_id(update.final_update_id);
        if (update.previous_final.has_value()) {
            wire.set_previous_final_update_id(*update.previous_final);
        }
        for (const auto& level : update.levels) {
            auto* wire_level = level.side == replay::Side::Bid ? wire.add_bids() : wire.add_asks();
            wire_level->set_price(level.price);
            wire_level->set_quantity(level.quantity);
        }
        return entry;
    }

    [[nodiscard]] PreconstructedEntry operator()(const replay::RebaselineOp& rebaseline) {
        auto entry = base(PreconstructedKind::Rebaseline);
        auto& wire = entry.baseline_wire;
        wire.set_venue(common_wire::VENUE_BINANCE);
        wire.set_market(market);
        wire.set_symbol(symbol);
        wire.set_schema_version("exchange-depth-snapshot.v1");
        wire.set_producer(std::string{kBenchmarkProducer});
        wire.set_producer_version(std::string{kBenchmarkProducerVersion});
        wire.set_request_id("phase6-rebaseline-" + std::to_string(rebaseline.source.event_index));
        apply_pending_quality(wire, *pending_metadata);
        pending_metadata->clear();
        wire.set_last_update_id(rebaseline.last_update_id);
        for (const auto& level : rebaseline.bids) {
            auto* wire_level = wire.add_bids();
            wire_level->set_price(level.price);
            wire_level->set_quantity(level.quantity);
        }
        for (const auto& level : rebaseline.asks) {
            auto* wire_level = wire.add_asks();
            wire_level->set_price(level.price);
            wire_level->set_quantity(level.quantity);
        }
        return entry;
    }

    [[nodiscard]] PreconstructedEntry operator()(const replay::ResetOp& reset) const {
        (void)reset;
        return base(PreconstructedKind::Reset);
    }

    [[nodiscard]] PreconstructedEntry operator()(const replay::SnapshotRequestOp& snapshot) const {
        auto entry = base(PreconstructedKind::Snapshot);
        if (snapshot.depth_limit.has_value()) {
            const auto limit = adapter::DepthLimit::create(*snapshot.depth_limit);
            if (std::holds_alternative<adapter::AdapterError>(limit)) {
                std::abort();
            }
            entry.snapshot_options.depth_limit = std::get<adapter::DepthLimit>(limit);
        }
        entry.snapshot_options.host_quality_facts.reserve(snapshot.host_quality_facts.size());
        for (const auto fact : snapshot.host_quality_facts) {
            entry.snapshot_options.host_quality_facts.push_back(host_fact(fact));
        }
        entry.snapshot_context = {expected,
                                  snapshot.producer,
                                  snapshot.producer_version,
                                  snapshot_origin(snapshot.source_origin),
                                  snapshot.generated_time_utc_ns,
                                  snapshot.generated_monotonic_ns,
                                  std::nullopt};
        if (snapshot.current_gap.has_value()) {
            entry.snapshot_context.current_gap = adapter::CurrentGapContext{
                snapshot.current_gap->first, recovery_state(snapshot.current_gap->second)};
        }
        return entry;
    }

    [[nodiscard]] PreconstructedEntry
    operator()(const replay::AdapterMetadataOp& metadata_op) const {
        *pending_metadata = metadata_op.observed_quality;
        return base(PreconstructedKind::Metadata);
    }

    [[nodiscard]] PreconstructedEntry operator()(const replay::MalformedRangeOp& malformed) const {
        auto entry = base(PreconstructedKind::MalformedRange);
        entry.malformed_first = malformed.first_update_id;
        entry.malformed_final = malformed.final_update_id;
        return entry;
    }
};

} // namespace

// Empty vectors are default-initialized; spelling empty initializers conflicts
// with readability-redundant-member-init under the CI clang-tidy policy.
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
PreconstructedEntry::PreconstructedEntry()
    : snapshot_context{{"", core::SequencePolicyKind::Spot},
                       "",
                       "",
                       adapter::SnapshotOrigin::GatewayLive,
                       0,
                       std::nullopt,
                       std::nullopt},
      snapshot_options{std::nullopt, {}}, conversion_spec{required_scale(2), required_scale(3)},
      expected{"", core::SequencePolicyKind::Spot} {}

WireIdentity benchmark_wire_identity() {
    return {benchmark_numeric_spec(), {"BTCUSDT", core::SequencePolicyKind::Spot}};
}

adapter::SnapshotContext benchmark_snapshot_context() {
    return {{"BTCUSDT", core::SequencePolicyKind::Spot},
            std::string{kBenchmarkProducer},
            std::string{kBenchmarkProducerVersion},
            adapter::SnapshotOrigin::GatewayLive,
            1'234'567,
            std::uint64_t{654'321},
            std::nullopt};
}

adapter::SnapshotOptions benchmark_limited_snapshot_options() {
    const auto limit =
        adapter::DepthLimit::create(static_cast<std::int64_t>(kM4LimitedSnapshotDepthLimit));
    if (std::holds_alternative<adapter::AdapterError>(limit)) {
        std::abort();
    }
    adapter::SnapshotOptions options;
    options.depth_limit = std::get<adapter::DepthLimit>(limit);
    return options;
}

bool benchmark_limited_snapshot_matches_identity(const adapter::SnapshotOptions& options,
                                                 std::size_t depth) {
    if (!options.depth_limit.has_value()) {
        return false;
    }
    const auto depth_limit = options.depth_limit.value();
    const auto expected_marker = "depth_limit=" + std::to_string(depth_limit.value()) + '\n';
    return m4_generated_workload_description("MakeLocalOrderBookSnapshot/Limited", depth)
               .find(expected_marker) != std::string::npos;
}

market_wire::ExchangeDepthSnapshot make_snapshot_wire(std::size_t depth) {
    const BookParams params{};
    market_wire::ExchangeDepthSnapshot wire;
    wire.set_venue(common_wire::VENUE_BINANCE);
    wire.set_market(common_wire::MARKET_SPOT);
    wire.set_symbol("BTCUSDT");
    wire.set_schema_version("exchange-depth-snapshot.v1");
    wire.set_producer(std::string{kBenchmarkProducer});
    wire.set_producer_version(std::string{kBenchmarkProducerVersion});
    wire.set_request_id("phase6-baseline");
    wire.set_last_update_id(params.base_update_id);
    for (std::size_t index = 0; index < depth; ++index) {
        auto* bid = wire.add_bids();
        bid->set_price(format_price_text(params.bid_start - static_cast<std::int64_t>(index)));
        bid->set_quantity(format_quantity_text(params.quantity_base));
        auto* ask = wire.add_asks();
        ask->set_price(format_price_text(params.ask_start + static_cast<std::int64_t>(index)));
        ask->set_quantity(format_quantity_text(params.quantity_base));
    }
    return wire;
}

// The adjacent ids intentionally follow the exchange wire's first/final order.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
market_wire::DepthUpdate make_update_wire(std::uint64_t first_update_id,
                                          std::uint64_t final_update_id,
                                          std::optional<std::uint64_t> previous_final_update_id) {
    const BookParams params{};
    market_wire::DepthUpdate wire;
    auto* metadata = wire.mutable_metadata();
    metadata->set_venue(common_wire::VENUE_BINANCE);
    metadata->set_market(common_wire::MARKET_SPOT);
    metadata->set_symbol("BTCUSDT");
    metadata->set_producer(std::string{kBenchmarkProducer});
    metadata->set_producer_version(std::string{kBenchmarkProducerVersion});
    metadata->set_connection_id("phase6-connection");
    metadata->set_stream(common_wire::STREAM_DIFF_DEPTH);
    metadata->set_schema_version("depth-update.v1");
    wire.set_first_update_id(first_update_id);
    wire.set_final_update_id(final_update_id);
    if (previous_final_update_id.has_value()) {
        wire.set_previous_final_update_id(*previous_final_update_id);
    }
    for (std::size_t index = 0; index < kM4UpdateLevelCount; ++index) {
        auto* bid = wire.add_bids();
        bid->set_price(format_price_text(params.bid_start - static_cast<std::int64_t>(index)));
        bid->set_quantity(
            format_quantity_text(params.quantity_base + 1 + static_cast<std::int64_t>(index)));
    }
    return wire;
}

market_wire::DepthUpdate make_update_wire(const M4SpotDepthUpdateCell& cell) {
    return make_update_wire(cell.first_update_id, cell.final_update_id,
                            cell.previous_final_update_id);
}

std::vector<std::pair<std::string, std::string>> checked_apply_canonical_sequence_fields() {
    return {
        {"policy", "Spot"},
        {"initial_update_id", std::to_string(kM4CheckedApplyPreparedUpdateId)},
        {"first_update_id", std::to_string(kM4CheckedApplyCell.first_update_id)},
        {"final_update_id", std::to_string(kM4CheckedApplyCell.final_update_id)},
        {"previous_final_update_id", "not_applicable"},
    };
}

std::string m4_generated_workload_description(std::string_view family, std::size_t depth) {
    std::string concrete = "m4_cell_v1\nfamily=" + std::string{family} + '\n';
    if (family == "AdaptDepthUpdate/Spot") {
        concrete += "update_level_count=" + std::to_string(kM4UpdateLevelCount) + '\n';
    } else {
        concrete += "depth=" + std::to_string(depth) + '\n';
    }
    const auto append_bytes = [&concrete](std::string_view label, const std::string& bytes) {
        concrete += label;
        concrete += '_';
        concrete += std::to_string(bytes.size());
        concrete += ':';
        concrete += bytes;
        concrete += '\n';
    };
    if (family == "AdaptExchangeDepthSnapshot/Spot" || family == "CheckedInstall") {
        append_bytes("snapshot_wire", make_snapshot_wire(depth).SerializeAsString());
    } else if (family == "AdaptDepthUpdate/Spot" || family == "CheckedApply") {
        const auto cell = family == "CheckedApply" ? kM4CheckedApplyCell : kM4AdaptDepthUpdateCell;
        append_bytes("update_wire", make_update_wire(cell).SerializeAsString());
        if (family == "CheckedApply") {
            concrete += "initial_bids=" + describe_levels(build_bid_levels(depth)) +
                        "\ninitial_asks=" + describe_levels(build_ask_levels(depth)) +
                        "\ninitial_update_id=" + std::to_string(kM4CheckedApplyPreparedUpdateId) +
                        "\npolicy=Spot\n";
        }
    } else {
        concrete += "projection_bids=" + describe_levels(build_bid_levels(depth)) +
                    "\nprojection_asks=" + describe_levels(build_ask_levels(depth)) +
                    "\nprojection_update_id=1000001\npolicy=Spot\n";
        if (family == "MakeLocalOrderBookSnapshot/Limited") {
            concrete += "depth_limit=" + std::to_string(kM4LimitedSnapshotDepthLimit) + '\n';
        } else {
            concrete += "depth_limit=unlimited\n";
        }
    }
    return concrete;
}

std::vector<PreconstructedEntry> preconstruct_adapter_wire(const replay::ReplayFixture& fixture) {
    const auto conversion_spec =
        core::NumericSpec{required_scale(fixture.identity.numeric_spec.price_scale),
                          required_scale(fixture.identity.numeric_spec.quantity_scale)};
    const auto expected = adapter::ExpectedIdentity{fixture.identity.symbol,
                                                    core_policy(fixture.identity.sequence_policy)};
    const auto wire_market_value = wire_market(fixture.identity.market);

    std::vector<PreconstructedEntry> entries;
    entries.reserve(fixture.replay.operations.size());
    std::vector<replay::HostQualityFact> pending_metadata;
    EntryBuilder builder{conversion_spec, expected, wire_market_value, fixture.identity.symbol,
                         &pending_metadata};
    for (const auto& operation : fixture.replay.operations) {
        entries.push_back(std::visit(builder, operation));
    }
    return entries;
}

} // namespace bmd_projection::m5::benchmark::adapter_support
