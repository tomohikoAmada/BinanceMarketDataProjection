#include <binance_market_data/projection/v1/numeric/decimal_format.hpp>
#include <binance_market_data/projection/v1/numeric/decimal_parse.hpp>
#include <binance_market_data/projection/v1/numeric/decimal_scale.hpp>
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
    return std::get<std::string>(formatted) == "1.2300" ? 0 : 5;
}
