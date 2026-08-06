#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>

#include <concepts>
#include <cstdint>
#include <type_traits>

namespace bmd = binance_market_data::projection::v1;

static_assert(std::three_way_comparable<bmd::UpdateId>);
static_assert(!std::is_aggregate_v<bmd::UpdateRange>);
static_assert(!std::is_copy_constructible_v<bmd::BookProjection>);
static_assert(std::is_nothrow_move_constructible_v<bmd::BookProjection>);
static_assert(std::is_nothrow_move_assignable_v<bmd::BookProjection>);
static_assert(static_cast<std::uint8_t>(bmd::SequencePolicyKind::Spot) == 0);
static_assert(static_cast<std::uint8_t>(bmd::SequencePolicyKind::UsdMPerpetual) == 1);

int book_projection_header_self_containment_anchor() {
    constexpr auto range = bmd::UpdateRange::try_create(bmd::UpdateId{0}, bmd::UpdateId{1});
    return range.has_value() ? 0 : 1;
}
