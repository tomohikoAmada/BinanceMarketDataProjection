#include "phase7_record.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace {

namespace bm = bmd_projection::m5::benchmark;
namespace alloc = bmd_projection::m5::allocation;

using alloc::LiveIneligibilityReason;
using alloc::MeasurementResult;
using alloc::PersistentLiveDelta;
using alloc::PersistentLiveDeltaSign;

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
[[nodiscard]] MeasurementResult
eligible_measurement(std::uint64_t before, std::uint64_t peak, std::uint64_t after,
                     std::uint64_t allocs, std::uint64_t bytes, std::uint64_t frees,
                     std::uint64_t freed_bytes, PersistentLiveDelta delta, std::uint64_t peak_above,
                     std::uint64_t transient) {
    MeasurementResult result{};
    result.live_bytes_before = before;
    result.peak_live_bytes_absolute = peak;
    result.live_bytes_after = after;
    result.allocation_count = allocs;
    result.total_allocated_bytes = bytes;
    result.deallocation_count = frees;
    result.deallocated_bytes = freed_bytes;
    result.persistent_live_delta = delta;
    result.peak_above_entry = peak_above;
    result.transient_excess_over_persistent = transient;
    result.allocation_count_valid = true;
    result.total_allocated_bytes_valid = true;
    result.deallocation_count_valid = true;
    result.deallocated_bytes_valid = true;
    result.live_metrics_eligible = true;
    result.ineligibility_reason = LiveIneligibilityReason::none;
    result.operation_aborted = false;
    result.allocation_failure_observed = false;
    return result;
}
// NOLINTEND(bugprone-easily-swappable-parameters)

[[nodiscard]] bm::AllocationRecordInput base_input() {
    bm::AllocationRecordInput input;
    input.measurement_scope = "M2/apply_level/insert/8";
    input.operation_denominator = "apply_level";
    input.workload_id = "M2/apply_level/insert/8";
    input.workload_spec_sha256 = std::string(64, 'a');
    input.generator_schema = "M5_PHASE6_M2_CELLS_V1";
    input.generated_workload_sha256 = std::string(64, 'b');
    input.evidence_class = "exploratory";
    input.baseline_snapshot_a = "bracket open";
    input.baseline_snapshot_b = "bracket close";
    input.baseline_delta_formula = "exact A/B comparison -> persistent_live_delta";
    input.calibration_reference = "calibration/empty-bracket-v1";
    input.repetitions = 3;
    input.determinism_confirmed = true;
    return input;
}

} // namespace

// ---------------------------------------------------------------------------
// Canonical result text (golden cross-language shape).
// ---------------------------------------------------------------------------
TEST(Phase7RecordCanonicalResult, EligibleProfileGoldenTextAndSha) {
    auto input = base_input();
    input.measurement =
        eligible_measurement(1'000, 1'200, 1'100, 3, 64, 1, 32,
                             PersistentLiveDelta{PersistentLiveDeltaSign::positive, 100}, 200, 100);
    const auto text = bm::build_canonical_result_text(input);
    EXPECT_EQ(text, "{\"allocation_count\":3,\"allocation_count_valid\":true,"
                    "\"allocation_failure_observed\":false,\"deallocated_bytes\":32,"
                    "\"deallocated_bytes_valid\":true,\"deallocation_count\":1,"
                    "\"deallocation_count_valid\":true,\"determinism_confirmed\":true,"
                    "\"live_bytes_after\":1100,\"live_bytes_before\":1000,"
                    "\"live_metric_eligibility\":\"eligible\",\"operation_aborted\":false,"
                    "\"peak_above_entry\":200,\"peak_live_bytes_absolute\":1200,"
                    "\"persistent_live_delta\":{\"magnitude\":100,\"sign\":\"positive\"},"
                    "\"post_destroy_lifecycle_status\":\"not_applicable\","
                    "\"post_destroy_live_bytes\":null,\"repetitions\":3,"
                    "\"total_allocated_bytes\":64,\"total_allocated_bytes_valid\":true,"
                    "\"transient_excess_over_persistent\":100}");
    EXPECT_EQ(bm::json_text_sha256(text),
              "fcbbc656c73734374de3f338661be1222b4ef5fd0f1959527bcc67ba304cc49d");
}

TEST(Phase7RecordCanonicalResult, IneligibleProfileReportsNullsNotEstimates) {
    auto input = base_input();
    input.repetitions = 1;
    input.measurement = MeasurementResult{};
    input.measurement.allocation_count = 5;
    input.measurement.total_allocated_bytes = 512;
    input.measurement.deallocation_count = 5;
    input.measurement.allocation_count_valid = true;
    input.measurement.total_allocated_bytes_valid = true;
    input.measurement.deallocation_count_valid = true;
    input.measurement.deallocated_bytes_valid = false;
    input.measurement.live_metrics_eligible = false;
    input.measurement.ineligibility_reason = LiveIneligibilityReason::unknown_pointer_delete;
    const auto text = bm::build_canonical_result_text(input);
    EXPECT_EQ(text, "{\"allocation_count\":5,\"allocation_count_valid\":true,"
                    "\"allocation_failure_observed\":false,\"deallocated_bytes\":null,"
                    "\"deallocated_bytes_valid\":false,\"deallocation_count\":5,"
                    "\"deallocation_count_valid\":true,\"determinism_confirmed\":true,"
                    "\"live_bytes_after\":null,\"live_bytes_before\":null,"
                    "\"live_metric_eligibility\":{\"reason_code\":\"unknown_pointer_delete\","
                    "\"status\":\"ineligible\"},\"operation_aborted\":false,"
                    "\"peak_above_entry\":null,\"peak_live_bytes_absolute\":null,"
                    "\"persistent_live_delta\":null,"
                    "\"post_destroy_lifecycle_status\":\"not_applicable\","
                    "\"post_destroy_live_bytes\":null,\"repetitions\":1,"
                    "\"total_allocated_bytes\":512,\"total_allocated_bytes_valid\":true,"
                    "\"transient_excess_over_persistent\":null}");
    EXPECT_EQ(bm::json_text_sha256(text),
              "15479dc35c2012ce3e7daa25cb3219ce415cff144361a86405a991d68bc17ae5");
}

// ---------------------------------------------------------------------------
// Replay exact rational (OD-M5-P7-013 / adversarial case 27): 3 over 2 events
// stays {3, 2}; no integer division anywhere.
// ---------------------------------------------------------------------------
TEST(Phase7RecordReplayRational, ThreeOverTwoEventsStaysExact) {
    auto input = base_input();
    input.measurement_scope = "CoreNormalizedReplay/Spot";
    input.operation_denominator = "replay_pass";
    input.repetitions = 1;
    input.has_post_destroy_snapshot = true;
    input.post_destroy_live_bytes = 3'000;
    input.post_destroy_lifecycle_status = "destroyed";
    input.measurement =
        eligible_measurement(4'000, 4'000, 4'000, 3, 96, 3, 96,
                             PersistentLiveDelta{PersistentLiveDeltaSign::zero, 0}, 0, 0);
    input.replay_aggregate = bm::ReplayAggregate{};
    input.replay_aggregate->aggregate_allocation_count = 3;
    input.replay_aggregate->aggregate_allocated_bytes = 96;
    input.replay_aggregate->aggregate_deallocation_count = 3;
    input.replay_aggregate->aggregate_deallocated_bytes = 96;
    input.replay_aggregate->event_count = 2;
    input.replay_aggregate->derived_per_event_allocations = {3, 2};
    input.replay_aggregate->derived_per_event_bytes = {96, 2};
    const auto text = bm::build_canonical_result_text(input);
    EXPECT_EQ(text, "{\"allocation_count\":3,\"allocation_count_valid\":true,"
                    "\"allocation_failure_observed\":false,\"deallocated_bytes\":96,"
                    "\"deallocated_bytes_valid\":true,\"deallocation_count\":3,"
                    "\"deallocation_count_valid\":true,\"determinism_confirmed\":true,"
                    "\"live_bytes_after\":4000,\"live_bytes_before\":4000,"
                    "\"live_metric_eligibility\":\"eligible\",\"operation_aborted\":false,"
                    "\"peak_above_entry\":0,\"peak_live_bytes_absolute\":4000,"
                    "\"persistent_live_delta\":{\"magnitude\":0,\"sign\":\"zero\"},"
                    "\"post_destroy_lifecycle_status\":\"destroyed\","
                    "\"post_destroy_live_bytes\":3000,\"repetitions\":1,"
                    "\"replay_aggregate\":{\"aggregate_allocated_bytes\":96,"
                    "\"aggregate_allocation_count\":3,\"aggregate_deallocated_bytes\":96,"
                    "\"aggregate_deallocation_count\":3,"
                    "\"derived_per_event_allocations\":{\"denominator\":2,\"numerator\":3},"
                    "\"derived_per_event_bytes\":{\"denominator\":2,\"numerator\":96},"
                    "\"event_count\":2},\"total_allocated_bytes\":96,"
                    "\"total_allocated_bytes_valid\":true,"
                    "\"transient_excess_over_persistent\":0}");
    EXPECT_EQ(bm::json_text_sha256(text),
              "2eb7565f4614363b1667bead5122ed6529290340336d3f7f036b47efeb84ddc2");
}

// ---------------------------------------------------------------------------
// Full record assembly.
// ---------------------------------------------------------------------------
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(Phase7RecordRecordJson, CarriesBoundaryIdentityAndBindingSha) {
    auto input = base_input();
    input.measurement =
        eligible_measurement(1'000, 1'200, 1'100, 3, 64, 1, 32,
                             PersistentLiveDelta{PersistentLiveDeltaSign::positive, 100}, 200, 100);
    const auto record = bm::build_allocation_record_json(input);
    const auto find = [&record](std::string_view fragment) {
        return record.find(fragment) != std::string::npos;
    };
    EXPECT_TRUE(find("\"schema\":\"M5_PHASE7_ALLOCATION_RECORD_V1\""));
    EXPECT_TRUE(find("\"measurement_contract_version\":\"M5_PHASE7_MEASUREMENT_CONTRACT_V1\""));
    EXPECT_TRUE(find("\"allocation_boundary\":\"cxx_replaceable_global_new\""));
    EXPECT_TRUE(find("\"workload_id\":\"M2/apply_level/insert/8\""));
    EXPECT_TRUE(find("\"calibration_record\":{\"reference\":\"calibration/empty-bracket-v1\","
                     "\"subtracted\":false}"));
    EXPECT_TRUE(find("\"result_payload_sha256\":"
                     "\"fcbbc656c73734374de3f338661be1222b4ef5fd0f1959527bcc67ba304cc49d\""));
    EXPECT_TRUE(find("\"persistent_live_delta\":{\"sign\":\"positive\",\"magnitude\":100}"));
    EXPECT_TRUE(find("\"provenance\":{"));
    EXPECT_TRUE(find("\"source\":{\"git_sha\""));
    EXPECT_TRUE(find("\"m4_dependency_identity\":{\"status\""));
    EXPECT_TRUE(find("\"determinism_confirmed\":true"));
}

TEST(Phase7RecordRecordJson, NegativeDeltaIsRepresentedExactly) {
    auto input = base_input();
    input.measurement =
        eligible_measurement(2'000, 2'000, 1'500, 0, 0, 1, 500,
                             PersistentLiveDelta{PersistentLiveDeltaSign::negative, 500}, 0, 0);
    const auto record = bm::build_allocation_record_json(input);
    EXPECT_NE(record.find("\"persistent_live_delta\":{\"sign\":\"negative\",\"magnitude\":500}"),
              std::string::npos);
}

// ---------------------------------------------------------------------------
// Footprint record (OD-M5-P7-006).
// ---------------------------------------------------------------------------
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(Phase7RecordFootprint, SeparatesMeasuredModelAndRss) {
    bm::FootprintRecordInput input;
    input.depth_per_side = 1'000;
    input.measurement_scope = "M5_Footprint/Depth/1000";
    input.generator_schema = "M5_PHASE6_M2_CELLS_V1";
    input.pre_experiment_baseline_live_bytes = 100;
    input.empty_book_live_bytes = 200;
    input.bids_only_live_bytes = 20'200;
    input.both_sides_live_bytes = 40'200;
    input.post_destroy_live_bytes = 100;
    input.post_destroy_lifecycle_status = "consistent";
    input.eligible = true;
    input.repetitions = 3;
    input.determinism_confirmed = true;
    input.calibration_reference = "calibration/empty-bracket-v1";
    input.node_structural_model_description =
        "std::map node allocation request already includes node structure";
    input.node_structural_model_toolchain = "compiler:stdlib";
    input.allocator_backing_model_description = "environment-specific estimate";
    input.allocator_backing_model_scope = "environment/toolchain/allocator/size-class";
    const auto record = bm::build_footprint_record_json(input);
    const auto find = [&record](std::string_view fragment) {
        return record.find(fragment) != std::string::npos;
    };
    EXPECT_TRUE(find("\"schema\":\"M5_PHASE7_FOOTPRINT_RECORD_V1\""));
    EXPECT_TRUE(find("\"measured_requested_heap_bytes_total\":40000"));
    EXPECT_TRUE(find("\"measured_requested_heap_bytes_per_side_bids\":20000"));
    EXPECT_TRUE(find("\"measured_requested_heap_bytes_per_side_asks\":20000"));
    EXPECT_TRUE(find("\"measured_bytes_per_level_per_side_bids\":"
                     "{\"numerator\":20000,\"denominator\":1000}"));
    EXPECT_TRUE(find("\"node_structural_model\":{\"non_additive\":true,"));
    EXPECT_TRUE(find("\"allocator_backing_model\":{\"evidence_class\":\"estimated\","));
    EXPECT_TRUE(find("\"rss\":\"not_measured\""));
    EXPECT_TRUE(find("\"post_destroy_lifecycle_status\":\"consistent\""));
    EXPECT_TRUE(find("\"eligibility\":{\"status\":\"eligible\"}"));
}

// ---------------------------------------------------------------------------
// Payload assembly and wrapper.
// ---------------------------------------------------------------------------
TEST(Phase7RecordPayload, AssemblesRecordsAndCalibration) {
    auto input = base_input();
    input.measurement =
        eligible_measurement(1'000, 1'000, 1'000, 0, 0, 0, 0,
                             PersistentLiveDelta{PersistentLiveDeltaSign::zero, 0}, 0, 0);
    const auto record = bm::build_allocation_record_json(input);
    bm::CalibrationRecordInput calibration;
    calibration.calibration_id = "calibration/empty-bracket-v1";
    calibration.description = "empty measurement bracket";
    calibration.measurement =
        eligible_measurement(5'000, 5'000, 5'000, 0, 0, 0, 0,
                             PersistentLiveDelta{PersistentLiveDeltaSign::zero, 0}, 0, 0);
    const auto calibration_json = bm::build_calibration_record_json(calibration);
    const auto payload = bm::build_allocation_payload_json({record}, {calibration_json});
    const auto find = [&payload](std::string_view fragment) {
        return payload.find(fragment) != std::string::npos;
    };
    EXPECT_TRUE(find("\"schema\":\"M5_PHASE7_ALLOCATION_PAYLOAD_V1\""));
    EXPECT_TRUE(find("\"record_count\":1"));
    EXPECT_TRUE(find("\"calibration_record_count\":1"));
    EXPECT_TRUE(find("\"calibration_records\":[{\"calibration_id\":"
                     "\"calibration/empty-bracket-v1\""));
    EXPECT_TRUE(find("\"records\":[{\"schema\":\"M5_PHASE7_ALLOCATION_RECORD_V1\""));
    EXPECT_TRUE(find("\"subtracted_from_measurements\":false"));
}

// ---------------------------------------------------------------------------
// Workload spec field extraction.
// ---------------------------------------------------------------------------
TEST(Phase7RecordWorkloadSpec, ExtractsCanonicalFields) {
    const std::string text = "benchmark_name=Cell\ngenerator_schema=S\ngenerated_workload_sha256=" +
                             std::string(64, 'c') + "\nseed=not_applicable\n";
    EXPECT_EQ(bm::workload_spec_field(text, "benchmark_name"), "Cell");
    EXPECT_EQ(bm::workload_spec_field(text, "generator_schema"), "S");
    EXPECT_EQ(bm::workload_spec_field(text, "seed"), "not_applicable");
    EXPECT_EQ(bm::workload_spec_field(text, "missing"), "");
}
