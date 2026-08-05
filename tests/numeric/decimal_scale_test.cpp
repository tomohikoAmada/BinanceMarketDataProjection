#include <binance_market_data/projection/v1/numeric/decimal_scale.hpp>
#include <binance_market_data/projection/v1/numeric/numeric_spec.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <type_traits>

namespace bmd = binance_market_data::projection::v1;

static_assert(std::is_trivially_copyable_v<bmd::DecimalScale>);
static_assert(std::is_standard_layout_v<bmd::DecimalScale>);
static_assert(!std::is_convertible_v<std::uint32_t, bmd::DecimalScale>);

TEST(DecimalScaleTest, AcceptsInclusiveSupportedRangeSamples) {
    for (const std::uint32_t value : {0U, 1U, 8U, 18U}) {
        SCOPED_TRACE(value);
        const auto scale = bmd::DecimalScale::create(value);
        ASSERT_TRUE(scale.has_value());
        EXPECT_EQ(scale->value(), value);
    }
}

TEST(DecimalScaleTest, RejectsValuesAboveMaximum) {
    EXPECT_FALSE(bmd::DecimalScale::create(19).has_value());
    EXPECT_FALSE(bmd::DecimalScale::create(std::numeric_limits<std::uint32_t>::max()).has_value());
}

TEST(DecimalScaleTest, NumericSpecStoresIndependentExplicitScales) {
    const auto price_scale = *bmd::DecimalScale::create(8);
    const auto quantity_scale = *bmd::DecimalScale::create(3);
    const bmd::NumericSpec first{price_scale, quantity_scale};
    const bmd::NumericSpec second{price_scale, quantity_scale};
    EXPECT_EQ(first, second);
    EXPECT_EQ(first.price_scale.value(), 8);
    EXPECT_EQ(first.quantity_scale.value(), 3);
}

TEST(DecimalScaleTest, MaximumIsEighteen) { EXPECT_EQ(bmd::kMaxDecimalScale, 18); }
