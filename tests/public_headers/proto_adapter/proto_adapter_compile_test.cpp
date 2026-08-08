#include <binance_market_data/projection_adapter/v1/proto_adapter.hpp>

#include <binance_market_data/common/v1/enums.pb.h>
#include <binance_market_data/common/v1/metadata.pb.h>
#include <binance_market_data/market/v1/market_events.pb.h>
#include <binance_market_data/projection/v1/numeric/decimal_format.hpp>
#include <binance_market_data/projection/v1/numeric/decimal_parse.hpp>
#include <binance_market_data/projection/v1/order_book/order_book.hpp>
#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>
#include <binance_market_data/projection/v1/snapshots.pb.h>

#include <concepts>
#include <type_traits>

namespace adapter = binance_market_data::projection_adapter::v1;

template <typename Owner>
concept HasPublicUncheckedView = requires(const Owner& owner) { owner.view_unchecked(); };

static_assert(!std::is_copy_constructible_v<adapter::AdaptedBookBaseline>);
static_assert(std::is_nothrow_move_constructible_v<adapter::AdaptedBookBaseline>);
static_assert(!std::is_copy_constructible_v<adapter::AdaptedDepthBatch>);
static_assert(std::is_nothrow_move_constructible_v<adapter::AdaptedDepthBatch>);
static_assert(!HasPublicUncheckedView<adapter::AdaptedBookBaseline>);
static_assert(!HasPublicUncheckedView<adapter::AdaptedDepthBatch>);

int proto_adapter_header_self_containment_anchor() {
    return std::get<adapter::DepthLimit>(adapter::DepthLimit::create(1)).value();
}
