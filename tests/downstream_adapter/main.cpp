#include <binance_market_data/projection_adapter/v1/proto_adapter.hpp>

#include <binance_market_data/common/v1/enums.pb.h>
#include <binance_market_data/market/v1/market_events.pb.h>

#include <optional>
#include <variant>

int main() {
    namespace adapter = binance_market_data::projection_adapter::v1;
    namespace core = binance_market_data::projection::v1;
    namespace common_wire = binance_market_data::common::v1;
    namespace market_wire = binance_market_data::market::v1;

    const auto price_scale = core::DecimalScale::create(2);
    const auto quantity_scale = core::DecimalScale::create(3);
    if (!price_scale.has_value() || !quantity_scale.has_value()) {
        return 1;
    }
    const core::NumericSpec spec{*price_scale, *quantity_scale};
    const adapter::ExpectedIdentity identity{"BTCUSDT", core::SequencePolicyKind::Spot};

    market_wire::ExchangeDepthSnapshot wire;
    wire.set_venue(common_wire::VENUE_BINANCE);
    wire.set_market(common_wire::MARKET_SPOT);
    wire.set_symbol("BTCUSDT");
    wire.set_schema_version("exchange-depth-snapshot.v1");
    wire.set_producer("consumer");
    wire.set_producer_version("1");
    wire.set_request_id("request-1");
    wire.set_last_update_id(10);
    auto* bid = wire.add_bids();
    bid->set_price("100.00");
    bid->set_quantity("1.000");

    auto adapted = adapter::adapt_exchange_depth_snapshot(wire, spec, identity);
    if (!std::holds_alternative<adapter::AdaptedBookBaseline>(adapted)) {
        return 2;
    }
    core::BookProjection projection{spec, core::SequencePolicyKind::Spot};
    const auto installed = std::get<adapter::AdaptedBookBaseline>(adapted).install_into(projection);
    if (!std::holds_alternative<core::InstallResult>(installed)) {
        return 3;
    }

    const adapter::SnapshotContext context{
        identity, "consumer",   "1",         adapter::SnapshotOrigin::HistoryReplay,
        123,      std::nullopt, std::nullopt};
    const auto snapshot = adapter::make_local_order_book_snapshot(projection, context, {});
    if (!std::holds_alternative<core::LocalOrderBookSnapshot>(snapshot)) {
        return 4;
    }
    const auto& output = std::get<core::LocalOrderBookSnapshot>(snapshot);
    return output.last_update_id() == 10 && output.bids_size() == 1 && !output.synchronized() ? 0
                                                                                              : 5;
}
