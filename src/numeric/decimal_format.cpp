#include <binance_market_data/projection/v1/numeric/decimal_format.hpp>

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <system_error>

namespace binance_market_data::projection::v1 {
namespace {

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

[[nodiscard]] DecimalFormatResult format_units(std::int64_t units, DecimalScale storage_scale,
                                               std::size_t output_fraction_digits) {
    const auto storage_fraction_digits = static_cast<std::size_t>(storage_scale.value());
    auto rendered_units = units;
    auto rendered_fraction_digits = storage_fraction_digits;

    if (output_fraction_digits < storage_fraction_digits) {
        const auto scale_difference = storage_fraction_digits - output_fraction_digits;
        const auto divisor = kPowersOfTen.at(scale_difference);
        if (rendered_units % divisor != 0) {
            return DecimalError{DecimalErrorCode::InexactScale, kNoErrorOffset};
        }
        rendered_units /= divisor;
        rendered_fraction_digits = output_fraction_digits;
    }

    std::array<char, 20> digit_buffer{};
    const auto conversion = std::to_chars(
        digit_buffer.data(), digit_buffer.data() + digit_buffer.size(), rendered_units);
    if (conversion.ec != std::errc{}) {
        return DecimalError{DecimalErrorCode::Overflow, kNoErrorOffset};
    }
    const auto digit_count = static_cast<std::size_t>(conversion.ptr - digit_buffer.data());

    std::size_t base_length = digit_count;
    if (rendered_fraction_digits > 0) {
        base_length =
            digit_count > rendered_fraction_digits ? digit_count + 1 : rendered_fraction_digits + 2;
    }

    const auto extra_zeroes = output_fraction_digits - rendered_fraction_digits;
    const bool needs_decimal_point = rendered_fraction_digits == 0 && extra_zeroes > 0;
    std::string output;
    const auto extra_decimal_point = needs_decimal_point ? std::size_t{1} : std::size_t{0};
    if (extra_zeroes > output.max_size() - base_length - extra_decimal_point) {
        return DecimalError{DecimalErrorCode::Overflow, kNoErrorOffset};
    }
    output.reserve(base_length + extra_decimal_point + extra_zeroes);

    if (rendered_fraction_digits == 0) {
        output.append(digit_buffer.data(), digit_count);
    } else if (digit_count <= rendered_fraction_digits) {
        output.append("0.");
        output.append(rendered_fraction_digits - digit_count, '0');
        output.append(digit_buffer.data(), digit_count);
    } else {
        const auto integer_digit_count = digit_count - rendered_fraction_digits;
        output.append(digit_buffer.data(), integer_digit_count);
        output.push_back('.');
        output.append(digit_buffer.data() + integer_digit_count, rendered_fraction_digits);
    }

    if (needs_decimal_point) {
        output.push_back('.');
    }
    output.append(extra_zeroes, '0');
    return output;
}

} // namespace

DecimalFormatResult format_price(PriceUnits value, DecimalScale storage_scale,
                                 std::size_t output_fraction_digits) {
    return format_units(value.value(), storage_scale, output_fraction_digits);
}

DecimalFormatResult format_quantity(QuantityUnits value, DecimalScale storage_scale,
                                    std::size_t output_fraction_digits) {
    return format_units(value.value(), storage_scale, output_fraction_digits);
}

DecimalFormatResult format_price_fixed(PriceUnits value, DecimalScale storage_scale) {
    return format_price(value, storage_scale, storage_scale.value());
}

DecimalFormatResult format_quantity_fixed(QuantityUnits value, DecimalScale storage_scale) {
    return format_quantity(value, storage_scale, storage_scale.value());
}

} // namespace binance_market_data::projection::v1
