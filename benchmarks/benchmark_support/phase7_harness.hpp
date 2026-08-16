#pragma once

// Phase-7 measurement harness shared by the dedicated allocation measurement
// executables (bmd_projection_allocation_m2_m3 / _m4 / _replay /
// _footprint). PRIVATE benchmark-support machinery (OD-M5-P7-002): never
// installed, never linked into production.
//
// Bracket discipline (OD-M5-P7-007/014):
//   - preparation, pools, and warmup happen entirely OUTSIDE brackets;
//   - each measured execution brackets exactly ONE logical production
//     operation (A at bracket open, event-driven P, B at bracket close);
//   - owning returned values stay alive at B; their destruction runs outside
//     the bracket and produces the separate post-destroy snapshot D;
//   - cross-repetition exact equality of the normalized metrics is required
//     (OD-M5-P7-015); any variation is instrumentation contamination and the
//     cell is recorded with determinism_confirmed=false (run fails closed).
//
// The replacement global new/delete surface is compiled directly into each
// measurement executable (benchmarks/benchmark_support/allocation_global_new.cpp),
// never through a library.

#include "allocation_instrumentation.hpp"
#include "phase7_record.hpp"
#include "workload_spec.hpp"
#include "wrapper.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace bmd_projection::m5::benchmark {

struct Phase7RunOptions final {
    std::string output_path;
    std::string wrapper_path;
    std::string evidence_class{"exploratory"};
    std::size_t repetitions{3};
    // Optional development subset filter: a cell runs only when its
    // measurement scope contains the substring. The exact-inventory
    // completeness assertion is skipped while a filter is active.
    std::string filter;
};

[[nodiscard]] Phase7RunOptions parse_phase7_options(int argc, char** argv);

struct Phase7CellOutcome final {
    m5::allocation::MeasurementResult measurement;
    std::string disposition_error;
    bool disposition_ok{true};
    bool operation_ok{true};
    bool determinism_confirmed{};
    bool has_post_destroy{};
    std::uint64_t post_destroy_live_bytes{};
    std::string post_destroy_lifecycle_status{"not_applicable"};
};

// Normalized per-operation metrics (OD-M5-P7-015): the values whose exact
// equality across equal preconditions is required.
struct Phase7NormalizedMetrics final {
    std::uint64_t allocation_count{};
    std::uint64_t total_allocated_bytes{};
    std::uint64_t deallocation_count{};
    std::uint64_t deallocated_bytes{};
    allocation::PersistentLiveDelta persistent_delta{};
    std::uint64_t peak_above_entry{};
    std::uint64_t transient_excess_over_persistent{};

    friend constexpr bool operator==(const Phase7NormalizedMetrics&,
                                     const Phase7NormalizedMetrics&) = default;
};

[[nodiscard]] Phase7NormalizedMetrics
normalized_metrics_of(const m5::allocation::MeasurementResult& result) noexcept;

class Phase7Harness final {
  public:
    explicit Phase7Harness(Phase7RunOptions options);

    // Runs `operation` inside a fresh measurement bracket once per repetition.
    // `prepare`, when non-empty, runs BEFORE each bracket (state setup that
    // must stay outside the measured region). `destroy_owner`, when non-empty,
    // destroys the owning result AFTER each bracket closes (B was taken with
    // the owner alive); the post-destroy snapshot D is recorded and the
    // lifecycle status is derived (destroyed when D == A,
    // destroyed_mismatch otherwise — OD-M5-P7-005).
    [[nodiscard]] Phase7CellOutcome
    measure_cell(const std::function<void()>& operation,
                 const std::function<void()>& destroy_owner = {},
                 const std::function<void()>& prepare = {});

    // One empty-bracket calibration measurement (reported separately, never
    // subtracted; OD-M5-P7-006/007).
    [[nodiscard]] m5::allocation::MeasurementResult measure_calibration();

    // Workload identity lookup over the static registry.
    [[nodiscard]] const std::pair<std::string, std::string>* find_workload(
        std::string_view name) const;

    void add_record(std::string record_json) { records_.push_back(std::move(record_json)); }

    void add_calibration_record(std::string calibration_json) {
        calibration_records_.push_back(std::move(calibration_json));
    }

    // Writes the records payload and the M5_ALLOCATION_WRAPPER_V1 wrapper
    // binding it. Returns false on IO/binding failure.
    [[nodiscard]] bool emit();

    [[nodiscard]] const Phase7RunOptions& options() const noexcept { return options_; }
    [[nodiscard]] const std::vector<std::string>& records() const noexcept { return records_; }

  private:
    Phase7RunOptions options_;
    std::vector<std::string> records_;
    std::vector<std::string> calibration_records_;
};

} // namespace bmd_projection::m5::benchmark
