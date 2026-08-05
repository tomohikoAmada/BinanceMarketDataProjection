#pragma once

#include <compare>
#include <cstdint>
#include <optional>

namespace binance_market_data::projection::v1 {

class QuantityUnits final {
  public:
    [[nodiscard]] static constexpr std::optional<QuantityUnits>
    create(std::int64_t value) noexcept {
        if (value < 0) {
            return std::nullopt;
        }
        return QuantityUnits{value};
    }

    [[nodiscard]] constexpr std::int64_t value() const noexcept { return value_; }

    friend constexpr auto operator<=>(QuantityUnits, QuantityUnits) noexcept = default;

  private:
    explicit constexpr QuantityUnits(std::int64_t value) noexcept : value_{value} {}

    std::int64_t value_;
};

} // namespace binance_market_data::projection::v1
