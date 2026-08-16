#include "allocation_instrumentation.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <limits>
#include <new>

namespace bmd_projection::m5::allocation {

namespace {

// Checked-add result: on overflow the value saturates at UINT64_MAX and the
// caller must flip the matching sticky flag (OD-M5-P7-019). Counters never
// wrap modulo 2^64.
struct CheckedAddResult final {
    std::uint64_t value;
    bool overflowed;
};

[[nodiscard]] CheckedAddResult checked_add(std::uint64_t lhs, std::uint64_t rhs) noexcept {
    if (rhs > std::numeric_limits<std::uint64_t>::max() - lhs) {
        return {std::numeric_limits<std::uint64_t>::max(), true};
    }
    return {lhs + rhs, false};
}

// Constant-initialized provenance table state.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
detail::InstrumentationState g_state{};

// Defense-in-depth recursion guard: instrumentation bookkeeping performs no
// allocation, so this must never trip; if it does, the allocation is routed
// around recording and a fail-closed instrumentation error is set
// (OD-M5-P7-002 / OD-M5-P7-019).
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
thread_local bool g_recursion_guard = false;

class RecursionGuard final {
  public:
    RecursionGuard() noexcept { g_recursion_guard = true; }

    ~RecursionGuard() { g_recursion_guard = false; }

    RecursionGuard(const RecursionGuard&) = delete;
    RecursionGuard& operator=(const RecursionGuard&) = delete;
    RecursionGuard(RecursionGuard&&) = delete;
    RecursionGuard& operator=(RecursionGuard&&) = delete;
};

[[nodiscard]] std::size_t effective_capacity(const detail::InstrumentationState& state) noexcept {
    if (state.capacity_clamp != 0 && state.capacity_clamp < kProvenanceCapacity) {
        return state.capacity_clamp;
    }
    return kProvenanceCapacity;
}

[[nodiscard]] std::size_t hash_of(void* ptr) noexcept {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto value = reinterpret_cast<std::uintptr_t>(ptr);
    const auto mixed = (static_cast<std::uint64_t>(value) >> 3U) * 0x9E3779B97F4A7C15ULL;
    return static_cast<std::size_t>(mixed);
}

[[nodiscard]] std::size_t advance(std::size_t index, std::size_t capacity) noexcept {
    return (index + 1U == capacity) ? 0U : index + 1U;
}

enum class InsertOutcome : std::uint8_t {
    inserted,
    collision,
    overflow,
};

// Bounded open-addressing insert with tombstones. Probes at most the
// effective capacity once; deterministic; never allocates; never resizes.
// An occupied slot already holding ptr is a stale-entry collision
// (sticky instrumentation ERROR, OD-M5-P7-019); a table with no free slot
// yields overflow (sticky live-metric INELIGIBILITY; the pointer is still
// returned by the operator).
[[nodiscard]] InsertOutcome insert_slot(detail::InstrumentationState& state, void* ptr,
                                        AllocationRecordInput input) noexcept {
    const auto raw_size = static_cast<std::uint64_t>(input.raw_size);
    const auto backing_size = static_cast<std::uint64_t>(input.backing_size);
    const auto capacity = effective_capacity(state);
    auto index = hash_of(ptr) % capacity;
    std::size_t candidate = detail::no_slot();
    for (std::size_t probes = 0; probes < capacity; ++probes) {
        auto& slot = state.slots.at(index);
        if (slot.state == detail::kOccupied) {
            if (slot.ptr == ptr) {
                return InsertOutcome::collision;
            }
        } else if (slot.state == detail::kEmpty) {
            const auto target = (candidate == detail::no_slot()) ? index : candidate;
            state.slots.at(target).ptr = ptr;
            state.slots.at(target).raw_requested_size = raw_size;
            state.slots.at(target).backing_request_size = backing_size;
            state.slots.at(target).state = detail::kOccupied;
            return InsertOutcome::inserted;
        } else {
            if (candidate == detail::no_slot()) {
                candidate = index;
            }
        }
        index = advance(index, capacity);
    }
    if (candidate != detail::no_slot()) {
        state.slots.at(candidate).ptr = ptr;
        state.slots.at(candidate).raw_requested_size = raw_size;
        state.slots.at(candidate).backing_request_size = backing_size;
        state.slots.at(candidate).state = detail::kOccupied;
        return InsertOutcome::inserted;
    }
    return InsertOutcome::overflow;
}

// Live-bytes update: wrap/underflow sets the sticky wrap flag and freezes the
// counter (OD-M5-P7-019); the affected metric class becomes INELIGIBLE.
void add_live_bytes(detail::InstrumentationState& state, std::uint64_t size) noexcept {
    if (state.live_bytes_wrapped) {
        return;
    }
    const auto result = checked_add(state.live_bytes, size);
    if (result.overflowed) {
        state.live_bytes_wrapped = true;
    }
    state.live_bytes = result.value;
}

void subtract_live_bytes(detail::InstrumentationState& state, std::uint64_t size) noexcept {
    if (state.live_bytes_wrapped) {
        return;
    }
    if (size > state.live_bytes) {
        state.live_bytes_wrapped = true;
        state.live_bytes = 0;
        return;
    }
    state.live_bytes -= size;
}

void update_peak(detail::InstrumentationState& state) noexcept {
    if (!state.live_bytes_wrapped && state.measurement_active &&
        state.live_bytes > state.bracket_peak) {
        state.bracket_peak = state.live_bytes;
    }
}

} // namespace

namespace detail {

InstrumentationState& state() noexcept { return g_state; }

std::size_t no_slot() noexcept { return std::numeric_limits<std::size_t>::max(); }

std::size_t find_slot(const InstrumentationState& state, void* ptr) noexcept {
    if (ptr == nullptr) {
        return no_slot();
    }
    const auto capacity = effective_capacity(state);
    auto index = hash_of(ptr) % capacity;
    for (std::size_t probes = 0; probes < capacity; ++probes) {
        const auto& slot = state.slots.at(index);
        if (slot.state == kEmpty) {
            return no_slot();
        }
        if (slot.state == kOccupied && slot.ptr == ptr) {
            return index;
        }
        index = advance(index, capacity);
    }
    return no_slot();
}

} // namespace detail

void record_successful_allocation(void* ptr, AllocationRecordInput input) noexcept {
    auto& state = detail::state();
    if (g_recursion_guard) {
        state.recursion_observed = true;
        return;
    }
    const RecursionGuard guard{};
    const auto raw_size = static_cast<std::uint64_t>(input.raw_size);
    const auto backing_size = static_cast<std::uint64_t>(input.backing_size);

    const auto outcome = insert_slot(state, ptr, input);
    if (outcome == InsertOutcome::collision) {
        state.stale_entry_collision = true;
    } else if (outcome == InsertOutcome::overflow) {
        state.provenance_table_overflowed = true;
    }

    // Invariant: live_bytes == sum of provenance-recorded raw sizes. An
    // allocation whose entry could not be recorded (overflow) does not enter
    // live accounting; the live metric class is sticky-INELIGIBLE anyway.
    if (outcome == InsertOutcome::inserted) {
        add_live_bytes(state, raw_size);
    }

    if (state.measurement_active) {
        if (!state.allocation_count_overflowed) {
            const auto result = checked_add(state.bracket_allocation_count, 1U);
            if (result.overflowed) {
                state.allocation_count_overflowed = true;
            }
            state.bracket_allocation_count = result.value;
        }
        if (!state.total_allocated_bytes_overflowed) {
            const auto result = checked_add(state.bracket_total_allocated_bytes, raw_size);
            if (result.overflowed) {
                state.total_allocated_bytes_overflowed = true;
            }
            state.bracket_total_allocated_bytes = result.value;
        }
        if (!state.backing_diagnostic_overflowed) {
            const auto result = checked_add(state.bracket_backing_request_bytes, backing_size);
            if (result.overflowed) {
                state.backing_diagnostic_overflowed = true;
            }
            state.bracket_backing_request_bytes = result.value;
        }
        update_peak(state);
    }
}

void record_deallocation(void* ptr, DeallocationRecordInput input) noexcept {
    if (ptr == nullptr) {
        return;
    }
    auto& state = detail::state();
    if (g_recursion_guard) {
        state.recursion_observed = true;
        return;
    }
    const RecursionGuard guard{};

    const auto index = detail::find_slot(state, ptr);
    if (index == detail::no_slot()) {
        // Unknown-pointer delete: not counted; live/deallocated-byte metric
        // class INELIGIBLE for the bracket (OD-M5-P7-019). Storage release is
        // the replacement operator's responsibility and still happens.
        if (state.measurement_active) {
            state.bracket_unknown_pointer_delete = true;
        }
        return;
    }

    const auto recorded_size = state.slots.at(index).raw_requested_size;
    if (input.has_supplied_size && input.supplied_size != recorded_size) {
        // Sized-delete consistency check against the recorded raw size;
        // mismatch is a run-level instrumentation ERROR. Accounting still uses
        // the authoritative recorded size (OD-M5-P7-003).
        state.sized_delete_mismatch = true;
    }

    subtract_live_bytes(state, recorded_size);

    state.slots.at(index).ptr = nullptr;
    state.slots.at(index).raw_requested_size = 0;
    state.slots.at(index).backing_request_size = 0;
    state.slots.at(index).state = detail::kTombstone;

    if (state.measurement_active) {
        if (!state.deallocation_count_overflowed) {
            const auto result = checked_add(state.bracket_deallocation_count, 1U);
            if (result.overflowed) {
                state.deallocation_count_overflowed = true;
            }
            state.bracket_deallocation_count = result.value;
        }
        if (!state.deallocated_bytes_overflowed) {
            const auto result = checked_add(state.bracket_deallocated_bytes, recorded_size);
            if (result.overflowed) {
                state.deallocated_bytes_overflowed = true;
            }
            state.bracket_deallocated_bytes = result.value;
        }
        update_peak(state);
    }
}

void record_backing_failure() noexcept {
    auto& state = detail::state();
    if (state.measurement_active) {
        state.bracket_allocation_failure = true;
    }
}

std::uint64_t live_bytes_snapshot() noexcept { return detail::state().live_bytes; }

namespace {

[[nodiscard]] std::uint64_t frozen_counter_value(bool overflowed) noexcept {
    return overflowed ? std::numeric_limits<std::uint64_t>::max() : 0;
}

} // namespace

MeasurementScope::MeasurementScope() {
    auto& state = detail::state();
    if (state.measurement_active) {
        // Nested measurement brackets are REJECTED with a stable diagnostic
        // (OD-M5-P7-019). std::fputs/std::abort are allocation-free and
        // iostream-free.
        std::fputs("M5 Phase-7 allocation instrumentation: nested measurement scope rejected\n",
                   stderr);
        assert(false && "M5 Phase-7 allocation instrumentation: nested measurement scope rejected");
        std::abort();
    }
    // Reset bracket traffic counters. A counter whose class overflowed at run
    // level stays frozen at UINT64_MAX; the class remains INVALID for the run.
    state.bracket_allocation_count = frozen_counter_value(state.allocation_count_overflowed);
    state.bracket_total_allocated_bytes =
        frozen_counter_value(state.total_allocated_bytes_overflowed);
    state.bracket_deallocation_count = frozen_counter_value(state.deallocation_count_overflowed);
    state.bracket_deallocated_bytes = frozen_counter_value(state.deallocated_bytes_overflowed);
    state.bracket_backing_request_bytes = frozen_counter_value(state.backing_diagnostic_overflowed);
    state.bracket_unknown_pointer_delete = false;
    state.bracket_allocation_failure = false;
    // A = current live bytes; P starts at A (the peak covers the open point).
    state.bracket_peak = state.live_bytes;
    state.measurement_active = true;
    result_ = MeasurementResult{};
    result_.live_bytes_before = state.live_bytes;
    uncaught_exceptions_at_entry_ = std::uncaught_exceptions();
}

MeasurementScope::~MeasurementScope() { finish(); }

void MeasurementScope::finish() noexcept {
    if (closed_) {
        return;
    }
    closed_ = true;
    auto& state = detail::state();
    state.measurement_active = false;
    result_.operation_aborted = std::uncaught_exceptions() != uncaught_exceptions_at_entry_;
    result_.allocation_failure_observed = state.bracket_allocation_failure;

    const auto a = result_.live_bytes_before;
    const auto b = state.live_bytes;
    result_.live_bytes_after = b;
    // P = max over the closed bracket, including the open point (P >= A) and
    // the close point (P >= B): no sampling, no interpolation (OD-M5-P7-005).
    result_.peak_live_bytes_absolute = (state.bracket_peak > b) ? state.bracket_peak : b;
    result_.allocation_count = state.bracket_allocation_count;
    result_.total_allocated_bytes = state.bracket_total_allocated_bytes;
    result_.deallocation_count = state.bracket_deallocation_count;
    result_.deallocated_bytes = state.bracket_deallocated_bytes;
    result_.instrument_backing_request_bytes = state.bracket_backing_request_bytes;

    // Persistent live change: exact {sign, magnitude}, never unsigned B - A.
    if (b > a) {
        result_.persistent_live_delta = {PersistentLiveDeltaSign::positive, b - a};
    } else if (b == a) {
        result_.persistent_live_delta = {PersistentLiveDeltaSign::zero, 0};
    } else {
        result_.persistent_live_delta = {PersistentLiveDeltaSign::negative, a - b};
    }

    // Normalized transient metrics (OD-M5-P7-005). The subtractions are valid
    // by construction: P >= A and P >= max(A, B); the asserts document the
    // invariant and never depend on unsigned wraparound.
    const auto peak = result_.peak_live_bytes_absolute;
    assert(peak >= a);
    result_.peak_above_entry = peak - a;
    const auto ceiling = (a > b) ? a : b;
    assert(peak >= ceiling);
    result_.transient_excess_over_persistent = peak - ceiling;

    // Eligibility (fail-closed, deterministic precedence order).
    result_.live_metrics_eligible = true;
    result_.deallocated_bytes_valid = true;
    result_.ineligibility_reason = LiveIneligibilityReason::none;
    if (state.recursion_observed) {
        result_.ineligibility_reason = LiveIneligibilityReason::instrumentation_error;
    } else if (state.provenance_table_overflowed) {
        result_.ineligibility_reason = LiveIneligibilityReason::provenance_table_overflow;
    } else if (state.stale_entry_collision) {
        result_.ineligibility_reason = LiveIneligibilityReason::stale_entry_collision;
    } else if (state.sized_delete_mismatch) {
        result_.ineligibility_reason = LiveIneligibilityReason::sized_delete_mismatch;
    } else if (state.live_bytes_wrapped) {
        result_.ineligibility_reason = LiveIneligibilityReason::live_bytes_arithmetic_wrap;
    } else if (state.bracket_unknown_pointer_delete) {
        result_.ineligibility_reason = LiveIneligibilityReason::unknown_pointer_delete;
    }
    if (result_.ineligibility_reason != LiveIneligibilityReason::none) {
        result_.live_metrics_eligible = false;
        result_.deallocated_bytes_valid = false;
    }
    if (state.deallocated_bytes_overflowed) {
        result_.deallocated_bytes_valid = false;
    }
    result_.allocation_count_valid = !state.allocation_count_overflowed;
    result_.total_allocated_bytes_valid = !state.total_allocated_bytes_overflowed;
    result_.deallocation_count_valid = !state.deallocation_count_overflowed;
    result_.backing_diagnostic_valid = !state.backing_diagnostic_overflowed;
}

const MeasurementResult& MeasurementScope::result() const noexcept { return result_; }

bool MeasurementScope::measurement_active() noexcept { return detail::state().measurement_active; }

} // namespace bmd_projection::m5::allocation
