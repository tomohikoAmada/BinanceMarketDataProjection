// M5 Phase-7 M2/M3 allocation characterization executable
// (OD-M5-P7-008/009/010/011).
//
// Dedicated single-threaded measurement executable owning the replacement
// global new/delete surface (allocation_global_new.cpp compiled directly in,
// never via a library; OD-M5-P7-002). It measures exactly the accepted
// Phase-7 M2 inventory, the complete 48-cell M3 accepted live-apply matrix,
// the classification cells, and the Component/Proxy diagnostic cells, reusing
// the accepted Phase-6 workload identities and cell semantics.
//
//   - one full untimed workload-equivalent warmup pass over the entire
//     inventory before any measurement (OD-M5-P7-014);
//   - every measured execution brackets exactly one logical production
//     operation; preparation and pools live entirely outside brackets
//     (OD-M5-P7-007);
//   - owning-output cells (all_levels/top_levels/Component/ProxyRebuild)
//     keep the result alive at B and destroy it outside the bracket (D);
//   - best_bid/best_ask/quantity_at and Stale/Duplicate/Gap are
//     zero-allocation controls; Reset is deallocation-only; B=0 cells remain
//     first-class cells of the 48-cell matrix (never removed/reinterpreted);
//   - cross-repetition normalized metrics must be exactly equal
//     (OD-M5-P7-015).

#include "benchmark_support/allocation_instrumentation.hpp"
#include "benchmark_support/m2_cells.hpp"
#include "benchmark_support/m2_workload_specs.hpp"
#include "benchmark_support/m3_cells.hpp"
#include "benchmark_support/m3_workload_specs.hpp"
#include "benchmark_support/phase7_harness.hpp"
#include "benchmark_support/phase7_record.hpp"
#include "benchmark_support/workload_spec.hpp"
#include "benchmark_support/wrapper.hpp"
#include "canonical_text.hpp"

#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

namespace core = binance_market_data::projection::v1;
namespace bm = bmd_projection::m5::benchmark;
namespace alloc = bmd_projection::m5::allocation;

constexpr std::size_t kRoutineDepths[] = {8, 100, 1'000};
constexpr std::size_t kFullDepthSet[] = {0, 8, 100, 1'000, 5'000, 10'000};
constexpr std::size_t kBatchSet[] = {1, 10, 100};
constexpr std::size_t kTopNSet[] = {1, 5, 50};

constexpr std::string_view kCalibrationId = "calibration/empty-bracket-v1";

const auto kWorkloadSpecRegistration = [] {
    bm::register_m2_workload_specs(false);
    bm::register_m3_workload_specs();
    return 0;
}();

// ---------------------------------------------------------------------------
// Inventory (canonical order; exact match with the Phase-7 required set).
// ---------------------------------------------------------------------------
[[nodiscard]] std::vector<std::string> build_inventory() {
    std::vector<std::string> names;
    for (const auto family : {"insert", "update", "delete", "missing_delete"}) {
        for (const auto depth : kRoutineDepths) {
            names.push_back("M2/apply_level/" + std::string{family} + "/" + std::to_string(depth));
        }
    }
    for (const auto batch : kBatchSet) {
        for (const auto depth : kRoutineDepths) {
            names.push_back("M2/apply_updates/" + std::to_string(batch) + "/" +
                            std::to_string(depth));
        }
    }
    for (const auto depth : kFullDepthSet) {
        names.push_back("M2/replace_all/" + std::to_string(depth));
    }
    for (const auto depth : kFullDepthSet) {
        names.push_back("M2/all_levels/" + std::to_string(depth));
    }
    for (const auto limit : kTopNSet) {
        for (const auto depth : kRoutineDepths) {
            names.push_back("M2/top_levels/" + std::to_string(limit) + "/" + std::to_string(depth));
        }
    }
    for (const auto family : {"best_bid", "best_ask", "quantity_at/hit", "quantity_at/miss"}) {
        for (const auto depth : kRoutineDepths) {
            names.push_back("M2/" + std::string{family} + "/" + std::to_string(depth));
        }
    }
    for (const auto policy :
         {core::SequencePolicyKind::Spot, core::SequencePolicyKind::UsdMPerpetual}) {
        const auto policy_name =
            policy == core::SequencePolicyKind::Spot ? "Spot" : "UsdMPerpetual";
        for (const auto depth : {0, 8, 100, 1'000, 5'000, 10'000}) {
            for (const auto batch : {0, 1, 10, 100}) {
                names.push_back("M3/LiveApply/Accepted/" + std::string{policy_name} + "/D" +
                                std::to_string(depth) + "/B" + std::to_string(batch));
            }
        }
        for (const auto kind : {"Stale", "Duplicate", "Gap", "Reset", "BaselineInstall"}) {
            names.push_back("M3/Classification/" + std::string{kind} + "/" + policy_name);
        }
    }
    for (const auto depth : kRoutineDepths) {
        names.push_back("M3/Component/AllLevelsBothSides/" + std::to_string(depth));
        names.push_back("M3/Proxy/CandidateRebuildFromVectors/" + std::to_string(depth));
        names.push_back("M3/Proxy/CandidateApplyUpdates/" + std::to_string(depth));
        names.push_back("M3/Proxy/OrderBookMoveCommit/" + std::to_string(depth));
    }
    return names;
}

// ---------------------------------------------------------------------------
// Record metadata.
// ---------------------------------------------------------------------------
[[nodiscard]] bm::AllocationRecordInput make_record_input(const bm::Phase7Harness& harness,
                                                          const std::string& name,
                                                          std::string operation_denominator,
                                                          const bm::Phase7CellOutcome& outcome) {
    bm::AllocationRecordInput input;
    input.measurement_scope = name;
    input.operation_denominator = std::move(operation_denominator);
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
        "bracket open (prepared state alive; harness input/pools outside the bracket)";
    input.baseline_snapshot_b =
        "bracket close (operation returned; owning result alive where applicable)";
    input.baseline_delta_formula =
        "exact A/B comparison producing persistent_live_delta {sign, magnitude}; "
        "P - A and P - max(A, B) normalized transients";
    input.calibration_reference = std::string{kCalibrationId};
    return input;
}

[[nodiscard]] std::string disposition_failure(const std::string& name, std::string_view detail) {
    return name + " disposition drift: " + std::string{detail};
}

// ---------------------------------------------------------------------------
// Per-family measurement.
// ---------------------------------------------------------------------------
void warmup_step(const std::string& name) {
    if (name.starts_with("M2/apply_level/")) {
        const auto parts = name.substr(std::string_view{"M2/apply_level/"}.size());
        const auto family = parts.substr(0, parts.find('/'));
        const auto depth = static_cast<std::size_t>(
            std::strtoull(parts.substr(parts.find('/') + 1).c_str(), nullptr, 10));
        const auto kind = family == "insert"   ? bm::M2ApplyLevelKind::Insert
                          : family == "update" ? bm::M2ApplyLevelKind::Update
                          : family == "delete" ? bm::M2ApplyLevelKind::Delete
                                               : bm::M2ApplyLevelKind::MissingDelete;
        bm::M2ApplyLevelCell cell{kind, depth};
        cell.prepare();
        static_cast<void>(cell.execute_step(0));
        return;
    }
    if (name.starts_with("M2/apply_updates/")) {
        const auto parts = name.substr(std::string_view{"M2/apply_updates/"}.size());
        const auto batch = static_cast<std::size_t>(
            std::strtoull(parts.substr(0, parts.find('/')).c_str(), nullptr, 10));
        const auto depth = static_cast<std::size_t>(
            std::strtoull(parts.substr(parts.find('/') + 1).c_str(), nullptr, 10));
        bm::M2ApplyUpdatesCell cell{{depth, batch, bm::M2ApplyUpdatesMix::ReplacementHeavy}};
        cell.prepare();
        cell.execute_step(0);
        return;
    }
    if (name.starts_with("M2/replace_all/")) {
        const auto depth = static_cast<std::size_t>(std::strtoull(
            name.substr(std::string_view{"M2/replace_all/"}.size()).c_str(), nullptr, 10));
        bm::M2ReplaceAllCell cell{depth};
        cell.prepare();
        cell.execute_step();
        return;
    }
    if (name.starts_with("M2/all_levels/") || name.starts_with("M2/top_levels/") ||
        name.starts_with("M2/best_bid/") || name.starts_with("M2/best_ask/") ||
        name.starts_with("M2/quantity_at/hit/") || name.starts_with("M2/quantity_at/miss/")) {
        std::size_t depth = 0;
        std::size_t limit = 0;
        if (name.starts_with("M2/top_levels/")) {
            const auto parts = name.substr(std::string_view{"M2/top_levels/"}.size());
            limit = static_cast<std::size_t>(
                std::strtoull(parts.substr(0, parts.find('/')).c_str(), nullptr, 10));
            depth = static_cast<std::size_t>(
                std::strtoull(parts.substr(parts.find('/') + 1).c_str(), nullptr, 10));
        } else {
            const auto slash = name.rfind('/');
            depth = static_cast<std::size_t>(
                std::strtoull(name.substr(slash + 1).c_str(), nullptr, 10));
        }
        bm::M2QueryCell cell{{depth, limit}};
        cell.prepare();
        if (name.starts_with("M2/best_bid/")) {
            static_cast<void>(cell.best(core::BookSide::Bid));
        } else if (name.starts_with("M2/best_ask/")) {
            static_cast<void>(cell.best(core::BookSide::Ask));
        } else if (name.starts_with("M2/quantity_at/hit/")) {
            static_cast<void>(cell.quantity_at_hit());
        } else if (name.starts_with("M2/quantity_at/miss/")) {
            static_cast<void>(cell.quantity_at_miss());
        } else if (name.starts_with("M2/top_levels/")) {
            static_cast<void>(cell.top_levels(core::BookSide::Bid));
        } else {
            static_cast<void>(cell.all_levels(core::BookSide::Bid));
        }
        return;
    }
    if (name.starts_with("M3/LiveApply/Accepted/")) {
        const auto parts = name.substr(std::string_view{"M3/LiveApply/Accepted/"}.size());
        const auto policy = parts.starts_with("Spot") ? core::SequencePolicyKind::Spot
                                                      : core::SequencePolicyKind::UsdMPerpetual;
        const auto dpos = parts.find("/D");
        const auto bpos = parts.find("/B");
        const auto depth = static_cast<std::size_t>(
            std::strtoull(parts.substr(dpos + 2, bpos - dpos - 2).c_str(), nullptr, 10));
        const auto batch =
            static_cast<std::size_t>(std::strtoull(parts.substr(bpos + 2).c_str(), nullptr, 10));
        bm::M3AcceptedCell cell{{policy, depth, batch}};
        cell.prepare();
        const auto result = cell.execute_step(0);
        if (result.disposition != core::ApplyDisposition::Applied ||
            result.status_after != core::ProjectionStatus::Synchronized) {
            std::fprintf(stderr, "M3 accepted warmup disposition drift: %s\n", name.c_str());
            std::exit(2);
        }
        return;
    }
    if (name.starts_with("M3/Classification/")) {
        const auto parts = name.substr(std::string_view{"M3/Classification/"}.size());
        const auto kind_name = parts.substr(0, parts.find('/'));
        const auto policy = parts.ends_with("Spot") ? core::SequencePolicyKind::Spot
                                                    : core::SequencePolicyKind::UsdMPerpetual;
        const auto kind = kind_name == "Stale"       ? bm::M3ClassificationKind::Stale
                          : kind_name == "Duplicate" ? bm::M3ClassificationKind::Duplicate
                          : kind_name == "Gap"       ? bm::M3ClassificationKind::Gap
                          : kind_name == "Reset"     ? bm::M3ClassificationKind::Reset
                                                     : bm::M3ClassificationKind::BaselineInstall;
        bm::M3ClassificationCell cell{{kind, policy, bm::kM3ClassificationDepth}};
        cell.prepare();
        static_cast<void>(cell.execute_step(0));
        return;
    }
    if (name.starts_with("M3/Component/AllLevelsBothSides/")) {
        const auto depth = static_cast<std::size_t>(std::strtoull(
            name.substr(std::string_view{"M3/Component/AllLevelsBothSides/"}.size()).c_str(),
            nullptr, 10));
        bm::M3ProxyCells cells{depth};
        cells.prepare();
        static_cast<void>(cells.all_levels_both_sides());
        return;
    }
    if (name.starts_with("M3/Proxy/CandidateRebuildFromVectors/")) {
        const auto depth = static_cast<std::size_t>(std::strtoull(
            name.substr(std::string_view{"M3/Proxy/CandidateRebuildFromVectors/"}.size()).c_str(),
            nullptr, 10));
        bm::M3ProxyCells cells{depth};
        cells.prepare();
        static_cast<void>(cells.candidate_rebuild_from_vectors());
        return;
    }
    if (name.starts_with("M3/Proxy/CandidateApplyUpdates/")) {
        const auto depth = static_cast<std::size_t>(std::strtoull(
            name.substr(std::string_view{"M3/Proxy/CandidateApplyUpdates/"}.size()).c_str(),
            nullptr, 10));
        bm::M3ProxyCells cells{depth};
        cells.prepare();
        auto candidate = cells.candidate_rebuild_from_vectors();
        cells.candidate_apply_updates(candidate, 0);
        return;
    }
    if (name.starts_with("M3/Proxy/OrderBookMoveCommit/")) {
        const auto depth = static_cast<std::size_t>(std::strtoull(
            name.substr(std::string_view{"M3/Proxy/OrderBookMoveCommit/"}.size()).c_str(), nullptr,
            10));
        auto destination = bm::build_order_book(depth);
        auto source = bm::build_order_book(depth);
        bm::M3ProxyCells::move_commit(destination, std::move(source));
        return;
    }
    std::fprintf(stderr, "unknown Phase-7 M2/M3 cell: %s\n", name.c_str());
    std::exit(2);
}

// ---------------------------------------------------------------------------
// Measured cells.
// ---------------------------------------------------------------------------
[[nodiscard]] std::string measure_m2_apply_level(bm::Phase7Harness& harness,
                                                 const std::string& name, const std::string& family,
                                                 std::size_t depth) {
    const auto kind = family == "insert"   ? bm::M2ApplyLevelKind::Insert
                      : family == "update" ? bm::M2ApplyLevelKind::Update
                      : family == "delete" ? bm::M2ApplyLevelKind::Delete
                                           : bm::M2ApplyLevelKind::MissingDelete;
    const auto expected = kind == bm::M2ApplyLevelKind::Insert   ? core::LevelChange::Inserted
                          : kind == bm::M2ApplyLevelKind::Update ? core::LevelChange::Updated
                          : kind == bm::M2ApplyLevelKind::Delete ? core::LevelChange::Removed
                                                                 : core::LevelChange::Unchanged;
    bm::M2ApplyLevelCell cell{kind, depth};
    cell.prepare();
    core::LevelChange change{core::LevelChange::Unchanged};
    std::uint64_t accumulator = 0;
    std::size_t rep = 0;
    const auto outcome = harness.measure_cell([&] {
        change = cell.execute_step(rep);
        accumulator += static_cast<std::uint64_t>(static_cast<std::uint8_t>(change));
        ++rep;
    });
    auto input = make_record_input(harness, name, "apply_level", outcome);
    if (change != expected || !outcome.operation_ok) {
        input.determinism_confirmed = false;
        input.baseline_snapshot_b = "bracket close (operation returned)";
        std::fprintf(stderr, "%s\n",
                     disposition_failure(name, change == expected ? "operation aborted"
                                                                  : "disposition drift")
                         .c_str());
    }
    std::printf("[phase7] %s allocs=%llu bytes=%llu deallocs=%llu accum=%llu\n", name.c_str(),
                static_cast<unsigned long long>(outcome.measurement.allocation_count),
                static_cast<unsigned long long>(outcome.measurement.total_allocated_bytes),
                static_cast<unsigned long long>(outcome.measurement.deallocation_count),
                static_cast<unsigned long long>(accumulator));
    return bm::build_allocation_record_json(input);
}

[[nodiscard]] std::string measure_m2_apply_updates(bm::Phase7Harness& harness,
                                                   const std::string& name, std::size_t depth,
                                                   std::size_t batch) {
    bm::M2ApplyUpdatesCell cell{{depth, batch, bm::M2ApplyUpdatesMix::ReplacementHeavy}};
    cell.prepare();
    std::uint64_t accumulator = 0;
    const auto outcome = harness.measure_cell([&] {
        cell.execute_step(0);
        accumulator += cell.book().level_count(core::BookSide::Bid);
    });
    auto input = make_record_input(harness, name, "apply_updates", outcome);
    input.determinism_confirmed = input.determinism_confirmed && outcome.operation_ok;
    std::printf("[phase7] %s allocs=%llu bytes=%llu deallocs=%llu accum=%llu\n", name.c_str(),
                static_cast<unsigned long long>(outcome.measurement.allocation_count),
                static_cast<unsigned long long>(outcome.measurement.total_allocated_bytes),
                static_cast<unsigned long long>(outcome.measurement.deallocation_count),
                static_cast<unsigned long long>(accumulator));
    return bm::build_allocation_record_json(input);
}

[[nodiscard]] std::string measure_m2_replace_all(bm::Phase7Harness& harness,
                                                 const std::string& name, std::size_t depth) {
    bm::M2ReplaceAllCell cell{depth};
    cell.prepare();
    std::uint64_t accumulator = 0;
    const auto outcome = harness.measure_cell([&] {
        cell.execute_step();
        accumulator += cell.book().level_count(core::BookSide::Bid);
        accumulator += cell.book().level_count(core::BookSide::Ask);
    });
    auto input = make_record_input(harness, name, "replace_all", outcome);
    input.determinism_confirmed = input.determinism_confirmed && outcome.operation_ok;
    std::printf("[phase7] %s allocs=%llu bytes=%llu deallocs=%llu accum=%llu\n", name.c_str(),
                static_cast<unsigned long long>(outcome.measurement.allocation_count),
                static_cast<unsigned long long>(outcome.measurement.total_allocated_bytes),
                static_cast<unsigned long long>(outcome.measurement.deallocation_count),
                static_cast<unsigned long long>(accumulator));
    return bm::build_allocation_record_json(input);
}

[[nodiscard]] std::string measure_m2_query(bm::Phase7Harness& harness, const std::string& name,
                                           std::string_view operation, std::size_t depth,
                                           std::size_t limit) {
    bm::M2QueryCell cell{{depth, limit}};
    cell.prepare();
    std::uint64_t accumulator = 0;
    if (operation == "best_bid" || operation == "best_ask" || operation == "quantity_at/hit" ||
        operation == "quantity_at/miss") {
        const auto outcome = harness.measure_cell([&] {
            std::optional<core::BookLevel> best;
            std::optional<core::QuantityUnits> quantity;
            if (operation == "best_bid" || operation == "best_ask") {
                best =
                    cell.best(operation == "best_bid" ? core::BookSide::Bid : core::BookSide::Ask);
                accumulator +=
                    best.has_value() ? static_cast<std::uint64_t>(best->price.value()) : 0xDEADULL;
            } else if (operation == "quantity_at/hit") {
                quantity = cell.quantity_at_hit();
                accumulator += quantity.has_value() ? static_cast<std::uint64_t>(quantity->value())
                                                    : 0xDEADULL;
            } else {
                quantity = cell.quantity_at_miss();
                accumulator += quantity.has_value() ? static_cast<std::uint64_t>(quantity->value())
                                                    : 0xDEADULL;
            }
        });
        auto input = make_record_input(harness, name, std::string{operation}, outcome);
        input.determinism_confirmed = input.determinism_confirmed && outcome.operation_ok;
        if (outcome.measurement.allocation_count != 0 ||
            outcome.measurement.total_allocated_bytes != 0) {
            input.determinism_confirmed = false;
            std::fprintf(stderr, "%s zero-allocation control reported traffic\n", name.c_str());
        }
        std::printf("[phase7] %s allocs=%llu bytes=%llu accum=%llu\n", name.c_str(),
                    static_cast<unsigned long long>(outcome.measurement.allocation_count),
                    static_cast<unsigned long long>(outcome.measurement.total_allocated_bytes),
                    static_cast<unsigned long long>(accumulator));
        return bm::build_allocation_record_json(input);
    }
    // Owning-output cells (all_levels / top_levels): B with the owner alive;
    // destroy outside the bracket; D recorded.
    std::vector<core::BookLevel> owned_bids;
    std::vector<core::BookLevel> owned_asks;
    const auto outcome = harness.measure_cell(
        [&] {
            if (operation == "top_levels") {
                owned_bids = cell.top_levels(core::BookSide::Bid);
                accumulator += owned_bids.size();
                if (!owned_bids.empty()) {
                    accumulator += static_cast<std::uint64_t>(owned_bids.front().price.value());
                }
            } else {
                owned_bids = cell.all_levels(core::BookSide::Bid);
                owned_asks = cell.all_levels(core::BookSide::Ask);
                accumulator += owned_bids.size() + owned_asks.size();
            }
        },
        [&] {
            std::vector<core::BookLevel>{}.swap(owned_bids);
            std::vector<core::BookLevel>{}.swap(owned_asks);
        });
    auto input = make_record_input(harness, name, std::string{operation}, outcome);
    input.determinism_confirmed = input.determinism_confirmed && outcome.operation_ok;
    std::printf("[phase7] %s allocs=%llu bytes=%llu deallocs=%llu accum=%llu D=%llu\n",
                name.c_str(), static_cast<unsigned long long>(outcome.measurement.allocation_count),
                static_cast<unsigned long long>(outcome.measurement.total_allocated_bytes),
                static_cast<unsigned long long>(outcome.measurement.deallocation_count),
                static_cast<unsigned long long>(accumulator),
                static_cast<unsigned long long>(outcome.post_destroy_live_bytes));
    return bm::build_allocation_record_json(input);
}

[[nodiscard]] std::string measure_m3_accepted(bm::Phase7Harness& harness, const std::string& name,
                                              core::SequencePolicyKind policy, std::size_t depth,
                                              std::size_t batch) {
    bm::M3AcceptedCell cell{{policy, depth, batch}};
    cell.prepare();
    core::ApplyDisposition disposition{core::ApplyDisposition::RejectedWrongState};
    core::ProjectionStatus status{core::ProjectionStatus::AwaitingBaseline};
    std::uint64_t accumulator = 0;
    std::size_t rep = 0;
    const auto outcome = harness.measure_cell([&] {
        const auto result = cell.execute_step(rep);
        disposition = result.disposition;
        status = result.status_after;
        accumulator += static_cast<std::uint64_t>(static_cast<std::uint8_t>(result.disposition));
        accumulator += cell.current_update_id().value();
        ++rep;
    });
    auto input = make_record_input(harness, name, "BookProjection::apply", outcome);
    if (disposition != core::ApplyDisposition::Applied ||
        status != core::ProjectionStatus::Synchronized || !outcome.operation_ok) {
        input.determinism_confirmed = false;
        std::fprintf(stderr, "%s\n",
                     disposition_failure(name, "expected Applied/Synchronized").c_str());
    }
    std::printf("[phase7] %s allocs=%llu bytes=%llu deallocs=%llu accum=%llu\n", name.c_str(),
                static_cast<unsigned long long>(outcome.measurement.allocation_count),
                static_cast<unsigned long long>(outcome.measurement.total_allocated_bytes),
                static_cast<unsigned long long>(outcome.measurement.deallocation_count),
                static_cast<unsigned long long>(accumulator));
    return bm::build_allocation_record_json(input);
}

[[nodiscard]] std::string measure_m3_classification(bm::Phase7Harness& harness,
                                                    const std::string& name,
                                                    bm::M3ClassificationKind kind,
                                                    core::SequencePolicyKind policy) {
    bm::M3ClassificationCell cell{{kind, policy, bm::kM3ClassificationDepth}};
    cell.prepare();
    bm::M3ClassificationResult result;
    std::uint64_t accumulator = 0;
    std::size_t rep = 0;
    const auto outcome = harness.measure_cell([&] {
        result = cell.execute_step(rep);
        accumulator += static_cast<std::uint64_t>(static_cast<std::uint8_t>(result.status_after));
        ++rep;
    });
    bool valid = false;
    std::string denominator = "classification";
    switch (kind) {
    case bm::M3ClassificationKind::Stale:
        valid = result.apply_disposition == core::ApplyDisposition::IgnoredStale;
        denominator = "BookProjection::apply";
        break;
    case bm::M3ClassificationKind::Duplicate:
        valid = result.apply_disposition == core::ApplyDisposition::IgnoredDuplicate;
        denominator = "BookProjection::apply";
        break;
    case bm::M3ClassificationKind::Gap:
        valid = result.apply_disposition == core::ApplyDisposition::GapDetected;
        denominator = "BookProjection::apply";
        break;
    case bm::M3ClassificationKind::Reset:
        valid = result.status_after == core::ProjectionStatus::AwaitingBaseline;
        denominator = "BookProjection::reset";
        break;
    case bm::M3ClassificationKind::BaselineInstall:
        valid = result.install_disposition == core::InstallDisposition::Installed &&
                result.status_after == core::ProjectionStatus::AwaitingBridge;
        denominator = "BookProjection::install_baseline";
        break;
    }
    auto input = make_record_input(harness, name, denominator, outcome);
    input.determinism_confirmed = input.determinism_confirmed && valid && outcome.operation_ok;
    if (kind == bm::M3ClassificationKind::Stale || kind == bm::M3ClassificationKind::Duplicate ||
        kind == bm::M3ClassificationKind::Gap) {
        if (outcome.measurement.allocation_count != 0 ||
            outcome.measurement.total_allocated_bytes != 0) {
            input.determinism_confirmed = false;
            std::fprintf(stderr, "%s zero-allocation control reported traffic\n", name.c_str());
        }
    } else if (kind == bm::M3ClassificationKind::Reset) {
        if (outcome.measurement.allocation_count != 0) {
            input.determinism_confirmed = false;
            std::fprintf(stderr, "%s reset cell must be deallocation-only\n", name.c_str());
        }
    }
    if (!valid) {
        std::fprintf(stderr, "%s\n",
                     disposition_failure(name, "classification disposition").c_str());
    }
    std::printf("[phase7] %s allocs=%llu bytes=%llu deallocs=%llu accum=%llu\n", name.c_str(),
                static_cast<unsigned long long>(outcome.measurement.allocation_count),
                static_cast<unsigned long long>(outcome.measurement.total_allocated_bytes),
                static_cast<unsigned long long>(outcome.measurement.deallocation_count),
                static_cast<unsigned long long>(accumulator));
    return bm::build_allocation_record_json(input);
}

[[nodiscard]] std::string measure_m3_component_all_levels(bm::Phase7Harness& harness,
                                                          const std::string& name,
                                                          std::size_t depth) {
    bm::M3ProxyCells cells{depth};
    cells.prepare();
    std::vector<core::BookLevel> owned;
    std::uint64_t accumulator = 0;
    const auto outcome = harness.measure_cell(
        [&] {
            owned = cells.all_levels_both_sides();
            accumulator += owned.size();
        },
        [&] { std::vector<core::BookLevel>{}.swap(owned); });
    auto input = make_record_input(harness, name, "all_levels_both_sides", outcome);
    input.determinism_confirmed = input.determinism_confirmed && outcome.operation_ok;
    std::printf("[phase7] %s allocs=%llu bytes=%llu accum=%llu\n", name.c_str(),
                static_cast<unsigned long long>(outcome.measurement.allocation_count),
                static_cast<unsigned long long>(outcome.measurement.total_allocated_bytes),
                static_cast<unsigned long long>(accumulator));
    return bm::build_allocation_record_json(input);
}

[[nodiscard]] std::string measure_m3_proxy_rebuild(bm::Phase7Harness& harness,
                                                   const std::string& name, std::size_t depth) {
    bm::M3ProxyCells cells{depth};
    cells.prepare();
    core::OrderBook owned{bm::benchmark_numeric_spec()};
    std::uint64_t accumulator = 0;
    const auto outcome = harness.measure_cell(
        [&] {
            owned = cells.candidate_rebuild_from_vectors();
            accumulator += owned.level_count(core::BookSide::Bid);
        },
        [&] { owned = core::OrderBook{bm::benchmark_numeric_spec()}; });
    auto input = make_record_input(harness, name, "replace_all", outcome);
    input.determinism_confirmed = input.determinism_confirmed && outcome.operation_ok;
    std::printf("[phase7] %s allocs=%llu bytes=%llu accum=%llu\n", name.c_str(),
                static_cast<unsigned long long>(outcome.measurement.allocation_count),
                static_cast<unsigned long long>(outcome.measurement.total_allocated_bytes),
                static_cast<unsigned long long>(accumulator));
    return bm::build_allocation_record_json(input);
}

[[nodiscard]] std::string measure_m3_proxy_apply_updates(bm::Phase7Harness& harness,
                                                         const std::string& name,
                                                         std::size_t depth) {
    bm::M3ProxyCells cells{depth};
    cells.prepare();
    const auto pool_size = bm::pool_iteration_count(depth);
    bm::StatePool<core::OrderBook> candidate_pool;
    candidate_pool.fill(pool_size, [&cells] { return cells.candidate_rebuild_from_vectors(); });
    std::uint64_t accumulator = 0;
    std::size_t rep = 0;
    const auto outcome = harness.measure_cell([&] {
        auto& candidate = candidate_pool.at(rep);
        cells.candidate_apply_updates(candidate, rep);
        accumulator += candidate.level_count(core::BookSide::Bid);
        ++rep;
    });
    auto input = make_record_input(harness, name, "apply_updates", outcome);
    input.determinism_confirmed = input.determinism_confirmed && outcome.operation_ok;
    std::printf("[phase7] %s allocs=%llu bytes=%llu accum=%llu\n", name.c_str(),
                static_cast<unsigned long long>(outcome.measurement.allocation_count),
                static_cast<unsigned long long>(outcome.measurement.total_allocated_bytes),
                static_cast<unsigned long long>(accumulator));
    return bm::build_allocation_record_json(input);
}

[[nodiscard]] std::string measure_m3_proxy_move_commit(bm::Phase7Harness& harness,
                                                       const std::string& name, std::size_t depth) {
    const auto pool_size = bm::pool_iteration_count(depth);
    bm::StatePool<core::OrderBook> destination_pool;
    bm::StatePool<core::OrderBook> source_pool;
    {
        std::vector<core::BookLevel> old_bids;
        std::vector<core::BookLevel> old_asks;
        old_bids.reserve(depth);
        old_asks.reserve(depth);
        for (std::size_t index = 0; index < depth; ++index) {
            old_bids.push_back({bm::price_units(30'000 - static_cast<std::int64_t>(index)),
                                bm::quantity_units(1)});
            old_asks.push_back({bm::price_units(30'001 + static_cast<std::int64_t>(index)),
                                bm::quantity_units(1)});
        }
        destination_pool.fill(pool_size, [&] {
            core::OrderBook book{bm::benchmark_numeric_spec()};
            book.replace_all(old_bids, old_asks);
            return book;
        });
        source_pool.fill(pool_size, [depth] { return bm::build_order_book(depth); });
    }
    std::uint64_t accumulator = 0;
    std::size_t rep = 0;
    const auto outcome = harness.measure_cell([&] {
        bm::M3ProxyCells::move_commit(destination_pool.at(rep), std::move(source_pool.at(rep)));
        accumulator += destination_pool.at(rep).level_count(core::BookSide::Bid);
        ++rep;
    });
    auto input = make_record_input(harness, name, "move_assignment_with_destruction", outcome);
    input.determinism_confirmed = input.determinism_confirmed && outcome.operation_ok;
    std::printf("[phase7] %s allocs=%llu bytes=%llu deallocs=%llu accum=%llu\n", name.c_str(),
                static_cast<unsigned long long>(outcome.measurement.allocation_count),
                static_cast<unsigned long long>(outcome.measurement.total_allocated_bytes),
                static_cast<unsigned long long>(outcome.measurement.deallocation_count),
                static_cast<unsigned long long>(accumulator));
    return bm::build_allocation_record_json(input);
}

// ---------------------------------------------------------------------------
// Entry point.
// ---------------------------------------------------------------------------
int run_phase7_m2_m3(int argc, char** argv) {
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

    const auto inventory = build_inventory();
    std::printf("[phase7] M2/M3 warmup pass over %zu cells\n", inventory.size());
    for (const auto& name : inventory) {
        warmup_step(name);
    }

    bm::CalibrationRecordInput calibration;
    calibration.calibration_id = std::string{kCalibrationId};
    calibration.evidence_class = options.evidence_class;
    calibration.description = "empty measurement bracket (instrumentation bookkeeping baseline; "
                              "reported separately and never subtracted)";
    calibration.measurement = harness.measure_calibration();
    harness.add_calibration_record(bm::build_calibration_record_json(calibration));

    std::size_t measured = 0;
    bool run_failed = false;
    std::vector<std::string> measured_names;
    for (const auto& name : inventory) {
        if (!options.filter.empty() && name.find(options.filter) == std::string::npos) {
            continue;
        }
        std::string record;
        if (name.starts_with("M2/apply_level/")) {
            const auto parts = name.substr(std::string_view{"M2/apply_level/"}.size());
            const auto family = parts.substr(0, parts.find('/'));
            const auto depth = static_cast<std::size_t>(
                std::strtoull(parts.substr(parts.find('/') + 1).c_str(), nullptr, 10));
            record = measure_m2_apply_level(harness, name, family, depth);
        } else if (name.starts_with("M2/apply_updates/")) {
            const auto parts = name.substr(std::string_view{"M2/apply_updates/"}.size());
            const auto batch = static_cast<std::size_t>(
                std::strtoull(parts.substr(0, parts.find('/')).c_str(), nullptr, 10));
            const auto depth = static_cast<std::size_t>(
                std::strtoull(parts.substr(parts.find('/') + 1).c_str(), nullptr, 10));
            record = measure_m2_apply_updates(harness, name, depth, batch);
        } else if (name.starts_with("M2/replace_all/")) {
            const auto depth = static_cast<std::size_t>(std::strtoull(
                name.substr(std::string_view{"M2/replace_all/"}.size()).c_str(), nullptr, 10));
            record = measure_m2_replace_all(harness, name, depth);
        } else if (name.starts_with("M2/all_levels/")) {
            const auto depth = static_cast<std::size_t>(std::strtoull(
                name.substr(std::string_view{"M2/all_levels/"}.size()).c_str(), nullptr, 10));
            record = measure_m2_query(harness, name, "all_levels", depth, 0);
        } else if (name.starts_with("M2/top_levels/")) {
            const auto parts = name.substr(std::string_view{"M2/top_levels/"}.size());
            const auto limit = static_cast<std::size_t>(
                std::strtoull(parts.substr(0, parts.find('/')).c_str(), nullptr, 10));
            const auto depth = static_cast<std::size_t>(
                std::strtoull(parts.substr(parts.find('/') + 1).c_str(), nullptr, 10));
            record = measure_m2_query(harness, name, "top_levels", depth, limit);
        } else if (name.starts_with("M2/best_bid/") || name.starts_with("M2/best_ask/") ||
                   name.starts_with("M2/quantity_at/hit/") ||
                   name.starts_with("M2/quantity_at/miss/")) {
            const auto slash = name.rfind('/');
            const auto depth = static_cast<std::size_t>(
                std::strtoull(name.substr(slash + 1).c_str(), nullptr, 10));
            const auto op_slash = name.find('/');
            const auto operation = name.substr(op_slash + 1, slash - op_slash - 1);
            record = measure_m2_query(harness, name, operation, depth, 0);
        } else if (name.starts_with("M3/LiveApply/Accepted/")) {
            const auto parts = name.substr(std::string_view{"M3/LiveApply/Accepted/"}.size());
            const auto policy = parts.starts_with("Spot") ? core::SequencePolicyKind::Spot
                                                          : core::SequencePolicyKind::UsdMPerpetual;
            const auto dpos = parts.find("/D");
            const auto bpos = parts.find("/B");
            const auto depth = static_cast<std::size_t>(
                std::strtoull(parts.substr(dpos + 2, bpos - dpos - 2).c_str(), nullptr, 10));
            const auto batch = static_cast<std::size_t>(
                std::strtoull(parts.substr(bpos + 2).c_str(), nullptr, 10));
            record = measure_m3_accepted(harness, name, policy, depth, batch);
        } else if (name.starts_with("M3/Classification/")) {
            const auto parts = name.substr(std::string_view{"M3/Classification/"}.size());
            const auto kind_name = parts.substr(0, parts.find('/'));
            const auto policy = parts.ends_with("Spot") ? core::SequencePolicyKind::Spot
                                                        : core::SequencePolicyKind::UsdMPerpetual;
            const auto kind = kind_name == "Stale"       ? bm::M3ClassificationKind::Stale
                              : kind_name == "Duplicate" ? bm::M3ClassificationKind::Duplicate
                              : kind_name == "Gap"       ? bm::M3ClassificationKind::Gap
                              : kind_name == "Reset"     ? bm::M3ClassificationKind::Reset
                                                     : bm::M3ClassificationKind::BaselineInstall;
            record = measure_m3_classification(harness, name, kind, policy);
        } else if (name.starts_with("M3/Component/AllLevelsBothSides/")) {
            const auto depth = static_cast<std::size_t>(std::strtoull(
                name.substr(std::string_view{"M3/Component/AllLevelsBothSides/"}.size()).c_str(),
                nullptr, 10));
            record = measure_m3_component_all_levels(harness, name, depth);
        } else if (name.starts_with("M3/Proxy/CandidateRebuildFromVectors/")) {
            const auto depth = static_cast<std::size_t>(std::strtoull(
                name.substr(std::string_view{"M3/Proxy/CandidateRebuildFromVectors/"}.size())
                    .c_str(),
                nullptr, 10));
            record = measure_m3_proxy_rebuild(harness, name, depth);
        } else if (name.starts_with("M3/Proxy/CandidateApplyUpdates/")) {
            const auto depth = static_cast<std::size_t>(std::strtoull(
                name.substr(std::string_view{"M3/Proxy/CandidateApplyUpdates/"}.size()).c_str(),
                nullptr, 10));
            record = measure_m3_proxy_apply_updates(harness, name, depth);
        } else if (name.starts_with("M3/Proxy/OrderBookMoveCommit/")) {
            const auto depth = static_cast<std::size_t>(std::strtoull(
                name.substr(std::string_view{"M3/Proxy/OrderBookMoveCommit/"}.size()).c_str(),
                nullptr, 10));
            record = measure_m3_proxy_move_commit(harness, name, depth);
        } else {
            std::fprintf(stderr, "unknown Phase-7 M2/M3 cell: %s\n", name.c_str());
            return 2;
        }
        harness.add_record(record);
        measured_names.push_back(name);
        ++measured;
        // Fail closed: a record whose determinism/control contract failed
        // makes the whole run invalid (the record itself carries the
        // machine-readable failure).
        const auto marker = std::string{"\"determinism_confirmed\":false"};
        if (record.find(marker) != std::string::npos) {
            run_failed = true;
        }
    }

    if (options.filter.empty()) {
        if (measured_names != inventory) {
            std::fprintf(stderr,
                         "Phase-7 M2/M3 inventory completeness failure: measured %zu of %zu\n",
                         measured, inventory.size());
            return 2;
        }
        std::size_t accepted_cells = 0;
        for (const auto& name : measured_names) {
            if (name.starts_with("M3/LiveApply/Accepted/")) {
                ++accepted_cells;
            }
        }
        if (accepted_cells != 48) {
            std::fprintf(stderr, "expected 48 M3 accepted cells, measured %zu\n", accepted_cells);
            return 2;
        }
    }

    if (!harness.emit()) {
        return 1;
    }
    std::printf("[phase7] M2/M3 allocation characterization complete: %zu cells\n", measured);
    return run_failed ? 1 : 0;
}

} // namespace

int main(int argc, char** argv) { return run_phase7_m2_m3(argc, argv); }
