#include "canonical_text.hpp"
#include "replay_parser.hpp"

#include <gtest/gtest.h>

#include <string>
#include <variant>

namespace replay = bmd_projection::m5::replay;

namespace {

constexpr char kHeader[] = "REPLAY_V1 market=Spot symbol=BTCUSDT price_scale=8 quantity_scale=8 "
                           "policy=Spot fixture_id=x\n";

void expect_category(const replay::Result<std::monostate>& result, replay::ErrorCategory category) {
    ASSERT_TRUE(std::holds_alternative<replay::ParseError>(result));
    EXPECT_EQ(std::get<replay::ParseError>(result).category, category);
}

void expect_parse_error(std::string text, replay::ErrorCategory category) {
    const auto result = replay::parse_replay_log(text);
    ASSERT_TRUE(std::holds_alternative<replay::ParseError>(result));
    EXPECT_EQ(std::get<replay::ParseError>(result).category, category);
}

} // namespace

TEST(M5ReplayParserTest, ParsesAllOperationKindsAndPreservesDecimalLexemes) {
    const std::string log = std::string{kHeader} +
                            "INSTALL_BASELINE 10 B:1.2300,2|B:1.2,3 A:1.3,4\n"
                            "ADAPTER_METADATA Duplicate,OutOfOrder\n"
                            "DEPTH_UPDATE 9 11 pu=- B:1.2300,+1.5|A:1.3,-0\n"
                            "REBASELINE 20 - -\n"
                            "RESET\n"
                            "SNAPSHOT_REQUEST - - snap producer version RecorderReplay 123 - -\n"
                            "MALFORMED_RANGE 9 3\n";
    const auto result = replay::parse_replay_log(log);
    ASSERT_TRUE(std::holds_alternative<replay::NormalizedReplay>(result));
    const auto& normalized = std::get<replay::NormalizedReplay>(result);
    ASSERT_EQ(normalized.operations.size(), 7U);
    const auto& baseline = std::get<replay::InstallBaselineOp>(normalized.operations[0]);
    EXPECT_EQ(baseline.bids[0].price, "1.2300");
    EXPECT_EQ(baseline.bids[1].price, "1.2");
    const auto& update = std::get<replay::DepthUpdateOp>(normalized.operations[2]);
    EXPECT_FALSE(update.previous_final.has_value());
    EXPECT_EQ(update.levels[0].quantity, "+1.5");
    EXPECT_EQ(update.levels[1].quantity, "-0");
    EXPECT_EQ(std::get<replay::MalformedRangeOp>(normalized.operations[6]).first_update_id, 9U);
    EXPECT_EQ(normalized.operations[1].index(), 5U);
    EXPECT_EQ(normalized.operations[0].index(), 0U);
}

TEST(M5ReplayParserTest, RetainsSourceEventAndLineDiagnostics) {
    const auto result =
        replay::parse_replay_log(std::string{kHeader} + "RESET\n" + "MALFORMED_RANGE 4 2\n");
    ASSERT_TRUE(std::holds_alternative<replay::NormalizedReplay>(result));
    const auto& operations = std::get<replay::NormalizedReplay>(result).operations;
    EXPECT_EQ(std::get<replay::ResetOp>(operations[0]).source,
              (replay::SourceLocation{0, 2, "RESET"}));
    EXPECT_EQ(std::get<replay::MalformedRangeOp>(operations[1]).source.line_number, 3U);
}

TEST(M5ReplayParserTest, RejectsPendingAdapterMetadataUnlessImmediatelyFollowedByUpdate) {
    expect_parse_error(std::string{kHeader} + "ADAPTER_METADATA Duplicate\n",
                       replay::ErrorCategory::ReplaySyntax);
    expect_parse_error(std::string{kHeader} + "ADAPTER_METADATA Duplicate\nRESET\n",
                       replay::ErrorCategory::ReplaySyntax);
}

TEST(M5ReplayParserTest, AcceptsMalformedDomainRangeAsAValidReplayEvent) {
    const auto result = replay::parse_replay_log(std::string{kHeader} + "MALFORMED_RANGE 9 3\n");
    ASSERT_TRUE(std::holds_alternative<replay::NormalizedReplay>(result));
    EXPECT_EQ(
        std::get<replay::MalformedRangeOp>(std::get<replay::NormalizedReplay>(result).operations[0])
            .final_update_id,
        3U);
}

TEST(M5ReplayParserTest, RejectsUnknownEventsArityAndNonCanonicalIntegers) {
    expect_parse_error(std::string{kHeader} + "UNKNOWN\n", replay::ErrorCategory::ReplaySyntax);
    expect_parse_error(std::string{kHeader} + "RESET extra\n", replay::ErrorCategory::ReplaySyntax);
    expect_parse_error(std::string{kHeader} + "MALFORMED_RANGE 01 2\n",
                       replay::ErrorCategory::ReplaySyntax);
    expect_parse_error(std::string{kHeader} + "MALFORMED_RANGE 18446744073709551616 2\n",
                       replay::ErrorCategory::ReplaySyntax);
    expect_parse_error(std::string{kHeader} + "MALFORMED_RANGE +1 2\n",
                       replay::ErrorCategory::ReplaySyntax);
    expect_parse_error(std::string{kHeader} + "DEPTH_UPDATE 1 2 pu=-0 B:1,2\n",
                       replay::ErrorCategory::ReplaySyntax);
    expect_parse_error(std::string{kHeader} + "INSTALL_BASELINE 1 B:1,2# -\n",
                       replay::ErrorCategory::ReplaySyntax);
}

TEST(M5ReplayParserTest, CanonicalByteValidationRejectsAllRequiredLineViolations) {
    const std::string valid = std::string{kHeader} + "RESET\n";
    for (const auto& invalid : {
             std::string{"\xef\xbb\xbf"} + valid,
             std::string{kHeader} + "RESET\r\n",
             std::string{kHeader} + "RESET\r",
             std::string{kHeader} + "RESET",
             std::string{kHeader} + "\n",
             std::string{kHeader} + " RESET\n",
             std::string{kHeader} + "RESET \n",
             std::string{kHeader} + "RE\tSET\n",
             std::string{kHeader} + "RESET  extra\n",
         }) {
        expect_category(replay::validate_canonical_bytes(invalid),
                        replay::ErrorCategory::InvalidCanonicalBytes);
    }
}

TEST(M5ReplayParserTest, RejectsMalformedUtf8) {
    expect_category(replay::validate_canonical_bytes(std::string{"\xc0\xaf\n"}),
                    replay::ErrorCategory::InvalidCanonicalBytes);
    expect_category(replay::validate_canonical_bytes(std::string{"\xe2\x82\n"}),
                    replay::ErrorCategory::InvalidCanonicalBytes);
    expect_category(replay::validate_canonical_bytes(std::string{"\xed\xa0\x80\n"}),
                    replay::ErrorCategory::InvalidCanonicalBytes);
    expect_category(replay::validate_canonical_bytes(std::string{"\xf4\x90\x80\x80\n"}),
                    replay::ErrorCategory::InvalidCanonicalBytes);
}

TEST(M5ReplayParserTest, IntegerHelpersUseCanonicalUnsignedSpelling) {
    EXPECT_TRUE(replay::is_canonical_integer("0"));
    EXPECT_TRUE(replay::is_canonical_integer("18446744073709551615"));
    EXPECT_FALSE(replay::is_canonical_integer("00"));
    EXPECT_FALSE(replay::is_canonical_integer("-0"));
    EXPECT_FALSE(replay::is_canonical_integer("+1"));
    EXPECT_FALSE(replay::is_canonical_integer("0x1"));
}
