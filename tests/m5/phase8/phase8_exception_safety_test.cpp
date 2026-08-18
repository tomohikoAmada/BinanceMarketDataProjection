#include <gtest/gtest.h>

#include "phase8_absl_btree_map.hpp"
#include "phase8_sorted_vector_batch_lww.hpp"
#include "phase8_sorted_vector_naive.hpp"
#include "phase8_std_map_control.hpp"
#include "phase8_test_common.hpp"
#include "phase8_throwing_allocator.hpp"

#include <new>
#include <type_traits>
#include <vector>

namespace {

namespace core = binance_market_data::projection::v1;
using bmd_projection::m5::phase8::test::L;
using bmd_projection::m5::phase8::test::Levels;
using bmd_projection::m5::phase8::test::P;
using bmd_projection::m5::phase8::test::Q;
using bmd_projection::m5::phase8::test::TestSpec;
using bmd_projection::m5::phase8::test::ThrowingAlloc;
using bmd_projection::m5::phase8::test::ThrowingFailpoint;

using ThrowingModelTypes =
    ::testing::Types<bmd_projection::m5::phase8::Phase8StdMapControl<ThrowingAlloc>,
                     bmd_projection::m5::phase8::Phase8SortedVectorNaive<ThrowingAlloc>,
                     bmd_projection::m5::phase8::Phase8AbslBtreeMap<ThrowingAlloc>,
                     bmd_projection::m5::phase8::Phase8SortedVectorBatchLww<ThrowingAlloc>>;

} // namespace

template <typename Model> class Phase8ExceptionSafetyTest : public ::testing::Test {
  protected:
    using model_type = Model;

    // The injected test allocator is stateless (is_always_equal), so every
    // model keeps its accepted noexcept move ownership contract even in the
    // throwing variant.
    static_assert(bmd_projection::m5::phase8::Phase8ModelConcept<Model>,
                  "throwing-allocator model must still satisfy the shared protocol");
    static_assert(std::is_nothrow_move_constructible_v<Model>,
                  "throwing-allocator model must keep noexcept move construction");
    static_assert(std::is_nothrow_move_assignable_v<Model>,
                  "throwing-allocator model must keep noexcept move assignment");
};

TYPED_TEST_SUITE(Phase8ExceptionSafetyTest, ThrowingModelTypes);

// A replace_all whose temporary-model construction fails halfway must leave the
// original model completely unchanged: NumericSpec, bids, and asks.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TYPED_TEST(Phase8ExceptionSafetyTest, FailedReplaceAllLeavesStateUnchanged) {
    using model_type = typename TestFixture::model_type;

    ThrowingFailpoint::disarm();
    model_type model{TestSpec()};
    model.apply_level(core::BookSide::Bid, P(10), Q(1));
    model.apply_level(core::BookSide::Bid, P(30), Q(3));
    model.apply_level(core::BookSide::Ask, P(200), Q(2));

    const auto spec_before = model.numeric_spec();
    const auto bids_before = model.all_levels(core::BookSide::Bid);
    const auto asks_before = model.all_levels(core::BookSide::Ask);

    // Enough inputs to force several content allocations during the temporary
    // model construction.
    const auto bids = Levels({L(1, 1), L(2, 2), L(3, 3), L(4, 4), L(5, 5), L(6, 6)});
    const auto asks = Levels({L(11, 1), L(12, 2), L(13, 3), L(14, 4), L(15, 5)});

    for (const std::size_t budget : {std::size_t{0}, std::size_t{1}, std::size_t{3}}) {
        ThrowingFailpoint::arm(budget);
        EXPECT_THROW(
            { model.replace_all(bids, asks); }, std::bad_alloc)
            << "replace_all must throw under allocation budget " << budget;
        ThrowingFailpoint::disarm();

        EXPECT_EQ(model.numeric_spec(), spec_before)
            << "NumericSpec must be unchanged after failed replace_all (budget " << budget << ")";
        EXPECT_EQ(model.all_levels(core::BookSide::Bid), bids_before)
            << "bids must be unchanged after failed replace_all (budget " << budget << ")";
        EXPECT_EQ(model.all_levels(core::BookSide::Ask), asks_before)
            << "asks must be unchanged after failed replace_all (budget " << budget << ")";
    }

    // With the failpoint disarmed the same call genuinely commits, proving the
    // failure injection above actually broke a replace that would have changed
    // state.
    ThrowingFailpoint::disarm();
    model.replace_all(bids, asks);
    EXPECT_EQ(model.all_levels(core::BookSide::Bid),
              Levels({L(6, 6), L(5, 5), L(4, 4), L(3, 3), L(2, 2), L(1, 1)}));
    EXPECT_EQ(model.all_levels(core::BookSide::Ask),
              Levels({L(11, 1), L(12, 2), L(13, 3), L(14, 4), L(15, 5)}));

    ThrowingFailpoint::disarm();
}