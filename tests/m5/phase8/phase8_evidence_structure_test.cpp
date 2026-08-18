#include "phase8_absl_btree_map.hpp"
#include "phase8_sorted_vector_batch_lww.hpp"
#include "phase8_sorted_vector_naive.hpp"
#include "phase8_std_map_control.hpp"
#include "phase8_workload.hpp"

#include "book_state.hpp"
#include "m2_cells.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <span>
#include <string>
#include <string_view>

namespace bmd_projection::m5::phase8::test {
namespace {

template <typename Model>
[[nodiscard]] std::string execute_and_digest(const Phase8Workload& workload) {
    Model model{bmd_projection::m5::benchmark::benchmark_numeric_spec()};
    model.replace_all(std::span{workload.initial_bids}, std::span{workload.initial_asks});
    switch (workload.operation) {
    case Phase8Operation::apply_level: {
        const auto& update = workload.updates.front();
        static_cast<void>(model.apply_level(update.side, update.price, update.quantity));
        break;
    }
    case Phase8Operation::apply_updates:
        for (const auto& updates : workload.update_batches) {
            model.apply_updates(std::span{updates});
        }
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

const Phase8Workload& workload_named(std::string_view id) {
    const auto& workloads = phase8_workloads();
    const auto found = std::find_if(workloads.begin(), workloads.end(),
                                    [id](const auto& workload) { return workload.id == id; });
    EXPECT_NE(found, workloads.end());
    return *found;
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

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(Phase8EvidenceStructure, Phase6IdentitiesBindToCanonicalPreparedInputs) {
    namespace bm = bmd_projection::m5::benchmark;

    const auto& apply_level = workload_named("M2/apply_level/update/8");
    bm::M2ApplyLevelCell update_cell{bm::M2ApplyLevelKind::Update, 8};
    update_cell.prepare();
    EXPECT_EQ(apply_level.generated_workload_sha256, update_cell.generated_workload_sha256());
    EXPECT_EQ(apply_level.initial_bids, update_cell.book().all_levels(core::BookSide::Bid));
    EXPECT_EQ(apply_level.initial_asks, update_cell.book().all_levels(core::BookSide::Ask));
    ASSERT_EQ(apply_level.updates.size(), update_cell.prepared_update_slot_count());
    for (std::size_t index = 0; index < apply_level.updates.size(); ++index) {
        EXPECT_EQ(apply_level.updates[index], update_cell.prepared_update_slots()[index]);
    }

    const auto& insert_level = workload_named("M2/apply_level/insert/8");
    bm::M2ApplyLevelCell insert_cell{bm::M2ApplyLevelKind::Insert, 8};
    insert_cell.prepare();
    EXPECT_EQ(insert_level.generated_workload_sha256, insert_cell.generated_workload_sha256());
    EXPECT_EQ(insert_level.initial_bids, insert_cell.book().all_levels(core::BookSide::Bid));
    EXPECT_EQ(insert_level.initial_asks, insert_cell.book().all_levels(core::BookSide::Ask));
    ASSERT_EQ(insert_level.updates.size(), 1U);
    EXPECT_EQ(insert_level.updates.front(), insert_cell.prepared_update());

    const auto& delete_level = workload_named("M2/apply_level/delete/1000");
    bm::M2ApplyLevelCell delete_cell{bm::M2ApplyLevelKind::Delete, 1'000};
    delete_cell.prepare();
    EXPECT_EQ(delete_level.generated_workload_sha256, delete_cell.generated_workload_sha256());
    EXPECT_EQ(delete_level.initial_bids, delete_cell.book().all_levels(core::BookSide::Bid));
    EXPECT_EQ(delete_level.initial_asks, delete_cell.book().all_levels(core::BookSide::Ask));
    ASSERT_EQ(delete_level.updates.size(), 1U);
    EXPECT_EQ(delete_level.updates.front(), delete_cell.prepared_update());

    const auto& small_apply_updates = workload_named("M2/apply_updates/10/8");
    bm::M2ApplyUpdatesCell small_updates_cell{{8, 10, bm::M2ApplyUpdatesMix::ReplacementHeavy}};
    small_updates_cell.prepare();
    EXPECT_EQ(small_apply_updates.generated_workload_sha256,
              small_updates_cell.generated_workload_sha256());
    EXPECT_EQ(small_apply_updates.initial_bids,
              small_updates_cell.book().all_levels(core::BookSide::Bid));
    EXPECT_EQ(small_apply_updates.initial_asks,
              small_updates_cell.book().all_levels(core::BookSide::Ask));
    ASSERT_EQ(small_apply_updates.update_batches.size(), small_updates_cell.prepared_batch_count());
    for (std::size_t index = 0; index < small_apply_updates.update_batches.size(); ++index) {
        EXPECT_EQ(small_apply_updates.update_batches[index],
                  small_updates_cell.prepared_batch(index));
    }

    const auto& apply_updates = workload_named("M2/apply_updates/100/1000");
    bm::M2ApplyUpdatesCell updates_cell{{1'000, 100, bm::M2ApplyUpdatesMix::ReplacementHeavy}};
    updates_cell.prepare();
    EXPECT_EQ(apply_updates.generated_workload_sha256, updates_cell.generated_workload_sha256());
    EXPECT_EQ(apply_updates.initial_bids, updates_cell.book().all_levels(core::BookSide::Bid));
    EXPECT_EQ(apply_updates.initial_asks, updates_cell.book().all_levels(core::BookSide::Ask));
    ASSERT_EQ(apply_updates.update_batches.size(), updates_cell.prepared_batch_count());
    for (std::size_t index = 0; index < apply_updates.update_batches.size(); ++index) {
        EXPECT_EQ(apply_updates.update_batches[index], updates_cell.prepared_batch(index));
    }

    const auto& replace_all = workload_named("M2/replace_all/100");
    bm::M2ReplaceAllCell replace_cell{100};
    replace_cell.prepare();
    EXPECT_EQ(replace_all.generated_workload_sha256, replace_cell.generated_workload_sha256());
    EXPECT_EQ(replace_all.initial_bids, replace_cell.book().all_levels(core::BookSide::Bid));
    EXPECT_EQ(replace_all.initial_asks, replace_cell.book().all_levels(core::BookSide::Ask));
    EXPECT_EQ(replace_all.bids, replace_cell.replacement_bids());
    EXPECT_EQ(replace_all.asks, replace_cell.replacement_asks());

    const auto& large_replace_all = workload_named("M2/replace_all/1000");
    bm::M2ReplaceAllCell large_replace_cell{1'000};
    large_replace_cell.prepare();
    EXPECT_EQ(large_replace_all.generated_workload_sha256,
              large_replace_cell.generated_workload_sha256());
    EXPECT_EQ(large_replace_all.initial_bids,
              large_replace_cell.book().all_levels(core::BookSide::Bid));
    EXPECT_EQ(large_replace_all.initial_asks,
              large_replace_cell.book().all_levels(core::BookSide::Ask));
    EXPECT_EQ(large_replace_all.bids, large_replace_cell.replacement_bids());
    EXPECT_EQ(large_replace_all.asks, large_replace_cell.replacement_asks());

    const auto& top_levels = workload_named("M2/top_levels/5/8");
    bm::M2QueryCell query_cell{{8, 5}};
    query_cell.prepare();
    EXPECT_EQ(top_levels.initial_bids, query_cell.book().all_levels(core::BookSide::Bid));
    EXPECT_EQ(top_levels.initial_asks, query_cell.book().all_levels(core::BookSide::Ask));
    EXPECT_EQ(top_levels.generated_workload_sha256,
              bm::m2_query_generated_sha256("top_levels/5", {8, 5}));

    const auto& deep_top_levels = workload_named("M2/top_levels/50/1000");
    bm::M2QueryCell deep_query_cell{{1'000, 50}};
    deep_query_cell.prepare();
    EXPECT_EQ(deep_top_levels.initial_bids, deep_query_cell.book().all_levels(core::BookSide::Bid));
    EXPECT_EQ(deep_top_levels.initial_asks, deep_query_cell.book().all_levels(core::BookSide::Ask));
    EXPECT_EQ(deep_top_levels.generated_workload_sha256,
              bm::m2_query_generated_sha256("top_levels/50", {1'000, 50}));
}

} // namespace
} // namespace bmd_projection::m5::phase8::test
