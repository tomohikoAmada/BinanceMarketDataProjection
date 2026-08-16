// M5 Phase-7 M4 allocation characterization executable (OD-M5-P7-012).
//
// Dedicated single-threaded measurement executable owning the replacement
// global new/delete surface. Built only with BMD_PROJECTION_BUILD_PROTO_ADAPTER=ON
// (mirroring the Phase-6 M4 availability rule; absence fails validation
// closed). It measures the eight accepted M4 families at depths {8, 100, 1000}
// with the exact Phase-6 measurement boundaries:
//
//   AdaptExchangeDepthSnapshot / AdaptDepthUpdate:
//       bracket = the adaptation call only; the inbound Protobuf wire message
//       is preconstructed OUTSIDE the bracket (wire construction is setup,
//       OD-M5-P6-011); the owning AdaptedBookBaseline / AdaptedDepthBatch is
//       alive at B and destroyed outside the bracket (D lifecycle).
//   CheckedInstall / CheckedApply:
//       the owner is pre-adapted outside the bracket; bracket = the checked
//       production invocation against a freshly prepared projection pool.
//   MakeLocalOrderBookSnapshot/{Unlimited,Limited}:
//       SnapshotContext/Options preconstructed outside; bracket = the
//       snapshot builder; the owning message defines the B/D lifecycle.
//   SerializeSnapshot/{FreshBuffer,ReusedBuffer}:
//       bracket = the serialization call; FreshBuffer is the formal primary
//       (fresh owning buffer per execution, B/D lifecycle); ReusedBuffer is
//       the optional diagnostic whose capacity is established during
//       preparation, so the measured steady-state path allocates nothing.
//
// All M4 metrics are scoped to allocation_boundary = cxx_replaceable_global_new:
// an OBSERVED SUBSET through the replaceable global-new boundary, never
// complete protobuf heap traffic and never heap_complete (OD-M5-P7-012,
// M5-P7-MR-007).

#include "benchmark_support/adapter_wire_support.hpp"
#include "benchmark_support/allocation_instrumentation.hpp"
#include "benchmark_support/book_state.hpp"
#include "benchmark_support/m4_workload_specs.hpp"
#include "benchmark_support/phase7_harness.hpp"
#include "benchmark_support/phase7_record.hpp"
#include "benchmark_support/workload_spec.hpp"
#include "benchmark_support/wrapper.hpp"
#include "canonical_text.hpp"

#include <binance_market_data/projection/v1/snapshots.pb.h>
#include <binance_market_data/projection_adapter/v1/proto_adapter.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {

namespace adapter = binance_market_data::projection_adapter::v1;
namespace core = binance_market_data::projection::v1;
namespace bm = bmd_projection::m5::benchmark;
namespace wire_support = bmd_projection::m5::benchmark::adapter_support;

constexpr std::size_t kM4DepthSet[] = {8, 100, 1'000};
constexpr std::string_view kCalibrationId = "calibration/empty-bracket-v1";

const auto kM4SpecRegistration = [] {
    bm::register_m4_workload_specs();
    return 0;
}();

[[nodiscard]] std::vector<std::string> build_m4_inventory() {
    const std::string_view families[] = {"AdaptExchangeDepthSnapshot/Spot",
                                         "AdaptDepthUpdate/Spot",
                                         "CheckedInstall",
                                         "CheckedApply",
                                         "MakeLocalOrderBookSnapshot/Unlimited",
                                         "MakeLocalOrderBookSnapshot/Limited",
                                         "SerializeSnapshot/FreshBuffer",
                                         "SerializeSnapshot/ReusedBuffer"};
    std::vector<std::string> names;
    for (const auto family : families) {
        for (const auto depth : kM4DepthSet) {
            names.push_back("M4/" + std::string{family} + "/" + std::to_string(depth));
        }
    }
    return names;
}

[[nodiscard]] std::string family_of(const std::string& name) {
    const auto first = std::string_view{"M4/"}.size();
    const auto last = name.rfind('/');
    return name.substr(first, last - first);
}

[[nodiscard]] std::size_t depth_of(const std::string& name) {
    return static_cast<std::size_t>(
        std::strtoull(name.substr(name.rfind('/') + 1).c_str(), nullptr, 10));
}

[[nodiscard]] bm::AllocationRecordInput make_record_input(const bm::Phase7Harness& harness,
                                                          const std::string& name,
                                                          const bm::Phase7CellOutcome& outcome) {
    bm::AllocationRecordInput input;
    input.measurement_scope = name;
    input.operation_denominator = family_of(name);
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
        "bracket open (wire/owner/context/options preconstructed outside; prepared "
        "projection state alive)";
    input.baseline_snapshot_b =
        "bracket close (operation returned; owning result alive where applicable)";
    input.baseline_delta_formula =
        "exact A/B comparison producing persistent_live_delta {sign, magnitude}; "
        "P - A and P - max(A, B) normalized transients; scoped to observed "
        "cxx_replaceable_global_new traffic (observed subset, never heap_complete)";
    input.calibration_reference = std::string{kCalibrationId};
    return input;
}

// ---------------------------------------------------------------------------
// Family runners.
// ---------------------------------------------------------------------------
[[nodiscard]] std::string measure_adapt_snapshot(bm::Phase7Harness& harness,
                                                 const std::string& name, std::size_t depth) {
    const auto identity = wire_support::benchmark_wire_identity();
    const auto wire = wire_support::make_snapshot_wire(depth);
    std::optional<std::variant<adapter::AdaptedBookBaseline, adapter::AdapterError>> owned;
    bool produced_ok = false;
    std::uint64_t accumulator = 0;
    const auto outcome = harness.measure_cell(
        [&] {
            owned = adapter::adapt_exchange_depth_snapshot(wire, identity.numeric_spec,
                                                           identity.expected);
            produced_ok =
                owned.has_value() && std::holds_alternative<adapter::AdaptedBookBaseline>(*owned);
            if (produced_ok) {
                accumulator += std::get<adapter::AdaptedBookBaseline>(*owned)
                                   .metadata()
                                   .observed_quality.size();
            }
        },
        [&] { owned.reset(); });
    auto input = make_record_input(harness, name, outcome);
    if (!produced_ok || !outcome.operation_ok) {
        input.determinism_confirmed = false;
        std::fprintf(stderr, "%s adaptation produced an adapter error\n", name.c_str());
    }
    std::printf("[phase7] %s allocs=%llu bytes=%llu accum=%llu\n", name.c_str(),
                static_cast<unsigned long long>(outcome.measurement.allocation_count),
                static_cast<unsigned long long>(outcome.measurement.total_allocated_bytes),
                static_cast<unsigned long long>(accumulator));
    return bm::build_allocation_record_json(input);
}

[[nodiscard]] std::string measure_adapt_update(bm::Phase7Harness& harness,
                                               const std::string& name) {
    const auto identity = wire_support::benchmark_wire_identity();
    const auto wire = wire_support::make_update_wire(wire_support::kM4AdaptDepthUpdateCell);
    std::optional<std::variant<adapter::AdaptedDepthBatch, adapter::AdapterError>> owned;
    bool produced_ok = false;
    std::uint64_t accumulator = 0;
    const auto outcome = harness.measure_cell(
        [&] {
            owned = adapter::adapt_depth_update(wire, identity.numeric_spec, identity.expected);
            produced_ok =
                owned.has_value() && std::holds_alternative<adapter::AdaptedDepthBatch>(*owned);
            if (produced_ok) {
                accumulator +=
                    std::get<adapter::AdaptedDepthBatch>(*owned).metadata().observed_quality.size();
            }
        },
        [&] { owned.reset(); });
    auto input = make_record_input(harness, name, outcome);
    if (!produced_ok || !outcome.operation_ok) {
        input.determinism_confirmed = false;
        std::fprintf(stderr, "%s adaptation produced an adapter error\n", name.c_str());
    }
    std::printf("[phase7] %s allocs=%llu bytes=%llu accum=%llu\n", name.c_str(),
                static_cast<unsigned long long>(outcome.measurement.allocation_count),
                static_cast<unsigned long long>(outcome.measurement.total_allocated_bytes),
                static_cast<unsigned long long>(accumulator));
    return bm::build_allocation_record_json(input);
}

[[nodiscard]] std::string measure_checked_install(bm::Phase7Harness& harness,
                                                  const std::string& name, std::size_t depth) {
    const auto identity = wire_support::benchmark_wire_identity();
    const auto wire = wire_support::make_snapshot_wire(depth);
    const auto adapted =
        adapter::adapt_exchange_depth_snapshot(wire, identity.numeric_spec, identity.expected);
    if (!std::holds_alternative<adapter::AdaptedBookBaseline>(adapted)) {
        std::fprintf(stderr, "%s pre-adaptation failed\n", name.c_str());
        std::exit(2);
    }
    const auto& owner = std::get<adapter::AdaptedBookBaseline>(adapted);
    const auto pool_size = bm::pool_iteration_count(depth);
    bm::StatePool<core::BookProjection> pool;
    pool.fill(pool_size, [&identity] {
        return core::BookProjection{identity.numeric_spec, core::SequencePolicyKind::Spot};
    });
    core::InstallDisposition disposition{core::InstallDisposition::RejectedWrongState};
    std::uint64_t accumulator = 0;
    std::size_t rep = 0;
    const auto outcome = harness.measure_cell([&] {
        auto& target = pool.at(rep);
        const auto installed = owner.install_into(target);
        if (std::holds_alternative<core::InstallResult>(installed)) {
            const auto& result = std::get<core::InstallResult>(installed);
            disposition = result.disposition;
            accumulator +=
                static_cast<std::uint64_t>(static_cast<std::uint8_t>(result.status_after));
        } else {
            disposition = core::InstallDisposition::RejectedWrongState;
        }
        ++rep;
    });
    auto input = make_record_input(harness, name, outcome);
    if (disposition != core::InstallDisposition::Installed || !outcome.operation_ok) {
        input.determinism_confirmed = false;
        std::fprintf(stderr, "%s disposition drift\n", name.c_str());
    }
    std::printf("[phase7] %s allocs=%llu bytes=%llu accum=%llu\n", name.c_str(),
                static_cast<unsigned long long>(outcome.measurement.allocation_count),
                static_cast<unsigned long long>(outcome.measurement.total_allocated_bytes),
                static_cast<unsigned long long>(accumulator));
    return bm::build_allocation_record_json(input);
}

[[nodiscard]] std::string measure_checked_apply(bm::Phase7Harness& harness, const std::string& name,
                                                std::size_t depth) {
    const auto identity = wire_support::benchmark_wire_identity();
    const auto wire = wire_support::make_update_wire(wire_support::kM4CheckedApplyCell);
    const auto adapted =
        adapter::adapt_depth_update(wire, identity.numeric_spec, identity.expected);
    if (!std::holds_alternative<adapter::AdaptedDepthBatch>(adapted)) {
        std::fprintf(stderr, "%s pre-adaptation failed\n", name.c_str());
        std::exit(2);
    }
    const auto& owner = std::get<adapter::AdaptedDepthBatch>(adapted);
    const auto pool_size = bm::pool_iteration_count(depth);
    bm::StatePool<core::BookProjection> pool;
    pool.fill(pool_size, [depth] {
        return bm::build_synchronized_projection(core::SequencePolicyKind::Spot, depth);
    });
    core::ApplyDisposition disposition{core::ApplyDisposition::RejectedWrongState};
    std::uint64_t accumulator = 0;
    std::size_t rep = 0;
    const auto outcome = harness.measure_cell([&] {
        auto& target = pool.at(rep);
        const auto applied = owner.apply_to(target);
        if (std::holds_alternative<core::ApplyResult>(applied)) {
            const auto& result = std::get<core::ApplyResult>(applied);
            disposition = result.disposition;
            accumulator +=
                static_cast<std::uint64_t>(static_cast<std::uint8_t>(result.status_after));
        } else {
            disposition = core::ApplyDisposition::RejectedWrongState;
        }
        ++rep;
    });
    auto input = make_record_input(harness, name, outcome);
    if (disposition != core::ApplyDisposition::Applied || !outcome.operation_ok) {
        input.determinism_confirmed = false;
        std::fprintf(stderr, "%s disposition drift\n", name.c_str());
    }
    std::printf("[phase7] %s allocs=%llu bytes=%llu accum=%llu\n", name.c_str(),
                static_cast<unsigned long long>(outcome.measurement.allocation_count),
                static_cast<unsigned long long>(outcome.measurement.total_allocated_bytes),
                static_cast<unsigned long long>(accumulator));
    return bm::build_allocation_record_json(input);
}

[[nodiscard]] adapter::SnapshotContext make_snapshot_context() {
    return {{"BTCUSDT", core::SequencePolicyKind::Spot},
            "phase7-allocation",
            "1",
            adapter::SnapshotOrigin::GatewayLive,
            1'234'567,
            std::uint64_t{654'321},
            std::nullopt};
}

[[nodiscard]] std::string measure_make_snapshot(bm::Phase7Harness& harness, const std::string& name,
                                                std::size_t depth, bool limited) {
    const auto projection =
        bm::build_synchronized_projection(core::SequencePolicyKind::Spot, depth);
    const auto context = make_snapshot_context();
    adapter::SnapshotOptions options;
    if (limited) {
        const auto limit = adapter::DepthLimit::create(20);
        if (std::holds_alternative<adapter::AdapterError>(limit)) {
            std::fprintf(stderr, "%s depth limit invalid\n", name.c_str());
            std::exit(2);
        }
        options.depth_limit = std::get<adapter::DepthLimit>(limit);
    }
    std::optional<std::variant<core::LocalOrderBookSnapshot, adapter::AdapterError>> owned;
    bool produced_ok = false;
    std::uint64_t accumulator = 0;
    const auto outcome = harness.measure_cell(
        [&] {
            owned = adapter::make_local_order_book_snapshot(projection, context, options);
            produced_ok =
                owned.has_value() && std::holds_alternative<core::LocalOrderBookSnapshot>(*owned);
            if (produced_ok) {
                accumulator += static_cast<std::uint64_t>(
                    std::get<core::LocalOrderBookSnapshot>(*owned).bids_size() +
                    std::get<core::LocalOrderBookSnapshot>(*owned).asks_size());
            }
        },
        [&] { owned.reset(); });
    auto input = make_record_input(harness, name, outcome);
    if (!produced_ok || !outcome.operation_ok) {
        input.determinism_confirmed = false;
        std::fprintf(stderr, "%s snapshot builder produced an adapter error\n", name.c_str());
    }
    std::printf("[phase7] %s allocs=%llu bytes=%llu accum=%llu\n", name.c_str(),
                static_cast<unsigned long long>(outcome.measurement.allocation_count),
                static_cast<unsigned long long>(outcome.measurement.total_allocated_bytes),
                static_cast<unsigned long long>(accumulator));
    return bm::build_allocation_record_json(input);
}

[[nodiscard]] std::string measure_serialize(bm::Phase7Harness& harness, const std::string& name,
                                            std::size_t depth, bool reused) {
    const auto projection =
        bm::build_synchronized_projection(core::SequencePolicyKind::Spot, depth);
    const auto produced = adapter::make_local_order_book_snapshot(
        projection, make_snapshot_context(), adapter::SnapshotOptions{});
    if (!std::holds_alternative<core::LocalOrderBookSnapshot>(produced)) {
        std::fprintf(stderr, "%s snapshot preconstruction failed\n", name.c_str());
        std::exit(2);
    }
    const auto& snapshot = std::get<core::LocalOrderBookSnapshot>(produced);
    std::uint64_t accumulator = 0;
    if (reused) {
        // Optional diagnostic: the reused buffer's capacity is established
        // during preparation (one-time effect absorbed, OD-M5-P7-014); the
        // measured steady-state serialization writes into the retained
        // capacity without allocating.
        std::string buffer;
        static_cast<void>(snapshot.SerializeToString(&buffer));
        const auto outcome = harness.measure_cell([&] {
            buffer.clear();
            const bool ok = snapshot.SerializeToString(&buffer);
            accumulator += ok ? buffer.size() : 0xDEADULL;
            if (!buffer.empty()) {
                accumulator += static_cast<unsigned char>(buffer.front());
            }
        });
        auto input = make_record_input(harness, name, outcome);
        input.baseline_snapshot_a =
            "bracket open (reused buffer with pre-established capacity; clear() outside the "
            "bracket retains capacity)";
        input.determinism_confirmed = input.determinism_confirmed && outcome.operation_ok;
        std::printf("[phase7] %s allocs=%llu bytes=%llu accum=%llu\n", name.c_str(),
                    static_cast<unsigned long long>(outcome.measurement.allocation_count),
                    static_cast<unsigned long long>(outcome.measurement.total_allocated_bytes),
                    static_cast<unsigned long long>(accumulator));
        return bm::build_allocation_record_json(input);
    }
    // Formal primary: a fresh owning buffer per execution; B with the buffer
    // alive, destruction outside the bracket (D).
    std::string buffer;
    const auto outcome = harness.measure_cell(
        [&] {
            const bool ok = snapshot.SerializeToString(&buffer);
            accumulator += ok ? buffer.size() : 0xDEADULL;
            if (!buffer.empty()) {
                accumulator += static_cast<unsigned char>(buffer.front());
            }
        },
        [&] { std::string{}.swap(buffer); });
    auto input = make_record_input(harness, name, outcome);
    input.determinism_confirmed = input.determinism_confirmed && outcome.operation_ok;
    std::printf("[phase7] %s allocs=%llu bytes=%llu accum=%llu\n", name.c_str(),
                static_cast<unsigned long long>(outcome.measurement.allocation_count),
                static_cast<unsigned long long>(outcome.measurement.total_allocated_bytes),
                static_cast<unsigned long long>(accumulator));
    return bm::build_allocation_record_json(input);
}

// ---------------------------------------------------------------------------
// Warmup: one full untimed workload-equivalent pass over the same cell
// sequence (OD-M5-P7-014); Protobuf one-time initialization is absorbed here.
// ---------------------------------------------------------------------------
void warmup_pass() {
    const auto identity = wire_support::benchmark_wire_identity();
    {
        const auto wire = wire_support::make_snapshot_wire(1'000);
        auto adapted =
            adapter::adapt_exchange_depth_snapshot(wire, identity.numeric_spec, identity.expected);
        static_cast<void>(adapted);
        auto projection = bm::build_synchronized_projection(core::SequencePolicyKind::Spot, 1'000);
        const auto produced = adapter::make_local_order_book_snapshot(
            projection, make_snapshot_context(), adapter::SnapshotOptions{});
        if (std::holds_alternative<core::LocalOrderBookSnapshot>(produced)) {
            std::string buffer;
            static_cast<void>(
                std::get<core::LocalOrderBookSnapshot>(produced).SerializeToString(&buffer));
        }
    }
    std::printf("[phase7] M4 warmup complete\n");
}

int run_phase7_m4(int argc, char** argv) {
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

    warmup_pass();

    bm::Phase7Harness harness{options};

    bm::CalibrationRecordInput calibration;
    calibration.calibration_id = std::string{kCalibrationId};
    calibration.evidence_class = options.evidence_class;
    calibration.description = "empty measurement bracket (instrumentation bookkeeping baseline; "
                              "reported separately and never subtracted)";
    calibration.measurement = harness.measure_calibration();
    harness.add_calibration_record(bm::build_calibration_record_json(calibration));

    const auto inventory = build_m4_inventory();
    bool run_failed = false;
    std::vector<std::string> measured_names;
    for (const auto& name : inventory) {
        if (!options.filter.empty() && name.find(options.filter) == std::string::npos) {
            continue;
        }
        const auto family = family_of(name);
        const auto depth = depth_of(name);
        std::string record;
        if (family == "AdaptExchangeDepthSnapshot/Spot") {
            record = measure_adapt_snapshot(harness, name, depth);
        } else if (family == "AdaptDepthUpdate/Spot") {
            record = measure_adapt_update(harness, name);
        } else if (family == "CheckedInstall") {
            record = measure_checked_install(harness, name, depth);
        } else if (family == "CheckedApply") {
            record = measure_checked_apply(harness, name, depth);
        } else if (family == "MakeLocalOrderBookSnapshot/Unlimited") {
            record = measure_make_snapshot(harness, name, depth, false);
        } else if (family == "MakeLocalOrderBookSnapshot/Limited") {
            record = measure_make_snapshot(harness, name, depth, true);
        } else if (family == "SerializeSnapshot/FreshBuffer") {
            record = measure_serialize(harness, name, depth, false);
        } else if (family == "SerializeSnapshot/ReusedBuffer") {
            record = measure_serialize(harness, name, depth, true);
        } else {
            std::fprintf(stderr, "unknown Phase-7 M4 cell: %s\n", name.c_str());
            return 2;
        }
        harness.add_record(record);
        measured_names.push_back(name);
        if (record.find("\"determinism_confirmed\":false") != std::string::npos) {
            run_failed = true;
        }
    }

    if (options.filter.empty() && measured_names != inventory) {
        std::fprintf(stderr, "Phase-7 M4 inventory completeness failure: measured %zu of %zu\n",
                     measured_names.size(), inventory.size());
        return 2;
    }

    if (!harness.emit()) {
        return 1;
    }
    std::printf("[phase7] M4 allocation characterization complete: %zu cells\n",
                measured_names.size());
    return run_failed ? 1 : 0;
}

} // namespace

int main(int argc, char** argv) { return run_phase7_m4(argc, argv); }
