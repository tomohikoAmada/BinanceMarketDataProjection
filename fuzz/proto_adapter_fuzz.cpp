#include <binance_market_data/projection_adapter/v1/proto_adapter.hpp>

#include <binance_market_data/common/v1/enums.pb.h>

#include "test_helpers.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace adapter = binance_market_data::projection_adapter::v1;
namespace core = binance_market_data::projection::v1;
namespace common_wire = binance_market_data::common::v1;
namespace market_wire = binance_market_data::market::v1;
namespace helper = bmd_projection_test;

namespace {

[[nodiscard]] core::DecimalScale scale(std::uint8_t value) {
    const auto result = core::DecimalScale::create(static_cast<std::uint32_t>(value % 19U));
    if (!result.has_value()) {
        std::abort();
    }
    return *result;
}

[[nodiscard]] std::string decimal(std::uint8_t whole, std::uint8_t fraction, bool zero) {
    if (zero) {
        return "0";
    }
    return std::to_string(static_cast<unsigned int>(whole % 100U) + 1U) + "." +
           std::to_string(static_cast<unsigned int>(fraction % 10U));
}

[[nodiscard]] std::uint64_t id(const std::uint8_t* data) noexcept {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < 8; ++index) {
        value |= static_cast<std::uint64_t>(data[index]) << (index * 8U);
    }
    return value;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size < 24) {
        return 0;
    }

    const core::NumericSpec spec{scale(data[0]), scale(data[1])};
    const bool usd_m = (data[2] & 1U) != 0U;
    const auto policy =
        usd_m ? core::SequencePolicyKind::UsdMPerpetual : core::SequencePolicyKind::Spot;
    const auto market = usd_m ? common_wire::MARKET_USD_M_PERPETUAL : common_wire::MARKET_SPOT;
    const adapter::ExpectedIdentity expected{"BTCUSDT", policy};

    market_wire::ExchangeDepthSnapshot baseline;
    baseline.set_venue((data[3] & 4U) != 0U ? common_wire::VENUE_UNSPECIFIED
                                            : common_wire::VENUE_BINANCE);
    baseline.set_market(market);
    baseline.set_symbol((data[3] & 8U) != 0U ? "bad symbol" : "BTCUSDT");
    baseline.set_schema_version((data[3] & 16U) != 0U ? "wrong" : "exchange-depth-snapshot.v1");
    baseline.set_producer("fuzzer");
    baseline.set_producer_version("1");
    baseline.set_request_id("request-1");
    baseline.set_last_update_id(id(data + 4));
    auto* baseline_bid = baseline.add_bids();
    baseline_bid->set_price(decimal(data[12], data[13], (data[3] & 32U) != 0U));
    baseline_bid->set_quantity(decimal(data[14], data[15], (data[3] & 64U) != 0U));

    core::BookProjection projection{spec, policy};
    const auto before_baseline = helper::checkpoint(projection);
    auto adapted_baseline = adapter::adapt_exchange_depth_snapshot(baseline, spec, expected);
    if (std::holds_alternative<adapter::AdaptedBookBaseline>(adapted_baseline)) {
        auto owner = std::move(std::get<adapter::AdaptedBookBaseline>(adapted_baseline));
        baseline.Clear();
        const auto installed = owner.install_into(projection);
        if (!std::holds_alternative<core::InstallResult>(installed)) {
            std::abort();
        }
    } else if (helper::checkpoint(projection) != before_baseline) {
        std::abort();
    }

    market_wire::DepthUpdate update;
    auto* metadata = update.mutable_metadata();
    metadata->set_venue(common_wire::VENUE_BINANCE);
    metadata->set_market(market);
    metadata->set_symbol("BTCUSDT");
    metadata->set_producer("fuzzer");
    metadata->set_producer_version("1");
    metadata->set_connection_id("connection-1");
    metadata->set_stream((data[16] & 1U) != 0U ? common_wire::STREAM_AGG_TRADE
                                               : common_wire::STREAM_DIFF_DEPTH);
    metadata->set_schema_version("depth-update.v1");
    update.set_first_update_id(id(data + 8));
    update.set_final_update_id(id(data + 16));
    if ((data[17] & 1U) != 0U) {
        update.set_previous_final_update_id(id(data + 4));
    }
    auto* level = ((data[18] & 1U) != 0U) ? update.add_asks() : update.add_bids();
    level->set_price(decimal(data[19], data[20], false));
    level->set_quantity(decimal(data[21], data[22], (data[23] & 1U) != 0U));

    const auto before_update = helper::checkpoint(projection);
    auto adapted_update = adapter::adapt_depth_update(update, spec, expected);
    if (std::holds_alternative<adapter::AdaptedDepthBatch>(adapted_update)) {
        auto owner = std::move(std::get<adapter::AdaptedDepthBatch>(adapted_update));
        const core::NumericSpec wrong_spec{scale(static_cast<std::uint8_t>(data[0] + 1U)),
                                           spec.quantity_scale};
        core::BookProjection wrong_target{wrong_spec, policy};
        const auto wrong_before = helper::checkpoint(wrong_target);
        const auto rejected_binding = owner.apply_to(wrong_target);
        if (!std::holds_alternative<adapter::AdapterError>(rejected_binding) ||
            helper::checkpoint(wrong_target) != wrong_before) {
            std::abort();
        }
        update.Clear();
        static_cast<void>(owner.apply_to(projection));
    } else if (helper::checkpoint(projection) != before_update) {
        std::abort();
    }

    adapter::SnapshotContext context{
        expected,     "fuzzer",     "1",         adapter::SnapshotOrigin::HistoryReplay,
        id(data + 8), std::nullopt, std::nullopt};
    if (projection.status() == core::ProjectionStatus::NeedsResync) {
        context.current_gap =
            adapter::CurrentGapContext{id(data + 16), adapter::GapRecoveryState::ResyncRequired};
    }
    adapter::SnapshotOptions options;
    if ((data[23] & 2U) != 0U) {
        options.depth_limit = std::get<adapter::DepthLimit>(adapter::DepthLimit::create(1));
    }
    const auto before_output = helper::checkpoint(projection);
    static_cast<void>(adapter::make_local_order_book_snapshot(projection, context, options));
    if (helper::checkpoint(projection) != before_output) {
        std::abort();
    }
    if ((data[23] & 4U) != 0U) {
        projection.reset();
        const auto reset_checkpoint = helper::checkpoint(projection);
        const auto reset_output =
            adapter::make_local_order_book_snapshot(projection, context, options);
        if (!std::holds_alternative<adapter::AdapterError>(reset_output) ||
            helper::checkpoint(projection) != reset_checkpoint) {
            std::abort();
        }
    }
    return 0;
}
