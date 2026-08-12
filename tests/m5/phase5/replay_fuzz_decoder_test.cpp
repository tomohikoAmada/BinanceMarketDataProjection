#include "replay_fuzz_decoder.hpp"

#include <gtest/gtest.h>

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

TEST(ReplayFuzzDecoderTest, EmptyInputReturnsNullopt) {
    EXPECT_FALSE(decode(nullptr, 0).has_value());
    const std::uint8_t* null_data = nullptr;
    EXPECT_FALSE(decode(null_data, 1).has_value());
}

TEST(ReplayFuzzDecoderTest, MinimumSizeReturnsValid) {
    // Header + 1 op count + 1 op
    const std::uint8_t data[] = {0x20, 0x08, 0x07, 'B', 'T', 'C', 'U', 'S', 'D', 'T',
                                 0x01,  // operation count
                                 0x03}; // opcode 3 = Reset
    auto result = decode(data, sizeof(data));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->market, replay::Market::Spot);
    EXPECT_EQ(result->numeric_spec.price_scale, 8U);
    EXPECT_EQ(result->numeric_spec.quantity_scale, 8U);
    EXPECT_EQ(result->operations.size(), 1U);
}

TEST(ReplayFuzzDecoderTest, SingleByteInput) {
    const std::uint8_t data[] = {0xAA};
    auto result = decode(data, sizeof(data));
    // Not enough for a complete operation
    EXPECT_FALSE(result.has_value() || result.has_value());
    // At minimum the header alone isn't enough without any operations
}

TEST(ReplayFuzzDecoderTest, CoreModeDecoded) {
    const std::uint8_t data[] = {0x20,                // Core, Spot, price_scale=8
                                 0x08,                // quantity_scale=8
                                 0x03, 'B', 'T', 'C', // symbol "BTC"
                                 0x01,                // 1 operation
                                 0x03};               // Reset
    auto result = decode(data, sizeof(data));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->mode, decoder::DecodedMode::CoreOnly);
    EXPECT_EQ(result->symbol, "BTC");
}

TEST(ReplayFuzzDecoderTest, AdapterModeDecoded) {
    const std::uint8_t data[] = {0x21,                // Adapter, Spot, price_scale=8
                                 0x08,                // quantity_scale=8
                                 0x03, 'B', 'T', 'C', // symbol "BTC"
                                 0x01,                // 1 operation
                                 0x03,                // Reset
                                 // Adapter scenario bytes
                                 0x00,                // venue=Binance, market=Spot
                                 0x03, 'B', 'T', 'C', // adapter_wire_symbol
                                 0x03, 'B', 'T', 'C', // adapter_expected_symbol
                                 0x08, 0x08,          // conversion spec
                                 0x08, 0x08,          // projection spec
                                 0x00};               // policy
    auto result = decode(data, sizeof(data));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->mode, decoder::DecodedMode::AdapterEnabled);
    EXPECT_EQ(result->scenario_venue, FuzzVenue::Binance);
    EXPECT_EQ(result->scenario_market, FuzzMarket::Spot);
}

TEST(ReplayFuzzDecoderTest, UsdmMarketDecoded) {
    const std::uint8_t data[] = {0x22,                // Core, UsdM, price_scale=8
                                 0x08,                // quantity_scale=8
                                 0x03, 'E', 'T', 'H', // symbol "ETH"
                                 0x01,                // 1 operation
                                 0x03};               // Reset
    auto result = decode(data, sizeof(data));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->market, replay::Market::UsdMPerpetual);
    EXPECT_EQ(result->sequence_policy, replay::SequencePolicy::UsdMPerpetual);
}

TEST(ReplayFuzzDecoderTest, AllOperationTypesReachable) {
    // InstallBaseline
    {
        const std::uint8_t data[] = {
            0x20, 0x08, 0x03, 'B', 'T', 'C',
            0x01,             // 1 operation
            0x00,             // InstallBaseline
            0x18,             // last_update_id = 1 (special)
            0x01,             // 1 bid level
            0x00,             // side = Bid
            0x00, 0x32, 0x01, // decimal token style, byte1 (int_len=2, 5,0), int digit
            0x00, 0x32, 0x01, // quantity token
            0x00};            // 0 asks
        auto result = decode(data, sizeof(data));
        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(result->operations.size(), 1U);
        EXPECT_TRUE(std::holds_alternative<replay::InstallBaselineOp>(result->operations[0]));
    }

    // DepthUpdate
    {
        const std::uint8_t data[] = {0x20, 0x08, 0x03, 'B', 'T', 'C',
                                     0x01,  // 1 operation
                                     0x01,  // DepthUpdate
                                     0x18,  // first = 1
                                     0x18,  // final = 1
                                     0x00,  // no previous
                                     0x00}; // 0 levels
        auto result = decode(data, sizeof(data));
        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(result->operations.size(), 1U);
        EXPECT_TRUE(std::holds_alternative<replay::DepthUpdateOp>(result->operations[0]));
    }

    // Rebaseline
    {
        const std::uint8_t data[] = {0x20, 0x08, 0x03, 'B', 'T', 'C',
                                     0x01,  // 1 operation
                                     0x02,  // Rebaseline
                                     0x18,  // last_update_id = 1
                                     0x00,  // 0 bids
                                     0x00}; // 0 asks
        auto result = decode(data, sizeof(data));
        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(result->operations.size(), 1U);
        EXPECT_TRUE(std::holds_alternative<replay::RebaselineOp>(result->operations[0]));
    }

    // Reset
    {
        const std::uint8_t data[] = {0x20, 0x08, 0x03, 'B', 'T', 'C',
                                     0x01,  // 1 operation
                                     0x03}; // Reset
        auto result = decode(data, sizeof(data));
        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(result->operations.size(), 1U);
        EXPECT_TRUE(std::holds_alternative<replay::ResetOp>(result->operations[0]));
    }

    // SnapshotRequest
    {
        const std::uint8_t data[] = {0x20, 0x08, 0x03, 'B', 'T', 'C',
                                     0x01,                  // 1 operation
                                     0x04,                  // SnapshotRequest
                                     0x01, 0x05,            // depth_limit=5
                                     0x00,                  // 0 quality facts
                                     0x03, 'a',  'b',  'c', // snapshot_id
                                     0x01, 'x',             // producer
                                     0x01, 'y',             // producer_version
                                     0x00,                  // GatewayLive
                                     0x18,                  // generated_time = 1
                                     0x00,                  // no monotonic
                                     0x00};                 // no gap
        auto result = decode(data, sizeof(data));
        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(result->operations.size(), 1U);
        EXPECT_TRUE(std::holds_alternative<replay::SnapshotRequestOp>(result->operations[0]));
    }

    // AdapterMetadata
    {
        const std::uint8_t data[] = {0x20, 0x08, 0x03, 'B', 'T', 'C',
                                     0x01,  // 1 operation
                                     0x05,  // AdapterMetadata
                                     0x00}; // 0 quality facts
        auto result = decode(data, sizeof(data));
        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(result->operations.size(), 1U);
        EXPECT_TRUE(std::holds_alternative<replay::AdapterMetadataOp>(result->operations[0]));
    }

    // MalformedRange
    {
        const std::uint8_t data[] = {0x20, 0x08, 0x03, 'B', 'T', 'C',
                                     0x01,  // 1 operation
                                     0x06,  // MalformedRange
                                     0x28,  // first = 2 (special)
                                     0x18}; // final = 1 (special) → first > final
        auto result = decode(data, sizeof(data));
        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(result->operations.size(), 1U);
        EXPECT_TRUE(std::holds_alternative<replay::MalformedRangeOp>(result->operations[0]));
    }
}

TEST(ReplayFuzzDecoderTest, Uint64SpecialValuesReachable) {
    // Test that ByteCursor::read_var_u64 reaches all 6 special values.
    // Special flag bit 3 set, special index in bits 4-6.
    auto test_value = [](std::uint8_t byte, std::uint64_t expected) {
        const std::uint8_t data[] = {byte};
        ByteCursor cursor(data, sizeof(data));
        EXPECT_EQ(cursor.read_var_u64(), expected);
    };

    test_value(0x08, 0);              // special=0 → 0
    test_value(0x18, 1);              // special=1 → 1
    test_value(0x28, 2);              // special=2 → 2
    test_value(0x38, UINT64_MAX);     // special=3 → UINT64_MAX
    test_value(0x48, UINT64_MAX - 1); // special=4 → UINT64_MAX-1
    test_value(0x58, UINT64_MAX - 2); // special=5 → UINT64_MAX-2
}

TEST(ReplayFuzzDecoderTest, NumericSpecBounds) {
    // Test that invalid scales are clamped.
    {
        const std::uint8_t data[] = {0xFC, // price_scale bits 2-7 = 0x3F = 63
                                     0xFF, // quantity_scale = 255
                                     0x03, 'B', 'T', 'C', 0x01, 0x03};
        auto result = decode(data, sizeof(data));
        ASSERT_TRUE(result.has_value());
        // 63 % 19 = 6, 255 % 19 = 8
        EXPECT_EQ(result->numeric_spec.price_scale, 6U);
        EXPECT_EQ(result->numeric_spec.quantity_scale, 8U);
    }
}

TEST(ReplayFuzzDecoderTest, PerInputOperationCap) {
    const std::uint8_t header[] = {0x20, 0x08, 0x03, 'B', 'T', 'C'};
    std::vector<std::uint8_t> data;
    data.insert(data.end(), std::begin(header), std::end(header));
    data.push_back(128); // operation count
    for (int i = 0; i < 128; ++i) {
        data.push_back(0x03); // Reset (1 byte each)
    }
    auto result = decode(data.data(), data.size());
    ASSERT_TRUE(result.has_value());
    EXPECT_LE(result->operations.size(), decoder::kMaxOperations);
}

TEST(ReplayFuzzDecoderTest, DeterministicIdenticalInput) {
    const std::uint8_t data[] = {0x20, 0x08, 0x03, 'B', 'T', 'C',
                                 0x02,  // 2 operations
                                 0x03,  // Reset
                                 0x03}; // Reset
    auto r1 = decode(data, sizeof(data));
    auto r2 = decode(data, sizeof(data));
    ASSERT_TRUE(r1.has_value());
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(r1->operations.size(), r2->operations.size());
    EXPECT_EQ(r1->symbol, r2->symbol);
    EXPECT_EQ(r1->market, r2->market);
}

TEST(ReplayFuzzDecoderTest, MalformedRangeReachability) {
    const std::uint8_t data[] = {0x20, 0x08, 0x03, 'B', 'T', 'C',
                                 0x01,  // 1 operation
                                 0x06,  // MalformedRange
                                 0x28,  // first = 2
                                 0x18}; // final = 1 —— first > final
    auto result = decode(data, sizeof(data));
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<replay::MalformedRangeOp>(result->operations[0]));
    const auto& op = std::get<replay::MalformedRangeOp>(result->operations[0]);
    EXPECT_EQ(op.first_update_id, 2U);
    EXPECT_EQ(op.final_update_id, 1U);
    EXPECT_GT(op.first_update_id, op.final_update_id);
}

TEST(ReplayFuzzDecoderTest, LevelCountCap) {
    const std::uint8_t data[] = {
        0x20,
        0x08,
        0x03,
        'B',
        'T',
        'C',
        0x01, // 1 operation
        0x00, // InstallBaseline
        0x18, // last_update_id = 1
        // Request 20 bid levels (capped at 8)
        20,
    };
    // Need enough bytes for 8 levels (each level needs side byte + 2 token headers)
    // Add padding bytes
    std::vector<std::uint8_t> d(std::begin(data), std::end(data));
    for (int i = 0; i < 100; ++i)
        d.push_back(0x00);
    auto result = decode(d.data(), d.size());
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<replay::InstallBaselineOp>(result->operations[0]));
    const auto& op = std::get<replay::InstallBaselineOp>(result->operations[0]);
    EXPECT_LE(op.bids.size(), decoder::kMaxLevelsPerEvent);
}

TEST(ReplayFuzzDecoderTest, SnapshotDepthLimitVariants) {
    {
        const std::uint8_t data[] = {0x20, 0x08, 0x03, 'B',
                                     'T',  'C',  0x01, 0x04, // SnapshotRequest
                                     0x00,                   // no depth limit
                                     0x00, 0x00, 0x00, 0x00, // empty strings
                                     0x00, 0x00,
                                     0x00,              // origin
                                     0x18, 0x00, 0x00}; // time, no monotonic, no gap
        auto result = decode(data, sizeof(data));
        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(std::holds_alternative<replay::SnapshotRequestOp>(result->operations[0]));
        auto& op = std::get<replay::SnapshotRequestOp>(result->operations[0]);
        EXPECT_FALSE(op.depth_limit.has_value());
    }
    {
        const std::uint8_t data[] = {
            0x20, 0x08, 0x03, 'B',  'T',  'C',  0x01, 0x04, // SnapshotRequest
            0x01, 0x00,                                     // depth_limit = 0
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00};
        auto result = decode(data, sizeof(data));
        ASSERT_TRUE(result.has_value());
        auto& op = std::get<replay::SnapshotRequestOp>(result->operations[0]);
        ASSERT_TRUE(op.depth_limit.has_value());
        EXPECT_EQ(*op.depth_limit, 0U);
    }
}

} // namespace
