#pragma once

#include <cstdint>
#include <optional>

namespace binance_market_data::projection::v1 {

inline constexpr std::uint8_t kMaxDecimalScale = 18;

class DecimalScale final {
  public:
    [[nodiscard]] static constexpr std::optional<DecimalScale>
    create(std::uint32_t value) noexcept {
        if (value > kMaxDecimalScale) {
            return std::nullopt;
        }
        return DecimalScale{static_cast<std::uint8_t>(value)};
    }

    [[nodiscard]] constexpr std::uint8_t value() const noexcept { return value_; }

    friend constexpr bool operator==(DecimalScale, DecimalScale) noexcept = default;

  private:
    explicit constexpr DecimalScale(std::uint8_t value) noexcept : value_{value} {}

    std::uint8_t value_;
};

} // namespace binance_market_data::projection::v1
