#pragma once

// M5 Phase-8 benchmark-test-only allocation failure seam (PR-A / WP1).
//
// Used exclusively by the Phase-8 conformance tests to demonstrate the
// replace_all strong exception guarantee deterministically. The ordinary
// candidate models default to std::allocator and are never routed through this
// type; this file is a test-only construction, not a production failpoint.

#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>

namespace bmd_projection::m5::phase8::test {

// Deterministic allocation failpoint. Stateless on purpose so that every
// model container move/swap path keeps its default noexcept contract even when
// this allocator is injected.
struct ThrowingFailpoint {
    static inline std::size_t remaining_allocations = 0;
    static inline bool enabled = false;

    static void arm(std::size_t allow_successes) {
        enabled = true;
        remaining_allocations = allow_successes;
    }

    static void disarm() {
        enabled = false;
        remaining_allocations = 0;
    }
};

template <typename T> class ThrowingAlloc {
  public:
    using value_type = T;
    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;
    using is_always_equal = std::true_type;

    constexpr ThrowingAlloc() noexcept = default;

    template <typename U>
    constexpr ThrowingAlloc(const ThrowingAlloc<U>& other [[maybe_unused]]) noexcept {}

    [[nodiscard]] T* allocate(std::size_t n) {
        if (ThrowingFailpoint::enabled) {
            if (ThrowingFailpoint::remaining_allocations == 0) {
                throw std::bad_alloc();
            }
            --ThrowingFailpoint::remaining_allocations;
        }
        return std::allocator<T>{}.allocate(n);
    }

    void deallocate(T* ptr, std::size_t n) noexcept { std::allocator<T>{}.deallocate(ptr, n); }

    template <typename U>
    [[nodiscard]] friend constexpr bool operator==(const ThrowingAlloc& lhs [[maybe_unused]],
                                                   const ThrowingAlloc<U>& rhs
                                                   [[maybe_unused]]) noexcept {
        return true;
    }

    template <typename U>
    [[nodiscard]] friend constexpr bool operator!=(const ThrowingAlloc& lhs [[maybe_unused]],
                                                   const ThrowingAlloc<U>& rhs
                                                   [[maybe_unused]]) noexcept {
        return false;
    }
};

static_assert(std::is_nothrow_move_constructible_v<ThrowingAlloc<int>>);
static_assert(std::is_nothrow_move_assignable_v<ThrowingAlloc<int>>);

} // namespace bmd_projection::m5::phase8::test