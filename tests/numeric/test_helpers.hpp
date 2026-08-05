#pragma once

#include <binance_market_data/projection/v1/numeric/decimal_error.hpp>
#include <binance_market_data/projection/v1/numeric/decimal_scale.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>

namespace bmd_test {

[[nodiscard]] inline binance_market_data::projection::v1::DecimalScale scale(std::uint32_t value) {
    const auto result = binance_market_data::projection::v1::DecimalScale::create(value);
    EXPECT_TRUE(result.has_value());
    return *result;
}

template <typename Result>
void expect_error(const Result& result,
                  binance_market_data::projection::v1::DecimalErrorCode expected_code,
                  std::size_t expected_offset) {
    using binance_market_data::projection::v1::DecimalError;
    ASSERT_TRUE(std::holds_alternative<DecimalError>(result));
    EXPECT_EQ(std::get<DecimalError>(result), (DecimalError{expected_code, expected_offset}));
}

} // namespace bmd_test
