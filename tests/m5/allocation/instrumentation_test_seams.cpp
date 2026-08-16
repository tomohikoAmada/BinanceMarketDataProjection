#include "instrumentation_test_seams.hpp"

#include "allocation_instrumentation.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace bmd_projection::m5::allocation::test {

namespace {

void clear_slot(detail::ProvenanceSlot& slot) noexcept {
    slot.ptr = nullptr;
    slot.raw_requested_size = 0;
    slot.backing_request_size = 0;
    slot.state = detail::kEmpty;
}

} // namespace

void forget_provenance_for_test(void* ptr) {
    auto& state = detail::state();
    assert(!state.measurement_active);
    if (ptr == nullptr) {
        return;
    }
    const auto index = detail::find_slot(state, ptr);
    if (index == detail::no_slot()) {
        return;
    }
    const auto size = state.slots.at(index).raw_requested_size;
    assert(size <= state.live_bytes);
    clear_slot(state.slots.at(index));
    if (!state.live_bytes_wrapped) {
        state.live_bytes -= size;
    }
}

void set_backing_failure_for_test(std::uint64_t failures_remaining) noexcept {
    detail::state().backing_failure_remaining = failures_remaining;
}

void set_provenance_capacity_clamp_for_test(std::size_t capacity) noexcept {
    detail::state().capacity_clamp = capacity;
}

void seed_counter_for_test(CounterId counter, std::uint64_t value) noexcept {
    auto& state = detail::state();
    switch (counter) {
    case CounterId::allocation_count:
        assert(state.measurement_active);
        state.bracket_allocation_count = value;
        return;
    case CounterId::total_allocated_bytes:
        assert(state.measurement_active);
        state.bracket_total_allocated_bytes = value;
        return;
    case CounterId::deallocation_count:
        assert(state.measurement_active);
        state.bracket_deallocation_count = value;
        return;
    case CounterId::deallocated_bytes:
        assert(state.measurement_active);
        state.bracket_deallocated_bytes = value;
        return;
    case CounterId::live_bytes:
        state.live_bytes = value;
        return;
    }
}

void capture_next_freed_backing_for_test() noexcept {
    auto& state = detail::state();
    state.reuse_capture_pending = true;
    state.reuse_captured = nullptr;
    state.reuse_captured_backing = 0;
}

bool has_captured_backing_for_test() noexcept { return detail::state().reuse_ready; }

void release_captured_backing_for_test() noexcept {
    auto& state = detail::state();
    if (state.reuse_ready && state.reuse_captured != nullptr) {
        // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
        std::free(state.reuse_captured);
    }
    state.reuse_capture_pending = false;
    state.reuse_ready = false;
    state.reuse_captured = nullptr;
    state.reuse_captured_backing = 0;
}

void reset_instrumentation_state_for_test() noexcept {
    auto& state = detail::state();
    assert(!state.measurement_active);
    release_captured_backing_for_test();
    for (auto& slot : state.slots) {
        clear_slot(slot);
    }
    state.live_bytes = 0;
    state.provenance_table_overflowed = false;
    state.stale_entry_collision = false;
    state.sized_delete_mismatch = false;
    state.live_bytes_wrapped = false;
    state.allocation_count_overflowed = false;
    state.total_allocated_bytes_overflowed = false;
    state.deallocation_count_overflowed = false;
    state.deallocated_bytes_overflowed = false;
    state.backing_diagnostic_overflowed = false;
    state.recursion_observed = false;
    state.bracket_allocation_count = 0;
    state.bracket_total_allocated_bytes = 0;
    state.bracket_deallocation_count = 0;
    state.bracket_deallocated_bytes = 0;
    state.bracket_backing_request_bytes = 0;
    state.bracket_peak = 0;
    state.bracket_unknown_pointer_delete = false;
    state.bracket_allocation_failure = false;
    state.backing_failure_remaining = 0;
    state.capacity_clamp = 0;
}

bool has_provenance_for_test(void* ptr) noexcept {
    return detail::find_slot(detail::state(), ptr) != detail::no_slot();
}

std::uint64_t recorded_size_for_test(void* ptr) noexcept {
    const auto& state = detail::state();
    const auto index = detail::find_slot(state, ptr);
    assert(index != detail::no_slot());
    return state.slots.at(index).raw_requested_size;
}

bool provenance_table_overflowed_for_test() noexcept {
    return detail::state().provenance_table_overflowed;
}

bool stale_entry_collision_for_test() noexcept { return detail::state().stale_entry_collision; }

bool sized_delete_mismatch_for_test() noexcept { return detail::state().sized_delete_mismatch; }

bool live_bytes_wrapped_for_test() noexcept { return detail::state().live_bytes_wrapped; }

bool recursion_observed_for_test() noexcept { return detail::state().recursion_observed; }

bool counter_overflowed_for_test(CounterId counter) noexcept {
    const auto& state = detail::state();
    switch (counter) {
    case CounterId::allocation_count:
        return state.allocation_count_overflowed;
    case CounterId::total_allocated_bytes:
        return state.total_allocated_bytes_overflowed;
    case CounterId::deallocation_count:
        return state.deallocation_count_overflowed;
    case CounterId::deallocated_bytes:
        return state.deallocated_bytes_overflowed;
    case CounterId::live_bytes:
        return state.live_bytes_wrapped;
    }
    return false;
}

std::uintptr_t force_observable_allocation_for_test(void* ptr) noexcept {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<std::uintptr_t>(ptr);
}

} // namespace bmd_projection::m5::allocation::test
