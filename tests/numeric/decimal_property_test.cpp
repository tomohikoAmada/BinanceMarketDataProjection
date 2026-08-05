#include "test_helpers.hpp"

#include <binance_market_data/projection/v1/numeric/decimal_format.hpp>
#include <binance_market_data/projection/v1/numeric/decimal_parse.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <variant>

namespace bmd = binance_market_data::projection::v1;

namespace {

constexpr std::uint64_t kPropertySeed = 0x6D315F4E554D4552ULL;
constexpr std::size_t kValidPropertyCases = 10'000;
constexpr std::size_t kInvalidPropertyCases = 10'000;

constexpr std::array<std::int64_t, 19> kPowersOfTen{
    1,
    10,
    100,
    1'000,
    10'000,
    100'000,
    1'000'000,
    10'000'000,
    100'000'000,
    1'000'000'000,
    10'000'000'000,
    100'000'000'000,
    1'000'000'000'000,
    10'000'000'000'000,
    100'000'000'000'000,
    1'000'000'000'000'000,
    10'000'000'000'000'000,
    100'000'000'000'000'000,
    1'000'000'000'000'000'000,
};

class DeterministicGenerator final {
  public:
    explicit DeterministicGenerator(std::uint64_t seed) : state_{seed} {}

    [[nodiscard]] std::uint64_t next() noexcept {
        state_ ^= state_ >> 12U;
        state_ ^= state_ << 25U;
        state_ ^= state_ >> 27U;
        return state_ * 0x2545F4914F6CDD1DULL;
    }

    [[nodiscard]] std::uint64_t bounded(std::uint64_t exclusive_upper_bound) noexcept {
        return next() % exclusive_upper_bound;
    }

  private:
    std::uint64_t state_;
};

[[nodiscard]] std::int64_t random_nonnegative_units(DeterministicGenerator& generator) {
    constexpr auto maximum = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    return static_cast<std::int64_t>(generator.next() & maximum);
}

} // namespace

TEST(DecimalPropertyTest, ExactFormatsRoundtripForTenThousandDeterministicCases) {
    DeterministicGenerator generator{kPropertySeed};
    for (std::size_t case_index = 0; case_index < kValidPropertyCases; ++case_index) {
        SCOPED_TRACE(testing::Message() << "seed=" << kPropertySeed << " case=" << case_index);
        const auto storage_value = static_cast<std::uint32_t>(generator.bounded(19));
        const auto output_value = static_cast<std::uint32_t>(generator.bounded(19));
        const auto storage_scale = bmd_test::scale(storage_value);

        auto units = random_nonnegative_units(generator);
        if (output_value < storage_value) {
            const auto divisor = kPowersOfTen.at(storage_value - output_value);
            units = (units / divisor) * divisor;
        }

        const bool test_price = generator.bounded(2) == 1;
        if (test_price && units == 0) {
            units = 1;
            if (output_value < storage_value) {
                units = kPowersOfTen.at(storage_value - output_value);
            }
        }

        if (test_price) {
            const auto value = bmd_test::price_units(units);
            const auto formatted = bmd::format_price(value, storage_scale, output_value);
            ASSERT_TRUE(std::holds_alternative<std::string>(formatted));
            const auto& text = std::get<std::string>(formatted);
            const auto parsed = bmd::parse_price(text, storage_scale);
            ASSERT_TRUE(std::holds_alternative<bmd::ParsedDecimal<bmd::PriceUnits>>(parsed));
            const auto parsed_value = std::get<bmd::ParsedDecimal<bmd::PriceUnits>>(parsed);
            EXPECT_EQ(parsed_value.value, value);
            EXPECT_EQ(parsed_value.source_fraction_digits, output_value);
            const auto reconstructed = bmd::format_price(parsed_value.value, storage_scale,
                                                         parsed_value.source_fraction_digits);
            ASSERT_TRUE(std::holds_alternative<std::string>(reconstructed));
            EXPECT_EQ(std::get<std::string>(reconstructed), text);
        } else {
            const auto value = bmd_test::quantity_units(units);
            const auto formatted = bmd::format_quantity(value, storage_scale, output_value);
            ASSERT_TRUE(std::holds_alternative<std::string>(formatted));
            const auto& text = std::get<std::string>(formatted);
            const auto parsed = bmd::parse_quantity(text, storage_scale);
            ASSERT_TRUE(std::holds_alternative<bmd::ParsedDecimal<bmd::QuantityUnits>>(parsed));
            const auto parsed_value = std::get<bmd::ParsedDecimal<bmd::QuantityUnits>>(parsed);
            EXPECT_EQ(parsed_value.value, value);
            EXPECT_EQ(parsed_value.source_fraction_digits, output_value);
            const auto reconstructed = bmd::format_quantity(parsed_value.value, storage_scale,
                                                            parsed_value.source_fraction_digits);
            ASSERT_TRUE(std::holds_alternative<std::string>(reconstructed));
            EXPECT_EQ(std::get<std::string>(reconstructed), text);
        }
    }
}

TEST(DecimalPropertyTest, MutationsAndOverflowBoundariesRejectTenThousandCases) {
    DeterministicGenerator generator{kPropertySeed ^ 0xBAD5CA1EULL};
    constexpr auto maximum = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());

    for (std::size_t case_index = 0; case_index < kInvalidPropertyCases; ++case_index) {
        SCOPED_TRACE(testing::Message()
                     << "seed=" << (kPropertySeed ^ 0xBAD5CA1EULL) << " case=" << case_index);
        if (case_index % 2 == 0) {
            const auto scale_value = static_cast<std::uint32_t>(generator.bounded(19));
            const auto scale = bmd_test::scale(scale_value);
            const auto units = random_nonnegative_units(generator);
            const auto valid = bmd::format_quantity_fixed(bmd_test::quantity_units(units), scale);
            ASSERT_TRUE(std::holds_alternative<std::string>(valid));
            auto mutated = std::get<std::string>(valid);
            const auto invalid_offset = static_cast<std::size_t>(generator.bounded(mutated.size()));
            mutated[invalid_offset] = 'x';
            bmd_test::expect_error(bmd::parse_quantity(mutated, scale),
                                   bmd::DecimalErrorCode::InvalidSyntax, invalid_offset);
        } else {
            const auto delta = generator.bounded(4'096) + 1;
            const auto overflow_text = std::to_string(maximum + delta);
            const auto result = bmd::parse_quantity(overflow_text, bmd_test::scale(0));
            ASSERT_TRUE(std::holds_alternative<bmd::DecimalError>(result));
            EXPECT_EQ(std::get<bmd::DecimalError>(result).code, bmd::DecimalErrorCode::Overflow);
        }
    }
}
