#include <binance_market_data/projection/v1/numeric/decimal_format.hpp>
#include <binance_market_data/projection/v1/numeric/decimal_parse.hpp>
#include <binance_market_data/projection/v1/numeric/decimal_scale.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <variant>

namespace bmd = binance_market_data::projection::v1;

namespace {

void require_invariant(bool condition) {
    if (!condition) {
        std::abort();
    }
}

[[nodiscard]] unsigned int decode_mode(std::uint8_t value) noexcept {
    if (value == static_cast<std::uint8_t>('P')) {
        return 0;
    }
    if (value == static_cast<std::uint8_t>('Q')) {
        return 1;
    }
    if (value == static_cast<std::uint8_t>('R')) {
        return 2;
    }
    return static_cast<unsigned int>(value) % 3U;
}

[[nodiscard]] std::uint32_t decode_scale(std::uint8_t value) noexcept {
    if (value >= static_cast<std::uint8_t>('0') && value <= static_cast<std::uint8_t>('9')) {
        return static_cast<std::uint32_t>(value - static_cast<std::uint8_t>('0'));
    }
    if (value >= static_cast<std::uint8_t>('A') && value <= static_cast<std::uint8_t>('I')) {
        return static_cast<std::uint32_t>(value - static_cast<std::uint8_t>('A')) + 10U;
    }
    return static_cast<std::uint32_t>(value) % 19U;
}

void fuzz_price(std::string_view text, bmd::DecimalScale scale) {
    const auto result = bmd::parse_price(text, scale);
    const auto* parsed = std::get_if<bmd::ParsedDecimal<bmd::PriceUnits>>(&result);
    if (parsed == nullptr) {
        return;
    }

    require_invariant(parsed->value.value() > 0);
    const auto formatted = bmd::format_price(parsed->value, scale, parsed->source_fraction_digits);
    const auto* reconstructed = std::get_if<std::string>(&formatted);
    require_invariant(reconstructed != nullptr);
    require_invariant(*reconstructed == text);
    const auto reparsed = bmd::parse_price(*reconstructed, scale);
    const auto* reparsed_value = std::get_if<bmd::ParsedDecimal<bmd::PriceUnits>>(&reparsed);
    require_invariant(reparsed_value != nullptr);
    require_invariant(*reparsed_value == *parsed);
}

void fuzz_quantity(std::string_view text, bmd::DecimalScale scale, bool require_positive) {
    const auto result = require_positive ? bmd::parse_positive_quantity(text, scale)
                                         : bmd::parse_quantity(text, scale);
    const auto* parsed = std::get_if<bmd::ParsedDecimal<bmd::QuantityUnits>>(&result);
    if (parsed == nullptr) {
        return;
    }

    require_invariant(parsed->value.value() >= 0);
    require_invariant(!require_positive || parsed->value.value() > 0);
    const auto formatted =
        bmd::format_quantity(parsed->value, scale, parsed->source_fraction_digits);
    const auto* reconstructed = std::get_if<std::string>(&formatted);
    require_invariant(reconstructed != nullptr);
    require_invariant(*reconstructed == text);
    const auto reparsed = require_positive ? bmd::parse_positive_quantity(*reconstructed, scale)
                                           : bmd::parse_quantity(*reconstructed, scale);
    const auto* reparsed_value = std::get_if<bmd::ParsedDecimal<bmd::QuantityUnits>>(&reparsed);
    require_invariant(reparsed_value != nullptr);
    require_invariant(*reparsed_value == *parsed);
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size < 2) {
        return 0;
    }

    const auto mode = decode_mode(data[0]);
    const auto scale_value = decode_scale(data[1]);
    const auto scale = *bmd::DecimalScale::create(scale_value);
    const std::string_view text{reinterpret_cast<const char*>(data + 2), size - 2};

    switch (mode) {
    case 0:
        fuzz_price(text, scale);
        break;
    case 1:
        fuzz_quantity(text, scale, false);
        break;
    case 2:
        fuzz_quantity(text, scale, true);
        break;
    }
    return 0;
}
