#pragma once

// Test-only seams for the Phase-7 allocation instrumentation adversarial suite
// (OD-M5-P7-019 / OD-M5-P7-020). Compiled ONLY into the dedicated
// bmd_projection_allocation_instrumentation_tests executable. Ordinary Phase-7
// measurement executables neither see nor can link these functions
// (OD-M5-P7-002); in particular the provenance-forget seam exists nowhere
// outside this test target.

#include <cstddef>
#include <cstdint>

namespace bmd_projection::m5::allocation::test {

enum class CounterId : std::uint8_t {
    allocation_count,
    total_allocated_bytes,
    deallocation_count,
    deallocated_bytes,
    live_bytes,
};

// OD-M5-P7-020 case 25 (M5-P7-RR-001): removes the provenance entry for ptr
// WITHOUT freeing the allocation, keeping instrumentation bookkeeping
// consistent (no false stale live record). Requires: no active measurement
// bracket. The legally matched replacement ::operator delete(ptr) that
// follows exercises the real unknown-provenance branch.
void forget_provenance_for_test(void* ptr);

// Deterministic backing failure: the next `failures_remaining` replacement
// allocation requests fail before malloc (throwing forms throw bad_alloc,
// nothrow forms return nullptr) without recording any state.
void set_backing_failure_for_test(std::uint64_t failures_remaining) noexcept;

// Deterministic effective capacity clamp (0 restores the default capacity).
// Intended for use on an empty provenance table (e.g. right after
// reset_instrumentation_state_for_test).
void set_provenance_capacity_clamp_for_test(std::size_t capacity) noexcept;

// Seeds a counter for near-overflow testing. The four traffic counters apply
// to the ACTIVE measurement bracket (requires an active bracket); live_bytes
// applies globally. A seeded value does not itself raise any flag; the next
// checked update forces the overflow.
void seed_counter_for_test(CounterId counter, std::uint64_t value) noexcept;

// Deterministic pointer-reuse backing support: the NEXT replacement
// deallocation captures the backing block instead of freeing it; the next
// replacement allocation then reuses the captured block (same raw-size/
// alignment shape required), guaranteeing the same payload address.
void capture_next_freed_backing_for_test() noexcept;

[[nodiscard]] bool has_captured_backing_for_test() noexcept;

// Frees a captured backing block if one is pending (releases the seam's
// deferred ownership without allocating).
void release_captured_backing_for_test() noexcept;

// Deterministic full state reset: clears the provenance table, counters,
// sticky flags, and seam state while preserving the invariant
// live_bytes == sum(recorded sizes). Pre-existing framework allocations
// become untracked; their later deallocations are ordinary unknown-pointer
// deletes outside brackets. Requires: no active measurement bracket.
void reset_instrumentation_state_for_test() noexcept;

// Read seams for deterministic assertions on the instrumentation internals.
[[nodiscard]] bool has_provenance_for_test(void* ptr) noexcept;
[[nodiscard]] std::uint64_t recorded_size_for_test(void* ptr) noexcept;
[[nodiscard]] bool provenance_table_overflowed_for_test() noexcept;
[[nodiscard]] bool stale_entry_collision_for_test() noexcept;
[[nodiscard]] bool sized_delete_mismatch_for_test() noexcept;
[[nodiscard]] bool live_bytes_wrapped_for_test() noexcept;
[[nodiscard]] bool recursion_observed_for_test() noexcept;
[[nodiscard]] bool counter_overflowed_for_test(CounterId counter) noexcept;

// Cross-TU opaque address escape: passing an allocation's address through
// this function (defined in the seams translation unit, so no optimizer can
// see through it) forces real heap materialization of trivial non-escaping
// `new` expressions that -O3 would otherwise legally elide ([expr.new]/10),
// keeping the adversarial `new`/`delete` expression forms deterministic
// across Debug and Release.
[[nodiscard]] std::uintptr_t force_observable_allocation_for_test(void* ptr) noexcept;

} // namespace bmd_projection::m5::allocation::test
