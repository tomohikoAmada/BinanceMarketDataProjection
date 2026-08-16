// M5 Phase-7 persistent footprint experiment (OD-M5-P7-006).
//
// Dedicated single-threaded measurement executable owning the replacement
// global new/delete surface. For each accepted depth D in
// {100, 1000, 5000, 10000} it records exact live-bytes snapshots:
//
//   S_base  pre-experiment baseline (no book)
//   S_empty empty OrderBook constructed with the fixed NumericSpec
//   S_bids  bids populated to D levels (replace_all from harness-owned
//           vectors built outside any bracket; no intermediate mutation
//           traffic pollutes the persistent delta)
//   S_both  asks populated to D levels
//   D       post-destroy snapshot (must equal S_base; else INELIGIBLE)
//
// Reported per cell: measured total/per-side deltas, exact rational
// bytes-per-level {numerator, D}, the fixed-object footprint and empty-book
// baseline (measured once), post-destroy lifecycle status, and the strictly
// separated model fields: NODE STRUCTURAL MODEL — NON-ADDITIVE,
// ALLOCATOR BACKING MODEL — ESTIMATED, RSS — NOT MEASURED. The calibration
// record is reported separately and never subtracted. No additive
// measured+model total is ever manufactured (OD-M5-P7-006, M5-P7-MR-010).

#include "benchmark_support/allocation_instrumentation.hpp"
#include "benchmark_support/book_state.hpp"
#include "benchmark_support/phase7_harness.hpp"
#include "benchmark_support/phase7_record.hpp"
#include "benchmark_support/wrapper.hpp"

#include <binance_market_data/projection/v1/order_book/order_book.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace core = binance_market_data::projection::v1;
namespace bm = bmd_projection::m5::benchmark;
namespace alloc = bmd_projection::m5::allocation;

constexpr std::size_t kFootprintDepths[] = {100, 1'000, 5'000, 10'000};
constexpr std::string_view kCalibrationId = "calibration/empty-bracket-v1";

// One full untimed workload-equivalent warmup pass (OD-M5-P7-014): construct
// and destroy a depth-1000 book and emit one progress line, so one-time
// allocator/runtime/stdio paths are absorbed before any snapshot.
void footprint_warmup() {
    auto book = bm::build_order_book(1'000);
    static_cast<void>(book.best_bid());
    std::printf("[phase7] footprint warmup complete\n");
}

struct FootprintSnapshots final {
    std::uint64_t base{};
    std::uint64_t empty{};
    std::uint64_t bids{};
    std::uint64_t both{};
    std::uint64_t post_destroy{};
};

// Measures one depth cell: snapshots only (no measurement bracket; the
// deltas are exact snapshot comparisons — OD-M5-P7-006/007).
[[nodiscard]] FootprintSnapshots measure_depth(std::size_t depth) {
    FootprintSnapshots snapshots;
    snapshots.base = alloc::live_bytes_snapshot();
    {
        core::OrderBook book{bm::benchmark_numeric_spec()};
        snapshots.empty = alloc::live_bytes_snapshot();
        const auto bids = bm::build_bid_levels(depth);
        book.replace_all(bids, {});
        snapshots.bids = alloc::live_bytes_snapshot();
        const auto asks = bm::build_ask_levels(depth);
        book.replace_all(bids, asks);
        snapshots.both = alloc::live_bytes_snapshot();
    }
    snapshots.post_destroy = alloc::live_bytes_snapshot();
    return snapshots;
}

[[nodiscard]] std::string fixed_object_scope() { return "M5_Footprint/FixedObject"; }

int run_phase7_footprint(int argc, char** argv) {
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

    footprint_warmup();

    bm::Phase7Harness harness{options};

    bm::CalibrationRecordInput calibration;
    calibration.calibration_id = std::string{kCalibrationId};
    calibration.evidence_class = options.evidence_class;
    calibration.description =
        "empty measurement bracket (instrumentation bookkeeping baseline; "
        "reported separately and never subtracted)";
    calibration.measurement = harness.measure_calibration();
    harness.add_calibration_record(bm::build_calibration_record_json(calibration));

    const std::string toolchain =
        std::string{bm::standard_library_name()} + " " + bm::standard_library_version();

    bool run_failed = false;

    // Fixed-object footprint: from no book to an empty book, measured once.
    {
        bm::FootprintRecordInput input;
        input.measurement_scope = fixed_object_scope();
        input.evidence_class = options.evidence_class;
        input.generator_schema = "M5_PHASE6_M2_CELLS_V1";
        input.generator_seed = "not_applicable";
        input.depth_per_side = 0;
        input.post_destroy_lifecycle_status = "mismatch:post_destroy";
        input.ineligibility_reason = "post_destroy_live_bytes_differ_from_pre_experiment_baseline";
        input.repetitions = 1;
        input.calibration_reference = std::string{kCalibrationId};
        input.node_structural_model_description =
            "std::map node allocation request already includes the node object "
            "structure (links/key/value/padding); the model is explanatory and "
            "never added to measured requested bytes";
        input.node_structural_model_toolchain = toolchain;
        input.allocator_backing_model_description =
            "environment/toolchain/allocator-specific estimate of backing "
            "overhead outside the raw request size; no universal fixed header "
            "size is assumed";
        input.allocator_backing_model_scope =
            "environment/toolchain/allocator/size-class-specific";
        const auto snapshots = measure_depth(0);
        input.pre_experiment_baseline_live_bytes = snapshots.base;
        input.empty_book_live_bytes = snapshots.empty;
        input.bids_only_live_bytes = snapshots.bids;
        input.both_sides_live_bytes = snapshots.both;
        input.post_destroy_live_bytes = snapshots.post_destroy;
        input.post_destroy_lifecycle_status =
            snapshots.post_destroy == snapshots.base ? "consistent" : "mismatch:post_destroy";
        input.eligible = input.post_destroy_lifecycle_status == "consistent";
        input.determinism_confirmed = input.eligible;
        std::printf("[phase7] M5_Footprint/FixedObject empty=%llu base=%llu\n",
                    static_cast<unsigned long long>(snapshots.empty),
                    static_cast<unsigned long long>(snapshots.base));
        run_failed = run_failed || !input.eligible;
        harness.add_record(bm::build_footprint_record_json(input));
    }

    for (const auto depth : kFootprintDepths) {
        const auto scope = "M5_Footprint/Depth/" + std::to_string(depth);
        if (!options.filter.empty() && scope.find(options.filter) == std::string::npos) {
            continue;
        }
        bm::FootprintRecordInput input;
        input.measurement_scope = scope;
        input.evidence_class = options.evidence_class;
        input.generator_schema = "M5_PHASE6_M2_CELLS_V1";
        input.generator_seed = "not_applicable";
        input.depth_per_side = depth;
        input.post_destroy_lifecycle_status = "mismatch:post_destroy";
        input.ineligibility_reason = "post_destroy_live_bytes_differ_from_pre_experiment_baseline";
        input.repetitions = options.repetitions;
        input.determinism_confirmed = true;
        input.calibration_reference = std::string{kCalibrationId};
        input.node_structural_model_description =
            "std::map node allocation request already includes the node object "
            "structure (links/key/value/padding); the model is explanatory and "
            "never added to measured requested bytes";
        input.node_structural_model_toolchain = toolchain;
        input.allocator_backing_model_description =
            "environment/toolchain/allocator-specific estimate of backing "
            "overhead outside the raw request size; no universal fixed header "
            "size is assumed";
        input.allocator_backing_model_scope =
            "environment/toolchain/allocator/size-class-specific";
        // All record input preparation happens BEFORE any snapshot
        // (OD-M5-P7-007): the snapshots below are the first measurements.
        const auto snapshots = measure_depth(depth);
        input.pre_experiment_baseline_live_bytes = snapshots.base;
        input.empty_book_live_bytes = snapshots.empty;
        input.bids_only_live_bytes = snapshots.bids;
        input.both_sides_live_bytes = snapshots.both;
        input.post_destroy_live_bytes = snapshots.post_destroy;
        input.post_destroy_lifecycle_status =
            snapshots.post_destroy == snapshots.base ? "consistent" : "mismatch:post_destroy";
        input.eligible = input.post_destroy_lifecycle_status == "consistent";
        // Repetition discipline: re-measure the depth cell and require exact
        // snapshot equality (normalized footprint determinism, OD-M5-P7-015).
        for (std::size_t rep = 1; rep < options.repetitions; ++rep) {
            const auto repeat = measure_depth(depth);
            if (repeat.base != snapshots.base || repeat.empty != snapshots.empty ||
                repeat.bids != snapshots.bids || repeat.both != snapshots.both ||
                repeat.post_destroy != snapshots.post_destroy) {
                std::fprintf(stderr,
                             "depth %zu repeat %zu mismatch: base %llu/%llu empty %llu/%llu "
                             "bids %llu/%llu both %llu/%llu post %llu/%llu\n",
                             depth, rep,
                             static_cast<unsigned long long>(repeat.base),
                             static_cast<unsigned long long>(snapshots.base),
                             static_cast<unsigned long long>(repeat.empty),
                             static_cast<unsigned long long>(snapshots.empty),
                             static_cast<unsigned long long>(repeat.bids),
                             static_cast<unsigned long long>(snapshots.bids),
                             static_cast<unsigned long long>(repeat.both),
                             static_cast<unsigned long long>(snapshots.both),
                             static_cast<unsigned long long>(repeat.post_destroy),
                             static_cast<unsigned long long>(snapshots.post_destroy));
                input.determinism_confirmed = false;
                break;
            }
        }
        std::printf("[phase7] %s total=%llu per_side=%llu/%llu D=%llu\n",
                    input.measurement_scope.c_str(),
                    static_cast<unsigned long long>(snapshots.both - snapshots.empty),
                    static_cast<unsigned long long>(snapshots.bids - snapshots.empty),
                    static_cast<unsigned long long>(snapshots.both - snapshots.bids),
                    static_cast<unsigned long long>(snapshots.post_destroy));
        run_failed = run_failed || !input.eligible || !input.determinism_confirmed;
        harness.add_record(bm::build_footprint_record_json(input));
    }

    if (!harness.emit()) {
        return 1;
    }
    std::printf("[phase7] footprint characterization complete\n");
    return run_failed ? 1 : 0;
}

} // namespace

int main(int argc, char** argv) { return run_phase7_footprint(argc, argv); }
