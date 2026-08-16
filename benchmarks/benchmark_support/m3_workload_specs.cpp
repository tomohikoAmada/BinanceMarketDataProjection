#include "m3_workload_specs.hpp"

#include "m3_cells.hpp"
#include "workload_spec.hpp"

#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>

#include <cstddef>
#include <string>
#include <string_view>

namespace bmd_projection::m5::benchmark {
namespace {

namespace core = binance_market_data::projection::v1;

constexpr std::size_t kDepthSet[] = {0, 8, 100, 1'000, 5'000, 10'000};
constexpr std::size_t kBatchSet[] = {0, 1, 10, 100};
constexpr std::size_t kComponentDepthSet[] = {8, 100, 1'000};

[[nodiscard]] std::string_view policy_label(core::SequencePolicyKind policy) noexcept {
    return policy == core::SequencePolicyKind::Spot ? "Spot" : "UsdMPerpetual";
}

[[nodiscard]] std::string_view kind_label(M3ClassificationKind kind) noexcept {
    switch (kind) {
    case M3ClassificationKind::Stale:
        return "Stale";
    case M3ClassificationKind::Duplicate:
        return "Duplicate";
    case M3ClassificationKind::Gap:
        return "Gap";
    case M3ClassificationKind::Reset:
        return "Reset";
    case M3ClassificationKind::BaselineInstall:
        return "BaselineInstall";
    }
    return "Unknown";
}

} // namespace

void register_m3_workload_specs() {
    for (const auto policy :
         {core::SequencePolicyKind::Spot, core::SequencePolicyKind::UsdMPerpetual}) {
        for (const auto depth : kDepthSet) {
            for (const auto batch : kBatchSet) {
                const auto name = "M3/LiveApply/Accepted/" + std::string{policy_label(policy)} +
                                  "/D" + std::to_string(depth) + "/B" + std::to_string(batch);
                auto& builder = register_workload(name);
                builder.set("benchmark_name", name);
                builder.set("operation", "BookProjection::apply");
                builder.set("policy", policy_label(policy));
                builder.set("depth_per_side", depth);
                builder.set("batch", batch);
                builder.set("expected_disposition", "Applied");
                builder.set("precondition", "Synchronized");
                if (depth == 0 && batch > 0) {
                    builder.set("edge", "empty_book_insertion");
                }
                if (batch == 0) {
                    builder.set("edge", "advancing_empty_level_batch");
                }
                builder.set("generator_schema", "M5_PHASE6_M3_CELLS_V1");
                builder.set("generated_workload_sha256",
                            m3_accepted_generated_sha256({policy, depth, batch}));
                builder.set("primary_timer", "cpu");
                builder.set("primary_denominator", "cpu_time");
            }
        }
        for (const auto kind :
             {M3ClassificationKind::Stale, M3ClassificationKind::Duplicate,
              M3ClassificationKind::Gap, M3ClassificationKind::Reset,
              M3ClassificationKind::BaselineInstall}) {
            const auto name = "M3/Classification/" + std::string{kind_label(kind)} + "/" +
                              std::string{policy_label(policy)};
            auto& builder = register_workload(name);
            builder.set("benchmark_name", name);
            builder.set("operation", "classification");
            builder.set("policy", policy_label(policy));
            builder.set("classification", kind_label(kind));
            builder.set("depth_per_side", kM3ClassificationDepth);
            builder.set("generator_schema", "M5_PHASE6_M3_CELLS_V1");
            builder.set(
                "generated_workload_sha256",
                m3_classification_generated_sha256({kind, policy, kM3ClassificationDepth}));
            builder.set("primary_timer", "cpu");
            builder.set("primary_denominator", "cpu_time");
        }
    }
    // Component/proxy cells are policy-neutral book operations; Spot is the
    // recorded projection policy.
    for (const auto depth : kComponentDepthSet) {
        const auto register_proxy = [depth](std::string_view family, std::string_view operation) {
            const auto name = "M3/" + std::string{family} + "/" + std::to_string(depth);
            auto& builder = register_workload(name);
            builder.set("benchmark_name", name);
            builder.set("operation", operation);
            builder.set("depth_per_side", depth);
            builder.set("policy", "Spot");
            builder.set("proxy_component_measurement", "true");
            builder.set("generator_schema", "M5_PHASE6_M3_CELLS_V1");
            builder.set("generated_workload_sha256",
                        m3_proxy_generated_sha256(operation, depth));
            builder.set("primary_timer", "cpu");
            builder.set("primary_denominator", "cpu_time");
        };
        register_proxy("Component/AllLevelsBothSides", "all_levels_both_sides");
        register_proxy("Proxy/CandidateRebuildFromVectors", "replace_all");
        register_proxy("Proxy/CandidateApplyUpdates", "apply_updates");
        register_proxy("Proxy/OrderBookMoveCommit", "move_assignment_with_destruction");
    }
}

} // namespace bmd_projection::m5::benchmark
