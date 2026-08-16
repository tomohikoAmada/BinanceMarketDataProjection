#include "phase7_record.hpp"

#include "benchmark_build_identity.hpp"
#include "canonical_text.hpp"
#include "environment_identity.hpp"
#include "phase6_json.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace bmd_projection::m5::benchmark {
namespace {

using allocation::LiveIneligibilityReason;
using allocation::PersistentLiveDeltaSign;

[[nodiscard]] std::string_view sign_name(PersistentLiveDeltaSign sign) noexcept {
    switch (sign) {
    case PersistentLiveDeltaSign::negative:
        return "negative";
    case PersistentLiveDeltaSign::zero:
        return "zero";
    case PersistentLiveDeltaSign::positive:
        return "positive";
    }
    return "zero";
}

[[nodiscard]] std::string_view reason_name(LiveIneligibilityReason reason) noexcept {
    switch (reason) {
    case LiveIneligibilityReason::none:
        return "none";
    case LiveIneligibilityReason::provenance_table_overflow:
        return "provenance_table_overflow";
    case LiveIneligibilityReason::unknown_pointer_delete:
        return "unknown_pointer_delete";
    case LiveIneligibilityReason::stale_entry_collision:
        return "stale_entry_collision";
    case LiveIneligibilityReason::sized_delete_mismatch:
        return "sized_delete_mismatch";
    case LiveIneligibilityReason::live_bytes_arithmetic_wrap:
        return "live_bytes_arithmetic_wrap";
    case LiveIneligibilityReason::instrumentation_error:
        return "instrumentation_error";
    }
    return "instrumentation_error";
}

[[nodiscard]] std::string sha256_of(std::string_view text) {
    const auto hash = bmd_projection::m5::replay::sha256_hex(text);
    if (!std::holds_alternative<std::string>(hash)) {
        std::abort();
    }
    return std::get<std::string>(hash);
}

// Reused Phase-6 provenance shapes serialized once (OD-M5-P7-016); the
// per-record provenance block and the wrapper share this emission.
[[nodiscard]] std::string provenance_json(const Phase7Provenance& provenance) {
    json::Writer writer;
    writer.begin_object();
    writer.key("source");
    writer.begin_object();
    writer.key("git_sha");
    writer.value(provenance.source_git_sha);
    writer.key("status");
    writer.value(provenance.source_status);
    writer.key("dirty_at_configure");
    writer.value(provenance.source_dirty_at_configure);
    writer.end_object();
    writer.key("binary");
    writer.begin_object();
    writer.key("path");
    writer.value(provenance.binary_path);
    writer.key("sha256");
    writer.value(provenance.binary_sha256);
    writer.end_object();
    writer.key("build");
    writer.begin_object();
    writer.key("compiler");
    writer.begin_object();
    writer.key("id");
    writer.value(provenance.compiler_id);
    writer.key("version");
    writer.value(provenance.compiler_version);
    writer.end_object();
    writer.key("cxx_standard");
    writer.value(provenance.cxx_standard);
    writer.key("build_type");
    writer.value(provenance.build_type);
    writer.key("sanitizer_state");
    writer.value(provenance.sanitizer_state);
    writer.key("lto_state");
    writer.value(provenance.lto_state);
    writer.key("standard_library");
    writer.begin_object();
    writer.key("name");
    writer.value(provenance.standard_library_name);
    writer.key("version");
    writer.value(provenance.standard_library_version);
    writer.key("detection_status");
    writer.value(provenance.standard_library_detection_status);
    writer.end_object();
    writer.key("conan_lock_sha256");
    writer.value(provenance.conan_lock_sha256);
    writer.end_object();
    writer.key("environment");
    writer.begin_object();
    writer.key("os_name");
    writer.value(provenance.os_name);
    writer.key("os_version");
    writer.value(provenance.os_version);
    writer.key("architecture");
    writer.value(provenance.architecture);
    writer.key("cpu_model");
    writer.value(provenance.cpu_model);
    writer.key("logical_core_count");
    writer.value(provenance.logical_core_count);
    writer.end_object();
    writer.key("m4_dependency_identity");
    writer.begin_object();
    writer.key("status");
    writer.value(provenance.m4_dependency_status);
    if (provenance.m4_dependency_status == "ON") {
        writer.key("contracts_source_revision");
        writer.value(provenance.contracts_source_revision);
        writer.key("contracts_conan_reference");
        writer.value(provenance.contracts_conan_reference);
        writer.key("contracts_recipe_revision");
        writer.value(provenance.contracts_recipe_revision);
        writer.key("contracts_package_id");
        writer.value(provenance.contracts_package_id);
        writer.key("protobuf_runtime_version");
        writer.value(provenance.protobuf_runtime_version);
        writer.key("protobuf_runtime_rrev");
        writer.value(provenance.protobuf_runtime_rrev);
    } else {
        writer.key("reason");
        writer.value("not_applicable_core_only_payload");
    }
    writer.end_object();
    writer.end_object();
    return writer.str();
}

[[nodiscard]] bool live_metrics_present(const AllocationRecordInput& input) {
    return input.measurement.live_metrics_eligible;
}

// Canonical result object emission. Keys are emitted in lexicographic order
// so the C++ canonical text equals the Python validator's
// json.dumps(obj, sort_keys=True, separators=(",", ":")) reconstruction.
void write_canonical_result_fields(json::Writer& writer, const AllocationRecordInput& input) {
    const auto& m = input.measurement;
    const bool live_present = live_metrics_present(input);
    writer.key("allocation_count");
    writer.value(m.allocation_count);
    writer.key("allocation_count_valid");
    writer.value(m.allocation_count_valid);
    writer.key("allocation_failure_observed");
    writer.value(input.allocation_failure_observed);
    writer.key("deallocated_bytes");
    if (m.deallocated_bytes_valid) {
        writer.value(m.deallocated_bytes);
    } else {
        writer.value_null();
    }
    writer.key("deallocated_bytes_valid");
    writer.value(m.deallocated_bytes_valid);
    writer.key("deallocation_count");
    writer.value(m.deallocation_count);
    writer.key("deallocation_count_valid");
    writer.value(m.deallocation_count_valid);
    writer.key("determinism_confirmed");
    writer.value(input.determinism_confirmed);
    writer.key("live_bytes_after");
    if (live_present) {
        writer.value(m.live_bytes_after);
    } else {
        writer.value_null();
    }
    writer.key("live_bytes_before");
    if (live_present) {
        writer.value(m.live_bytes_before);
    } else {
        writer.value_null();
    }
    writer.key("live_metric_eligibility");
    if (live_present) {
        writer.value("eligible");
    } else {
        writer.begin_object();
        writer.key("reason_code");
        writer.value(reason_name(m.ineligibility_reason));
        writer.key("status");
        writer.value("ineligible");
        writer.end_object();
    }
    writer.key("operation_aborted");
    writer.value(input.operation_aborted);
    writer.key("peak_above_entry");
    if (live_present) {
        writer.value(m.peak_above_entry);
    } else {
        writer.value_null();
    }
    writer.key("peak_live_bytes_absolute");
    if (live_present) {
        writer.value(m.peak_live_bytes_absolute);
    } else {
        writer.value_null();
    }
    writer.key("persistent_live_delta");
    if (live_present) {
        writer.begin_object();
        writer.key("magnitude");
        writer.value(m.persistent_live_delta.magnitude);
        writer.key("sign");
        writer.value(sign_name(m.persistent_live_delta.sign));
        writer.end_object();
    } else {
        writer.value_null();
    }
    writer.key("post_destroy_lifecycle_status");
    writer.value(input.post_destroy_lifecycle_status);
    writer.key("post_destroy_live_bytes");
    if (input.has_post_destroy_snapshot) {
        writer.value(input.post_destroy_live_bytes);
    } else {
        writer.value_null();
    }
    if (input.replay_aggregate.has_value()) {
        writer.key("replay_aggregate");
        writer.begin_object();
        writer.key("aggregate_allocated_bytes");
        writer.value(input.replay_aggregate->aggregate_allocated_bytes);
        writer.key("aggregate_allocation_count");
        writer.value(input.replay_aggregate->aggregate_allocation_count);
        writer.key("aggregate_deallocated_bytes");
        writer.value(input.replay_aggregate->aggregate_deallocated_bytes);
        writer.key("aggregate_deallocation_count");
        writer.value(input.replay_aggregate->aggregate_deallocation_count);
        writer.key("derived_per_event_allocations");
        writer.begin_object();
        writer.key("denominator");
        writer.value(input.replay_aggregate->derived_per_event_allocations.denominator);
        writer.key("numerator");
        writer.value(input.replay_aggregate->derived_per_event_allocations.numerator);
        writer.end_object();
        writer.key("derived_per_event_bytes");
        writer.begin_object();
        writer.key("denominator");
        writer.value(input.replay_aggregate->derived_per_event_bytes.denominator);
        writer.key("numerator");
        writer.value(input.replay_aggregate->derived_per_event_bytes.numerator);
        writer.end_object();
        writer.key("event_count");
        writer.value(input.replay_aggregate->event_count);
        writer.end_object();
    }
    writer.key("repetitions");
    writer.value(input.repetitions);
    writer.key("total_allocated_bytes");
    writer.value(m.total_allocated_bytes);
    writer.key("total_allocated_bytes_valid");
    writer.value(m.total_allocated_bytes_valid);
    writer.key("transient_excess_over_persistent");
    if (live_present) {
        writer.value(m.transient_excess_over_persistent);
    } else {
        writer.value_null();
    }
}

// Metric/live-model tail of an allocation record (after provenance).
void write_measurement_fields(json::Writer& writer, const AllocationRecordInput& input) {
    const auto& m = input.measurement;
    writer.key("allocation_count");
    writer.value(m.allocation_count);
    writer.key("total_allocated_bytes");
    writer.value(m.total_allocated_bytes);
    writer.key("deallocation_count");
    writer.value(m.deallocation_count);
    if (m.deallocated_bytes_valid) {
        writer.key("deallocated_bytes");
        writer.value(m.deallocated_bytes);
    }
    writer.key("live_bytes_before");
    writer.value(m.live_bytes_before);
    writer.key("peak_live_bytes_absolute");
    writer.value(m.peak_live_bytes_absolute);
    writer.key("live_bytes_after");
    writer.value(m.live_bytes_after);
    writer.key("persistent_live_delta");
    writer.begin_object();
    writer.key("sign");
    writer.value(sign_name(m.persistent_live_delta.sign));
    writer.key("magnitude");
    writer.value(m.persistent_live_delta.magnitude);
    writer.end_object();
    writer.key("peak_above_entry");
    writer.value(m.peak_above_entry);
    writer.key("transient_excess_over_persistent");
    writer.value(m.transient_excess_over_persistent);
    writer.key("live_metric_eligibility");
    if (m.live_metrics_eligible) {
        writer.value("eligible");
    } else {
        writer.begin_object();
        writer.key("status");
        writer.value("ineligible");
        writer.key("reason_code");
        writer.value(reason_name(m.ineligibility_reason));
        writer.end_object();
    }
    writer.key("metric_validity");
    writer.begin_object();
    writer.key("allocation_count_valid");
    writer.value(m.allocation_count_valid);
    writer.key("total_allocated_bytes_valid");
    writer.value(m.total_allocated_bytes_valid);
    writer.key("deallocation_count_valid");
    writer.value(m.deallocation_count_valid);
    writer.key("deallocated_bytes_valid");
    writer.value(m.deallocated_bytes_valid);
    writer.end_object();
    writer.key("operation_aborted");
    writer.value(input.operation_aborted);
    writer.key("allocation_failure_observed");
    writer.value(input.allocation_failure_observed);
    if (input.has_post_destroy_snapshot) {
        writer.key("post_destroy_live_bytes");
        writer.value(input.post_destroy_live_bytes);
    }
    writer.key("post_destroy_lifecycle_status");
    writer.value(input.post_destroy_lifecycle_status);
    writer.key("baseline_definition");
    writer.begin_object();
    writer.key("snapshot_a");
    writer.value(input.baseline_snapshot_a);
    writer.key("snapshot_b");
    writer.value(input.baseline_snapshot_b);
    writer.key("delta_formula");
    writer.value(input.baseline_delta_formula);
    writer.end_object();
    writer.key("calibration_record");
    writer.begin_object();
    writer.key("reference");
    writer.value(input.calibration_reference);
    writer.key("subtracted");
    writer.value(false);
    writer.end_object();
    writer.key("repetitions");
    writer.value(input.repetitions);
    writer.key("determinism_confirmed");
    writer.value(input.determinism_confirmed);
    if (input.replay_aggregate.has_value()) {
        writer.key("replay_aggregate");
        writer.begin_object();
        writer.key("aggregate_allocation_count");
        writer.value(input.replay_aggregate->aggregate_allocation_count);
        writer.key("aggregate_allocated_bytes");
        writer.value(input.replay_aggregate->aggregate_allocated_bytes);
        writer.key("aggregate_deallocation_count");
        writer.value(input.replay_aggregate->aggregate_deallocation_count);
        writer.key("aggregate_deallocated_bytes");
        writer.value(input.replay_aggregate->aggregate_deallocated_bytes);
        writer.key("event_count");
        writer.value(input.replay_aggregate->event_count);
        writer.key("derived_per_event_allocations");
        writer.begin_object();
        writer.key("numerator");
        writer.value(input.replay_aggregate->derived_per_event_allocations.numerator);
        writer.key("denominator");
        writer.value(input.replay_aggregate->derived_per_event_allocations.denominator);
        writer.end_object();
        writer.key("derived_per_event_bytes");
        writer.begin_object();
        writer.key("numerator");
        writer.value(input.replay_aggregate->derived_per_event_bytes.numerator);
        writer.key("denominator");
        writer.value(input.replay_aggregate->derived_per_event_bytes.denominator);
        writer.end_object();
        writer.end_object();
    }
    writer.key("result_payload_sha256");
    writer.value(json_text_sha256(build_canonical_result_text(input)));
}

} // namespace

std::string json_text_sha256(std::string_view text) { return sha256_of(text); }

std::string workload_spec_field(std::string_view canonical_text, std::string_view key) {
    const auto prefix = std::string{key} + "=";
    std::istringstream stream{std::string{canonical_text}};
    std::string line;
    while (std::getline(stream, line)) {
        if (line.starts_with(prefix)) {
            return line.substr(prefix.size());
        }
    }
    return {};
}

Phase7Provenance Phase7ProvenanceCollect::collect() {
    const auto source_state = compute_source_provenance_state();
    Phase7Provenance provenance;
    provenance.source_git_sha = BMD_P6_GIT_SHA;
    provenance.source_status = source_state.known ? "known" : "unavailable";
    provenance.source_dirty_at_configure = source_state.dirty;
    provenance.binary_path = current_executable_path();
    provenance.binary_sha256 = sha256_file_hex(provenance.binary_path);
    provenance.compiler_id = BMD_P6_COMPILER_ID;
    provenance.compiler_version = BMD_P6_COMPILER_VERSION;
    provenance.cxx_standard = BMD_P6_CXX_STANDARD;
    provenance.build_type = BMD_P6_BUILD_TYPE;
    provenance.sanitizer_state = BMD_P6_SANITIZER_STATE;
    provenance.lto_state = BMD_P6_LTO_STATE;
    provenance.standard_library_name = standard_library_name();
    provenance.standard_library_version = standard_library_version();
    provenance.standard_library_detection_status = standard_library_detection_status();
    provenance.conan_lock_sha256 = BMD_P6_CONAN_LOCK_SHA256;
    const auto environment = collect_environment_identity();
    provenance.os_name = environment.os_name;
    provenance.os_version = environment.os_version;
    provenance.architecture = environment.architecture;
    provenance.cpu_model = environment.cpu_model;
    provenance.logical_core_count = environment.logical_core_count;
    provenance.m4_dependency_status = BMD_P6_ADAPTER_ENABLED;
    provenance.contracts_source_revision = BMD_P6_CONTRACTS_SOURCE_REVISION;
    provenance.contracts_conan_reference = BMD_P6_CONTRACTS_CONAN_REFERENCE;
    provenance.contracts_recipe_revision = BMD_P6_CONTRACTS_RECIPE_REVISION;
    provenance.contracts_package_id = BMD_P6_CONTRACTS_PACKAGE_ID;
    provenance.protobuf_runtime_version = BMD_P6_PROTOBUF_RUNTIME_VERSION;
    provenance.protobuf_runtime_rrev = BMD_P6_PROTOBUF_RUNTIME_RREV;
    return provenance;
}

std::string build_canonical_result_text(const AllocationRecordInput& input) {
    json::Writer writer;
    writer.begin_object();
    write_canonical_result_fields(writer, input);
    writer.end_object();
    return writer.str();
}

std::string build_allocation_record_json(const AllocationRecordInput& input) {
    const auto provenance = Phase7ProvenanceCollect::collect();
    const auto provenance_text = provenance_json(provenance);

    json::Writer writer;
    writer.begin_object();
    writer.key("schema");
    writer.value(kAllocationRecordSchema);
    writer.key("measurement_contract_version");
    writer.value(kPhase7MeasurementContract);
    writer.key("evidence_class");
    writer.value(input.evidence_class);
    writer.key("measurement_scope");
    writer.value(input.measurement_scope);
    writer.key("operation_denominator");
    writer.value(input.operation_denominator);
    writer.key("allocation_boundary");
    writer.value(allocation::kAllocationBoundary);
    writer.key("workload_id");
    writer.value(input.workload_id);
    writer.key("workload_spec_sha256");
    writer.value(input.workload_spec_sha256);
    writer.key("generator_schema");
    writer.value(input.generator_schema);
    writer.key("generated_workload_sha256");
    writer.value(input.generated_workload_sha256);
    writer.key("fixture_identity");
    writer.begin_object();
    writer.key("seed");
    writer.value(input.seed);
    writer.key("canonical_log_sha256");
    writer.value(input.canonical_log_sha256);
    writer.key("event_count");
    if (input.event_count.has_value()) {
        writer.value(*input.event_count);
    } else {
        writer.value_null();
    }
    writer.end_object();
    writer.key("provenance");
    writer.value_raw(provenance_text);
    write_measurement_fields(writer, input);
    writer.end_object();
    return writer.str();
}

std::string build_canonical_result_text(const FootprintRecordInput& input) {
    json::Writer writer;
    writer.begin_object();
    writer.key("bids_only_live_bytes");
    writer.value(input.bids_only_live_bytes);
    writer.key("both_sides_live_bytes");
    writer.value(input.both_sides_live_bytes);
    writer.key("depth_per_side");
    writer.value(input.depth_per_side);
    writer.key("determinism_confirmed");
    writer.value(input.determinism_confirmed);
    writer.key("eligible");
    writer.value(input.eligible);
    writer.key("empty_book_live_bytes");
    writer.value(input.empty_book_live_bytes);
    writer.key("post_destroy_lifecycle_status");
    writer.value(input.post_destroy_lifecycle_status);
    writer.key("post_destroy_live_bytes");
    writer.value(input.post_destroy_live_bytes);
    writer.key("pre_experiment_baseline_live_bytes");
    writer.value(input.pre_experiment_baseline_live_bytes);
    writer.key("repetitions");
    writer.value(input.repetitions);
    writer.end_object();
    return writer.str();
}

std::string build_footprint_record_json(const FootprintRecordInput& input) {
    const auto provenance = Phase7ProvenanceCollect::collect();
    const auto provenance_text = provenance_json(provenance);
    const auto canonical_result = build_canonical_result_text(input);
    // Every delta is an exact snapshot comparison (OD-M5-P7-007): the
    // footprint snapshots are monotone non-decreasing by construction, but the
    // producer still computes the exact difference explicitly.
    const auto delta = [](std::uint64_t later, std::uint64_t earlier) -> std::uint64_t {
        return later >= earlier ? later - earlier : earlier - later;
    };
    const auto total_bytes = delta(input.both_sides_live_bytes, input.empty_book_live_bytes);
    const auto bids_bytes = delta(input.bids_only_live_bytes, input.empty_book_live_bytes);
    const auto asks_bytes = delta(input.both_sides_live_bytes, input.bids_only_live_bytes);

    json::Writer writer;
    writer.begin_object();
    writer.key("schema");
    writer.value(kFootprintRecordSchema);
    writer.key("measurement_contract_version");
    writer.value(kPhase7MeasurementContract);
    writer.key("evidence_class");
    writer.value(input.evidence_class);
    writer.key("measurement_scope");
    writer.value(input.measurement_scope);
    writer.key("allocation_boundary");
    writer.value(allocation::kAllocationBoundary);
    writer.key("depth_per_side");
    writer.value(input.depth_per_side);
    writer.key("generator_identity");
    writer.begin_object();
    writer.key("schema");
    writer.value(input.generator_schema);
    writer.key("seed");
    writer.value(input.generator_seed);
    writer.end_object();
    writer.key("provenance");
    writer.value_raw(provenance_text);
    writer.key("snapshots");
    writer.begin_object();
    writer.key("pre_experiment_baseline_live_bytes");
    writer.value(input.pre_experiment_baseline_live_bytes);
    writer.key("empty_book_live_bytes");
    writer.value(input.empty_book_live_bytes);
    writer.key("bids_only_live_bytes");
    writer.value(input.bids_only_live_bytes);
    writer.key("both_sides_live_bytes");
    writer.value(input.both_sides_live_bytes);
    writer.key("post_destroy_live_bytes");
    writer.value(input.post_destroy_live_bytes);
    writer.end_object();
    writer.key("measured_requested_heap_bytes_total");
    writer.value(total_bytes);
    writer.key("measured_requested_heap_bytes_per_side_bids");
    writer.value(bids_bytes);
    writer.key("measured_requested_heap_bytes_per_side_asks");
    writer.value(asks_bytes);
    writer.key("measured_bytes_per_level_per_side_bids");
    writer.begin_object();
    writer.key("numerator");
    writer.value(bids_bytes);
    writer.key("denominator");
    writer.value(input.depth_per_side);
    writer.end_object();
    writer.key("measured_bytes_per_level_per_side_asks");
    writer.begin_object();
    writer.key("numerator");
    writer.value(asks_bytes);
    writer.key("denominator");
    writer.value(input.depth_per_side);
    writer.end_object();
    writer.key("post_destroy_lifecycle_status");
    writer.value(input.post_destroy_lifecycle_status);
    writer.key("node_structural_model");
    writer.begin_object();
    writer.key("non_additive");
    writer.value(true);
    writer.key("description");
    writer.value(input.node_structural_model_description);
    writer.key("toolchain");
    writer.value(input.node_structural_model_toolchain);
    writer.end_object();
    writer.key("allocator_backing_model");
    writer.begin_object();
    writer.key("evidence_class");
    writer.value("estimated");
    writer.key("description");
    writer.value(input.allocator_backing_model_description);
    writer.key("scope");
    writer.value(input.allocator_backing_model_scope);
    writer.end_object();
    writer.key("rss");
    writer.value("not_measured");
    writer.key("eligibility");
    writer.begin_object();
    writer.key("status");
    writer.value(input.eligible ? "eligible" : "ineligible");
    if (!input.eligible) {
        writer.key("reason_code");
        writer.value(input.ineligibility_reason);
    }
    writer.end_object();
    writer.key("calibration_record");
    writer.begin_object();
    writer.key("reference");
    writer.value(input.calibration_reference);
    writer.key("subtracted");
    writer.value(false);
    writer.end_object();
    writer.key("repetitions");
    writer.value(input.repetitions);
    writer.key("determinism_confirmed");
    writer.value(input.determinism_confirmed);
    writer.key("result_payload_sha256");
    writer.value(json_text_sha256(canonical_result));
    writer.end_object();
    return writer.str();
}

std::string build_calibration_record_json(const CalibrationRecordInput& input) {
    const auto& m = input.measurement;
    json::Writer writer;
    writer.begin_object();
    writer.key("calibration_id");
    writer.value(input.calibration_id);
    writer.key("evidence_class");
    writer.value(input.evidence_class);
    writer.key("allocation_boundary");
    writer.value(allocation::kAllocationBoundary);
    writer.key("description");
    writer.value(input.description);
    writer.key("allocation_count");
    writer.value(m.allocation_count);
    writer.key("total_allocated_bytes");
    writer.value(m.total_allocated_bytes);
    writer.key("deallocation_count");
    writer.value(m.deallocation_count);
    writer.key("live_bytes_before");
    writer.value(m.live_bytes_before);
    writer.key("peak_live_bytes_absolute");
    writer.value(m.peak_live_bytes_absolute);
    writer.key("live_bytes_after");
    writer.value(m.live_bytes_after);
    writer.key("live_metric_eligibility");
    if (m.live_metrics_eligible) {
        writer.value("eligible");
    } else {
        writer.begin_object();
        writer.key("status");
        writer.value("ineligible");
        writer.key("reason_code");
        writer.value(reason_name(m.ineligibility_reason));
        writer.end_object();
    }
    writer.key("subtracted_from_measurements");
    writer.value(false);
    writer.end_object();
    return writer.str();
}

std::string build_allocation_payload_json(const std::vector<std::string>& records,
                                          const std::vector<std::string>& calibration_records,
                                          const std::string& measurement_contract) {
    json::Writer writer;
    writer.begin_object();
    writer.key("schema");
    writer.value(kAllocationPayloadSchema);
    writer.key("measurement_contract_version");
    writer.value(measurement_contract);
    writer.key("record_count");
    writer.value(static_cast<std::uint64_t>(records.size()));
    writer.key("calibration_record_count");
    writer.value(static_cast<std::uint64_t>(calibration_records.size()));
    writer.key("calibration_records");
    writer.begin_array();
    for (const auto& calibration : calibration_records) {
        writer.value_raw(calibration);
    }
    writer.end_array();
    writer.key("records");
    writer.begin_array();
    for (const auto& record : records) {
        writer.value_raw(record);
    }
    writer.end_array();
    writer.end_object();
    return writer.str();
}

std::string build_allocation_wrapper_json(const AllocationWrapperInput& input) {
    const auto binary_sha = sha256_file_hex(input.binary_path);
    const auto payload_sha = sha256_file_hex(input.payload_path);
    if (binary_sha.empty() || payload_sha.empty()) {
        return {};
    }

    auto effective_class = input.evidence_class;
    std::string downgrade_reason;
    const auto source_state = compute_source_provenance_state();
    if (!source_state.known && input.evidence_class == "formal") {
        return {};
    }
    if (source_state.dirty && effective_class == "formal") {
        effective_class = "exploratory";
        downgrade_reason = "dirty_at_configure";
    }

    json::Writer writer;
    writer.begin_object();
    writer.key("schema");
    writer.value(kAllocationWrapperSchema);
    writer.key("measurement_contract_version");
    writer.value(kPhase7MeasurementContract);
    writer.key("evidence_class");
    writer.value(effective_class);
    writer.key("requested_evidence_class");
    writer.value(input.evidence_class);
    writer.key("evidence_class_downgrade_reason");
    if (downgrade_reason.empty()) {
        writer.value_null();
    } else {
        writer.value(downgrade_reason);
    }

    write_source_provenance_section(writer, source_state);
    write_binary_provenance_section(writer, input.binary_path, binary_sha);

    writer.key("build_identity");
    writer.begin_object();
    write_build_identity_section(writer);
    writer.end_object();

    write_environment_section(writer);
    write_m4_dependency_section(writer);

    writer.key("allocation_instrumentation_identity");
    writer.begin_object();
    writer.key("allocation_boundary");
    writer.value(allocation::kAllocationBoundary);
    writer.key("provenance_capacity");
    writer.value(static_cast<std::uint64_t>(allocation::kProvenanceCapacity));
    writer.key("tracking_model");
    writer.value("two_lifetime_tracking_and_measurement_v1");
    writer.key("backing_allocator");
    writer.value("std_malloc_std_free");
    writer.key("live_metric_model");
    writer.value("A_P_B_with_post_destroy_lifecycle_v1");
    writer.end_object();

    writer.key("measurement_identity");
    writer.begin_object();
    writer.key("warmup");
    writer.begin_object();
    writer.key("kind");
    writer.value(input.warmup_kind);
    writer.key("count");
    writer.value(input.warmup_count);
    writer.end_object();
    writer.key("repetitions");
    writer.value(input.repetitions);
    writer.key("bracket_discipline");
    writer.value("single_logical_operation_per_bracket");
    writer.key("preparation");
    writer.value("outside_measurement_bracket");
    writer.key("calibration");
    writer.value("reported_never_subtracted");
    writer.end_object();

    write_workload_identities_section(writer, input.workloads);

    writer.key("result_payload");
    writer.begin_object();
    writer.key("path");
    writer.value(input.payload_path);
    writer.key("sha256");
    writer.value(payload_sha);
    writer.key("schema");
    writer.value(kAllocationPayloadSchema);
    writer.end_object();

    writer.end_object();
    return writer.str();
}

} // namespace bmd_projection::m5::benchmark
