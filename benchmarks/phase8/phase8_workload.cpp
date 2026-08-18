#include "phase8_workload.hpp"

#include "../benchmark_support/m2_cells.hpp"
#include "../benchmark_support/m2_workload_specs.hpp"
#include "../benchmark_support/replay_checksum.hpp"
#include "../benchmark_support/workload_spec.hpp"
#include "canonical_text.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace bmd_projection::m5::phase8 {
namespace {

namespace bm = bmd_projection::m5::benchmark;

[[nodiscard]] const std::pair<std::string, std::string>& spec_for(std::string_view id) {
    const auto& specs = bm::registered_workloads();
    const auto found = std::find_if(specs.begin(), specs.end(),
                                    [id](const auto& entry) { return entry.first == id; });
    if (found == specs.end()) {
        std::abort();
    }
    return *found;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
[[nodiscard]] std::string spec_field(std::string_view text, std::string_view key) {
    const std::string prefix = std::string{key} + '=';
    std::size_t offset = 0;
    while (offset < text.size()) {
        const auto end = text.find('\n', offset);
        const auto line_end = end == std::string_view::npos ? text.size() : end;
        if (text.substr(offset, line_end - offset).starts_with(prefix)) {
            return std::string{
                text.substr(offset + prefix.size(), line_end - offset - prefix.size())};
        }
        if (end == std::string_view::npos) {
            break;
        }
        offset = end + 1U;
    }
    return {};
}

[[nodiscard]] std::string sha256_text(std::string_view text) {
    const auto hash = bmd_projection::m5::replay::sha256_hex(text);
    if (!std::holds_alternative<std::string>(hash)) {
        std::abort();
    }
    return std::get<std::string>(hash);
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
[[nodiscard]] Phase8Workload standard_workload(std::string id, Phase8Operation operation,
                                               std::size_t depth, std::size_t batch = 0,
                                               std::size_t limit = 0) {
    const auto& spec = spec_for(id);
    Phase8Workload workload;
    workload.id = std::move(id);
    workload.workload_spec_text = spec.second;
    workload.workload_spec_sha256 = sha256_text(workload.workload_spec_text);
    workload.generated_workload_sha256 = spec_field(spec.second, "generated_workload_sha256");
    workload.generator_schema = spec_field(spec.second, "generator_schema");
    workload.operation = operation;
    workload.depth = depth;
    workload.batch = batch;
    workload.query_limit = limit;
    workload.bids = bm::build_bid_levels(depth);
    workload.asks = bm::build_ask_levels(depth);
    return workload;
}
// NOLINTEND(bugprone-easily-swappable-parameters)

void fill_operation_input(Phase8Workload& workload) {
    const bm::BookParams params{};
    if (workload.operation == Phase8Operation::apply_level) {
        if (workload.id.find("/insert/") != std::string::npos) {
            workload.updates.push_back(
                {core::BookSide::Bid,
                 bm::price_units(params.bid_start - static_cast<std::int64_t>(workload.depth) - 1),
                 bm::quantity_units(params.quantity_base + 1)});
        } else if (workload.id.find("/delete/") != std::string::npos) {
            workload.updates.push_back(
                {core::BookSide::Bid, bm::price_units(params.bid_start), bm::quantity_units(0)});
        } else {
            workload.updates.push_back({core::BookSide::Bid, bm::price_units(params.bid_start),
                                        bm::quantity_units(params.quantity_base + 1)});
        }
    }
    if (workload.operation == Phase8Operation::apply_updates &&
        workload.id.starts_with("M2/apply_updates/")) {
        bm::M2ApplyUpdatesCell cell{
            {workload.depth, workload.batch, bm::M2ApplyUpdatesMix::ReplacementHeavy}};
        cell.prepare();
        for (std::size_t index = 0; index < cell.prepared_batch_count(); ++index) {
            workload.update_batches.push_back(cell.prepared_batch(index));
        }
        if (!workload.update_batches.empty()) {
            workload.updates = workload.update_batches.front();
        }
    }
}

[[nodiscard]] Phase8Workload mixed_workload() {
    const bm::BookParams params{};
    constexpr std::size_t depth = 1'000;
    Phase8Workload workload;
    workload.id = "M5_PHASE8/mixed_updates/1000";
    workload.operation = Phase8Operation::apply_updates;
    workload.depth = depth;
    workload.batch = 6;
    workload.generator_schema = "M5_PHASE8_CANDIDATE_WORKLOAD_V1";
    workload.bids = bm::build_bid_levels(depth);
    workload.asks = bm::build_ask_levels(depth);
    workload.updates = {
        {core::BookSide::Bid, bm::price_units(params.bid_start),
         bm::quantity_units(params.quantity_base + 1)},
        {core::BookSide::Ask, bm::price_units(params.ask_start), bm::quantity_units(0)},
        {core::BookSide::Bid,
         bm::price_units(params.bid_start - static_cast<std::int64_t>(depth) - 1),
         bm::quantity_units(params.quantity_base + 2)},
        {core::BookSide::Bid, bm::price_units(params.bid_start),
         bm::quantity_units(params.quantity_base + 2)},
        {core::BookSide::Bid, bm::price_units(params.bid_start),
         bm::quantity_units(params.quantity_base + 3)},
        {core::BookSide::Ask,
         bm::price_units(params.ask_start + static_cast<std::int64_t>(depth) + 1),
         bm::quantity_units(params.quantity_base + 4)},
    };
    workload.update_batches.push_back(workload.updates);
    const std::string description = workload.id +
                                    "\ninitial_bids=" + bm::describe_levels(workload.bids) +
                                    "\ninitial_asks=" + bm::describe_levels(workload.asks) +
                                    "\nupdates=" + bm::describe_updates(workload.updates);
    workload.generated_workload_sha256 = sha256_text(description);

    auto& builder = bm::register_workload(workload.id);
    builder.set("benchmark_name", workload.id);
    builder.set("operation", "apply_updates");
    builder.set("operation_mix", "mixed_insert_update_delete_lww");
    builder.set("depth_per_side", depth);
    builder.set("batch", workload.batch);
    builder.set("generator_schema", workload.generator_schema);
    builder.set("generated_workload_sha256", workload.generated_workload_sha256);
    const auto& spec = spec_for(workload.id);
    workload.workload_spec_text = spec.second;
    workload.workload_spec_sha256 = sha256_text(workload.workload_spec_text);
    return workload;
}

} // namespace

const std::vector<Phase8Workload>& phase8_workloads() {
    static const std::vector<Phase8Workload> workloads = [] {
        bm::register_m2_workload_specs(false);
        std::vector<Phase8Workload> result;
        result.push_back(
            standard_workload("M2/apply_level/insert/8", Phase8Operation::apply_level, 8));
        result.push_back(
            standard_workload("M2/apply_level/update/8", Phase8Operation::apply_level, 8));
        result.push_back(
            standard_workload("M2/apply_level/delete/1000", Phase8Operation::apply_level, 1'000));
        result.push_back(
            standard_workload("M2/apply_updates/10/8", Phase8Operation::apply_updates, 8, 10));
        result.push_back(standard_workload("M2/apply_updates/100/1000",
                                           Phase8Operation::apply_updates, 1'000, 100));
        result.push_back(mixed_workload());
        result.push_back(
            standard_workload("M2/replace_all/100", Phase8Operation::replace_all, 100));
        result.push_back(
            standard_workload("M2/replace_all/1000", Phase8Operation::replace_all, 1'000));
        result.push_back(
            standard_workload("M2/top_levels/5/8", Phase8Operation::top_levels, 8, 0, 5));
        result.push_back(
            standard_workload("M2/top_levels/50/1000", Phase8Operation::top_levels, 1'000, 0, 50));
        for (auto& workload : result) {
            fill_operation_input(workload);
        }
        return result;
    }();
    return workloads;
}

std::string_view phase8_operation_name(Phase8Operation operation) noexcept {
    switch (operation) {
    case Phase8Operation::apply_level:
        return "apply_level";
    case Phase8Operation::apply_updates:
        return "apply_updates";
    case Phase8Operation::replace_all:
        return "replace_all";
    case Phase8Operation::top_levels:
        return "top_levels";
    }
    return "unknown";
}

} // namespace bmd_projection::m5::phase8
