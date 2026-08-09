#include "production_decimal_observation.hpp"

#include "canonical_convert.hpp"
#include "operation_observation.hpp"

#include "replay_types.hpp"

#include <binance_market_data/projection/v1/numeric/decimal_parse.hpp>

#include <cstddef>
#include <optional>
#include <variant>

namespace bmd_projection::m5::oracle {
namespace {

namespace core = binance_market_data::projection::v1;

template <typename Units>
[[nodiscard]] CanonicalDecimalObservation
success_observation(const replay::LevelInput& level, std::size_t position,
                    CanonicalDecimalRole role, const core::ParsedDecimal<Units>& parsed,
                    core::DecimalScale storage_scale) {
    return {level.side == replay::Side::Bid ? CanonicalBookSide::Bid : CanonicalBookSide::Ask,
            position, role,
            CanonicalDecimalValue{parsed.value.value(), storage_scale.value(),
                                  parsed.source_fraction_digits}};
}

[[nodiscard]] CanonicalDecimalObservation failure_observation(const replay::LevelInput& level,
                                                              std::size_t position,
                                                              CanonicalDecimalRole role,
                                                              const core::DecimalError& error) {
    return {level.side == replay::Side::Bid ? CanonicalBookSide::Bid : CanonicalBookSide::Ask,
            position, role, CanonicalDecimalFailure{to_canonical(error.code), error.offset}};
}

void record_first_error(ProductionLevelObservation& observation,
                        const core::DecimalError& error) noexcept {
    if (!observation.first_error.has_value()) {
        observation.first_error = to_canonical(error.code);
    }
}

} // namespace

ProductionLevelObservation observe_production_levels(const std::vector<replay::LevelInput>& levels,
                                                     core::NumericSpec numeric_spec) {
    ProductionLevelObservation observation;
    observation.decimals.reserve(levels.size() * 2);
    observation.levels.reserve(levels.size());
    for (std::size_t position = 0; position < levels.size(); ++position) {
        const auto& level = levels[position];
        const auto price = core::parse_price(level.price, numeric_spec.price_scale);
        const auto quantity = core::parse_quantity(level.quantity, numeric_spec.quantity_scale);

        if (const auto* failure = std::get_if<core::DecimalError>(&price)) {
            observation.decimals.push_back(
                failure_observation(level, position, CanonicalDecimalRole::Price, *failure));
            record_first_error(observation, *failure);
        } else {
            observation.decimals.push_back(success_observation(
                level, position, CanonicalDecimalRole::Price,
                std::get<core::ParsedDecimal<core::PriceUnits>>(price), numeric_spec.price_scale));
        }

        if (const auto* failure = std::get_if<core::DecimalError>(&quantity)) {
            observation.decimals.push_back(
                failure_observation(level, position, CanonicalDecimalRole::Quantity, *failure));
            record_first_error(observation, *failure);
        } else {
            observation.decimals.push_back(
                success_observation(level, position, CanonicalDecimalRole::Quantity,
                                    std::get<core::ParsedDecimal<core::QuantityUnits>>(quantity),
                                    numeric_spec.quantity_scale));
        }

        if (std::holds_alternative<core::ParsedDecimal<core::PriceUnits>>(price) &&
            std::holds_alternative<core::ParsedDecimal<core::QuantityUnits>>(quantity)) {
            observation.levels.push_back(
                {std::get<core::ParsedDecimal<core::PriceUnits>>(price).value,
                 std::get<core::ParsedDecimal<core::QuantityUnits>>(quantity).value});
        }
    }
    return observation;
}

} // namespace bmd_projection::m5::oracle
