#include "canonical_observation.hpp"

#include "../oracle/operation_observation.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace oracle = bmd_projection::m5::oracle;
namespace replay = bmd_projection::m5::replay;
namespace semantic = bmd_projection::m5::semantic;

oracle::OperationObservation make_minimal_obs(std::size_t index, replay::EventKind kind) {
    oracle::OperationObservation observation;
    observation.event_index = index;
    observation.event_kind = kind;
    observation.result.value = oracle::MetadataOutcome{};
    observation.checkpoint.status = oracle::CanonicalStatus::Synchronized;
    return observation;
}

oracle::SnapshotOutcome make_snapshot() {
    oracle::SnapshotOutcome snapshot;
    snapshot.policy = oracle::CanonicalPolicy::Spot;
    snapshot.symbol = "BTCUSDT";
    snapshot.producer = "gw-01";
    snapshot.producer_version = "2.0.1";
    snapshot.source = oracle::CanonicalSnapshotSource::RecorderReplay;
    snapshot.generated_time_utc_ns = 1699999999000000000ULL;
    snapshot.last_update_id = 5000;
    snapshot.synchronized = true;
    snapshot.bids = {{"100.00", "1.500"}, {"99.50", "2.000"}};
    snapshot.asks = {{"100.50", "0.750"}};
    snapshot.quality_flags = {oracle::CanonicalQualityFlag::Duplicate};
    snapshot.depth_limit = 20;
    return snapshot;
}

oracle::CanonicalGapEvidence make_gap() {
    oracle::CanonicalGapEvidence gap;
    gap.last_accepted_final = 99;
    gap.first_update_id = 200;
    gap.final_update_id = 201;
    gap.reason = oracle::CanonicalGapReason::SpotLiveForwardGap;
    gap.policy = oracle::CanonicalPolicy::Spot;
    return gap;
}

void expect_canonical_line_discipline(const std::string& text) {
    ASSERT_FALSE(text.empty());
    EXPECT_EQ(text.back(), '\n');
    ASSERT_GT(text.size(), 1U);
    EXPECT_NE(text[text.size() - 2], '\n');
    EXPECT_EQ(text.find('\r'), std::string::npos);
    EXPECT_EQ(text.find('\t'), std::string::npos);
    EXPECT_EQ(text.find("  "), std::string::npos);

    for (const char character : text) {
        const auto byte = static_cast<unsigned char>(character);
        if (byte < 0x20U) {
            EXPECT_EQ(byte, static_cast<unsigned char>('\n'));
        }
    }

    std::size_t line_start = 0;
    while (line_start < text.size()) {
        const auto line_end = text.find('\n', line_start);
        ASSERT_NE(line_end, std::string::npos);
        const std::string_view line{text.data() + line_start, line_end - line_start};
        ASSERT_FALSE(line.empty());
        EXPECT_NE(line.front(), ' ');
        EXPECT_NE(line.back(), ' ');
        line_start = line_end + 1;
    }
}

TEST(CanonicalObservationTest, SchemaVersionIsFrozen) {
    EXPECT_EQ(std::string(semantic::kObservationSchemaV1), "M5_SEMANTIC_OBSERVATION_V1");
}

TEST(CanonicalObservationTest, SimpleObservationMatchesGoldenBytes) {
    const auto observation = make_minimal_obs(0, replay::EventKind::Reset);
    const auto text = semantic::serialize_observation(observation);

    EXPECT_EQ(text, "OBS 0 RESET\n"
                    "RESULT MetadataOutcome\n"
                    "CHECKPOINT Synchronized LAST_UPDATE - GAP - VISIBLE false BIDS 0 ASKS 0 "
                    "SCALES 0 0\n"
                    "SNAPSHOT -\n"
                    "DECIMALS 0\n");
    expect_canonical_line_discipline(text);
}

TEST(CanonicalObservationTest, DecimalObservationsMatchGoldenBytes) {
    auto observation = make_minimal_obs(4, replay::EventKind::DepthUpdate);
    observation.result.value =
        oracle::ApplyOutcome{oracle::CanonicalDisposition::Applied,
                             oracle::CanonicalStatus::Synchronized, 100, std::nullopt};
    observation.decimal_observations = {
        {oracle::CanonicalBookSide::Bid, 0, oracle::CanonicalDecimalRole::Price,
         oracle::CanonicalDecimalValue{100, 2, 2}},
        {oracle::CanonicalBookSide::Ask, 3, oracle::CanonicalDecimalRole::Quantity,
         oracle::CanonicalDecimalFailure{oracle::CanonicalDecimalError::InexactScale, 4}},
    };

    const auto text = semantic::serialize_observation(observation);
    EXPECT_EQ(text, "OBS 4 DEPTH_UPDATE\n"
                    "RESULT ApplyOutcome Applied Synchronized LAST_UPDATE 100 GAP -\n"
                    "CHECKPOINT Synchronized LAST_UPDATE - GAP - VISIBLE false BIDS 0 ASKS 0 "
                    "SCALES 0 0\n"
                    "SNAPSHOT -\n"
                    "DECIMALS 2\n"
                    "DECIMAL Bid 0 Price VALUE 100 2 2\n"
                    "DECIMAL Ask 3 Quantity FAILURE InexactScale 4\n");
    expect_canonical_line_discipline(text);
}

TEST(CanonicalObservationTest, GapEvidenceAndVectorsMatchGoldenBytes) {
    auto observation = make_minimal_obs(7, replay::EventKind::DepthUpdate);
    const auto gap = make_gap();
    observation.result.value = oracle::ApplyOutcome{oracle::CanonicalDisposition::GapDetected,
                                                    oracle::CanonicalStatus::NeedsResync, 99, gap};
    observation.checkpoint.status = oracle::CanonicalStatus::NeedsResync;
    observation.checkpoint.last_update_id = 99;
    observation.checkpoint.last_gap = gap;
    observation.checkpoint.bids = {{1000, 10}, {999, 5}};
    observation.checkpoint.asks = {{1001, 8}};
    observation.checkpoint.price_scale = 2;
    observation.checkpoint.quantity_scale = 3;

    const auto text = semantic::serialize_observation(observation);
    EXPECT_EQ(text, "OBS 7 DEPTH_UPDATE\n"
                    "RESULT ApplyOutcome GapDetected NeedsResync LAST_UPDATE 99 GAP 99 200 201 - "
                    "SpotLiveForwardGap Spot\n"
                    "CHECKPOINT NeedsResync LAST_UPDATE 99 GAP 99 200 201 - SpotLiveForwardGap "
                    "Spot VISIBLE false BIDS 2 1000 10 999 5 ASKS 1 1001 8 SCALES 2 3\n"
                    "SNAPSHOT -\n"
                    "DECIMALS 0\n");
    expect_canonical_line_discipline(text);
}

TEST(CanonicalObservationTest, AdapterSuccessAndErrorMatchGoldenBytes) {
    auto success_observation = make_minimal_obs(1, replay::EventKind::InstallBaseline);
    oracle::AdapterSuccessOutcome success;
    success.core_result = oracle::InstallOutcome{oracle::CanonicalDisposition::Installed,
                                                 oracle::CanonicalStatus::AwaitingBridge, 500};
    success.observed_quality = {oracle::CanonicalQualityFlag::Duplicate,
                                oracle::CanonicalQualityFlag::Overlap};
    success_observation.result.value = success;
    const auto success_text = semantic::serialize_observation(success_observation);
    EXPECT_NE(success_text.find("RESULT AdapterSuccessOutcome InstallOutcome Installed "
                                "AwaitingBridge LAST_UPDATE 500 QUALITY 2 Duplicate Overlap\n"),
              std::string::npos);
    expect_canonical_line_discipline(success_text);

    auto error_observation = make_minimal_obs(2, replay::EventKind::DepthUpdate);
    error_observation.result.value = oracle::AdapterErrorOutcome{
        oracle::CanonicalAdapterCode::InvalidDecimal, oracle::CanonicalAdapterField::BidPrice,
        oracle::CanonicalDecimalError::Overflow};
    const auto error_text = semantic::serialize_observation(error_observation);
    EXPECT_NE(error_text.find("RESULT AdapterErrorOutcome InvalidDecimal BidPrice "
                              "DECIMAL_ERROR Overflow\n"),
              std::string::npos);
    expect_canonical_line_discipline(error_text);
}

TEST(CanonicalObservationTest, SnapshotMatchesGoldenBytes) {
    auto observation = make_minimal_obs(8, replay::EventKind::SnapshotRequest);
    const auto snapshot = make_snapshot();
    observation.result.value = snapshot;
    observation.snapshot = snapshot;

    const std::string golden_snapshot =
        "SnapshotOutcome Spot SYMBOL 7:42544355534454 PRODUCER 5:67772d3031 "
        "PRODUCER_VERSION 5:322e302e31 SOURCE RecorderReplay GENERATED_UTC_NS "
        "1699999999000000000 GENERATED_MONOTONIC_NS - LAST_UPDATE 5000 SYNCHRONIZED true "
        "BIDS 2 6:3130302e3030 5:312e353030 5:39392e3530 5:322e303030 "
        "ASKS 1 6:3130302e3530 5:302e373530 QUALITY 1 Duplicate DEPTH_LIMIT 20 "
        "GAP_DESCRIPTOR -";
    const auto text = semantic::serialize_observation(observation);
    EXPECT_NE(text.find("RESULT " + golden_snapshot + "\n"), std::string::npos);
    EXPECT_NE(text.find("SNAPSHOT " + golden_snapshot + "\n"), std::string::npos);
    expect_canonical_line_discipline(text);
}

TEST(CanonicalObservationTest, PresentGapDescriptorMatchesGoldenBytes) {
    auto observation = make_minimal_obs(9, replay::EventKind::SnapshotRequest);
    auto snapshot = make_snapshot();
    snapshot.gap_descriptor = oracle::GapDescriptorObservation{
        11, 12, 13, oracle::CanonicalReasonCode::SequenceGapDetected,
        oracle::CanonicalResyncState::ResyncInProgress};
    observation.result.value = snapshot;
    const auto text = semantic::serialize_observation(observation);
    EXPECT_NE(text.find("GAP_DESCRIPTOR 11 12 13 SequenceGapDetected ResyncInProgress"),
              std::string::npos);
    expect_canonical_line_discipline(text);
}

TEST(CanonicalObservationTest, StringsUseInjectiveUtf8ByteHexEncoding) {
    const std::vector<std::pair<std::string, std::string>> cases = {
        {"", "0:"},          {"ASCII", "5:4153434949"},
        {"a b", "3:612062"}, {":", "1:3a"},
        {"\\", "1:5c"},      {"\"", "1:22"},
        {"\n", "1:0a"},      {"\r", "1:0d"},
        {"\t", "1:09"},      {"\xE2\x82\xAC", "3:e282ac"},
    };

    for (const auto& [input, expected_token] : cases) {
        auto observation = make_minimal_obs(0, replay::EventKind::SnapshotRequest);
        auto snapshot = make_snapshot();
        snapshot.symbol = input;
        observation.result.value = snapshot;
        const auto text = semantic::serialize_observation(observation);
        EXPECT_NE(text.find(" SYMBOL " + expected_token + " PRODUCER "), std::string::npos)
            << "input byte count=" << input.size();
        expect_canonical_line_discipline(text);
    }
}

TEST(CanonicalObservationTest, AllEventKindsHaveFrozenNames) {
    const std::vector<std::pair<replay::EventKind, std::string_view>> cases = {
        {replay::EventKind::InstallBaseline, "INSTALL_BASELINE"},
        {replay::EventKind::DepthUpdate, "DEPTH_UPDATE"},
        {replay::EventKind::Rebaseline, "REBASELINE"},
        {replay::EventKind::Reset, "RESET"},
        {replay::EventKind::SnapshotRequest, "SNAPSHOT_REQUEST"},
        {replay::EventKind::AdapterMetadata, "ADAPTER_METADATA"},
        {replay::EventKind::MalformedRange, "MALFORMED_RANGE"},
    };
    for (const auto& [kind, name] : cases) {
        const auto text = semantic::serialize_observation(make_minimal_obs(0, kind));
        EXPECT_TRUE(text.starts_with("OBS 0 " + std::string(name) + "\n"));
    }
}

TEST(CanonicalObservationTest, AllCurrentOperationResultAlternativesAreExplicit) {
    std::vector<oracle::OperationResultValue> values;
    values.emplace_back(oracle::DecimalErrorOutcome{oracle::CanonicalDecimalError::Overflow});
    values.emplace_back(oracle::InstallOutcome{oracle::CanonicalDisposition::Installed,
                                               oracle::CanonicalStatus::AwaitingBridge, 500});
    values.emplace_back(oracle::ApplyOutcome{oracle::CanonicalDisposition::Applied,
                                             oracle::CanonicalStatus::Synchronized, 501,
                                             std::nullopt});
    values.emplace_back(oracle::AdapterErrorOutcome{oracle::CanonicalAdapterCode::UnsupportedVenue,
                                                    oracle::CanonicalAdapterField::Venue,
                                                    std::nullopt});
    oracle::AdapterSuccessOutcome adapter_success;
    adapter_success.core_result =
        oracle::ApplyOutcome{oracle::CanonicalDisposition::Applied,
                             oracle::CanonicalStatus::Synchronized, 501, std::nullopt};
    values.emplace_back(adapter_success);
    values.emplace_back(make_snapshot());
    values.emplace_back(oracle::SnapshotNotProducedOutcome{});
    values.emplace_back(oracle::ResetOutcome{});
    values.emplace_back(oracle::RangeOutcome{true});
    values.emplace_back(oracle::MetadataOutcome{});

    const std::vector<std::string_view> tags = {
        "DecimalErrorOutcome",
        "InstallOutcome",
        "ApplyOutcome",
        "AdapterErrorOutcome",
        "AdapterSuccessOutcome",
        "SnapshotOutcome",
        "SnapshotNotProducedOutcome",
        "ResetOutcome",
        "RangeOutcome",
        "MetadataOutcome",
    };
    ASSERT_EQ(values.size(), tags.size());
    for (std::size_t index = 0; index < values.size(); ++index) {
        auto observation = make_minimal_obs(index, replay::EventKind::DepthUpdate);
        observation.result.value = values[index];
        const auto text = semantic::serialize_observation(observation);
        EXPECT_NE(text.find(std::string(tags[index])), std::string::npos) << "index=" << index;
    }
}

TEST(CanonicalObservationTest, InvalidRuntimeEnumValuesFailClosed) {
    {
        auto observation = make_minimal_obs(0, static_cast<replay::EventKind>(255));
        EXPECT_THROW((void)semantic::serialize_observation(observation), std::invalid_argument);
    }
    {
        auto observation = make_minimal_obs(0, replay::EventKind::DepthUpdate);
        observation.decimal_observations = {{static_cast<oracle::CanonicalBookSide>(255), 0,
                                             oracle::CanonicalDecimalRole::Price,
                                             oracle::CanonicalDecimalValue{1, 0, 0}}};
        EXPECT_THROW((void)semantic::serialize_observation(observation), std::invalid_argument);
    }
    {
        auto observation = make_minimal_obs(0, replay::EventKind::DepthUpdate);
        observation.decimal_observations = {{oracle::CanonicalBookSide::Bid, 0,
                                             static_cast<oracle::CanonicalDecimalRole>(255),
                                             oracle::CanonicalDecimalValue{1, 0, 0}}};
        EXPECT_THROW((void)semantic::serialize_observation(observation), std::invalid_argument);
    }
    {
        auto observation = make_minimal_obs(0, replay::EventKind::DepthUpdate);
        observation.result.value =
            oracle::DecimalErrorOutcome{static_cast<oracle::CanonicalDecimalError>(255)};
        EXPECT_THROW((void)semantic::serialize_observation(observation), std::invalid_argument);
    }
    {
        auto observation = make_minimal_obs(0, replay::EventKind::InstallBaseline);
        observation.result.value =
            oracle::InstallOutcome{static_cast<oracle::CanonicalDisposition>(255),
                                   oracle::CanonicalStatus::AwaitingBridge, std::nullopt};
        EXPECT_THROW((void)semantic::serialize_observation(observation), std::invalid_argument);
    }
    {
        auto observation = make_minimal_obs(0, replay::EventKind::DepthUpdate);
        observation.checkpoint.status = static_cast<oracle::CanonicalStatus>(255);
        EXPECT_THROW((void)semantic::serialize_observation(observation), std::invalid_argument);
    }
    {
        auto observation = make_minimal_obs(0, replay::EventKind::DepthUpdate);
        auto gap = make_gap();
        gap.reason = static_cast<oracle::CanonicalGapReason>(255);
        observation.checkpoint.last_gap = gap;
        EXPECT_THROW((void)semantic::serialize_observation(observation), std::invalid_argument);
    }
    {
        auto observation = make_minimal_obs(0, replay::EventKind::DepthUpdate);
        auto gap = make_gap();
        gap.policy = static_cast<oracle::CanonicalPolicy>(255);
        observation.checkpoint.last_gap = gap;
        EXPECT_THROW((void)semantic::serialize_observation(observation), std::invalid_argument);
    }
    {
        auto observation = make_minimal_obs(0, replay::EventKind::DepthUpdate);
        observation.result.value =
            oracle::AdapterErrorOutcome{static_cast<oracle::CanonicalAdapterCode>(255),
                                        oracle::CanonicalAdapterField::Venue, std::nullopt};
        EXPECT_THROW((void)semantic::serialize_observation(observation), std::invalid_argument);
    }
    {
        auto observation = make_minimal_obs(0, replay::EventKind::DepthUpdate);
        observation.result.value = oracle::AdapterErrorOutcome{
            oracle::CanonicalAdapterCode::UnsupportedVenue,
            static_cast<oracle::CanonicalAdapterField>(255), std::nullopt};
        EXPECT_THROW((void)semantic::serialize_observation(observation), std::invalid_argument);
    }
    {
        auto observation = make_minimal_obs(0, replay::EventKind::DepthUpdate);
        oracle::AdapterSuccessOutcome success;
        success.core_result =
            oracle::ApplyOutcome{oracle::CanonicalDisposition::Applied,
                                 oracle::CanonicalStatus::Synchronized, 1, std::nullopt};
        success.observed_quality = {static_cast<oracle::CanonicalQualityFlag>(255)};
        observation.result.value = success;
        EXPECT_THROW((void)semantic::serialize_observation(observation), std::invalid_argument);
    }
    {
        auto observation = make_minimal_obs(0, replay::EventKind::SnapshotRequest);
        auto snapshot = make_snapshot();
        snapshot.source = static_cast<oracle::CanonicalSnapshotSource>(255);
        observation.result.value = snapshot;
        EXPECT_THROW((void)semantic::serialize_observation(observation), std::invalid_argument);
    }
    {
        auto observation = make_minimal_obs(0, replay::EventKind::SnapshotRequest);
        auto snapshot = make_snapshot();
        snapshot.gap_descriptor =
            oracle::GapDescriptorObservation{1, 2, 3, static_cast<oracle::CanonicalReasonCode>(255),
                                             oracle::CanonicalResyncState::ResyncRequired};
        observation.result.value = snapshot;
        EXPECT_THROW((void)semantic::serialize_observation(observation), std::invalid_argument);
    }
    {
        auto observation = make_minimal_obs(0, replay::EventKind::SnapshotRequest);
        auto snapshot = make_snapshot();
        snapshot.gap_descriptor = oracle::GapDescriptorObservation{
            1, 2, 3, oracle::CanonicalReasonCode::SequenceGapDetected,
            static_cast<oracle::CanonicalResyncState>(255)};
        observation.result.value = snapshot;
        EXPECT_THROW((void)semantic::serialize_observation(observation), std::invalid_argument);
    }
}

TEST(CanonicalObservationTest, DeterministicRepeatSerialization) {
    auto observation = make_minimal_obs(42, replay::EventKind::DepthUpdate);
    observation.result.value =
        oracle::ApplyOutcome{oracle::CanonicalDisposition::Applied,
                             oracle::CanonicalStatus::Synchronized, 12345, std::nullopt};
    observation.checkpoint.bids = {{1000, 10}, {900, 5}};
    observation.checkpoint.asks = {{1100, 20}};
    observation.checkpoint.price_scale = 2;
    observation.checkpoint.quantity_scale = 3;

    EXPECT_EQ(semantic::serialize_observation(observation),
              semantic::serialize_observation(observation));
}

} // namespace
