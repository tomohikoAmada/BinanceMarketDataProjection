#include "canonical_observation.hpp"
#include "semantic_digest.hpp"

#include "../oracle/operation_observation.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

namespace oracle = bmd_projection::m5::oracle;
namespace replay = bmd_projection::m5::replay;
namespace semantic = bmd_projection::m5::semantic;

oracle::OperationObservation make_obs(std::size_t index, replay::EventKind kind,
                                      std::int64_t price) {
    oracle::OperationObservation obs;
    obs.event_index = index;
    obs.event_kind = kind;
    oracle::ApplyOutcome apply;
    apply.disposition = oracle::CanonicalDisposition::Applied;
    apply.status_after = oracle::CanonicalStatus::Synchronized;
    apply.last_update_id_after = 100 + index;
    obs.result.value = apply;
    obs.checkpoint.status = oracle::CanonicalStatus::Synchronized;
    obs.checkpoint.last_update_id = 100 + index;
    obs.checkpoint.bids = {{price, 10}};
    obs.checkpoint.asks = {{price + 1, 5}};
    obs.checkpoint.price_scale = 2;
    obs.checkpoint.quantity_scale = 3;
    return obs;
}

TEST(SemanticDigestTest, SameStreamProducesSameDigest) {
    const std::vector<oracle::OperationObservation> obs = {
        make_obs(0, replay::EventKind::InstallBaseline, 100),
        make_obs(1, replay::EventKind::DepthUpdate, 101),
    };
    const auto d1 = semantic::compute_semantic_digest_from_observations(obs);
    const auto d2 = semantic::compute_semantic_digest_from_observations(obs);
    EXPECT_EQ(d1, d2);
    EXPECT_EQ(d1.size(), 64);
}

TEST(SemanticDigestTest, SerializerOwnsFinalLfAndDigestAddsNoSeparator) {
    const auto observation = make_obs(0, replay::EventKind::InstallBaseline, 100);
    const auto canonical = semantic::serialize_observation(observation);
    ASSERT_FALSE(canonical.empty());
    EXPECT_EQ(canonical.back(), '\n');
    ASSERT_GT(canonical.size(), 1U);
    EXPECT_NE(canonical[canonical.size() - 2], '\n');
    EXPECT_EQ(semantic::compute_semantic_digest({canonical}),
              "de3105ec85ba1cba8bdee4f9770464cda68e33fdbc51a846cdd3014e01e38242");
}

TEST(SemanticDigestTest, RejectsRecordsWithoutExactlyOneFinalLf) {
    EXPECT_TRUE(semantic::compute_semantic_digest({"OBS 0 RESET"}).empty());
    EXPECT_TRUE(semantic::compute_semantic_digest({"OBS 0 RESET\n\n"}).empty());
}

TEST(SemanticDigestTest, MutationProducesDifferentDigest) {
    auto obs1 = std::vector<oracle::OperationObservation>{
        make_obs(0, replay::EventKind::InstallBaseline, 100),
        make_obs(1, replay::EventKind::DepthUpdate, 101),
    };
    auto obs2 = obs1;
    obs2[1].checkpoint.bids[0].price = 999;

    const auto d1 = semantic::compute_semantic_digest_from_observations(obs1);
    const auto d2 = semantic::compute_semantic_digest_from_observations(obs2);
    EXPECT_NE(d1, d2);
}

TEST(SemanticDigestTest, OrderChangeProducesDifferentDigest) {
    auto obs1 = std::vector<oracle::OperationObservation>{
        make_obs(0, replay::EventKind::InstallBaseline, 100),
        make_obs(1, replay::EventKind::DepthUpdate, 101),
    };
    auto obs2 = std::vector<oracle::OperationObservation>{
        make_obs(1, replay::EventKind::DepthUpdate, 101),
        make_obs(0, replay::EventKind::InstallBaseline, 100),
    };

    const auto d1 = semantic::compute_semantic_digest_from_observations(obs1);
    const auto d2 = semantic::compute_semantic_digest_from_observations(obs2);
    EXPECT_NE(d1, d2);
}

TEST(SemanticDigestTest, OmissionProducesDifferentDigest) {
    auto obs1 = std::vector<oracle::OperationObservation>{
        make_obs(0, replay::EventKind::InstallBaseline, 100),
        make_obs(1, replay::EventKind::DepthUpdate, 101),
        make_obs(2, replay::EventKind::DepthUpdate, 102),
    };
    auto obs2 = std::vector<oracle::OperationObservation>{
        make_obs(0, replay::EventKind::InstallBaseline, 100),
        make_obs(2, replay::EventKind::DepthUpdate, 102),
    };

    const auto d1 = semantic::compute_semantic_digest_from_observations(obs1);
    const auto d2 = semantic::compute_semantic_digest_from_observations(obs2);
    EXPECT_NE(d1, d2);
}

TEST(SemanticDigestTest, EventKindChangeProducesDifferentDigest) {
    auto obs1 = std::vector<oracle::OperationObservation>{
        make_obs(0, replay::EventKind::InstallBaseline, 100),
    };
    auto obs2 = std::vector<oracle::OperationObservation>{
        make_obs(0, replay::EventKind::Rebaseline, 100),
    };

    const auto d1 = semantic::compute_semantic_digest_from_observations(obs1);
    const auto d2 = semantic::compute_semantic_digest_from_observations(obs2);
    EXPECT_NE(d1, d2);
}

TEST(SemanticDigestTest, DispositionChangeProducesDifferentDigest) {
    auto obs1 = std::vector<oracle::OperationObservation>{
        make_obs(0, replay::EventKind::DepthUpdate, 100),
    };
    auto obs2 = obs1;
    std::get<oracle::ApplyOutcome>(obs2[0].result.value).disposition =
        oracle::CanonicalDisposition::IgnoredStale;

    const auto d1 = semantic::compute_semantic_digest_from_observations(obs1);
    const auto d2 = semantic::compute_semantic_digest_from_observations(obs2);
    EXPECT_NE(d1, d2);
}

TEST(SemanticDigestTest, DigestIsLowercaseHex) {
    const std::vector<oracle::OperationObservation> obs = {
        make_obs(0, replay::EventKind::InstallBaseline, 100),
    };
    const auto digest = semantic::compute_semantic_digest_from_observations(obs);
    EXPECT_EQ(digest.size(), 64);
    for (char c : digest) {
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))
            << "unexpected digest char: " << c;
    }
}

TEST(SemanticDigestTest, DeterministicRepeatDigest) {
    const std::vector<oracle::OperationObservation> obs = {
        make_obs(0, replay::EventKind::InstallBaseline, 100),
        make_obs(1, replay::EventKind::DepthUpdate, 101),
        make_obs(2, replay::EventKind::SnapshotRequest, 102),
    };
    const auto d1 = semantic::compute_semantic_digest_from_observations(obs);
    const auto d2 = semantic::compute_semantic_digest_from_observations(obs);
    EXPECT_EQ(d1, d2);
}

} // namespace
