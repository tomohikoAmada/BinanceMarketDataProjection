#pragma once

// M5 Phase-7 machine-readable evidence schemas (OD-M5-P7-016).
//
// Phase-7-owned schema families:
//
//   M5_ALLOCATION_WRAPPER_V1          run-level provenance wrapper; reuses the
//                                     Phase-6 provenance field shapes (source/
//                                     binary/build/environment/M4 identity)
//                                     without duplicating their computation
//   M5_PHASE7_MEASUREMENT_CONTRACT_V1 measurement contract version constant
//   M5_PHASE7_ALLOCATION_RECORD_V1    one measured allocation cell
//   M5_PHASE7_FOOTPRINT_RECORD_V1     one persistent-footprint cell
//
// The records payload document uses the collection envelope
// M5_PHASE7_ALLOCATION_PAYLOAD_V1 (a run-level container, not a record
// schema); every individual record carries its own record schema identity.
//
// Measurement boundary: every allocation/live metric applies ONLY to traffic
// observed through cxx_replaceable_global_new (OD-M5-P7-004). No record may
// claim complete heap traffic; RSS is never measured (OD-M5-P7-006).
//
// Exact rationals: replay per-event derived values are {numerator,
// denominator} pairs over uint64; integer division never appears
// (OD-M5-P7-013, adversarial case 27). persistent_live_delta is
// {sign, magnitude} — never a forced signed subtraction (OD-M5-P7-005).
//
// The producer records machine-readable measurement values only; broad
// acceptance policy belongs to the independent Python validator.

#include "allocation_instrumentation.hpp"
#include "wrapper.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bmd_projection::m5::benchmark {

inline constexpr const char* kAllocationWrapperSchema = "M5_ALLOCATION_WRAPPER_V1";
inline constexpr const char* kPhase7MeasurementContract = "M5_PHASE7_MEASUREMENT_CONTRACT_V1";
inline constexpr const char* kAllocationRecordSchema = "M5_PHASE7_ALLOCATION_RECORD_V1";
inline constexpr const char* kFootprintRecordSchema = "M5_PHASE7_FOOTPRINT_RECORD_V1";
inline constexpr const char* kAllocationPayloadSchema = "M5_PHASE7_ALLOCATION_PAYLOAD_V1";

// Canonical result subtree binding (OD-M5-P7-016 "result_payload_sha256"):
// each record carries the SHA-256 of the canonical serialization of its own
// `result` object (JSON object with lexicographically sorted keys, no
// whitespace, minimal string escaping). The producer emits the same canonical
// shape the independent Python validator recomputes, so a swapped or tampered
// result value breaks the binding.
struct Rational final {
    std::uint64_t numerator{};
    std::uint64_t denominator{};

    friend constexpr bool operator==(const Rational&, const Rational&) noexcept = default;
};

struct ReplayAggregate final {
    std::uint64_t aggregate_allocation_count{};
    std::uint64_t aggregate_allocated_bytes{};
    std::uint64_t aggregate_deallocation_count{};
    std::uint64_t aggregate_deallocated_bytes{};
    std::uint64_t event_count{};
    // Derived per-event exact rationals (numerator = aggregate_total,
    // denominator = event_count); never integer division (OD-M5-P7-013).
    Rational derived_per_event_allocations{};
    Rational derived_per_event_bytes{};
};

// Per-record run-level provenance (reused Phase-6 shapes, recomputed by the
// shared Phase-6 wrapper helpers; the record duplicates them so every record
// is independently validatable).
struct Phase7Provenance final {
    std::string source_git_sha;
    std::string source_status;
    bool source_dirty_at_configure{};
    std::string binary_path;
    std::string binary_sha256;
    std::string compiler_id;
    std::string compiler_version;
    std::string cxx_standard;
    std::string build_type;
    std::string sanitizer_state;
    std::string lto_state;
    std::string standard_library_name;
    std::string standard_library_version;
    std::string standard_library_detection_status;
    std::string conan_lock_sha256;
    std::string os_name;
    std::string os_version;
    std::string architecture;
    std::string cpu_model;
    std::string logical_core_count;
    std::string m4_dependency_status;
    std::string contracts_source_revision;
    std::string contracts_conan_reference;
    std::string contracts_recipe_revision;
    std::string contracts_package_id;
    std::string protobuf_runtime_version;
    std::string protobuf_runtime_rrev;
};

struct AllocationRecordInput final {
    std::string measurement_scope;
    std::string operation_denominator;
    std::string workload_id;
    std::string workload_spec_sha256;
    std::string generator_schema;
    std::string generated_workload_sha256;
    std::string seed{"not_applicable"};
    std::string canonical_log_sha256{"not_applicable"};
    std::optional<std::uint64_t> event_count;
    std::string evidence_class{"exploratory"};
    // Frozen bracket result (A/P/B, counters, validity, eligibility).
    m5::allocation::MeasurementResult measurement;
    // Owning-output lifecycle (OD-M5-P7-005): D recorded after the owning
    // result is destroyed OUTSIDE the bracket; B→D is never in the counters.
    bool has_post_destroy_snapshot{};
    std::uint64_t post_destroy_live_bytes{};
    std::string post_destroy_lifecycle_status{"not_applicable"};
    std::optional<ReplayAggregate> replay_aggregate;
    // Baseline definition: where A and B were taken and the exact delta
    // formula (OD-M5-P7-007).
    std::string baseline_snapshot_a;
    std::string baseline_snapshot_b;
    std::string baseline_delta_formula;
    std::string calibration_reference;
    std::uint64_t repetitions{};
    bool determinism_confirmed{};
    bool operation_aborted{};
    bool allocation_failure_observed{};
};

struct FootprintRecordInput final {
    std::uint64_t depth_per_side{};
    std::string measurement_scope;
    std::string evidence_class{"exploratory"};
    std::string generator_schema;
    std::string generator_seed{"not_applicable"};
    std::uint64_t empty_book_live_bytes{};
    std::uint64_t bids_only_live_bytes{};
    std::uint64_t both_sides_live_bytes{};
    std::uint64_t pre_experiment_baseline_live_bytes{};
    std::uint64_t post_destroy_live_bytes{};
    std::string post_destroy_lifecycle_status{"not_applicable"};
    bool eligible{};
    std::string ineligibility_reason;
    std::uint64_t repetitions{};
    bool determinism_confirmed{};
    std::string calibration_reference;
    // Model fields (OD-M5-P7-006): measured, non-additive node model,
    // estimated allocator model, and RSS never measured — strictly separated.
    std::string node_structural_model_description;
    std::string node_structural_model_toolchain;
    std::string allocator_backing_model_description;
    std::string allocator_backing_model_scope;
};

struct CalibrationRecordInput final {
    std::string calibration_id;
    std::string evidence_class{"exploratory"};
    m5::allocation::MeasurementResult measurement;
    std::string description;
};

struct AllocationWrapperInput final {
    std::string evidence_class{"exploratory"};
    std::string binary_path;
    std::string payload_path;
    // (workload name, canonical spec text, spec SHA-256) — the same
    // WorkloadRecord shape the Phase-6 wrapper uses.
    std::vector<WorkloadRecord> workloads;
    std::uint64_t repetitions{};
    std::string warmup_kind{"explicit_workload_pass_v1"};
    std::uint64_t warmup_count{1};
};

struct Phase7ProvenanceCollect final {
    // Fills a Phase7Provenance from the reused generated build identity and
    // runtime environment collection (never recomputed independently).
    static Phase7Provenance collect();
};

// Record serialization. Every builder returns the exact JSON text; the
// payload document and wrapper are assembled from these pieces.
[[nodiscard]] std::string build_allocation_record_json(const AllocationRecordInput& input);
[[nodiscard]] std::string build_footprint_record_json(const FootprintRecordInput& input);
[[nodiscard]] std::string build_calibration_record_json(const CalibrationRecordInput& input);
[[nodiscard]] std::string
build_allocation_payload_json(const std::vector<std::string>& records,
                              const std::vector<std::string>& calibration_records,
                              const std::string& measurement_contract = kPhase7MeasurementContract);
[[nodiscard]] std::string build_allocation_wrapper_json(const AllocationWrapperInput& input);

// Canonical result subtree for the result_payload_sha256 binding. The
// canonical text is the JSON object with lexicographically sorted keys, no
// whitespace, minimal escaping (the same shape the Python validator
// recomputes via json.dumps(sort_keys=True, separators=(",", ":"))).
[[nodiscard]] std::string build_canonical_result_text(const AllocationRecordInput& input);
[[nodiscard]] std::string build_canonical_result_text(const FootprintRecordInput& input);

// Parses one canonical workload-spec field from canonical spec text.
[[nodiscard]] std::string workload_spec_field(std::string_view canonical_text,
                                              std::string_view key);

// JSON document SHA-256 helper reused by record and wrapper binding.
[[nodiscard]] std::string json_text_sha256(std::string_view text);

} // namespace bmd_projection::m5::benchmark
