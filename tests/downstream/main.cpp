#include <binance_market_data/projection/v1/numeric/decimal_format.hpp>
#include <binance_market_data/projection/v1/numeric/decimal_parse.hpp>
#include <binance_market_data/projection/v1/numeric/decimal_scale.hpp>
#include <binance_market_data/projection/v1/order_book/order_book.hpp>
#include <binance_market_data/projection/v1/version.hpp>

#include <iostream>
#include <string>
#include <variant>

int main() {
    namespace bmd = binance_market_data::projection::v1;

    std::cout << binance_market_data::projection::v1::library_version() << '\n';
    if (bmd::library_version().empty()) {
        return 1;
    }

    const auto scale = bmd::DecimalScale::create(8);
    if (!scale.has_value()) {
        return 2;
    }
    const auto parsed = bmd::parse_price("1.2300", *scale);
    if (!std::holds_alternative<bmd::ParsedDecimal<bmd::PriceUnits>>(parsed)) {
        return 3;
    }
    const auto value = std::get<bmd::ParsedDecimal<bmd::PriceUnits>>(parsed);
    const auto formatted = bmd::format_price(value.value, *scale, value.source_fraction_digits);
    if (!std::holds_alternative<std::string>(formatted)) {
        return 4;
    }
    if (std::get<std::string>(formatted) != "1.2300") {
        return 5;
    }

    const auto price_scale = bmd::DecimalScale::create(8);
    const auto qty_scale = bmd::DecimalScale::create(8);
    if (!price_scale.has_value() || !qty_scale.has_value()) {
        return 6;
    }
    bmd::NumericSpec spec{*price_scale, *qty_scale};
    bmd::OrderBook book{spec};

    const auto bid_price = bmd::PriceUnits::create(100000000);
    const auto bid_qty = bmd::QuantityUnits::create(500000000);
    if (!bid_price.has_value() || !bid_qty.has_value()) {
        return 7;
    }
    static_cast<void>(book.apply_level(bmd::BookSide::Bid, *bid_price, *bid_qty));

    const auto ask_price = bmd::PriceUnits::create(101000000);
    const auto ask_qty = bmd::QuantityUnits::create(300000000);
    if (!ask_price.has_value() || !ask_qty.has_value()) {
        return 8;
    }
    static_cast<void>(book.apply_level(bmd::BookSide::Ask, *ask_price, *ask_qty));

    const auto best_bid = book.best_bid();
    if (!best_bid.has_value() || !(best_bid->price == *bid_price) ||
        !(best_bid->quantity == *bid_qty)) {
        return 9;
    }

    const auto best_ask = book.best_ask();
    if (!best_ask.has_value() || !(best_ask->price == *ask_price) ||
        !(best_ask->quantity == *ask_qty)) {
        return 10;
    }

    static_cast<void>(book.apply_level(bmd::BookSide::Bid, *bid_price,
                                       bmd::QuantityUnits::create(0).value()));
    if (book.best_bid().has_value()) {
        return 11;
    }

    const auto top_bids = book.top_levels(bmd::BookSide::Bid, 5);
    if (!top_bids.empty()) {
        return 12;
    }

    bmd::BookLevel crossed_bid{*bmd::PriceUnits::create(100000000),
                               *bmd::QuantityUnits::create(10)};
    bmd::BookLevel crossed_ask{*bmd::PriceUnits::create(99000000),
                               *bmd::QuantityUnits::create(20)};
    const std::vector<bmd::BookLevel> crossed_bids{crossed_bid};
    const std::vector<bmd::BookLevel> crossed_asks{crossed_ask};
    book.replace_all(crossed_bids, crossed_asks);
    if (book.best_bid().value().price.value() <= book.best_ask().value().price.value()) {
        return 13;
    }

    std::cout << "consumer-ok\n";
    return 0;
}
