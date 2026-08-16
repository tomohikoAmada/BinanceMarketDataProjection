// M5 Phase-7 replay allocation characterization executable (OD-M5-P7-013).
//
// Dedicated single-threaded measurement executable owning the replacement
// global new/delete surface. It measures the four accepted replay identities
// (CoreNormalizedReplay/{Spot,UsdMPerpetual}, AdapterWireReplay/{Spot,
// UsdMPerpetual}) with the accepted small-tier workloads (generator
// m5-small-generator-v1, seed 548746690337, 2,048 events, canonical replay-log
// SHA-256 recorded in every result).
//
//   - the differential oracle verification runs exactly once, entirely
//     OUTSIDE any measurement bracket (mirroring Phase-6);
//   - one full untimed warmup pass with discarded state;
//   - per measured pass: fresh production state constructed OUTSIDE the
//     bracket; the bracket covers only the production pipeline (preloaded
//     normalized operations -> production M1 parsing -> production M3
//     BookProjection -> minimal FNV-1a consumption; preconstructed wire ->
//     production M4 adaptation -> checked M3 invocation for the adapter);
//   - post-pass final-state/checksum validation happens OUTSIDE the bracket;
//   - aggregates (allocation count / bytes / deallocations) and the exact
//     event_count are reported; per-event derived values are EXACT RATIONALS
//     {numerator = aggregate_total, denominator = event_count} — no integer
//     division, no divisibility requirement (OD-M5-P7-013, case 27).

#include "benchmark_support/allocation_instrumentation.hpp"
#include "benchmark_support/core_replay_executor.hpp"
#include "benchmark_support/phase7_harness.hpp"
#include "benchmark_support/phase7_record.hpp"
#include "benchmark_support/replay_checksum.hpp"
#include "benchmark_support/replay_workload_specs.hpp"
#include "benchmark_support/workload_spec.hpp"
#include "benchmark_support/wrapper.hpp"
#include "canonical_text.hpp"
#include "core_production_side.hpp"
#include "reference_side.hpp"
#include "replay_driver.hpp"
#include "small_workload.hpp"

#if defined(BMD_PROJECTION_PHASE6_ADAPTER_ENABLED)
#include "adapter_production_side.hpp"
#include "benchmark_support/adapter_replay_executor.hpp"
#include "benchmark_support/adapter_wire_support.hpp"
#endif

#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

namespace core = binance_market_data::projection::v1;
namespace bm = bmd_projection::m5::benchmark;
namespace oracle = bmd_projection::m5::oracle;
namespace phase3 = bmd_projection::m5::phase3;
namespace replay = bmd_projection::m5::replay;

constexpr std::string_view kCalibrationId = "calibration/empty-bracket-v1";

const auto kReplaySpecRegistration = [] {
    bm::register_replay_workload_specs();
    return 0;
}();

struct ReplayIdentity final {
    std::string name;
    replay::ReplayFixture fixture;
    oracle::ReplayMode mode;
};

[[nodiscard]] std::vector<ReplayIdentity> build_replay_inventory() {
    std::vector<ReplayIdentity> identities;
    identities.push_back({"CoreNormalizedReplay/Spot", phase3::make_spot_small_workload(),
                          oracle::ReplayMode::CoreOnly});
    identities.push_back({"CoreNormalizedReplay/UsdMPerpetual",
                          phase3::make_usdm_small_workload(), oracle::ReplayMode::CoreOnly});
#if defined(BMD_PROJECTION_PHASE6_ADAPTER_ENABLED)
    identities.push_back({"AdapterWireReplay/Spot", phase3::make_spot_small_workload(),
                          oracle::ReplayMode::AdapterEnabled});
    identities.push_back({"AdapterWireReplay/UsdMPerpetual",
                          phase3::make_usdm_small_workload(), oracle::ReplayMode::AdapterEnabled});
#endif
    return identities;
}

// Differential semantic preflight: production vs reference oracle, run exactly
// once outside any measured region (OD-M5-P7-013). A divergent workload never
// reaches measurement.
void verify_fixture_differentially(const replay::ReplayFixture& fixture, oracle::ReplayMode mode) {
    std::unique_ptr<oracle::ReplaySide> production;
    std::unique_ptr<oracle::ReplaySide> reference;
    switch (mode) {
    case oracle::ReplayMode::CoreOnly:
        production = oracle::make_core_production_side(fixture);
        reference = oracle::make_reference_side(fixture, oracle::ReplayMode::CoreOnly);
        break;
    case oracle::ReplayMode::AdapterEnabled:
#if defined(BMD_PROJECTION_PHASE6_ADAPTER_ENABLED)
        production = oracle::make_adapter_production_side(fixture);
        reference = oracle::make_reference_side(fixture, oracle::ReplayMode::AdapterEnabled);
#else
        std::abort();
#endif
        break;
    }
    oracle::ReplayDriver driver{fixture, std::move(production), std::move(reference),
                                oracle::ObservationRetention::RetainNone};
    const auto outcome = driver.run();
    if (outcome.first_divergence.has_value() ||
        outcome.processed_events != fixture.replay.operations.size()) {
        std::fprintf(stderr, "replay differential preflight divergence detected\n");
        std::exit(2);
    }
}

[[nodiscard]] bm::AllocationRecordInput make_record_input(const bm::Phase7Harness& harness,
                                                          const std::string& name,
                                                          const bm::Phase7CellOutcome& outcome,
                                                          std::uint64_t event_count) {
    bm::AllocationRecordInput input;
    input.measurement_scope = name;
    input.operation_denominator = "replay_pass";
    input.workload_id = name;
    const auto* workload = harness.find_workload(name);
    if (workload == nullptr) {
        std::fprintf(stderr, "missing workload identity for %s\n", name.c_str());
        std::exit(2);
    }
    const auto hash = bmd_projection::m5::replay::sha256_hex(workload->second);
    input.workload_spec_sha256 =
        std::holds_alternative<std::string>(hash) ? std::get<std::string>(hash) : std::string{};
    input.generator_schema = bm::workload_spec_field(workload->second, "generator_schema");
    input.generated_workload_sha256 =
        bm::workload_spec_field(workload->second, "generated_workload_sha256");
    input.seed = bm::workload_spec_field(workload->second, "seed");
    input.canonical_log_sha256 =
        bm::workload_spec_field(workload->second, "canonical_log_sha256");
    input.event_count = event_count;
    input.evidence_class = harness.options().evidence_class;
    input.measurement = outcome.measurement;
    input.has_post_destroy_snapshot = outcome.has_post_destroy;
    input.post_destroy_live_bytes = outcome.post_destroy_live_bytes;
    input.post_destroy_lifecycle_status = outcome.post_destroy_lifecycle_status;
    input.repetitions = harness.options().repetitions;
    input.determinism_confirmed = outcome.determinism_confirmed;
    input.operation_aborted = outcome.measurement.operation_aborted;
    input.allocation_failure_observed = outcome.measurement.allocation_failure_observed;
    input.baseline_snapshot_a =
        "bracket open (fresh production state alive; preloaded normalized inputs / "
        "preconstructed wire ready; differential oracle verification already completed "
        "outside the bracket)";
    input.baseline_snapshot_b =
        "bracket close (production pass returned; final-state/checksum validation runs "
        "outside the bracket)";
    input.baseline_delta_formula =
        "exact A/B comparison producing persistent_live_delta {sign, magnitude}; "
        "per-event derived values are exact rationals {numerator = aggregate_total, "
        "denominator = event_count} without integer division";
    input.calibration_reference = std::string{kCalibrationId};
    const auto& m = outcome.measurement;
    input.replay_aggregate = bm::ReplayAggregate{};
    input.replay_aggregate->aggregate_allocation_count = m.allocation_count;
    input.replay_aggregate->aggregate_allocated_bytes = m.total_allocated_bytes;
    input.replay_aggregate->aggregate_deallocation_count = m.deallocation_count;
    input.replay_aggregate->aggregate_deallocated_bytes = m.deallocated_bytes;
    input.replay_aggregate->event_count = event_count;
    input.replay_aggregate->derived_per_event_allocations = {m.allocation_count, event_count};
    input.replay_aggregate->derived_per_event_bytes = {m.total_allocated_bytes, event_count};
    return input;
}

int run_phase7_replay(int argc, char** argv) {
    auto options = bm::parse_phase7_options(argc, argv);
    if (options.output_path.empty() || options.wrapper_path.empty()) {
        std::fprintf(stderr, "--m5_output and --m5_wrapper_out are required\n");
        return 2;
    }
    if (options.evidence_class != "formal" && options.evidence_class != "exploratory") {
        std::fprintf(stderr, "invalid --m5_evidence_class value: %s\n",
                     options.evidence_class.c_str());
        return 2;
    }
    const auto source_state = bm::compute_source_provenance_state();
    if (options.evidence_class == "formal" && source_state.dirty) {
        options.evidence_class = "exploratory";
        std::fprintf(stderr, "dirty source: evidence class downgraded to exploratory\n");
    }

    bm::Phase7Harness harness{options};

    bm::CalibrationRecordInput calibration;
    calibration.calibration_id = std::string{kCalibrationId};
    calibration.evidence_class = options.evidence_class;
    calibration.description =
        "empty measurement bracket (instrumentation bookkeeping baseline; "
        "reported separately and never subtracted)";
    calibration.measurement = harness.measure_calibration();
    harness.add_calibration_record(bm::build_calibration_record_json(calibration));

    const auto inventory = build_replay_inventory();
    bool run_failed = false;
    std::size_t measured = 0;
    for (const auto& identity : inventory) {
        if (!options.filter.empty() &&
            identity.name.find(options.filter) == std::string::npos) {
            continue;
        }
        const auto& fixture = identity.fixture;
        // 1. Differential preflight, once, outside every measured region.
        verify_fixture_differentially(fixture, identity.mode);
        const auto event_count = fixture.replay.operations.size();

        // 2./3. Executor preallocation, expected checksum from a scratch pass,
        // and one full untimed warmup pass with discarded state — all outside
        // any measurement bracket (mirroring Phase-6 and OD-M5-P7-013/014).
        std::uint64_t expected_pass_checksum = 0;
        if (identity.mode == oracle::ReplayMode::CoreOnly) {
            bm::CoreReplayExecutor executor{fixture};
            {
                core::BookProjection projection{executor.numeric_spec(), executor.policy()};
                expected_pass_checksum = executor.run(projection);
            }
            {
                core::BookProjection warmup_projection{executor.numeric_spec(), executor.policy()};
                if (executor.run(warmup_projection) != expected_pass_checksum) {
                    std::fprintf(stderr, "%s warmup checksum mismatch\n", identity.name.c_str());
                    return 2;
                }
            }
            // 4. Measured passes: fresh production state per pass constructed
            //    outside the bracket; the bracket covers only the production
            //    replay pipeline.
            core::BookProjection measured_projection{executor.numeric_spec(), executor.policy()};
            std::uint64_t checksum_after_pass = 0;
            const auto outcome = harness.measure_cell(
                [&] { checksum_after_pass = executor.run(measured_projection); },
                {},
                [&] {
                    measured_projection =
                        core::BookProjection{executor.numeric_spec(), executor.policy()};
                });
            auto input = make_record_input(harness, identity.name, outcome, event_count);
            if (checksum_after_pass != expected_pass_checksum) {
                input.determinism_confirmed = false;
                std::fprintf(stderr, "%s post-pass checksum mismatch\n", identity.name.c_str());
            }
            std::printf("[phase7] %s allocs=%llu bytes=%llu deallocs=%llu events=%llu\n",
                        identity.name.c_str(),
                        static_cast<unsigned long long>(outcome.measurement.allocation_count),
                        static_cast<unsigned long long>(outcome.measurement.total_allocated_bytes),
                        static_cast<unsigned long long>(outcome.measurement.deallocation_count),
                        static_cast<unsigned long long>(event_count));
            harness.add_record(bm::build_allocation_record_json(input));
            ++measured;
            run_failed = run_failed || !input.determinism_confirmed;
            continue;
        }
#if defined(BMD_PROJECTION_PHASE6_ADAPTER_ENABLED)
        bm::AdapterReplayExecutor executor{
            fixture, bm::adapter_support::preconstruct_adapter_wire(fixture)};
        {
            core::BookProjection projection{executor.numeric_spec(), executor.policy()};
            expected_pass_checksum = executor.run(projection);
        }
        {
            core::BookProjection warmup_projection{executor.numeric_spec(), executor.policy()};
            if (executor.run(warmup_projection) != expected_pass_checksum) {
                std::fprintf(stderr, "%s warmup checksum mismatch\n", identity.name.c_str());
                return 2;
            }
        }
        core::BookProjection measured_projection{executor.numeric_spec(), executor.policy()};
        std::uint64_t checksum_after_pass = 0;
        const auto outcome = harness.measure_cell(
            [&] { checksum_after_pass = executor.run(measured_projection); },
            {},
            [&] {
                measured_projection =
                    core::BookProjection{executor.numeric_spec(), executor.policy()};
            });
        auto input = make_record_input(harness, identity.name, outcome, event_count);
        if (checksum_after_pass != expected_pass_checksum) {
            input.determinism_confirmed = false;
            std::fprintf(stderr, "%s post-pass checksum mismatch\n", identity.name.c_str());
        }
        std::printf("[phase7] %s allocs=%llu bytes=%llu deallocs=%llu events=%llu\n",
                    identity.name.c_str(),
                    static_cast<unsigned long long>(outcome.measurement.allocation_count),
                    static_cast<unsigned long long>(outcome.measurement.total_allocated_bytes),
                    static_cast<unsigned long long>(outcome.measurement.deallocation_count),
                    static_cast<unsigned long long>(event_count));
        harness.add_record(bm::build_allocation_record_json(input));
        ++measured;
        run_failed = run_failed || !input.determinism_confirmed;
#else
        std::abort();
#endif
    }

    if (options.filter.empty() && measured != inventory.size()) {
        std::fprintf(stderr, "Phase-7 replay inventory completeness failure: measured %zu of %zu\n",
                     measured, inventory.size());
        return 2;
    }

    if (!harness.emit()) {
        return 1;
    }
    std::printf("[phase7] replay allocation characterization complete: %zu identities\n",
                measured);
    return run_failed ? 1 : 0;
}

} // namespace

int main(int argc, char** argv) { return run_phase7_replay(argc, argv); }
