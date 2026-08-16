#include "m2_workload_specs.hpp"

#include "m2_cells.hpp"
#include "workload_spec.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace bmd_projection::m5::benchmark {
namespace {

constexpr std::size_t kRoutineDepths[] = {8, 100, 1'000};
constexpr std::size_t kFullDepthSet[] = {0, 8, 100, 1'000, 5'000, 10'000};
constexpr std::size_t kBatchSet[] = {1, 10, 100};
constexpr std::size_t kTopNSet[] = {1, 5, 50};

[[nodiscard]] std::string depth_name(std::size_t depth) { return std::to_string(depth); }

// The helpers below preserve the exact Phase-6 canonical field content and
// registration order (previously benchmarks/m2_benchmarks.cpp Specs).
void register_apply_level(std::string_view family, std::size_t depth,
                          std::string_view expected) {
    const auto name = "M2/apply_level/" + std::string{family} + "/" + depth_name(depth);
    auto& builder = register_workload(name);
    builder.set("benchmark_name", name);
    builder.set("operation", "apply_level");
    builder.set("operation_kind", family);
    builder.set("depth_per_side", depth);
    builder.set("expected_disposition", expected);
    builder.set("generator_schema", "M5_PHASE6_M2_CELLS_V1");
    const auto kind = family == "insert"   ? M2ApplyLevelKind::Insert
                      : family == "update" ? M2ApplyLevelKind::Update
                      : family == "delete" ? M2ApplyLevelKind::Delete
                                           : M2ApplyLevelKind::MissingDelete;
    builder.set("generated_workload_sha256", m2_apply_level_generated_sha256(kind, depth));
    builder.set("primary_timer", "cpu");
    builder.set("primary_denominator", "cpu_time");
}

void register_apply_updates(std::size_t depth, std::size_t batch, std::string_view mix) {
    const auto name = "M2/apply_updates/" + std::to_string(batch) + "/" + depth_name(depth);
    auto& builder = register_workload(name);
    builder.set("benchmark_name", name);
    builder.set("operation", "apply_updates");
    builder.set("depth_per_side", depth);
    builder.set("batch", batch);
    builder.set("operation_mix", mix);
    builder.set("generator_schema", "M5_PHASE6_M2_CELLS_V1");
    builder.set("generated_workload_sha256",
                m2_apply_updates_generated_sha256(
                    {depth, batch, M2ApplyUpdatesMix::ReplacementHeavy}));
    builder.set("primary_timer", "cpu");
    builder.set("primary_denominator", "cpu_time");
}

void register_apply_updates_mix(std::size_t depth) {
    const auto name = "M2/apply_updates/update_mix/" + depth_name(depth);
    auto& builder = register_workload(name);
    builder.set("benchmark_name", name);
    builder.set("operation", "apply_updates");
    builder.set("depth_per_side", depth);
    builder.set("batch", 100);
    builder.set("operation_mix", depth == 0 ? "insert_empty_book_edge" : "replacement_heavy");
    builder.set("primary_scaling_workload", "true");
    builder.set("generator_schema", "M5_PHASE6_M2_CELLS_V1");
    builder.set("generated_workload_sha256",
                m2_apply_updates_generated_sha256(
                    {depth, 100,
                     depth == 0 ? M2ApplyUpdatesMix::Insertion
                                : M2ApplyUpdatesMix::ReplacementHeavy}));
    builder.set("primary_timer", "cpu");
    builder.set("primary_denominator", "cpu_time");
}

void register_replace_all(std::size_t depth) {
    const auto name = "M2/replace_all/" + depth_name(depth);
    auto& builder = register_workload(name);
    builder.set("benchmark_name", name);
    builder.set("operation", "replace_all");
    builder.set("depth_per_side", depth);
    builder.set("generator_schema", "M5_PHASE6_M2_CELLS_V1");
    builder.set("generated_workload_sha256", m2_replace_all_generated_sha256(depth));
    builder.set("primary_timer", "cpu");
    builder.set("primary_denominator", "cpu_time");
}

void register_query(std::string_view family, std::size_t depth, std::size_t limit) {
    const auto name = "M2/" + std::string{family} + "/" + depth_name(depth);
    auto& builder = register_workload(name);
    builder.set("benchmark_name", name);
    builder.set("operation", family);
    builder.set("depth_per_side", depth);
    if (limit > 0) {
        builder.set("query_limit", limit);
    }
    builder.set("generator_schema", "M5_PHASE6_M2_CELLS_V1");
    builder.set("generated_workload_sha256",
                m2_query_generated_sha256(family, {depth, limit}));
    builder.set("primary_timer", "cpu");
    builder.set("primary_denominator", "cpu_time");
}

} // namespace

void register_m2_workload_specs(bool include_update_mix) {
    for (const auto depth : kRoutineDepths) {
        register_apply_level("insert", depth, "Inserted");
        register_apply_level("update", depth, "Updated");
        register_apply_level("delete", depth, "Removed");
        register_apply_level("missing_delete", depth, "Unchanged");
        register_query("best_bid", depth, 0);
        register_query("best_ask", depth, 0);
        register_query("quantity_at/hit", depth, 0);
        register_query("quantity_at/miss", depth, 0);
        register_query("all_levels", depth, 0);
        for (const auto batch : kBatchSet) {
            register_apply_updates(depth, batch, "replace");
        }
    }
    for (const auto depth : kFullDepthSet) {
        if (include_update_mix) {
            register_apply_updates_mix(depth);
        }
        register_replace_all(depth);
        register_query("all_levels", depth, 0);
    }
    for (const auto limit : kTopNSet) {
        for (const auto depth : kRoutineDepths) {
            register_query("top_levels/" + std::to_string(limit), depth, limit);
        }
    }
}

} // namespace bmd_projection::m5::benchmark
