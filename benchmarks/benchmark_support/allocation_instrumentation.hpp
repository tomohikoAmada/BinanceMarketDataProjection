#pragma once

// M5 Phase-7 (allocation/memory characterization) allocation instrumentation
// substrate — implementation PR-A, work package WP1.
//
// PRIVATE: this header belongs only to dedicated single-threaded Phase-7
// measurement executables and to the adversarial instrumentation test
// executable. It is never installed, never exported, and never included by
// Core, ProtoAdapter, the Phase-6 Google Benchmark executable, or any
// production consumer (OD-M5-P7-002).
//
// Contract: docs/M5_PHASE7_PREIMPLEMENTATION_DECISIONS.md
// (OD-M5-P7-002 through OD-M5-P7-007, OD-M5-P7-019, OD-M5-P7-020).
//
// Measurement boundary: cxx_replaceable_global_new (OD-M5-P7-004). This
// substrate measures ONLY traffic observed through the replaceable global
// new/delete surface; it makes no claim about direct malloc/arena channels.
//
// The bookkeeping state is constant-initialized static storage with a trivial
// destructor: no heap allocation, no dynamic initialization, no
// initialization/destruction-order dependency (OD-M5-P7-002). The provenance
// table is fixed-capacity storage that never grows (OD-M5-P7-019).

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace bmd_projection::m5::allocation {

inline constexpr std::string_view kAllocationBoundary = "cxx_replaceable_global_new";

// Fixed provenance capacity. The earlier 2^23 planning figure was an
// implementation-sizing estimate, NOT an accepted OD constant; this default is
// a compile-time knob. A dedicated executable may override it with
// BMD_P7_PROVENANCE_CAPACITY without changing measurement semantics. The
// adversarial test executable exercises deterministic capacity exhaustion via
// the test-only effective-capacity clamp seam instead of shrinking the table
// at compile time.
#ifndef BMD_P7_PROVENANCE_CAPACITY
inline constexpr std::size_t kProvenanceCapacity = 1U << 20;
#else
inline constexpr std::size_t kProvenanceCapacity = BMD_P7_PROVENANCE_CAPACITY;
#endif

// Input for the shared allocation-accounting helper invoked by every
// replacement global allocation function after its backing block succeeded.
struct AllocationRecordInput final {
    std::size_t raw_size{};
    std::size_t backing_size{};
};

// Input for the shared deallocation-accounting helper invoked by every
// replacement global deallocation function. A sized delete supplies the size
// argument only as a consistency check; the authoritative raw requested size
// always comes from the provenance table (OD-M5-P7-003).
struct DeallocationRecordInput final {
    bool has_supplied_size{};
    std::size_t supplied_size{};
};

// Records a successful intercepted allocation (provenance insert, live-bytes
// accounting, bracket traffic accounting). Never allocates; never throws.
void record_successful_allocation(void* ptr, AllocationRecordInput input) noexcept;

// Records an intercepted deallocation (provenance resolve/remove, live-bytes
// accounting, bracket traffic accounting, fail-closed unknown-pointer and
// sized-mismatch handling per OD-M5-P7-019). Never allocates; never throws.
void record_deallocation(void* ptr, DeallocationRecordInput input) noexcept;

// Marks a backing-request failure (checked-arithmetic overflow or malloc
// failure). The failed request is never counted and never recorded
// (OD-M5-P7-019); a failure inside an active bracket flags the bracket result.
void record_backing_failure() noexcept;

// Non-allocating process-lifetime read of the tracked live-bytes counter.
[[nodiscard]] std::uint64_t live_bytes_snapshot() noexcept;

// Signed persistent live change between bracket open and close
// (OD-M5-P7-005): the difference is NEVER forced through signed subtraction;
// the magnitude is an exact uint64 and the sign is explicit.
enum class PersistentLiveDeltaSign : std::uint8_t {
    negative,
    zero,
    positive,
};

struct PersistentLiveDelta final {
    PersistentLiveDeltaSign sign{};
    std::uint64_t magnitude{};

    friend constexpr bool operator==(const PersistentLiveDelta&,
                                     const PersistentLiveDelta&) = default;
};

// Machine-readable reason for live/deallocated-byte metric ineligibility
// (OD-M5-P7-005 / OD-M5-P7-019). The first violated condition in the defined
// precedence order is reported.
enum class LiveIneligibilityReason : std::uint8_t {
    none,
    provenance_table_overflow,
    unknown_pointer_delete,
    stale_entry_collision,
    sized_delete_mismatch,
    live_bytes_arithmetic_wrap,
    instrumentation_error,
};

// Frozen, non-allocating bracket result (OD-M5-P7-005). Raw A/P/B values are
// recorded for auditing; the normalized quantities
// (persistent_live_delta, peak_above_entry, transient_excess_over_persistent)
// are the primary per-operation evidence and are valid whenever the live
// metric class is eligible. Count classes are invalidated independently, only
// by their own counter overflow (OD-M5-P7-019).
struct MeasurementResult final {
    std::uint64_t live_bytes_before{};
    std::uint64_t peak_live_bytes_absolute{};
    std::uint64_t live_bytes_after{};
    std::uint64_t allocation_count{};
    std::uint64_t total_allocated_bytes{};
    std::uint64_t deallocation_count{};
    std::uint64_t deallocated_bytes{};
    std::uint64_t instrument_backing_request_bytes{};
    PersistentLiveDelta persistent_live_delta{};
    std::uint64_t peak_above_entry{};
    std::uint64_t transient_excess_over_persistent{};
    bool allocation_count_valid{};
    bool total_allocated_bytes_valid{};
    bool deallocation_count_valid{};
    bool deallocated_bytes_valid{};
    bool backing_diagnostic_valid{};
    bool live_metrics_eligible{};
    LiveIneligibilityReason ineligibility_reason{};
    bool operation_aborted{};
    bool allocation_failure_observed{};
};

// RAII measurement bracket (OD-M5-P7-005 / OD-M5-P7-019).
//
//   open:  requires no active scope (nested opening is a defined fail-closed
//          abort with a stable diagnostic); resets the bracket traffic
//          counters; A = current live bytes; P = A; activates measurement.
//   close: B = current live bytes; P = max(P, B); deactivates traffic
//          counting; freezes the result; derives the normalized metrics.
//
// The constructor and finalization never allocate. If the measured operation
// throws, the destructor still closes the bracket and the result is marked
// operation_aborted; measurement is never left permanently active. Opening a
// bracket never clears provenance (OD-M5-P7-002).
class MeasurementScope final {
  public:
    MeasurementScope();
    ~MeasurementScope();

    MeasurementScope(const MeasurementScope&) = delete;
    MeasurementScope& operator=(const MeasurementScope&) = delete;
    MeasurementScope(MeasurementScope&&) = delete;
    MeasurementScope& operator=(MeasurementScope&&) = delete;

    // Closes the bracket and freezes the result (idempotent; the destructor
    // calls it automatically for exception-safe RAII closure).
    void finish() noexcept;

    [[nodiscard]] const MeasurementResult& result() const noexcept;

    [[nodiscard]] static bool measurement_active() noexcept;

  private:
    MeasurementResult result_{};
    int uncaught_exceptions_at_entry_{};
    bool closed_{};
};

namespace detail {

// Slot state encoding relies on constant (zero) initialization: 0 == empty.
enum SlotState : std::uint8_t {
    kEmpty = 0,
    kOccupied = 1,
    kTombstone = 2,
};

struct ProvenanceSlot final {
    void* ptr;
    std::uint64_t raw_requested_size;
    std::uint64_t backing_request_size;
    std::uint8_t state;
};

// Constant-initialized, trivially destructible process-lifetime state
// (OD-M5-P7-002): usable before main() and after global destruction begins.
// Single-threaded measurement/test executables only; no synchronization.
// The trailing fields are test-only seam state: they are zero unless enabled
// by the test-seam translation unit and never affect measurement executables.
struct InstrumentationState final {
    ProvenanceSlot slots[kProvenanceCapacity];
    std::uint64_t live_bytes;
    bool provenance_table_overflowed;
    bool stale_entry_collision;
    bool sized_delete_mismatch;
    bool live_bytes_wrapped;
    bool allocation_count_overflowed;
    bool total_allocated_bytes_overflowed;
    bool deallocation_count_overflowed;
    bool deallocated_bytes_overflowed;
    bool backing_diagnostic_overflowed;
    bool recursion_observed;
    bool measurement_active;
    std::uint64_t bracket_allocation_count;
    std::uint64_t bracket_total_allocated_bytes;
    std::uint64_t bracket_deallocation_count;
    std::uint64_t bracket_deallocated_bytes;
    std::uint64_t bracket_backing_request_bytes;
    std::uint64_t bracket_peak;
    bool bracket_unknown_pointer_delete;
    bool bracket_allocation_failure;
    std::uint64_t backing_failure_remaining;
    std::size_t capacity_clamp;
    bool reuse_capture_pending;
    bool reuse_ready;
    void* reuse_captured;
    std::uint64_t reuse_captured_backing;
};

// Single process-lifetime state instance (defined in the core TU).
[[nodiscard]] InstrumentationState& state() noexcept;

// Sentinel returned by find_slot when the pointer is not tracked.
[[nodiscard]] std::size_t no_slot() noexcept;

// Bounded linear-probe lookup over the effective capacity. Never allocates.
[[nodiscard]] std::size_t find_slot(const InstrumentationState& state,
                                    void* ptr) noexcept;

} // namespace detail

} // namespace bmd_projection::m5::allocation
