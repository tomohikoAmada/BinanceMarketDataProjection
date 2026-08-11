#include "canonical_observation.hpp"
#include "semantic_digest.hpp"

#include "../oracle/operation_observation.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace {

namespace oracle = bmd_projection::m5::oracle;
namespace replay = bmd_projection::m5::replay;
namespace semantic = bmd_projection::m5::semantic;

oracle::OperationObservation make_minimal_obs(std::size_t index, replay::EventKind kind) {
    oracle::OperationObservation obs;
    obs.event_index = index;
    obs.event_kind = kind;
    obs.result.value = oracle::MetadataOutcome{};
    obs.checkpoint.status = oracle::CanonicalStatus::Synchronized;
    return obs;
}

TEST(CanonicalObservationTest, SchemaVersionIsFrozen) {
    EXPECT_EQ(std::string(semantic::kObservationSchemaV1), "M5_SEMANTIC_OBSERVATION_V1");
}

TEST(CanonicalObservationTest, SerializeEmptyObservation) {
    const auto obs = make_minimal_obs(0, replay::EventKind::Reset);
    const auto text = semantic::serialize_observation(obs);
    EXPECT_NE(text.find("OBS 0 RESET"), std::string::npos);
    EXPECT_NE(text.find("MetadataOutcome"), std::string::npos);
    EXPECT_NE(text.find("CHECKPOINT Synchronized"), std::string::npos);
}

TEST(CanonicalObservationTest, SerializeAllEventKinds) {
    const std::vector<replay::EventKind> kinds = {
        replay::EventKind::InstallBaseline, replay::EventKind::DepthUpdate,
        replay::EventKind::Rebaseline,      replay::EventKind::Reset,
        replay::EventKind::SnapshotRequest, replay::EventKind::AdapterMetadata,
        replay::EventKind::MalformedRange,
    };
    for (auto kind : kinds) {
        const auto obs = make_minimal_obs(0, kind);
        const auto text = semantic::serialize_observation(obs);
        EXPECT_NE(text.find("OBS 0 "), std::string::npos) << "kind=" << static_cast<int>(kind);
    }
}

TEST(CanonicalObservationTest, SerializeAllDispositions) {
    const std::vector<oracle::CanonicalDisposition> disps = {
        oracle::CanonicalDisposition::Installed,
        oracle::CanonicalDisposition::Applied,
        oracle::CanonicalDisposition::IgnoredStale,
        oracle::CanonicalDisposition::IgnoredDuplicate,
        oracle::CanonicalDisposition::GapDetected,
        oracle::CanonicalDisposition::RejectedWrongState,
    };
    for (auto disp : disps) {
        oracle::OperationObservation obs = make_minimal_obs(0, replay::EventKind::DepthUpdate);
        oracle::ApplyOutcome apply;
        apply.disposition = disp;
        apply.status_after = oracle::CanonicalStatus::Synchronized;
        apply.last_update_id_after = 100;
        obs.result.value = apply;
        const auto text = semantic::serialize_observation(obs);
        EXPECT_NE(text.find("ApplyOutcome"), std::string::npos)
            << "disposition=" << static_cast<int>(disp);
    }
}

TEST(CanonicalObservationTest, SerializeAllStatuses) {
    const std::vector<oracle::CanonicalStatus> statuses = {
        oracle::CanonicalStatus::AwaitingBaseline,
        oracle::CanonicalStatus::AwaitingBridge,
        oracle::CanonicalStatus::Synchronized,
        oracle::CanonicalStatus::NeedsResync,
    };
    for (auto status : statuses) {
        oracle::OperationObservation obs = make_minimal_obs(0, replay::EventKind::InstallBaseline);
        oracle::InstallOutcome inst;
        inst.disposition = oracle::CanonicalDisposition::Installed;
        inst.status_after = status;
        inst.last_update_id_after = 1;
        obs.result.value = inst;
        const auto text = semantic::serialize_observation(obs);
        EXPECT_NE(text.find("InstallOutcome"), std::string::npos)
            << "status=" << static_cast<int>(status);
    }
}

TEST(CanonicalObservationTest, SerializeAllResultVariants) {
    {
        oracle::OperationObservation obs = make_minimal_obs(0, replay::EventKind::DepthUpdate);
        obs.result.value = oracle::DecimalErrorOutcome{oracle::CanonicalDecimalError::Overflow};
        auto text = semantic::serialize_observation(obs);
        EXPECT_NE(text.find("DecimalErrorOutcome Overflow"), std::string::npos);
    }
    {
        oracle::OperationObservation obs = make_minimal_obs(0, replay::EventKind::InstallBaseline);
        obs.result.value = oracle::InstallOutcome{oracle::CanonicalDisposition::Installed,
                                                  oracle::CanonicalStatus::AwaitingBridge, 500};
        auto text = semantic::serialize_observation(obs);
        EXPECT_NE(text.find("InstallOutcome Installed AwaitingBridge some 500"), std::string::npos);
    }
    {
        oracle::OperationObservation obs = make_minimal_obs(0, replay::EventKind::DepthUpdate);
        obs.result.value =
            oracle::ApplyOutcome{oracle::CanonicalDisposition::Applied,
                                 oracle::CanonicalStatus::Synchronized, 1001, std::nullopt};
        auto text = semantic::serialize_observation(obs);
        EXPECT_NE(text.find("ApplyOutcome Applied Synchronized some 1001 nogap"),
                  std::string::npos);
    }
    {
        oracle::OperationObservation obs = make_minimal_obs(0, replay::EventKind::DepthUpdate);
        obs.result.value =
            oracle::AdapterErrorOutcome{oracle::CanonicalAdapterCode::UnsupportedVenue,
                                        oracle::CanonicalAdapterField::Venue, std::nullopt};
        auto text = semantic::serialize_observation(obs);
        EXPECT_NE(text.find("AdapterErrorOutcome UnsupportedVenue Venue none"), std::string::npos);
    }
    {
        oracle::OperationObservation obs = make_minimal_obs(0, replay::EventKind::DepthUpdate);
        oracle::AdapterSuccessOutcome as;
        as.core_result = oracle::InstallOutcome{oracle::CanonicalDisposition::Installed,
                                                oracle::CanonicalStatus::AwaitingBridge, 5};
        as.observed_quality = {oracle::CanonicalQualityFlag::Duplicate,
                               oracle::CanonicalQualityFlag::Overlap};
        obs.result.value = as;
        auto text = semantic::serialize_observation(obs);
        EXPECT_NE(text.find("AdapterSuccessOutcome"), std::string::npos);
    }
    {
        oracle::OperationObservation obs = make_minimal_obs(0, replay::EventKind::SnapshotRequest);
        oracle::SnapshotOutcome snap;
        snap.policy = oracle::CanonicalPolicy::Spot;
        snap.symbol = "BTCUSDT";
        snap.producer = "test";
        snap.producer_version = "1.0";
        snap.source = oracle::CanonicalSnapshotSource::GatewayLive;
        snap.synchronized = true;
        obs.result.value = snap;
        auto text = semantic::serialize_observation(obs);
        EXPECT_NE(text.find("SnapshotOutcome"), std::string::npos);
    }
    {
        oracle::OperationObservation obs = make_minimal_obs(0, replay::EventKind::SnapshotRequest);
        obs.result.value = oracle::SnapshotNotProducedOutcome{};
        auto text = semantic::serialize_observation(obs);
        EXPECT_NE(text.find("SnapshotNotProducedOutcome"), std::string::npos);
    }
    {
        oracle::OperationObservation obs = make_minimal_obs(0, replay::EventKind::Reset);
        obs.result.value = oracle::ResetOutcome{};
        auto text = semantic::serialize_observation(obs);
        EXPECT_NE(text.find("ResetOutcome"), std::string::npos);
    }
    {
        oracle::OperationObservation obs = make_minimal_obs(0, replay::EventKind::MalformedRange);
        obs.result.value = oracle::RangeOutcome{true};
        auto text = semantic::serialize_observation(obs);
        EXPECT_NE(text.find("RangeOutcome true"), std::string::npos);
    }
    {
        oracle::OperationObservation obs = make_minimal_obs(0, replay::EventKind::AdapterMetadata);
        obs.result.value = oracle::MetadataOutcome{};
        auto text = semantic::serialize_observation(obs);
        EXPECT_NE(text.find("MetadataOutcome"), std::string::npos);
    }
}

TEST(CanonicalObservationTest, SerializeOptionalAbsentAndPresent) {
    {
        oracle::OperationObservation obs = make_minimal_obs(0, replay::EventKind::DepthUpdate);
        oracle::InstallOutcome inst;
        inst.disposition = oracle::CanonicalDisposition::Installed;
        inst.status_after = oracle::CanonicalStatus::AwaitingBridge;
        inst.last_update_id_after = std::nullopt;
        obs.result.value = inst;
        auto text = semantic::serialize_observation(obs);
        EXPECT_NE(text.find("none"), std::string::npos);
    }
    {
        oracle::OperationObservation obs = make_minimal_obs(0, replay::EventKind::DepthUpdate);
        oracle::InstallOutcome inst;
        inst.disposition = oracle::CanonicalDisposition::Installed;
        inst.status_after = oracle::CanonicalStatus::AwaitingBridge;
        inst.last_update_id_after = 42;
        obs.result.value = inst;
        auto text = semantic::serialize_observation(obs);
        EXPECT_NE(text.find("some 42"), std::string::npos);
    }
}

TEST(CanonicalObservationTest, SerializeCheckpointWithLevels) {
    oracle::OperationObservation obs = make_minimal_obs(0, replay::EventKind::DepthUpdate);
    oracle::ApplyOutcome apply;
    apply.disposition = oracle::CanonicalDisposition::Applied;
    apply.status_after = oracle::CanonicalStatus::Synchronized;
    apply.last_update_id_after = 100;
    obs.result.value = apply;

    obs.checkpoint.status = oracle::CanonicalStatus::Synchronized;
    obs.checkpoint.last_update_id = 100;
    obs.checkpoint.synchronized_visible = true;
    obs.checkpoint.bids = {{1000, 10}, {999, 5}};
    obs.checkpoint.asks = {{1001, 8}};
    obs.checkpoint.price_scale = 2;
    obs.checkpoint.quantity_scale = 3;

    auto text = semantic::serialize_observation(obs);
    EXPECT_NE(text.find("Synchronized some 100 nogap true"), std::string::npos);
    EXPECT_NE(text.find("BIDS 2 1000 10 999 5"), std::string::npos);
    EXPECT_NE(text.find("ASKS 1 1001 8"), std::string::npos);
    EXPECT_NE(text.find("SCALES 2 3"), std::string::npos);
}

TEST(CanonicalObservationTest, SerializeGapEvidence) {
    oracle::OperationObservation obs = make_minimal_obs(0, replay::EventKind::DepthUpdate);
    oracle::ApplyOutcome apply;
    apply.disposition = oracle::CanonicalDisposition::GapDetected;
    apply.status_after = oracle::CanonicalStatus::NeedsResync;
    apply.last_update_id_after = 99;
    oracle::CanonicalGapEvidence gap;
    gap.last_accepted_final = 99;
    gap.first_update_id = 200;
    gap.final_update_id = 201;
    gap.previous_final = std::nullopt;
    gap.reason = oracle::CanonicalGapReason::SpotLiveForwardGap;
    gap.policy = oracle::CanonicalPolicy::Spot;
    apply.gap = gap;
    obs.result.value = apply;

    auto text = semantic::serialize_observation(obs);
    EXPECT_NE(text.find("SpotLiveForwardGap"), std::string::npos);
    EXPECT_NE(text.find("GapDetected"), std::string::npos);
}

TEST(CanonicalObservationTest, SerializeSnapshotFields) {
    oracle::OperationObservation obs = make_minimal_obs(0, replay::EventKind::SnapshotRequest);
    oracle::SnapshotOutcome snap;
    snap.policy = oracle::CanonicalPolicy::Spot;
    snap.symbol = "BTCUSDT";
    snap.producer = "gw-01";
    snap.producer_version = "2.0.1";
    snap.source = oracle::CanonicalSnapshotSource::RecorderReplay;
    snap.generated_time_utc_ns = 1699999999000000000ULL;
    snap.last_update_id = 5000;
    snap.synchronized = true;
    snap.bids = {{"100.00", "1.500"}, {"99.50", "2.000"}};
    snap.asks = {{"100.50", "0.750"}};
    snap.quality_flags = {oracle::CanonicalQualityFlag::Duplicate};
    snap.depth_limit = 20;

    obs.result.value = snap;
    auto text = semantic::serialize_observation(obs);
    EXPECT_NE(text.find("SnapshotOutcome"), std::string::npos);
    EXPECT_NE(text.find("Spot"), std::string::npos);
    EXPECT_NE(text.find("gw-01"), std::string::npos);
    EXPECT_NE(text.find("RecorderReplay"), std::string::npos);
    EXPECT_NE(text.find("Duplicate"), std::string::npos);
}

TEST(CanonicalObservationTest, SerializeStringWithSpecialChars) {
    oracle::OperationObservation obs = make_minimal_obs(0, replay::EventKind::SnapshotRequest);
    oracle::SnapshotOutcome snap;
    snap.policy = oracle::CanonicalPolicy::Spot;
    snap.symbol = "BTC:USDT";
    snap.producer = "gw-01";
    snap.producer_version = "1.0";
    snap.source = oracle::CanonicalSnapshotSource::GatewayLive;
    snap.synchronized = true;
    snap.bids = {{"100.00", "1.500"}};
    snap.asks = {{"100.50", "0.750"}};

    obs.result.value = snap;
    auto text = semantic::serialize_observation(obs);
    EXPECT_NE(text.find("BTC:USDT"), std::string::npos);
}

TEST(CanonicalObservationTest, DeterministicRepeatSerialization) {
    oracle::OperationObservation obs = make_minimal_obs(42, replay::EventKind::DepthUpdate);
    oracle::ApplyOutcome apply;
    apply.disposition = oracle::CanonicalDisposition::Applied;
    apply.status_after = oracle::CanonicalStatus::Synchronized;
    apply.last_update_id_after = 12345;
    obs.result.value = apply;
    obs.checkpoint.bids = {{1000, 10}, {900, 5}};
    obs.checkpoint.asks = {{1100, 20}};
    obs.checkpoint.price_scale = 2;
    obs.checkpoint.quantity_scale = 3;
    obs.checkpoint.status = oracle::CanonicalStatus::Synchronized;

    const auto text1 = semantic::serialize_observation(obs);
    const auto text2 = semantic::serialize_observation(obs);
    EXPECT_EQ(text1, text2);
}

TEST(CanonicalObservationTest, DecimalObservationsSerialized) {
    oracle::OperationObservation obs = make_minimal_obs(0, replay::EventKind::DepthUpdate);
    oracle::ApplyOutcome apply;
    apply.disposition = oracle::CanonicalDisposition::Applied;
    apply.status_after = oracle::CanonicalStatus::Synchronized;
    apply.last_update_id_after = 100;
    obs.result.value = apply;

    oracle::CanonicalDecimalObservation dec_obs;
    dec_obs.side = oracle::CanonicalBookSide::Bid;
    dec_obs.level_position = 0;
    dec_obs.role = oracle::CanonicalDecimalRole::Price;
    dec_obs.result = oracle::CanonicalDecimalValue{100, 2, 2};
    obs.decimal_observations.push_back(dec_obs);

    auto text = semantic::serialize_observation(obs);
    EXPECT_NE(text.find("DECIMALS 1"), std::string::npos);
    EXPECT_NE(text.find("DECIMAL Bid 0 Price VALUE 100 2 2"), std::string::npos);
}

} // namespace
