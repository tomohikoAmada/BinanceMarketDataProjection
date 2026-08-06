#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>

#include <gtest/gtest.h>

#include <concepts>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace bmd = binance_market_data::projection::v1;

// NOLINTBEGIN(bugprone-unchecked-optional-access)

static_assert(!std::is_convertible_v<std::uint64_t, bmd::UpdateId>);
static_assert(std::three_way_comparable<bmd::UpdateId>);
static_assert(!std::is_aggregate_v<bmd::UpdateRange>);
static_assert(!std::is_default_constructible_v<bmd::BookProjection>);
static_assert(!std::is_copy_constructible_v<bmd::BookProjection>);
static_assert(!std::is_copy_assignable_v<bmd::BookProjection>);
static_assert(std::is_nothrow_move_constructible_v<bmd::BookProjection>);
static_assert(std::is_nothrow_move_assignable_v<bmd::BookProjection>);
static_assert(static_cast<std::uint8_t>(bmd::SequencePolicyKind::Spot) == 0);
static_assert(static_cast<std::uint8_t>(bmd::SequencePolicyKind::UsdMPerpetual) == 1);

constexpr auto kZeroRange = bmd::UpdateRange::try_create(bmd::UpdateId{0}, bmd::UpdateId{0});
constexpr auto kFullRange =
    bmd::UpdateRange::try_create(bmd::UpdateId{0}, bmd::UpdateId{UINT64_MAX});
constexpr auto kMaxRange =
    bmd::UpdateRange::try_create(bmd::UpdateId{UINT64_MAX}, bmd::UpdateId{UINT64_MAX});
static_assert(kZeroRange.has_value());
static_assert(kFullRange.has_value());
static_assert(kMaxRange.has_value());
static_assert(kFullRange->first() == bmd::UpdateId{0});
static_assert(kFullRange->final() == bmd::UpdateId{UINT64_MAX});

TEST(UpdateIdTest, SupportsEntireUnsignedRangeAndOrdering) {
    const bmd::UpdateId zero{0};
    const bmd::UpdateId one{1};
    const bmd::UpdateId maximum{std::numeric_limits<std::uint64_t>::max()};

    EXPECT_EQ(zero.value(), 0U);
    EXPECT_EQ(maximum.value(), std::numeric_limits<std::uint64_t>::max());
    EXPECT_LT(zero, one);
    EXPECT_LT(one, maximum);
    EXPECT_EQ(one, bmd::UpdateId{1});
}

TEST(UpdateRangeTest, ValidRangesExposeInclusiveEndpoints) {
    const auto zero = bmd::UpdateRange::try_create(bmd::UpdateId{0}, bmd::UpdateId{0});
    const auto increasing = bmd::UpdateRange::try_create(bmd::UpdateId{4}, bmd::UpdateId{9});
    const auto maximum =
        bmd::UpdateRange::try_create(bmd::UpdateId{std::numeric_limits<std::uint64_t>::max()},
                                     bmd::UpdateId{std::numeric_limits<std::uint64_t>::max()});

    ASSERT_TRUE(zero.has_value());
    ASSERT_TRUE(increasing.has_value());
    ASSERT_TRUE(maximum.has_value());
    EXPECT_EQ(zero->first(), bmd::UpdateId{0});
    EXPECT_EQ(increasing->first(), bmd::UpdateId{4});
    EXPECT_EQ(increasing->final(), bmd::UpdateId{9});
    EXPECT_EQ(maximum->final(), bmd::UpdateId{UINT64_MAX});
    EXPECT_EQ(*increasing, *bmd::UpdateRange::try_create(bmd::UpdateId{4}, bmd::UpdateId{9}));
}

TEST(UpdateRangeTest, RejectsDescendingEndpoints) {
    EXPECT_FALSE(bmd::UpdateRange::try_create(bmd::UpdateId{9}, bmd::UpdateId{4}).has_value());
}

// NOLINTEND(bugprone-unchecked-optional-access)
