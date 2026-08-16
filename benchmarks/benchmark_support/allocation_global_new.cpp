// M5 Phase-7 (allocation/memory characterization): executable-local
// replacement global allocation/deallocation surface (implementation PR-A,
// work package WP1).
//
// This translation unit is compiled DIRECTLY into each dedicated Phase-7
// measurement/test executable (never into a library, never installed), so the
// replacement definitions have unambiguous ownership and cannot leak into
// Core, ProtoAdapter, the Phase-6 Google Benchmark executable, or normal
// consumers (OD-M5-P7-002).
//
// It implements the complete C++20 replaceable global allocation surface
// ([new.delete.single] / [new.delete.array]; OD-M5-P7-003): 8 allocation and
// 12 deallocation forms. Sized aligned deletes take
// (void*, std::size_t, std::align_val_t) — size BEFORE alignment.
//
// Backing model (OD-M5-P7-003 / M5-P7-MR-002):
//   effective     = max(raw_requested, 1)
//   backing       = checked_add(checked_add(effective, alignment - 1),
//                                sizeof(void*))
// every addition checked BEFORE malloc or pointer arithmetic; the aligned
// payload is placed inside the proven block range and the original block
// pointer is stored in the header slot immediately preceding the payload so
// every delete form can release the original backing exactly once. Raw
// requested-byte metrics never include alignment padding or the header
// (OD-M5-P7-004); the backing request size is the separate
// instrument_backing_request_bytes diagnostic.

#include "allocation_instrumentation.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <new>

namespace bmd_projection::m5::allocation {

namespace {

[[nodiscard]] bool checked_size_add(std::size_t lhs, std::size_t rhs,
                                    std::size_t& result) noexcept {
    if (rhs > std::numeric_limits<std::size_t>::max() - lhs) {
        return false;
    }
    result = lhs + rhs;
    return true;
}

// Shared allocation path behind every replacement allocation form.
//
// Failure semantics (OD-M5-P7-019): checked-arithmetic failure, deterministic
// test backing failure, and malloc failure all complete BEFORE provenance
// insertion, live accounting, and successful-allocation counting; the throwing
// forms throw std::bad_alloc, the nothrow forms return nullptr.
struct AllocationRequest final {
    std::size_t raw_size{};
    std::size_t alignment{};
    bool nothrow{};
};

[[nodiscard]] void* allocate(AllocationRequest request) {
    auto& state = detail::state();
    const bool nothrow = request.nothrow;

    // Test-only deterministic pointer-reuse backing support: when the reuse
    // seam captured a previously released backing block, this request reuses
    // it instead of calling malloc. The seam contract requires a request with
    // the same raw size and alignment shape as the captured allocation.
    void* block = nullptr;
    std::uint64_t captured_backing = 0;
    if (state.reuse_ready && state.reuse_captured != nullptr) {
        block = state.reuse_captured;
        captured_backing = state.reuse_captured_backing;
        state.reuse_ready = false;
        state.reuse_captured = nullptr;
        state.reuse_captured_backing = 0;
    }

    // Checked backing arithmetic, performed BEFORE malloc or pointer
    // arithmetic (M5-P7-MR-002). The header slot requires natural void*
    // alignment, so the effective alignment is at least alignof(void*); a
    // stronger alignment than requested always satisfies the request.
    auto alignment = request.alignment;
    if (alignment < alignof(void*)) {
        alignment = alignof(void*);
    }
    const std::size_t effective = (request.raw_size == 0) ? std::size_t{1} : request.raw_size;
    std::size_t padded = 0;
    std::size_t backing = 0;
    const bool padded_ok = checked_size_add(effective, alignment - 1U, padded);
    const bool backing_ok = padded_ok && checked_size_add(padded, sizeof(void*), backing);
    if (!backing_ok) {
        record_backing_failure();
        if (nothrow) {
            return nullptr;
        }
        throw std::bad_alloc{};
    }

    if (block == nullptr) {
        if (state.backing_failure_remaining != 0) {
            --state.backing_failure_remaining;
            record_backing_failure();
            if (nothrow) {
                return nullptr;
            }
            throw std::bad_alloc{};
        }
        // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
        block = std::malloc(backing);
        if (block == nullptr) {
            record_backing_failure();
            if (nothrow) {
                return nullptr;
            }
            throw std::bad_alloc{};
        }
    } else {
        // Test-only reuse seam contract: the reused request must fit the
        // captured block. This branch can only be reached when the test seam
        // captured a block; a violation aborts instead of overflowing the
        // block (the fields are zero in measurement executables).
        if (backing > captured_backing) {
            std::abort();
        }
    }

    // Aligned payload placement inside the proven range
    // [block, block + backing): block + sizeof(void*) + (alignment - 1) is at
    // most block + backing - 1 because backing = effective + (alignment - 1)
    // + sizeof(void*) and effective >= 1, so all pointer arithmetic below
    // stays within the allocated block (no wrap, no out-of-range access).
    // The int<->pointer casts are the unavoidable essence of aligned backing
    // placement; all offsets are computed within the proven range first.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
    auto* payload = reinterpret_cast<std::uint8_t*>(block) + sizeof(void*) + (alignment - 1U);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto payload_value = reinterpret_cast<std::uintptr_t>(payload);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,performance-no-int-to-ptr)
    payload = reinterpret_cast<std::uint8_t*>(payload_value &
                                              ~static_cast<std::uintptr_t>(alignment - 1U));
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
    *reinterpret_cast<void**>(payload - sizeof(void*)) = block;

    record_successful_allocation(payload, {request.raw_size, backing});
    return payload;
}

// Shared deallocation path behind every replacement deallocation form. The
// authoritative raw requested size comes from the provenance table; a supplied
// sized-delete argument is only a consistency check. Storage is released
// exactly once via the stored backing header (or captured by the test-only
// pointer-reuse seam instead of being freed).
void deallocate(void* ptr, bool has_supplied_size, std::size_t supplied_size) noexcept {
    if (ptr == nullptr) {
        return;
    }
    auto& state = detail::state();
    const bool capture = state.reuse_capture_pending;
    std::uint64_t captured_backing = 0;
    if (capture) {
        const auto index = detail::find_slot(state, ptr);
        if (index != detail::no_slot()) {
            captured_backing = state.slots.at(index).backing_request_size;
        }
    }
    record_deallocation(ptr, {has_supplied_size, supplied_size});
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
    auto* const header_slot = reinterpret_cast<std::uint8_t*>(ptr) - sizeof(void*);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    auto* const block = *reinterpret_cast<void**>(header_slot);
    if (capture) {
        state.reuse_capture_pending = false;
        state.reuse_captured = block;
        state.reuse_captured_backing = captured_backing;
        state.reuse_ready = true;
        return;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
    std::free(block);
}

} // namespace

} // namespace bmd_projection::m5::allocation

// ---------------------------------------------------------------------------
// Throwing single-object forms ([new.delete.single]/1, /9)
// ---------------------------------------------------------------------------

void* operator new(std::size_t size) {
    return bmd_projection::m5::allocation::allocate({size, alignof(std::max_align_t), false});
}

void* operator new(std::size_t size, std::align_val_t alignment) {
    return bmd_projection::m5::allocation::allocate(
        {size, static_cast<std::size_t>(alignment), false});
}

// ---------------------------------------------------------------------------
// Throwing array forms ([new.delete.array]/1, /9)
// ---------------------------------------------------------------------------

void* operator new[](std::size_t size) {
    return bmd_projection::m5::allocation::allocate({size, alignof(std::max_align_t), false});
}

void* operator new[](std::size_t size, std::align_val_t alignment) {
    return bmd_projection::m5::allocation::allocate(
        {size, static_cast<std::size_t>(alignment), false});
}

// ---------------------------------------------------------------------------
// Nothrow single-object forms ([new.delete.single]/4, /10)
// ---------------------------------------------------------------------------

void* operator new(std::size_t size, const std::nothrow_t& nothrow_tag) noexcept {
    static_cast<void>(nothrow_tag);
    return bmd_projection::m5::allocation::allocate({size, alignof(std::max_align_t), true});
}

void* operator new(std::size_t size, std::align_val_t alignment,
                   const std::nothrow_t& nothrow_tag) noexcept {
    static_cast<void>(nothrow_tag);
    return bmd_projection::m5::allocation::allocate(
        {size, static_cast<std::size_t>(alignment), true});
}

// ---------------------------------------------------------------------------
// Nothrow array forms ([new.delete.array]/4, /10)
// ---------------------------------------------------------------------------

void* operator new[](std::size_t size, const std::nothrow_t& nothrow_tag) noexcept {
    static_cast<void>(nothrow_tag);
    return bmd_projection::m5::allocation::allocate({size, alignof(std::max_align_t), true});
}

void* operator new[](std::size_t size, std::align_val_t alignment,
                     const std::nothrow_t& nothrow_tag) noexcept {
    static_cast<void>(nothrow_tag);
    return bmd_projection::m5::allocation::allocate(
        {size, static_cast<std::size_t>(alignment), true});
}

// ---------------------------------------------------------------------------
// Unsized single-object deallocation forms ([new.delete.single]/5, /11)
// ---------------------------------------------------------------------------

void operator delete(void* ptr) noexcept {
    bmd_projection::m5::allocation::deallocate(ptr, false, 0);
}

void operator delete(void* ptr, std::align_val_t alignment) noexcept {
    static_cast<void>(alignment);
    bmd_projection::m5::allocation::deallocate(ptr, false, 0);
}

// ---------------------------------------------------------------------------
// Sized single-object deallocation forms ([new.delete.single]/6, /12)
// NOTE: (void*, std::size_t, std::align_val_t) — size BEFORE alignment.
// ---------------------------------------------------------------------------

void operator delete(void* ptr, std::size_t size) noexcept {
    bmd_projection::m5::allocation::deallocate(ptr, true, size);
}

void operator delete(void* ptr, std::size_t size, std::align_val_t alignment) noexcept {
    static_cast<void>(alignment);
    bmd_projection::m5::allocation::deallocate(ptr, true, size);
}

// ---------------------------------------------------------------------------
// Nothrow single-object deallocation forms ([new.delete.single]/7, /13)
// ---------------------------------------------------------------------------

void operator delete(void* ptr, const std::nothrow_t& nothrow_tag) noexcept {
    static_cast<void>(nothrow_tag);
    bmd_projection::m5::allocation::deallocate(ptr, false, 0);
}

void operator delete(void* ptr, std::align_val_t alignment,
                     const std::nothrow_t& nothrow_tag) noexcept {
    static_cast<void>(alignment);
    static_cast<void>(nothrow_tag);
    bmd_projection::m5::allocation::deallocate(ptr, false, 0);
}

// ---------------------------------------------------------------------------
// Unsized array deallocation forms ([new.delete.array]/5, /11)
// ---------------------------------------------------------------------------

void operator delete[](void* ptr) noexcept {
    bmd_projection::m5::allocation::deallocate(ptr, false, 0);
}

void operator delete[](void* ptr, std::align_val_t alignment) noexcept {
    static_cast<void>(alignment);
    bmd_projection::m5::allocation::deallocate(ptr, false, 0);
}

// ---------------------------------------------------------------------------
// Sized array deallocation forms ([new.delete.array]/6, /12)
// NOTE: (void*, std::size_t, std::align_val_t) — size BEFORE alignment.
// ---------------------------------------------------------------------------

void operator delete[](void* ptr, std::size_t size) noexcept {
    bmd_projection::m5::allocation::deallocate(ptr, true, size);
}

void operator delete[](void* ptr, std::size_t size, std::align_val_t alignment) noexcept {
    static_cast<void>(alignment);
    bmd_projection::m5::allocation::deallocate(ptr, true, size);
}

// ---------------------------------------------------------------------------
// Nothrow array deallocation forms ([new.delete.array]/7, /13)
// ---------------------------------------------------------------------------

void operator delete[](void* ptr, const std::nothrow_t& nothrow_tag) noexcept {
    static_cast<void>(nothrow_tag);
    bmd_projection::m5::allocation::deallocate(ptr, false, 0);
}

void operator delete[](void* ptr, std::align_val_t alignment,
                       const std::nothrow_t& nothrow_tag) noexcept {
    static_cast<void>(alignment);
    static_cast<void>(nothrow_tag);
    bmd_projection::m5::allocation::deallocate(ptr, false, 0);
}
