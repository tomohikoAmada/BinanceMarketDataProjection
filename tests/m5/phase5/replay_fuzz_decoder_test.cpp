#include "replay_fuzz_decoder.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {
namespace decoder = bmd_projection::m5::replay::fuzz_decoder;
namespace replay = bmd_projection::m5::replay;

using decoder::ByteCursor;
using decoder::decode;
using decoder::FuzzCase;
using decoder::FuzzMarket;
using decoder::FuzzVenue;

template <typename... Args>
[[nodiscard]] auto make_data(Args... args) -> std::vector<std::uint8_t> {
    return {static_cast<std::uint8_t>(args)...};
}

TEST(ReplayFuzzDecoderTest, EmptyInputReturnsNullopt) {
    EXPECT_FALSE(decode(nullptr, 0).has_value());
    const std::uint8_t* null_data = nullptr;
    EXPECT_FALSE(decode(null_data, 1).has_value());
}

TEST(ReplayFuzzDecoderTest, MinimumSizeReturnsValid) {
    auto data = make_data(0x20, 0x08, 0x07, 'B', 'T', 'C', 'U', 'S', 'D', 'T', 0x01, 0x03);
    auto result = decode(data.data(), data.size());
    ASSERT_TRUE(result.has_value());
    const auto& r = result.value();
    EXPECT_EQ(r.market, replay::Market::Spot);
    EXPECT_EQ(r.numeric_spec.price_scale, 8U);
    EXPECT_EQ(r.numeric_spec.quantity_scale, 8U);
    EXPECT_EQ(r.operations.size(), 1U);
}

TEST(ReplayFuzzDecoderTest, SingleByteInput) {
    auto data = make_data(0xAA);
    auto result = decode(data.data(), data.size());
    EXPECT_FALSE(result.has_value());
}

TEST(ReplayFuzzDecoderTest, CoreModeDecoded) {
    auto data = make_data(0x20, 0x08, 0x03, 'B', 'T', 'C', 0x01, 0x03);
    auto result = decode(data.data(), data.size());
    ASSERT_TRUE(result.has_value());
    const auto& r = result.value();
    EXPECT_EQ(r.mode, decoder::DecodedMode::CoreOnly);
    EXPECT_EQ(r.symbol, "BTC");
}

TEST(ReplayFuzzDecoderTest, AdapterModeDecoded) {
    auto data = make_data(0x21, 0x08, 0x03, 'B', 'T', 'C', 0x01, 0x03, 0x00, 0x03, 'B', 'T', 'C',
                          0x03, 'B', 'T', 'C', 0x08, 0x08, 0x08, 0x08, 0x00);
    auto result = decode(data.data(), data.size());
    ASSERT_TRUE(result.has_value());
    const auto& r = result.value();
    EXPECT_EQ(r.mode, decoder::DecodedMode::AdapterEnabled);
    EXPECT_EQ(r.scenario_venue, FuzzVenue::Binance);
    EXPECT_EQ(r.scenario_market, FuzzMarket::Spot);
}

TEST(ReplayFuzzDecoderTest, UsdmMarketDecoded) {
    auto data = make_data(0x22, 0x08, 0x03, 'E', 'T', 'H', 0x01, 0x03);
    auto result = decode(data.data(), data.size());
    ASSERT_TRUE(result.has_value());
    const auto& r = result.value();
    EXPECT_EQ(r.market, replay::Market::UsdMPerpetual);
    EXPECT_EQ(r.sequence_policy, replay::SequencePolicy::UsdMPerpetual);
}

TEST(ReplayFuzzDecoderTest, AllOperationTypesReachable) {
    // InstallBaseline
    {
        auto data = make_data(0x20, 0x08, 0x03, 'B', 'T', 'C', 0x01, 0x00, 0x18, 0x01, 0x00, 0x00,
                              0x32, 0x01, 0x00, 0x32, 0x01, 0x00);
        auto result = decode(data.data(), data.size());
        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(result.value().operations.size(), 1U);
        EXPECT_TRUE(
            std::holds_alternative<replay::InstallBaselineOp>(result.value().operations[0]));
    }

    // DepthUpdate
    {
        auto data = make_data(0x20, 0x08, 0x03, 'B', 'T', 'C', 0x01, 0x01, 0x18, 0x18, 0x00, 0x00);
        auto result = decode(data.data(), data.size());
        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(result.value().operations.size(), 1U);
        EXPECT_TRUE(std::holds_alternative<replay::DepthUpdateOp>(result.value().operations[0]));
    }

    // Rebaseline
    {
        auto data = make_data(0x20, 0x08, 0x03, 'B', 'T', 'C', 0x01, 0x02, 0x18, 0x00, 0x00);
        auto result = decode(data.data(), data.size());
        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(result.value().operations.size(), 1U);
        EXPECT_TRUE(std::holds_alternative<replay::RebaselineOp>(result.value().operations[0]));
    }

    // Reset
    {
        auto data = make_data(0x20, 0x08, 0x03, 'B', 'T', 'C', 0x01, 0x03);
        auto result = decode(data.data(), data.size());
        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(result.value().operations.size(), 1U);
        EXPECT_TRUE(std::holds_alternative<replay::ResetOp>(result.value().operations[0]));
    }

    // SnapshotRequest
    {
        auto data = make_data(0x20, 0x08, 0x03, 'B', 'T', 'C', 0x01, 0x04, 0x01, 0x05, 0x00, 0x03,
                              'a', 'b', 'c', 0x01, 'x', 0x01, 'y', 0x00, 0x18, 0x00, 0x00);
        auto result = decode(data.data(), data.size());
        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(result.value().operations.size(), 1U);
        EXPECT_TRUE(
            std::holds_alternative<replay::SnapshotRequestOp>(result.value().operations[0]));
    }

    // AdapterMetadata
    {
        auto data = make_data(0x20, 0x08, 0x03, 'B', 'T', 'C', 0x01, 0x05, 0x00);
        auto result = decode(data.data(), data.size());
        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(result.value().operations.size(), 1U);
        EXPECT_TRUE(
            std::holds_alternative<replay::AdapterMetadataOp>(result.value().operations[0]));
    }

    // MalformedRange
    {
        auto data = make_data(0x20, 0x08, 0x03, 'B', 'T', 'C', 0x01, 0x06, 0x28, 0x18);
        auto result = decode(data.data(), data.size());
        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(result.value().operations.size(), 1U);
        EXPECT_TRUE(std::holds_alternative<replay::MalformedRangeOp>(result.value().operations[0]));
    }
}

TEST(ReplayFuzzDecoderTest, Uint64SpecialValuesReachable) {
    auto test_value = [](std::uint8_t byte, std::uint64_t expected) {
        auto data = make_data(byte);
        ByteCursor cursor(data.data(), data.size());
        EXPECT_EQ(cursor.read_var_u64(), expected);
    };

    test_value(0x08, 0U);
    test_value(0x18, 1U);
    test_value(0x28, 2U);
    test_value(0x38, UINT64_MAX);
    test_value(0x48, UINT64_MAX - 1U);
    test_value(0x58, UINT64_MAX - 2U);
}

TEST(ReplayFuzzDecoderTest, NumericSpecBounds) {
    auto data = make_data(0xFC, 0xFF, 0x03, 'B', 'T', 'C', 0x01, 0x03);
    auto result = decode(data.data(), data.size());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().numeric_spec.price_scale, 6U);
    EXPECT_EQ(result.value().numeric_spec.quantity_scale, 8U);
}

TEST(ReplayFuzzDecoderTest, PerInputOperationCap) {
    auto header = make_data(0x20, 0x08, 0x03, 'B', 'T', 'C');
    std::vector<std::uint8_t> data(header.begin(), header.end());
    data.push_back(128);
    for (int i = 0; i < 128; ++i) {
        data.push_back(0x03);
    }
    auto result = decode(data.data(), data.size());
    ASSERT_TRUE(result.has_value());
    EXPECT_LE(result.value().operations.size(), decoder::kMaxOperations);
}

TEST(ReplayFuzzDecoderTest, DeterministicIdenticalInput) {
    auto data = make_data(0x20, 0x08, 0x03, 'B', 'T', 'C', 0x02, 0x03, 0x03);
    auto r1 = decode(data.data(), data.size());
    auto r2 = decode(data.data(), data.size());
    ASSERT_TRUE(r1.has_value());
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(r1.value().operations.size(), r2.value().operations.size());
    EXPECT_EQ(r1.value().symbol, r2.value().symbol);
    EXPECT_EQ(r1.value().market, r2.value().market);
}

TEST(ReplayFuzzDecoderTest, MalformedRangeReachability) {
    auto data = make_data(0x20, 0x08, 0x03, 'B', 'T', 'C', 0x01, 0x06, 0x28, 0x18);
    auto result = decode(data.data(), data.size());
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<replay::MalformedRangeOp>(result.value().operations[0]));
    const auto& op = std::get<replay::MalformedRangeOp>(result.value().operations[0]);
    EXPECT_EQ(op.first_update_id, 2U);
    EXPECT_EQ(op.final_update_id, 1U);
    EXPECT_GT(op.first_update_id, op.final_update_id);
}

TEST(ReplayFuzzDecoderTest, LevelCountCap) {
    std::vector<std::uint8_t> data{0x20, 0x08, 0x03, 'B', 'T', 'C', 0x01, 0x00, 0x18, 20};
    for (int i = 0; i < 100; ++i) {
        data.push_back(0x00);
    }
    auto result = decode(data.data(), data.size());
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(
        std::holds_alternative<replay::InstallBaselineOp>(result.value().operations[0]));
    const auto& op = std::get<replay::InstallBaselineOp>(result.value().operations[0]);
    EXPECT_LE(op.bids.size(), decoder::kMaxLevelsPerEvent);
}

TEST(ReplayFuzzDecoderTest, SnapshotDepthLimitVariants) {
    {
        auto data = make_data(0x20, 0x08, 0x03, 'B', 'T', 'C', 0x01, 0x04, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x18, 0x00, 0x00);
        auto result = decode(data.data(), data.size());
        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(
            std::holds_alternative<replay::SnapshotRequestOp>(result.value().operations[0]));
        const auto& op = std::get<replay::SnapshotRequestOp>(result.value().operations[0]);
        EXPECT_FALSE(op.depth_limit.has_value());
    }
    {
        auto data = make_data(0x20, 0x08, 0x03, 'B', 'T', 'C', 0x01, 0x04, 0x01, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00);
        auto result = decode(data.data(), data.size());
        ASSERT_TRUE(result.has_value());
        const auto& op = std::get<replay::SnapshotRequestOp>(result.value().operations[0]);
        ASSERT_TRUE(op.depth_limit.has_value());
        EXPECT_EQ(*op.depth_limit, 0U);
    }
}

} // namespace
