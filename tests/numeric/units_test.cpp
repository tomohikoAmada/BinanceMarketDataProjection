#include <binance_market_data/projection/v1/numeric/price_units.hpp>
#include <binance_market_data/projection/v1/numeric/quantity_units.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <type_traits>

namespace bmd = binance_market_data::projection::v1;

static_assert(std::is_trivially_copyable_v<bmd::PriceUnits>);
static_assert(std::is_trivially_copyable_v<bmd::QuantityUnits>);
static_assert(std::is_standard_layout_v<bmd::PriceUnits>);
static_assert(std::is_standard_layout_v<bmd::QuantityUnits>);
static_assert(sizeof(bmd::PriceUnits) == sizeof(std::int64_t));
static_assert(sizeof(bmd::QuantityUnits) == sizeof(std::int64_t));
static_assert(!std::is_convertible_v<bmd::PriceUnits, bmd::QuantityUnits>);
static_assert(!std::is_convertible_v<bmd::QuantityUnits, bmd::PriceUnits>);
static_assert(!std::is_convertible_v<std::int64_t, bmd::PriceUnits>);
static_assert(!std::is_convertible_v<std::int64_t, bmd::QuantityUnits>);
static_assert(!std::is_convertible_v<bmd::PriceUnits, std::int64_t>);
static_assert(!std::is_convertible_v<bmd::QuantityUnits, std::int64_t>);

TEST(PriceUnitsTest, RequiresStrictlyPositiveValue) {
    EXPECT_FALSE(bmd::PriceUnits::create(-1).has_value());
    EXPECT_FALSE(bmd::PriceUnits::create(0).has_value());
    EXPECT_EQ(bmd::PriceUnits::create(1)->value(), 1);
}

TEST(PriceUnitsTest, AcceptsInt64Maximum) {
    const auto value = bmd::PriceUnits::create(std::numeric_limits<std::int64_t>::max());
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->value(), std::numeric_limits<std::int64_t>::max());
}

TEST(PriceUnitsTest, ComparesByUnits) {
    EXPECT_LT(*bmd::PriceUnits::create(10), *bmd::PriceUnits::create(11));
    EXPECT_EQ(*bmd::PriceUnits::create(11), *bmd::PriceUnits::create(11));
}

TEST(QuantityUnitsTest, AllowsZeroAndRejectsNegativeValue) {
    EXPECT_FALSE(bmd::QuantityUnits::create(-1).has_value());
    EXPECT_EQ(bmd::QuantityUnits::create(0)->value(), 0);
}

TEST(QuantityUnitsTest, AcceptsInt64Maximum) {
    const auto value = bmd::QuantityUnits::create(std::numeric_limits<std::int64_t>::max());
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->value(), std::numeric_limits<std::int64_t>::max());
}

TEST(QuantityUnitsTest, ComparesByUnits) {
    EXPECT_LT(*bmd::QuantityUnits::create(0), *bmd::QuantityUnits::create(1));
    EXPECT_EQ(*bmd::QuantityUnits::create(1), *bmd::QuantityUnits::create(1));
}
