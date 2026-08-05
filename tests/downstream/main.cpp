#include <binance_market_data/projection/v1/version.hpp>

#include <iostream>

int main() {
    std::cout << binance_market_data::projection::v1::library_version() << '\n';
    return binance_market_data::projection::v1::library_version().empty() ? 1 : 0;
}
