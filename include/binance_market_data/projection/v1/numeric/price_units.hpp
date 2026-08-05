#pragma once

#include <compare>
#include <cstdint>
#include <optional>

namespace binance_market_data::projection::v1 {

class PriceUnits final {
  public:
    [[nodiscard]] static constexpr std::optional<PriceUnits> create(std::int64_t value) noexcept {
        if (value <= 0) {
            return std::nullopt;
        }
        return PriceUnits{value};
    }

    [[nodiscard]] constexpr std::int64_t value() const noexcept { return value_; }

    friend constexpr auto operator<=>(PriceUnits, PriceUnits) noexcept = default;

  private:
    explicit constexpr PriceUnits(std::int64_t value) noexcept : value_{value} {}

    std::int64_t value_;
};

} // namespace binance_market_data::projection::v1
