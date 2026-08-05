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

struct ExactPropertyCase final {
    bmd::DecimalScale storage_scale;
    std::uint32_t output_fraction_digits;
    std::int64_t units;
    bool is_price;
};

[[nodiscard]] ExactPropertyCase make_exact_property_case(DeterministicGenerator& generator) {
    const auto storage_value = static_cast<std::uint32_t>(generator.bounded(19));
    const auto output_value = static_cast<std::uint32_t>(generator.bounded(19));
    auto units = random_nonnegative_units(generator);
    if (output_value < storage_value) {
        const auto divisor = kPowersOfTen.at(storage_value - output_value);
        units = (units / divisor) * divisor;
    }

    const bool is_price = generator.bounded(2) == 1;
    if (is_price && units == 0) {
        units = output_value < storage_value ? kPowersOfTen.at(storage_value - output_value) : 1;
    }
    return ExactPropertyCase{bmd_test::scale(storage_value), output_value, units, is_price};
}

void expect_price_roundtrip(const ExactPropertyCase& test_case) {
    const auto value = bmd_test::price_units(test_case.units);
    const auto formatted =
        bmd::format_price(value, test_case.storage_scale, test_case.output_fraction_digits);
    const auto* text = std::get_if<std::string>(&formatted);
    if (text == nullptr) {
        ADD_FAILURE() << "price format failed";
        return;
    }

    const auto parsed = bmd::parse_price(*text, test_case.storage_scale);
    const auto* parsed_value = std::get_if<bmd::ParsedDecimal<bmd::PriceUnits>>(&parsed);
    if (parsed_value == nullptr) {
        ADD_FAILURE() << "price parse failed";
        return;
    }
    EXPECT_EQ(parsed_value->value, value);
    EXPECT_EQ(parsed_value->source_fraction_digits, test_case.output_fraction_digits);

    const auto reconstructed = bmd::format_price(parsed_value->value, test_case.storage_scale,
                                                 parsed_value->source_fraction_digits);
    const auto* reconstructed_text = std::get_if<std::string>(&reconstructed);
    if (reconstructed_text == nullptr) {
        ADD_FAILURE() << "price reconstruction failed";
        return;
    }
    EXPECT_EQ(*reconstructed_text, *text);
}

void expect_quantity_roundtrip(const ExactPropertyCase& test_case) {
    const auto value = bmd_test::quantity_units(test_case.units);
    const auto formatted =
        bmd::format_quantity(value, test_case.storage_scale, test_case.output_fraction_digits);
    const auto* text = std::get_if<std::string>(&formatted);
    if (text == nullptr) {
        ADD_FAILURE() << "quantity format failed";
        return;
    }

    const auto parsed = bmd::parse_quantity(*text, test_case.storage_scale);
    const auto* parsed_value = std::get_if<bmd::ParsedDecimal<bmd::QuantityUnits>>(&parsed);
    if (parsed_value == nullptr) {
        ADD_FAILURE() << "quantity parse failed";
        return;
    }
    EXPECT_EQ(parsed_value->value, value);
    EXPECT_EQ(parsed_value->source_fraction_digits, test_case.output_fraction_digits);

    const auto reconstructed = bmd::format_quantity(parsed_value->value, test_case.storage_scale,
                                                    parsed_value->source_fraction_digits);
    const auto* reconstructed_text = std::get_if<std::string>(&reconstructed);
    if (reconstructed_text == nullptr) {
        ADD_FAILURE() << "quantity reconstruction failed";
        return;
    }
    EXPECT_EQ(*reconstructed_text, *text);
}

void expect_exact_roundtrip(const ExactPropertyCase& test_case) {
    if (test_case.is_price) {
        expect_price_roundtrip(test_case);
        return;
    }
    expect_quantity_roundtrip(test_case);
}

void expect_mutation_rejected(DeterministicGenerator& generator) {
    const auto scale_value = static_cast<std::uint32_t>(generator.bounded(19));
    const auto scale = bmd_test::scale(scale_value);
    const auto units = random_nonnegative_units(generator);
    const auto valid = bmd::format_quantity_fixed(bmd_test::quantity_units(units), scale);
    const auto* valid_text = std::get_if<std::string>(&valid);
    if (valid_text == nullptr) {
        ADD_FAILURE() << "quantity format failed";
        return;
    }

    auto mutated = *valid_text;
    const auto invalid_offset = static_cast<std::size_t>(generator.bounded(mutated.size()));
    mutated.at(invalid_offset) = 'x';
    bmd_test::expect_error(bmd::parse_quantity(mutated, scale),
                           bmd::DecimalErrorCode::InvalidSyntax, invalid_offset);
}

void expect_overflow_rejected(DeterministicGenerator& generator) {
    constexpr auto maximum = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    const auto delta = generator.bounded(4'096) + 1;
    const auto overflow_text = std::to_string(maximum + delta);
    bmd_test::expect_error(bmd::parse_quantity(overflow_text, bmd_test::scale(0)),
                           bmd::DecimalErrorCode::Overflow, overflow_text.size() - 1);
}

} // namespace

TEST(DecimalPropertyTest, ExactFormatsRoundtripForTenThousandDeterministicCases) {
    DeterministicGenerator generator{kPropertySeed};
    for (std::size_t case_index = 0; case_index < kValidPropertyCases; ++case_index) {
        SCOPED_TRACE(testing::Message() << "seed=" << kPropertySeed << " case=" << case_index);
        expect_exact_roundtrip(make_exact_property_case(generator));
    }
}

TEST(DecimalPropertyTest, MutationsAndOverflowBoundariesRejectTenThousandCases) {
    DeterministicGenerator generator{kPropertySeed ^ 0xBAD5CA1EULL};

    for (std::size_t case_index = 0; case_index < kInvalidPropertyCases; ++case_index) {
        SCOPED_TRACE(testing::Message()
                     << "seed=" << (kPropertySeed ^ 0xBAD5CA1EULL) << " case=" << case_index);
        if (case_index % 2 == 0) {
            expect_mutation_rejected(generator);
        } else {
            expect_overflow_rejected(generator);
        }
    }
}
