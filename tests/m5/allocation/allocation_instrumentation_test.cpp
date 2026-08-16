// Adversarial validation of the Phase-7 allocation instrumentation substrate
// (OD-M5-P7-020). This executable owns the replacement global new/delete
// surface; GoogleTest itself allocates, so provenance baselines include
// framework allocations, measurement brackets contain only the deliberate
// measured operation, and rich assertions run only after the bracket is
// closed. Expected values are hand-computed literals.

#include "allocation_instrumentation.hpp"
#include "instrumentation_test_main.hpp"
#include "instrumentation_test_seams.hpp"

#include "../../projection_state/test_helpers.hpp"

#include <binance_market_data/projection/v1/order_book/order_book.hpp>
#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>

#include <gtest/gtest.h>

#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <new>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace allocation = bmd_projection::m5::allocation;
namespace seams = bmd_projection::m5::allocation::test;
namespace bmd = binance_market_data::projection::v1;

using allocation::LiveIneligibilityReason;
using allocation::MeasurementResult;
using allocation::MeasurementScope;
using allocation::PersistentLiveDelta;
using allocation::PersistentLiveDeltaSign;

// Redeclarations of the complete replaceable allocation/deallocation surface
// ([new.delete.single] / [new.delete.array]; OD-M5-P7-003) so the direct-call
// adversarial cases compile identically under every standard library and
// tidy/parse configuration: the declarations below pin the exact surface the
// cases call, independently of which forms the platform <new> happens to
// expose. They are exact redeclarations of the same functions defined by the
// replacement translation unit in this executable; any signature mismatch
// would fail at link time (fail closed). Sized aligned forms take
// (void*, std::size_t, std::align_val_t) — size BEFORE alignment.
// NOLINTBEGIN(readability-redundant-declaration)
void* operator new(std::size_t size);
void* operator new[](std::size_t size);
void* operator new(std::size_t size, const std::nothrow_t& tag) noexcept;
void* operator new[](std::size_t size, const std::nothrow_t& tag) noexcept;
void* operator new(std::size_t size, std::align_val_t alignment);
void* operator new[](std::size_t size, std::align_val_t alignment);
void* operator new(std::size_t size, std::align_val_t alignment,
                   const std::nothrow_t& tag) noexcept;
void* operator new[](std::size_t size, std::align_val_t alignment,
                     const std::nothrow_t& tag) noexcept;
void operator delete(void* ptr) noexcept;
void operator delete(void* ptr, std::size_t size) noexcept;
void operator delete(void* ptr, std::align_val_t alignment) noexcept;
void operator delete(void* ptr, std::size_t size, std::align_val_t alignment) noexcept;
void operator delete(void* ptr, const std::nothrow_t& tag) noexcept;
void operator delete(void* ptr, std::align_val_t alignment, const std::nothrow_t& tag) noexcept;
void operator delete[](void* ptr) noexcept;
void operator delete[](void* ptr, std::size_t size) noexcept;
void operator delete[](void* ptr, std::align_val_t alignment) noexcept;
void operator delete[](void* ptr, std::size_t size, std::align_val_t alignment) noexcept;
void operator delete[](void* ptr, const std::nothrow_t& tag) noexcept;
void operator delete[](void* ptr, std::align_val_t alignment, const std::nothrow_t& tag) noexcept;
// NOLINTEND(readability-redundant-declaration)

namespace {

struct TestThrow final {};

// Fixed-size trivial scalar for deterministic plain scalar traffic.
struct alignas(1) Blob17 final {
    std::array<std::uint8_t, 17> data;
};

// Over-aligned type for the aligned-surface cases (OD-M5-P7-020 case 7).
struct alignas(64) OverAlignedBlock final {
    std::array<std::uint64_t, 8> data;
};

} // namespace

// Case 1: repeated plain scalar new/delete pairs (OD-M5-P7-020 case 1).
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(AllocationInstrumentationAdversarial, RepeatedPlainScalarNewDeleteCountsRawBytes) {
    MeasurementScope scope;
    for (int i = 0; i < 4; ++i) {
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
        auto* blob = new Blob17;
        static_cast<void>(seams::force_observable_allocation_for_test(blob));
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
        delete blob;
    }
    scope.finish();
    const MeasurementResult& r = scope.result();
    EXPECT_EQ(r.allocation_count, 4);
    EXPECT_EQ(r.total_allocated_bytes, 4 * 17);
    EXPECT_EQ(r.deallocation_count, 4);
    EXPECT_EQ(r.deallocated_bytes, 4 * 17);
    EXPECT_EQ(r.persistent_live_delta, (PersistentLiveDelta{PersistentLiveDeltaSign::zero, 0}));
    EXPECT_EQ(r.peak_above_entry, 17);
    EXPECT_EQ(r.transient_excess_over_persistent, 17);
    EXPECT_TRUE(r.live_metrics_eligible);
}

// Case 2: array forms counted exactly once per request; byte totals asserted
// against the argument actually observed by the replacement function, never a
// guessed n*sizeof(T) (OD-M5-P7-020 case 2).
TEST(AllocationInstrumentationAdversarial, ArrayFormsCountedOnceFaithfulBytes) {
    MeasurementScope scope;
    void* direct = ::operator new[](73);
    ::operator delete[](direct);
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    auto* real = new std::uint8_t[64];
    const auto recorded = seams::recorded_size_for_test(real);
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    delete[] real;
    scope.finish();
    const MeasurementResult& r = scope.result();
    EXPECT_EQ(r.allocation_count, 2);
    EXPECT_GE(recorded, 64);
    EXPECT_EQ(r.total_allocated_bytes, 73 + recorded);
    EXPECT_EQ(r.deallocation_count, 2);
    EXPECT_EQ(r.deallocated_bytes, 73 + recorded);
    EXPECT_TRUE(r.live_metrics_eligible);
}

// Case 3: direct ::operator new(0) (OD-M5-P7-020 case 3).
TEST(AllocationInstrumentationAdversarial, DirectOperatorNewZeroCountsOneByteZero) {
    MeasurementScope scope;
    void* p = ::operator new(0);
    ::operator delete(p);
    scope.finish();
    const MeasurementResult& r = scope.result();
    EXPECT_EQ(r.allocation_count, 1);
    EXPECT_EQ(r.total_allocated_bytes, 0);
    EXPECT_EQ(r.deallocation_count, 1);
    EXPECT_EQ(r.deallocated_bytes, 0);
    EXPECT_TRUE(r.live_metrics_eligible);
}

// Case 4: direct ::operator new[](0) (OD-M5-P7-020 case 4).
TEST(AllocationInstrumentationAdversarial, DirectOperatorNewArrayZeroCountsOneByteZero) {
    MeasurementScope scope;
    void* p = ::operator new[](0);
    ::operator delete[](p);
    scope.finish();
    const MeasurementResult& r = scope.result();
    EXPECT_EQ(r.allocation_count, 1);
    EXPECT_EQ(r.total_allocated_bytes, 0);
    EXPECT_EQ(r.deallocation_count, 1);
    EXPECT_EQ(r.deallocated_bytes, 0);
    EXPECT_TRUE(r.live_metrics_eligible);
}

// Case 5: nothrow success counted exactly once with faithful raw bytes
// (OD-M5-P7-020 case 5).
TEST(AllocationInstrumentationAdversarial, NothrowSuccessCountedOnce) {
    MeasurementScope scope;
    void* p = ::operator new(24, std::nothrow);
    scope.finish();
    const MeasurementResult& r = scope.result();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(r.allocation_count, 1);
    EXPECT_EQ(r.total_allocated_bytes, 24);
    EXPECT_TRUE(r.live_metrics_eligible);
    ::operator delete(p, std::nothrow);
}

// Case 6: deterministic nothrow/backing failure (OD-M5-P7-020 case 6).
TEST(AllocationInstrumentationAdversarial, DeterministicBackingFailureNoCountNoProvenance) {
    seams::set_backing_failure_for_test(1);
    EXPECT_THROW(static_cast<void>(::operator new(64)), std::bad_alloc);
    seams::set_backing_failure_for_test(1);
    EXPECT_THROW(static_cast<void>(::operator new[](32)), std::bad_alloc);

    const auto live_before = allocation::live_bytes_snapshot();
    MeasurementScope scope;
    seams::set_backing_failure_for_test(1);
    void* p = ::operator new(64, std::nothrow);
    scope.finish();
    const MeasurementResult& r = scope.result();
    EXPECT_EQ(p, nullptr);
    EXPECT_EQ(r.allocation_count, 0);
    EXPECT_EQ(r.total_allocated_bytes, 0);
    EXPECT_TRUE(r.allocation_failure_observed);
    EXPECT_EQ(allocation::live_bytes_snapshot(), live_before);
    EXPECT_FALSE(seams::stale_entry_collision_for_test());
}

// Case 7: over-aligned round trip with alignment correctness
// (OD-M5-P7-020 case 7).
TEST(AllocationInstrumentationAdversarial, OverAlignedNewDeleteRoundTrip) {
    MeasurementScope scope;
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    auto* block = new OverAlignedBlock;
    static_cast<void>(seams::force_observable_allocation_for_test(block));
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto address = reinterpret_cast<std::uintptr_t>(block);
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    auto* array = new OverAlignedBlock[3];
    static_cast<void>(seams::force_observable_allocation_for_test(array));
    const auto recorded = seams::recorded_size_for_test(array);
    scope.finish();
    const MeasurementResult& r = scope.result();
    EXPECT_EQ(address % 64, 0);
    EXPECT_EQ(r.allocation_count, 2);
    EXPECT_EQ(r.total_allocated_bytes, sizeof(OverAlignedBlock) + recorded);
    EXPECT_GE(recorded, 3 * sizeof(OverAlignedBlock));
    EXPECT_TRUE(r.live_metrics_eligible);
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    delete block;
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    delete[] array;
}

// Case 8: exact unsized aligned scalar/array deallocation
// (OD-M5-P7-020 case 8).
TEST(AllocationInstrumentationAdversarial, UnsizedAlignedDeleteScalarAndArray) {
    MeasurementScope scope;
    void* p = ::operator new(256, std::align_val_t{64});
    ::operator delete(p, std::align_val_t{64});
    void* q = ::operator new[](128, std::align_val_t{32});
    ::operator delete[](q, std::align_val_t{32});
    scope.finish();
    const MeasurementResult& r = scope.result();
    EXPECT_EQ(r.allocation_count, 2);
    EXPECT_EQ(r.total_allocated_bytes, 384);
    EXPECT_EQ(r.deallocation_count, 2);
    EXPECT_EQ(r.deallocated_bytes, 384);
    EXPECT_EQ(r.persistent_live_delta, (PersistentLiveDelta{PersistentLiveDeltaSign::zero, 0}));
    EXPECT_TRUE(r.live_metrics_eligible);
}

// Case 9: exact sized aligned scalar delete signature, size BEFORE alignment
// (OD-M5-P7-020 case 9, M5-P7-MR-001).
TEST(AllocationInstrumentationAdversarial, SizedAlignedScalarDeleteExactSignature) {
    MeasurementScope scope;
    void* p = ::operator new(200, std::align_val_t{64});
    ::operator delete(p, 200, std::align_val_t{64});
    scope.finish();
    const MeasurementResult& r = scope.result();
    EXPECT_EQ(r.allocation_count, 1);
    EXPECT_EQ(r.total_allocated_bytes, 200);
    EXPECT_EQ(r.deallocation_count, 1);
    EXPECT_EQ(r.deallocated_bytes, 200);
    EXPECT_TRUE(r.live_metrics_eligible);
    EXPECT_FALSE(seams::sized_delete_mismatch_for_test());
}

// Case 10: exact sized aligned array delete signature, size BEFORE alignment
// (OD-M5-P7-020 case 10, M5-P7-MR-001).
TEST(AllocationInstrumentationAdversarial, SizedAlignedArrayDeleteExactSignature) {
    MeasurementScope scope;
    void* q = ::operator new[](300, std::align_val_t{128});
    ::operator delete[](q, 300, std::align_val_t{128});
    scope.finish();
    const MeasurementResult& r = scope.result();
    EXPECT_EQ(r.allocation_count, 1);
    EXPECT_EQ(r.total_allocated_bytes, 300);
    EXPECT_EQ(r.deallocation_count, 1);
    EXPECT_EQ(r.deallocated_bytes, 300);
    EXPECT_TRUE(r.live_metrics_eligible);
    EXPECT_FALSE(seams::sized_delete_mismatch_for_test());
}

// Case 11: aligned near-SIZE_MAX throwing overflow (OD-M5-P7-020 case 11,
// M5-P7-MR-002).
TEST(AllocationInstrumentationAdversarial, AlignedNearSizeMaxThrowingOverflowBadAlloc) {
    constexpr std::size_t huge = std::numeric_limits<std::size_t>::max() - 10U;
    const auto live_before = allocation::live_bytes_snapshot();
    EXPECT_THROW(static_cast<void>(::operator new(huge, std::align_val_t{64})), std::bad_alloc);
    EXPECT_THROW(static_cast<void>(::operator new[](huge, std::align_val_t{64})), std::bad_alloc);
    EXPECT_EQ(allocation::live_bytes_snapshot(), live_before);
    EXPECT_FALSE(seams::provenance_table_overflowed_for_test());
    EXPECT_FALSE(seams::stale_entry_collision_for_test());
}

// Case 12: aligned near-SIZE_MAX nothrow overflow (OD-M5-P7-020 case 12,
// M5-P7-MR-002).
TEST(AllocationInstrumentationAdversarial, AlignedNearSizeMaxNothrowOverflowNull) {
    constexpr std::size_t huge = std::numeric_limits<std::size_t>::max() - 10U;
    EXPECT_EQ(::operator new(huge, std::align_val_t{64}, std::nothrow), nullptr);
    EXPECT_EQ(::operator new[](huge, std::align_val_t{64}, std::nothrow), nullptr);

    MeasurementScope scope;
    void* p = ::operator new(huge, std::align_val_t{64}, std::nothrow);
    void* q = ::operator new[](huge, std::align_val_t{64}, std::nothrow);
    scope.finish();
    const MeasurementResult& r = scope.result();
    EXPECT_EQ(p, nullptr);
    EXPECT_EQ(q, nullptr);
    EXPECT_EQ(r.allocation_count, 0);
    EXPECT_EQ(r.total_allocated_bytes, 0);
    EXPECT_TRUE(r.allocation_failure_observed);
}

// Case 13: pre-bracket allocation deleted inside the bracket resolves exact
// provenance and counts as in-bracket deallocation traffic only
// (OD-M5-P7-020 case 13, OD-M5-P7-007).
TEST(AllocationInstrumentationAdversarial, PreBracketAllocationInBracketDeleteResolves) {
    void* p = ::operator new(96);
    MeasurementScope scope;
    ::operator delete(p);
    scope.finish();
    const MeasurementResult& r = scope.result();
    EXPECT_EQ(r.allocation_count, 0);
    EXPECT_EQ(r.total_allocated_bytes, 0);
    EXPECT_EQ(r.deallocation_count, 1);
    EXPECT_EQ(r.deallocated_bytes, 96);
    EXPECT_EQ(r.persistent_live_delta,
              (PersistentLiveDelta{PersistentLiveDeltaSign::negative, 96}));
    EXPECT_EQ(r.peak_above_entry, 0);
    EXPECT_EQ(r.transient_excess_over_persistent, 0);
    EXPECT_TRUE(r.live_metrics_eligible);
}

// Case 14: allocation/deletion outside the bracket leaves counters exactly
// zero (OD-M5-P7-020 case 14).
TEST(AllocationInstrumentationAdversarial, OutsideBracketAllocDeleteCountersStayZero) {
    void* p = ::operator new(64);
    ::operator delete(p);
    MeasurementScope scope;
    scope.finish();
    const MeasurementResult& r = scope.result();
    EXPECT_EQ(r.allocation_count, 0);
    EXPECT_EQ(r.total_allocated_bytes, 0);
    EXPECT_EQ(r.deallocation_count, 0);
    EXPECT_EQ(r.deallocated_bytes, 0);
    EXPECT_EQ(r.persistent_live_delta, (PersistentLiveDelta{PersistentLiveDeltaSign::zero, 0}));
    EXPECT_TRUE(r.live_metrics_eligible);
}

// Case 15: pointer reuse observes fresh provenance (OD-M5-P7-020 case 15).
TEST(AllocationInstrumentationAdversarial, PointerReuseObservesFreshProvenance) {
    void* first = ::operator new(48);
    seams::capture_next_freed_backing_for_test();
    ::operator delete(first);
    EXPECT_TRUE(seams::has_captured_backing_for_test());
    void* second = ::operator new(48);
    EXPECT_EQ(second, first);
    EXPECT_TRUE(seams::has_provenance_for_test(second));
    EXPECT_EQ(seams::recorded_size_for_test(second), 48);
    EXPECT_FALSE(seams::stale_entry_collision_for_test());
    ::operator delete(second);
}

// Case 16: retained allocation => positive persistent delta
// (OD-M5-P7-020 case 16).
TEST(AllocationInstrumentationAdversarial, RetainedAllocationPositivePersistentDelta) {
    MeasurementScope scope;
    void* p = ::operator new(120);
    scope.finish();
    const MeasurementResult& r = scope.result();
    EXPECT_NE(p, nullptr);
    EXPECT_EQ(r.persistent_live_delta,
              (PersistentLiveDelta{PersistentLiveDeltaSign::positive, 120}));
    EXPECT_EQ(r.live_bytes_after, r.live_bytes_before + 120);
    EXPECT_EQ(r.peak_above_entry, 120);
    EXPECT_EQ(r.transient_excess_over_persistent, 0);
    EXPECT_TRUE(r.live_metrics_eligible);
    ::operator delete(p);
}

// Case 17: releasing preexisting allocations => negative persistent delta,
// not an error (OD-M5-P7-020 case 17).
TEST(AllocationInstrumentationAdversarial, ReleasedPreexistingAllocationNegativePersistentDelta) {
    void* keep = ::operator new(40);
    void* free_a = ::operator new(60);
    void* free_b = ::operator new(80);
    MeasurementScope scope;
    ::operator delete(free_a);
    ::operator delete(free_b);
    scope.finish();
    const MeasurementResult& r = scope.result();
    EXPECT_EQ(r.deallocation_count, 2);
    EXPECT_EQ(r.deallocated_bytes, 140);
    EXPECT_EQ(r.persistent_live_delta,
              (PersistentLiveDelta{PersistentLiveDeltaSign::negative, 140}));
    EXPECT_EQ(r.peak_above_entry, 0);
    EXPECT_EQ(r.transient_excess_over_persistent, 0);
    EXPECT_TRUE(r.live_metrics_eligible);
    ::operator delete(keep);
}

// Case 18: no-op bracket => zero persistent delta (OD-M5-P7-020 case 18).
TEST(AllocationInstrumentationAdversarial, NoOpZeroPersistentDelta) {
    MeasurementScope scope;
    scope.finish();
    const MeasurementResult& r = scope.result();
    EXPECT_EQ(r.allocation_count, 0);
    EXPECT_EQ(r.total_allocated_bytes, 0);
    EXPECT_EQ(r.deallocation_count, 0);
    EXPECT_EQ(r.persistent_live_delta, (PersistentLiveDelta{PersistentLiveDeltaSign::zero, 0}));
    EXPECT_EQ(r.peak_above_entry, 0);
    EXPECT_EQ(r.transient_excess_over_persistent, 0);
    EXPECT_TRUE(r.live_metrics_eligible);
}

// Case 19: owning-output B/D lifecycle (OD-M5-P7-020 case 19,
// OD-M5-P7-005): B includes the alive result; post-destroy snapshot D equals
// the pre-bracket A; B->D destruction is NOT in operation counters.
TEST(AllocationInstrumentationAdversarial, OwningOutputLifecycleBD) {
    const auto before = allocation::live_bytes_snapshot();
    {
        std::vector<std::uint64_t> owned;
        MeasurementScope scope;
        owned.reserve(3);
        owned.push_back(1);
        owned.push_back(2);
        owned.push_back(3);
        static_cast<void>(seams::force_observable_allocation_for_test(owned.data()));
        scope.finish();
        const MeasurementResult& r = scope.result();
        EXPECT_EQ(r.allocation_count, 1);
        EXPECT_EQ(r.total_allocated_bytes, 3 * sizeof(std::uint64_t));
        EXPECT_EQ(r.deallocation_count, 0);
        EXPECT_EQ(r.persistent_live_delta, (PersistentLiveDelta{PersistentLiveDeltaSign::positive,
                                                                3 * sizeof(std::uint64_t)}));
        EXPECT_TRUE(r.live_metrics_eligible);
    }
    EXPECT_EQ(allocation::live_bytes_snapshot(), before);
}

// Case 20: pool-footprint independence of the normalized metrics
// (OD-M5-P7-020 case 20, M5-P7-MR-005).
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(AllocationInstrumentationAdversarial, PoolFootprintIndependenceNormalizedMetrics) {
    struct ProfileOutcome final {
        std::uint64_t a{};
        std::uint64_t allocation_count{};
        std::uint64_t total_allocated_bytes{};
        std::uint64_t deallocation_count{};
        std::uint64_t deallocated_bytes{};
        PersistentLiveDelta delta{};
        std::uint64_t peak_above_entry{};
        std::uint64_t transient_excess_over_persistent{};
        void* retained{};
    };
    const auto run_profile = []() {
        ProfileOutcome outcome;
        MeasurementScope scope;
        void* retained = ::operator new(100);
        void* temporary = ::operator new(64);
        ::operator delete(temporary);
        scope.finish();
        const MeasurementResult& r = scope.result();
        outcome.a = r.live_bytes_before;
        outcome.allocation_count = r.allocation_count;
        outcome.total_allocated_bytes = r.total_allocated_bytes;
        outcome.deallocation_count = r.deallocation_count;
        outcome.deallocated_bytes = r.deallocated_bytes;
        outcome.delta = r.persistent_live_delta;
        outcome.peak_above_entry = r.peak_above_entry;
        outcome.transient_excess_over_persistent = r.transient_excess_over_persistent;
        outcome.retained = retained;
        return outcome;
    };

    std::vector<std::uint64_t> pool_small(16);
    const auto small_footprint = run_profile();
    std::vector<std::uint64_t> pool_big(1024);
    const auto big_footprint = run_profile();

    EXPECT_NE(small_footprint.a, big_footprint.a);
    EXPECT_EQ(small_footprint.allocation_count, big_footprint.allocation_count);
    EXPECT_EQ(small_footprint.total_allocated_bytes, big_footprint.total_allocated_bytes);
    EXPECT_EQ(small_footprint.deallocation_count, big_footprint.deallocation_count);
    EXPECT_EQ(small_footprint.deallocated_bytes, big_footprint.deallocated_bytes);
    EXPECT_EQ(small_footprint.delta, big_footprint.delta);
    EXPECT_EQ(small_footprint.peak_above_entry, big_footprint.peak_above_entry);
    EXPECT_EQ(small_footprint.transient_excess_over_persistent,
              big_footprint.transient_excess_over_persistent);
    EXPECT_EQ(small_footprint.allocation_count, 2);
    EXPECT_EQ(small_footprint.total_allocated_bytes, 164);
    EXPECT_EQ(small_footprint.deallocation_count, 1);
    EXPECT_EQ(small_footprint.deallocated_bytes, 64);
    EXPECT_EQ(small_footprint.delta, (PersistentLiveDelta{PersistentLiveDeltaSign::positive, 100}));
    EXPECT_EQ(small_footprint.peak_above_entry, 164);
    EXPECT_EQ(small_footprint.transient_excess_over_persistent, 64);

    ::operator delete(small_footprint.retained);
    ::operator delete(big_footprint.retained);
}

// Case 21: hand-computed peak ascent/descent profile with two deallocation
// orders (OD-M5-P7-020 case 21).
TEST(AllocationInstrumentationAdversarial, HandComputedPeakAscentDescentTwoFreeOrders) {
    {
        MeasurementScope scope;
        void* p1 = ::operator new(100);
        void* p2 = ::operator new(200);
        void* p3 = ::operator new(50);
        ::operator delete(p1);
        ::operator delete(p3);
        void* p4 = ::operator new(500);
        ::operator delete(p4);
        ::operator delete(p2);
        scope.finish();
        const MeasurementResult& r = scope.result();
        EXPECT_EQ(r.allocation_count, 4);
        EXPECT_EQ(r.total_allocated_bytes, 850);
        EXPECT_EQ(r.deallocation_count, 4);
        EXPECT_EQ(r.deallocated_bytes, 850);
        EXPECT_EQ(r.peak_live_bytes_absolute, r.live_bytes_before + 700);
        EXPECT_EQ(r.peak_above_entry, 700);
        EXPECT_EQ(r.transient_excess_over_persistent, 700);
        EXPECT_EQ(r.persistent_live_delta, (PersistentLiveDelta{PersistentLiveDeltaSign::zero, 0}));
        EXPECT_TRUE(r.live_metrics_eligible);
    }
    {
        MeasurementScope scope;
        void* p1 = ::operator new(100);
        void* p2 = ::operator new(200);
        void* p3 = ::operator new(50);
        void* p4 = ::operator new(500);
        ::operator delete(p3);
        ::operator delete(p2);
        ::operator delete(p1);
        ::operator delete(p4);
        scope.finish();
        const MeasurementResult& r = scope.result();
        EXPECT_EQ(r.peak_live_bytes_absolute, r.live_bytes_before + 850);
        EXPECT_EQ(r.peak_above_entry, 850);
        EXPECT_EQ(r.transient_excess_over_persistent, 850);
        EXPECT_TRUE(r.live_metrics_eligible);
    }
}

// Case 22: nested measurement scope rejection (OD-M5-P7-020 case 22,
// OD-M5-P7-019).
TEST(AllocationInstrumentationAdversarial, NestedMeasurementScopeRejected) {
    EXPECT_DEATH(
        {
            MeasurementScope outer;
            MeasurementScope inner;
            static_cast<void>(outer);
            static_cast<void>(inner);
        },
        "nested measurement scope rejected");
}

// Case 23: counter overflow fail-closed with frozen UINT64_MAX value
// (OD-M5-P7-020 case 23, OD-M5-P7-019).
TEST(AllocationInstrumentationAdversarial, CounterOverflowFailClosed) {
    seams::reset_instrumentation_state_for_test();
    {
        MeasurementScope scope;
        seams::seed_counter_for_test(seams::CounterId::total_allocated_bytes,
                                     std::numeric_limits<std::uint64_t>::max() - 10U);
        void* p = ::operator new(16);
        ::operator delete(p);
        scope.finish();
        const MeasurementResult& r = scope.result();
        EXPECT_EQ(r.total_allocated_bytes, std::numeric_limits<std::uint64_t>::max());
        EXPECT_FALSE(r.total_allocated_bytes_valid);
        EXPECT_TRUE(r.allocation_count_valid);
        EXPECT_EQ(r.allocation_count, 1);
        EXPECT_TRUE(r.deallocation_count_valid);
        EXPECT_EQ(r.deallocation_count, 1);
        EXPECT_TRUE(seams::counter_overflowed_for_test(seams::CounterId::total_allocated_bytes));
        EXPECT_FALSE(seams::live_bytes_wrapped_for_test());
    }
    {
        MeasurementScope scope;
        scope.finish();
        const MeasurementResult& r = scope.result();
        EXPECT_EQ(r.total_allocated_bytes, std::numeric_limits<std::uint64_t>::max());
        EXPECT_FALSE(r.total_allocated_bytes_valid);
    }
    seams::reset_instrumentation_state_for_test();
}

// Case 24: provenance capacity exhaustion fail-closed
// (OD-M5-P7-020 case 24, OD-M5-P7-019).
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(AllocationInstrumentationAdversarial, ProvenanceCapacityExhaustionFailClosed) {
    seams::reset_instrumentation_state_for_test();
    seams::set_provenance_capacity_clamp_for_test(8);
    std::array<void*, 8> live{};
    for (std::size_t i = 0; i < live.size(); ++i) {
        live.at(i) = ::operator new(10U + i);
    }
    {
        MeasurementScope scope;
        void* extra = ::operator new(100);
        ::operator delete(extra);
        scope.finish();
        const MeasurementResult& r = scope.result();
        EXPECT_EQ(r.allocation_count, 1);
        EXPECT_EQ(r.total_allocated_bytes, 100);
        EXPECT_TRUE(r.allocation_count_valid);
        EXPECT_TRUE(r.total_allocated_bytes_valid);
        EXPECT_FALSE(r.live_metrics_eligible);
        EXPECT_EQ(r.ineligibility_reason, LiveIneligibilityReason::provenance_table_overflow);
        EXPECT_TRUE(seams::provenance_table_overflowed_for_test());
    }
    void* q = ::operator new(16);
    ASSERT_NE(q, nullptr);
    ::operator delete(q);
    for (void* ptr : live) {
        ::operator delete(ptr);
    }
    seams::set_provenance_capacity_clamp_for_test(0);
    seams::reset_instrumentation_state_for_test();
}

// Case 25: standard-conforming unknown-provenance delete construction
// (OD-M5-P7-020 case 25, M5-P7-RR-001).
TEST(AllocationInstrumentationAdversarial, UnknownProvenanceDeleteStandardConforming) {
    constexpr std::uint64_t kSize = 123;
    seams::reset_instrumentation_state_for_test();
    void* p = ::operator new(kSize);
    EXPECT_TRUE(seams::has_provenance_for_test(p));
    EXPECT_EQ(seams::recorded_size_for_test(p), kSize);
    const auto live_before = allocation::live_bytes_snapshot();
    seams::forget_provenance_for_test(p);
    EXPECT_FALSE(seams::has_provenance_for_test(p));
    EXPECT_EQ(allocation::live_bytes_snapshot(), live_before - kSize);
    {
        MeasurementScope scope;
        ::operator delete(p);
        scope.finish();
        const MeasurementResult& r = scope.result();
        EXPECT_EQ(r.deallocation_count, 0);
        EXPECT_EQ(r.allocation_count, 0);
        EXPECT_FALSE(r.live_metrics_eligible);
        EXPECT_FALSE(r.deallocated_bytes_valid);
        EXPECT_EQ(r.ineligibility_reason, LiveIneligibilityReason::unknown_pointer_delete);
        EXPECT_TRUE(r.allocation_count_valid);
        EXPECT_TRUE(r.total_allocated_bytes_valid);
    }
    // Storage release executes exactly once: the capture seam intercepts the
    // exact release of a second standard-conforming unknown-provenance delete.
    void* p2 = ::operator new(kSize);
    seams::forget_provenance_for_test(p2);
    seams::capture_next_freed_backing_for_test();
    ::operator delete(p2);
    EXPECT_TRUE(seams::has_captured_backing_for_test());
    seams::release_captured_backing_for_test();
    EXPECT_FALSE(seams::has_captured_backing_for_test());
    seams::reset_instrumentation_state_for_test();
}

// Case 26: direct malloc/free bypass proves the measurement scope
// (OD-M5-P7-020 case 26).
TEST(AllocationInstrumentationAdversarial, MallocFreeBypassInvisible) {
    MeasurementScope scope;
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
    void* raw = std::malloc(1024);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
    std::free(raw);
    scope.finish();
    const MeasurementResult& r = scope.result();
    EXPECT_EQ(r.allocation_count, 0);
    EXPECT_EQ(r.total_allocated_bytes, 0);
    EXPECT_EQ(r.deallocation_count, 0);
    EXPECT_EQ(r.persistent_live_delta, (PersistentLiveDelta{PersistentLiveDeltaSign::zero, 0}));
    EXPECT_TRUE(r.live_metrics_eligible);
}

// Case 28: repeated-process normalized-metric determinism via subprocess
// re-execution (OD-M5-P7-020 case 28).
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(AllocationInstrumentationAdversarial, RepeatedProcessNormalizedDeterminism) {
    const auto spawn = []() -> std::optional<std::string> {
        const char* binary = bmd_projection::m5::allocation_test::test_binary_path();
        if (binary == nullptr) {
            return std::nullopt;
        }
        std::array<int, 2> pipe_fds{-1, -1};
        if (::pipe(pipe_fds.data()) != 0) {
            return std::nullopt;
        }
        const pid_t child = ::fork();
        if (child < 0) {
            ::close(pipe_fds.at(0));
            ::close(pipe_fds.at(1));
            return std::nullopt;
        }
        if (child == 0) {
            ::close(pipe_fds.at(0));
            if (::dup2(pipe_fds.at(1), STDOUT_FILENO) < 0) {
                _exit(127);
            }
            ::close(pipe_fds.at(1));
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
            ::execl(binary, binary, "--p7-determinism-child", static_cast<char*>(nullptr));
            _exit(127);
        }
        ::close(pipe_fds.at(1));
        std::string output;
        std::array<char, 256> buffer{};
        ssize_t read_count = 0;
        while ((read_count = ::read(pipe_fds.at(0), buffer.data(), buffer.size())) > 0) {
            output.append(buffer.data(), static_cast<std::size_t>(read_count));
        }
        ::close(pipe_fds.at(0));
        int status = 0;
        static_cast<void>(::waitpid(child, &status, 0));
        if (output.empty()) {
            return std::nullopt;
        }
        return output;
    };

    const auto first = spawn();
    const auto second = spawn();
    const auto third = spawn();
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    ASSERT_TRUE(third.has_value());
    // NOLINTBEGIN(bugprone-unchecked-optional-access)
    EXPECT_EQ(*first, *second);
    EXPECT_EQ(*second, *third);

    std::istringstream stream(*first);
    // NOLINTEND(bugprone-unchecked-optional-access)
    unsigned long long allocation_count = 0;
    unsigned long long total_allocated_bytes = 0;
    unsigned long long deallocation_count = 0;
    unsigned long long deallocated_bytes = 0;
    unsigned int delta_sign = 0;
    unsigned long long delta_magnitude = 0;
    unsigned long long peak_above_entry = 0;
    unsigned long long transient_excess = 0;
    stream >> allocation_count >> total_allocated_bytes >> deallocation_count >>
        deallocated_bytes >> delta_sign >> delta_magnitude >> peak_above_entry >> transient_excess;
    EXPECT_EQ(allocation_count, 3);
    EXPECT_EQ(total_allocated_bytes, 56);
    EXPECT_EQ(deallocation_count, 1);
    EXPECT_EQ(deallocated_bytes, 16);
    EXPECT_EQ(delta_sign, static_cast<unsigned int>(PersistentLiveDeltaSign::positive));
    EXPECT_EQ(delta_magnitude, 40);
    EXPECT_EQ(peak_above_entry, 48);
    EXPECT_EQ(transient_excess, 8);
}

// Case 29: real M2 map-node allocations before the bracket resolve exactly
// when destroyed/reset in the bracket (OD-M5-P7-020 case 29).
TEST(AllocationInstrumentationAdversarial, M2NodeDeallocationResolvesProvenance) {
    using bmd_projection_test::price, bmd_projection_test::quantity, bmd_projection_test::spec;
    bmd::OrderBook book{spec()};
    const std::vector<bmd::LevelUpdate> updates{
        {bmd::BookSide::Bid, price(100), quantity(5)},
        {bmd::BookSide::Bid, price(99), quantity(3)},
        {bmd::BookSide::Ask, price(101), quantity(4)},
    };
    book.apply_updates(updates);
    MeasurementScope scope;
    const auto change = book.apply_level(bmd::BookSide::Bid, price(100), quantity(0));
    scope.finish();
    const MeasurementResult& r = scope.result();
    EXPECT_EQ(change, bmd::LevelChange::Removed);
    EXPECT_EQ(r.allocation_count, 0);
    EXPECT_EQ(r.deallocation_count, 1);
    EXPECT_GT(r.deallocated_bytes, 0);
    EXPECT_EQ(r.persistent_live_delta,
              (PersistentLiveDelta{PersistentLiveDeltaSign::negative, r.deallocated_bytes}));
    EXPECT_TRUE(r.live_metrics_eligible);
}

// Case 29 (reset variant): M2 clear() frees every pre-bracket node with exact
// provenance resolution.
TEST(AllocationInstrumentationAdversarial, M2ClearResolvesPreBracketNodes) {
    using bmd_projection_test::price, bmd_projection_test::quantity, bmd_projection_test::spec;
    bmd::OrderBook book{spec()};
    const std::vector<bmd::LevelUpdate> updates{
        {bmd::BookSide::Bid, price(100), quantity(5)},
        {bmd::BookSide::Bid, price(99), quantity(3)},
        {bmd::BookSide::Ask, price(101), quantity(4)},
        {bmd::BookSide::Ask, price(102), quantity(6)},
    };
    book.apply_updates(updates);
    MeasurementScope scope;
    book.clear();
    scope.finish();
    const MeasurementResult& r = scope.result();
    EXPECT_EQ(r.allocation_count, 0);
    EXPECT_EQ(r.deallocation_count, 4);
    EXPECT_EQ(r.persistent_live_delta,
              (PersistentLiveDelta{PersistentLiveDeltaSign::negative, r.deallocated_bytes}));
    EXPECT_TRUE(r.live_metrics_eligible);
}

// Case 30: real M3 old-book destruction during accepted apply resolves
// through valid provenance (OD-M5-P7-020 case 30).
TEST(AllocationInstrumentationAdversarial, M3OldBookDestructionResolvesProvenance) {
    using bmd_projection_test::apply, bmd_projection_test::install, bmd_projection_test::price,
        bmd_projection_test::quantity, bmd_projection_test::spec;
    bmd::BookProjection projection{spec(), bmd::SequencePolicyKind::Spot};
    const std::vector<bmd::BookLevel> baseline_bids{
        {price(100), quantity(5)},
        {price(99), quantity(3)},
    };
    const std::vector<bmd::BookLevel> baseline_asks{
        {price(101), quantity(4)},
        {price(102), quantity(6)},
    };
    const std::vector<bmd::LevelUpdate> updates{
        {bmd::BookSide::Bid, price(100), quantity(7)},
        {bmd::BookSide::Bid, price(97), quantity(8)},
    };
    static_cast<void>(install(projection, 500, baseline_bids, baseline_asks));
    static_cast<void>(apply(projection, 499, 501));
    MeasurementScope scope;
    const auto result = apply(projection, 502, 502, 501, updates);
    scope.finish();
    const MeasurementResult& r = scope.result();
    EXPECT_EQ(result.disposition, bmd::ApplyDisposition::Applied);
    EXPECT_GT(r.allocation_count, 0);
    EXPECT_GT(r.deallocation_count, 0);
    EXPECT_GT(r.deallocated_bytes, 0);
    EXPECT_TRUE(r.live_metrics_eligible);
}

// OD-M5-P7-020 independence rule: container-driven profile cross-checked
// against an independently hand-derived expected total from the documented
// growth policy (libstdc++ and libc++ both double capacity on reallocation;
// the standard requires the new buffer to be allocated and filled before the
// old buffer is deallocated, so each reallocation transitions live bytes
// +new then -old).
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(AllocationInstrumentationAdversarial, VectorGrowthHandComputedCrossCheck) {
    std::vector<std::uint32_t> values;
    MeasurementScope scope;
    for (std::uint32_t v = 1; v <= 10; ++v) {
        values.push_back(v);
    }
    static_cast<void>(seams::force_observable_allocation_for_test(values.data()));
    scope.finish();
    const MeasurementResult& r = scope.result();
    // Capacities 1, 2, 4, 8, 16 elements: five 4-byte-element allocations.
    EXPECT_EQ(r.allocation_count, 5);
    EXPECT_EQ(r.total_allocated_bytes, 4U * (1U + 2U + 4U + 8U + 16U));
    // Reallocation frees the previous buffers: 1 + 2 + 4 + 8 elements.
    EXPECT_EQ(r.deallocation_count, 4);
    EXPECT_EQ(r.deallocated_bytes, 4U * (1U + 2U + 4U + 8U));
    EXPECT_EQ(r.persistent_live_delta,
              (PersistentLiveDelta{PersistentLiveDeltaSign::positive, 64}));
    // Live transitions: +4, +8, -4, +16, -8, +32, -16, +64, -32 relative to A:
    // the peak sits right after the final +64 allocation, before the -32 free.
    EXPECT_EQ(r.peak_above_entry, 96);
    EXPECT_EQ(r.transient_excess_over_persistent, 32);
    EXPECT_TRUE(r.live_metrics_eligible);
}

// OD-M5-P7-019 exception semantics: a throwing measured operation closes the
// bracket via RAII during unwinding and never leaves measurement permanently
// active; the next bracket starts clean.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(AllocationInstrumentationAdversarial, ThrowingOperationClosesBracketMarksAborted) {
    bool threw = false;
    void* retained = nullptr;
    try {
        MeasurementScope scope;
        retained = ::operator new(100);
        throw TestThrow{};
    } catch (const TestThrow&) {
        threw = true;
    }
    EXPECT_TRUE(threw);
    EXPECT_FALSE(MeasurementScope::measurement_active());
    ::operator delete(retained);
    MeasurementScope fresh;
    fresh.finish();
    const MeasurementResult& r = fresh.result();
    EXPECT_EQ(r.allocation_count, 0);
    EXPECT_EQ(r.deallocation_count, 0);
    EXPECT_TRUE(r.live_metrics_eligible);
}

// Systematic round trip of every one of the 20 replaceable forms via direct
// standard-conforming calls (OD-M5-P7-003 surface verification).
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(AllocationInstrumentationAdversarial, FullOperatorSurfaceRoundTrip) {
    MeasurementScope scope;
    void* p1 = ::operator new(10);
    void* p2 = ::operator new[](20);
    void* p3 = ::operator new(30, std::nothrow);
    void* p4 = ::operator new[](40, std::nothrow);
    void* p5 = ::operator new(50, std::align_val_t{64});
    void* p6 = ::operator new[](60, std::align_val_t{64});
    void* p7 = ::operator new(70, std::align_val_t{128}, std::nothrow);
    void* p8 = ::operator new[](80, std::align_val_t{128}, std::nothrow);
    void* p9 = ::operator new(0);
    void* p10 = ::operator new[](0);
    void* p11 = ::operator new(90);
    void* p12 = ::operator new(100, std::align_val_t{32});
    void* p13 = ::operator new[](110, std::align_val_t{32});
    void* p14 = ::operator new[](120);
    ::operator delete(p1);
    ::operator delete[](p2);
    ::operator delete(p3, std::nothrow);
    ::operator delete[](p4, std::nothrow);
    ::operator delete(p5, std::align_val_t{64});
    ::operator delete[](p6, std::align_val_t{64});
    ::operator delete(p7, std::align_val_t{128}, std::nothrow);
    ::operator delete[](p8, std::align_val_t{128}, std::nothrow);
    ::operator delete(p9, static_cast<std::size_t>(0));
    ::operator delete[](p10);
    ::operator delete(p11, 90);
    ::operator delete(p12, 100, std::align_val_t{32});
    ::operator delete[](p13, 110, std::align_val_t{32});
    ::operator delete[](p14, 120);
    scope.finish();
    const MeasurementResult& r = scope.result();
    EXPECT_EQ(r.allocation_count, 14);
    EXPECT_EQ(r.total_allocated_bytes, 780);
    EXPECT_EQ(r.deallocation_count, 14);
    EXPECT_EQ(r.deallocated_bytes, 780);
    EXPECT_EQ(r.persistent_live_delta, (PersistentLiveDelta{PersistentLiveDeltaSign::zero, 0}));
    EXPECT_TRUE(r.live_metrics_eligible);
    EXPECT_FALSE(seams::sized_delete_mismatch_for_test());
    EXPECT_FALSE(seams::stale_entry_collision_for_test());
}
