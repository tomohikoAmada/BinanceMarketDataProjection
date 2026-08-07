#include <binance_market_data/common/v1/enums.pb.h>
#include <binance_market_data/projection_adapter/v1/proto_adapter.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace {

namespace adapter = binance_market_data::projection_adapter::v1;
namespace common_wire = binance_market_data::common::v1;
namespace core = binance_market_data::projection::v1;
namespace market_wire = binance_market_data::market::v1;

class Generator final {
  public:
    explicit Generator(std::uint64_t seed) noexcept : state_{seed} {}
    [[nodiscard]] std::uint64_t next() noexcept {
        state_ = state_ * 6364136223846793005ULL + 1442695040888963407ULL;
        return state_;
    }

  private:
    std::uint64_t state_;
};

[[nodiscard]] core::DecimalScale scale(std::uint32_t value) {
    const auto result = core::DecimalScale::create(value);
    if (!result.has_value()) {
        ADD_FAILURE() << "invalid property-test scale: " << value;
        std::abort();
    }
    return result.value();
}

[[nodiscard]] core::NumericSpec spec(std::uint32_t value) { return {scale(value), scale(value)}; }

struct ReferenceDecimal final {
    std::uint64_t units;
    std::uint32_t scale;
};

[[nodiscard]] std::string reference_fixed(ReferenceDecimal value) {
    auto digits = std::to_string(value.units);
    if (value.scale == 0) {
        return digits;
    }
    if (digits.size() <= value.scale) {
        digits.insert(0, static_cast<std::size_t>(value.scale + 1) - digits.size(), '0');
    }
    digits.insert(digits.size() - value.scale, 1, '.');
    return digits;
}

// This parser is deliberately independent of M1 and the production adapter. It works only with
// primitive digits and explicit checked powers of ten.
[[nodiscard]] std::optional<std::uint64_t>
reference_parse(std::string_view text, std::uint32_t target_scale, bool allow_zero) {
    if (text.empty() || text.front() == '-' || text.front() == '+' || text.front() == '.' ||
        text.back() == '.') {
        return std::nullopt;
    }
    bool separator = false;
    std::uint32_t source_scale = 0;
    std::uint64_t digits = 0;
    for (const char character : text) {
        if (character == '.') {
            if (separator) {
                return std::nullopt;
            }
            separator = true;
            continue;
        }
        if (character < '0' || character > '9') {
            return std::nullopt;
        }
        const auto digit = static_cast<std::uint64_t>(character - '0');
        if (digits >
            (static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) - digit) / 10U) {
            return std::nullopt;
        }
        digits = digits * 10U + digit;
        if (separator) {
            ++source_scale;
        }
    }
    while (source_scale > target_scale) {
        if ((digits % 10U) != 0U) {
            return std::nullopt;
        }
        digits /= 10U;
        --source_scale;
    }
    while (source_scale < target_scale) {
        if (digits > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) / 10U) {
            return std::nullopt;
        }
        digits *= 10U;
        ++source_scale;
    }
    if (!allow_zero && digits == 0) {
        return std::nullopt;
    }
    return digits;
}

[[nodiscard]] market_wire::ExchangeDepthSnapshot baseline(std::uint32_t decimal_scale) {
    market_wire::ExchangeDepthSnapshot wire;
    wire.set_venue(common_wire::VENUE_BINANCE);
    wire.set_market(common_wire::MARKET_SPOT);
    wire.set_symbol("BTCUSDT");
    wire.set_schema_version("exchange-depth-snapshot.v1");
    wire.set_producer("property-model");
    wire.set_producer_version("1");
    wire.set_request_id("property-request");
    wire.set_last_update_id(100);
    auto* bid = wire.add_bids();
    bid->set_price(reference_fixed({12345, decimal_scale}));
    bid->set_quantity(reference_fixed({777, decimal_scale}));
    auto* ask = wire.add_asks();
    ask->set_price(reference_fixed({12346, decimal_scale}));
    ask->set_quantity(reference_fixed({888, decimal_scale}));
    return wire;
}

[[nodiscard]] market_wire::DepthUpdate update(std::string price, std::string quantity,
                                              std::uint64_t first, std::uint64_t final) {
    market_wire::DepthUpdate wire;
    auto* metadata = wire.mutable_metadata();
    metadata->set_venue(common_wire::VENUE_BINANCE);
    metadata->set_market(common_wire::MARKET_SPOT);
    metadata->set_symbol("BTCUSDT");
    metadata->set_producer("property-model");
    metadata->set_producer_version("1");
    metadata->set_connection_id("property-connection");
    metadata->set_stream(common_wire::STREAM_DIFF_DEPTH);
    metadata->set_schema_version("depth-update.v1");
    wire.set_first_update_id(first);
    wire.set_final_update_id(final);
    auto* bid = wire.add_bids();
    bid->set_price(std::move(price));
    bid->set_quantity(std::move(quantity));
    return wire;
}

[[nodiscard]] adapter::ExpectedIdentity identity() {
    return {"BTCUSDT", core::SequencePolicyKind::Spot};
}

[[nodiscard]] adapter::SnapshotContext context() {
    return {identity(), "property-model",    "1",         adapter::SnapshotOrigin::HistoryReplay,
            987654321,  std::uint64_t{1234}, std::nullopt};
}

[[nodiscard]] core::BookProjection make_projection(std::uint32_t decimal_scale) {
    core::BookProjection projection{spec(decimal_scale), core::SequencePolicyKind::Spot};
    auto adapted = adapter::adapt_exchange_depth_snapshot(baseline(decimal_scale),
                                                          spec(decimal_scale), identity());
    auto owner = std::move(std::get<adapter::AdaptedBookBaseline>(adapted));
    static_cast<void>(owner.install_into(projection));
    auto bridge = update(reference_fixed({12345, decimal_scale}),
                         reference_fixed({999, decimal_scale}), 99, 101);
    auto adapted_bridge = adapter::adapt_depth_update(bridge, spec(decimal_scale), identity());
    auto bridge_owner = std::move(std::get<adapter::AdaptedDepthBatch>(adapted_bridge));
    static_cast<void>(bridge_owner.apply_to(projection));
    return projection;
}

[[nodiscard]] adapter::SnapshotOptions snapshot_options() {
    adapter::SnapshotOptions options;
    options.depth_limit = std::get<adapter::DepthLimit>(adapter::DepthLimit::create(1));
    options.host_quality_facts = {adapter::HostQualityFact::Duplicate,
                                  adapter::HostQualityFact::Duplicate,
                                  adapter::HostQualityFact::Overlap};
    return options;
}

void expect_snapshot_prices(const core::LocalOrderBookSnapshot& output,
                            std::uint32_t decimal_scale) {
    EXPECT_EQ(output.bids(0).price(), reference_fixed({12345, decimal_scale}));
    EXPECT_EQ(output.asks(0).price(), reference_fixed({12346, decimal_scale}));
}

void expect_snapshot_quantities(const core::LocalOrderBookSnapshot& output,
                                std::uint32_t decimal_scale) {
    EXPECT_EQ(output.bids(0).quantity(), reference_fixed({999, decimal_scale}));
    EXPECT_EQ(output.asks(0).quantity(), reference_fixed({888, decimal_scale}));
}

void expect_snapshot_metadata(const core::LocalOrderBookSnapshot& output) {
    EXPECT_EQ(output.generated_time_utc_ns(), 987654321U);
    ASSERT_TRUE(output.has_generated_monotonic_ns());
    EXPECT_EQ(output.generated_monotonic_ns(), 1234U);
}

void expect_snapshot_quality(const core::LocalOrderBookSnapshot& output) {
    ASSERT_EQ(output.quality_flags_size(), 2);
    EXPECT_EQ(output.quality_flags(0), common_wire::QUALITY_FLAG_DUPLICATE);
    EXPECT_EQ(output.quality_flags(1), common_wire::QUALITY_FLAG_OVERLAP);
}

void expect_snapshot_shape(const core::LocalOrderBookSnapshot& output) {
    ASSERT_EQ(output.bids_size(), 1);
    ASSERT_EQ(output.asks_size(), 1);
}

void verify_output_for_scale(std::uint32_t decimal_scale) {
    auto first_projection = make_projection(decimal_scale);
    auto second_projection = make_projection(decimal_scale);
    const auto options = snapshot_options();
    const auto first =
        adapter::make_local_order_book_snapshot(first_projection, context(), options);
    const auto second =
        adapter::make_local_order_book_snapshot(second_projection, context(), options);
    ASSERT_TRUE(std::holds_alternative<core::LocalOrderBookSnapshot>(first));
    ASSERT_TRUE(std::holds_alternative<core::LocalOrderBookSnapshot>(second));
    const auto& output = std::get<core::LocalOrderBookSnapshot>(first);
    EXPECT_EQ(output.SerializeAsString(),
              std::get<core::LocalOrderBookSnapshot>(second).SerializeAsString());
    expect_snapshot_shape(output);
    expect_snapshot_prices(output, decimal_scale);
    expect_snapshot_quantities(output, decimal_scale);
    expect_snapshot_quality(output);
    expect_snapshot_metadata(output);
}

TEST(ProtoAdapterIndependentPropertyTest, MatchesPrimitiveInboundModelForFixedSeeds) {
    constexpr std::array<std::string_view, 8> invalid_decimals{
        "", ".", "1.", ".1", "+1", "1e2", "1_0", "9223372036854775808"};
    Generator generator{0x4D3450524F504552ULL};

    for (std::size_t iteration = 0; iteration < 5000; ++iteration) {
        const auto random = generator.next();
        const auto decimal_scale = static_cast<std::uint32_t>(random % 19U);
        const auto first = generator.next();
        auto final = generator.next();
        const auto selector = static_cast<unsigned int>((random >> 8U) % 12U);
        if (selector != 6U && first > final) {
            final = first;
        }

        std::string price = reference_fixed({(generator.next() % 1000000U) + 1U, decimal_scale});
        std::string quantity = reference_fixed({generator.next() % 1000000U, decimal_scale});
        bool reference_valid = true;
        auto wire = update(price, quantity, first, final);

        switch (selector) {
        case 0:
        case 1:
            break;
        case 2:
            wire.mutable_bids(0)->set_price(invalid_decimals.at(random % invalid_decimals.size()));
            break;
        case 3:
            wire.mutable_bids(0)->set_quantity("-1");
            break;
        case 4:
            wire.mutable_bids(0)->set_price("0");
            break;
        case 5:
            wire.mutable_bids(0)->set_price(price + "1");
            break;
        case 6:
            wire.set_first_update_id(std::numeric_limits<std::uint64_t>::max());
            wire.set_final_update_id(0);
            break;
        case 7:
            wire.mutable_metadata()->set_market(static_cast<common_wire::Market>(999));
            break;
        case 8:
            wire.mutable_metadata()->set_stream(common_wire::STREAM_BOOK_TICKER);
            break;
        case 9:
            wire.mutable_metadata()->mutable_quality_flags()->Add(999);
            break;
        case 10:
            wire.mutable_metadata()->set_symbol("ETHUSDT");
            break;
        case 11:
            wire.add_bids()->CopyFrom(wire.bids(0));
            break;
        default:
            FAIL() << "unreachable selector";
        }

        const auto price_reference = reference_parse(wire.bids(0).price(), decimal_scale, false);
        const auto quantity_reference =
            reference_parse(wire.bids(0).quantity(), decimal_scale, true);
        reference_valid = wire.first_update_id() <= wire.final_update_id() &&
                          price_reference.has_value() && quantity_reference.has_value() &&
                          wire.metadata().venue() == common_wire::VENUE_BINANCE &&
                          wire.metadata().market() == common_wire::MARKET_SPOT &&
                          wire.metadata().stream() == common_wire::STREAM_DIFF_DEPTH &&
                          wire.metadata().symbol() == "BTCUSDT";
        for (const auto raw_quality : wire.metadata().quality_flags()) {
            reference_valid = reference_valid && common_wire::QualityFlag_IsValid(raw_quality) &&
                              raw_quality != common_wire::QUALITY_FLAG_UNSPECIFIED;
        }

        const auto actual = adapter::adapt_depth_update(wire, spec(decimal_scale), identity());
        EXPECT_EQ(std::holds_alternative<adapter::AdaptedDepthBatch>(actual), reference_valid)
            << "iteration=" << iteration << " scale=" << decimal_scale << " selector=" << selector;
    }
}

TEST(ProtoAdapterIndependentPropertyTest, OutputMatchesPrimitiveModelAcrossAllScalesAndReplay) {
    for (std::uint32_t decimal_scale = 0; decimal_scale <= 18; ++decimal_scale) {
        verify_output_for_scale(decimal_scale);
    }
}

} // namespace
