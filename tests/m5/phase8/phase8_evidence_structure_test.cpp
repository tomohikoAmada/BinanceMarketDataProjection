#include "phase8_absl_btree_map.hpp"
#include "phase8_sorted_vector_batch_lww.hpp"
#include "phase8_sorted_vector_naive.hpp"
#include "phase8_std_map_control.hpp"
#include "phase8_workload.hpp"

#include "book_state.hpp"

#include <gtest/gtest.h>

#include <span>
#include <string>

namespace bmd_projection::m5::phase8::test {
namespace {

template <typename Model>
[[nodiscard]] std::string execute_and_digest(const Phase8Workload& workload) {
    Model model{bmd_projection::m5::benchmark::benchmark_numeric_spec()};
    model.replace_all(std::span{workload.bids}, std::span{workload.asks});
    switch (workload.operation) {
    case Phase8Operation::apply_level: {
        const auto& update = workload.updates.front();
        static_cast<void>(model.apply_level(update.side, update.price, update.quantity));
        break;
    }
    case Phase8Operation::apply_updates:
        model.apply_updates(std::span{workload.updates});
        break;
    case Phase8Operation::replace_all:
        model.replace_all(std::span{workload.bids}, std::span{workload.asks});
        break;
    case Phase8Operation::top_levels:
        static_cast<void>(model.top_levels(core::BookSide::Bid, workload.query_limit));
        break;
    }
    return phase8_digest(model);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(Phase8EvidenceStructure, RegistersExactlyTheApprovedCandidatesAndSharedWorkloads) {
    const auto& workloads = phase8_workloads();
    ASSERT_FALSE(workloads.empty());
    for (const auto& workload : workloads) {
        EXPECT_FALSE(workload.id.empty());
        EXPECT_EQ(workload.workload_spec_sha256.size(), 64U);
        EXPECT_EQ(workload.generated_workload_sha256.size(), 64U);
    }
    for (std::size_t index = 0; index < workloads.size(); ++index) {
        for (std::size_t other = index + 1; other < workloads.size(); ++other) {
            EXPECT_NE(workloads[index].id, workloads[other].id);
        }
    }
}

TEST(Phase8EvidenceStructure, AllCandidatesConsumeIdenticalLogicalInputAndDigest) {
    for (const auto& workload : phase8_workloads()) {
        const auto control = execute_and_digest<Phase8StdMapControl<>>(workload);
        EXPECT_EQ(control, execute_and_digest<Phase8SortedVectorNaive<>>(workload));
        EXPECT_EQ(control, execute_and_digest<Phase8AbslBtreeMap<>>(workload));
        EXPECT_EQ(control, execute_and_digest<Phase8SortedVectorBatchLww<>>(workload));
    }
}

} // namespace
} // namespace bmd_projection::m5::phase8::test
