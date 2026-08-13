#pragma once

// Structured byte decoder for M5 differential replay fuzzing.
//
// Maps arbitrary fuzz bytes deterministically into replay operations and
// replay context. The same byte string always produces the same structured
// test case. This decoder is the ONLY semantic input path for Phase-5 fuzzing;
// it does NOT route through canonical text parsing.
//
// Bounds:
//   operations        <= 64
//   levels per event  <= 8
//   decimal token     <= 31 bytes
//   snapshot strings  <= 15 bytes
//
// All reads are bounded and checked against the input buffer.

#include "replay_types.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace bmd_projection::m5::replay::fuzz_decoder {

inline constexpr std::size_t kMaxOperations = 64;
inline constexpr std::size_t kMaxLevelsPerEvent = 8;
inline constexpr std::size_t kMaxLevelTokenBytes = 31;
inline constexpr std::size_t kMaxSnapshotStringBytes = 15;
inline constexpr std::size_t kMaxSymbolBytes = 10;

enum class DecodedMode : std::uint8_t { CoreOnly, AdapterEnabled };

// Test-only adapter scenario dimensions. These are mapped to
// bmd_projection::m5::oracle::AdapterScenario by the fuzz harness.
enum class FuzzVenue : std::uint8_t { Binance, Unspecified, UnknownNumeric };
enum class FuzzMarket : std::uint8_t { Spot, UsdMPerpetual, Unspecified, UnknownNumeric };

struct FuzzCase final {
    DecodedMode mode{DecodedMode::CoreOnly};
    Market market{Market::Spot};
    NumericSpec numeric_spec;
    SequencePolicy sequence_policy{SequencePolicy::Spot};
    std::string symbol;
    std::vector<Operation> operations;

    // Adapter scenario dimensions — only meaningful in AdapterEnabled mode.
    FuzzVenue scenario_venue{FuzzVenue::Binance};
    FuzzMarket scenario_market{FuzzMarket::Spot};
    std::string adapter_wire_symbol;
    std::string adapter_expected_symbol;
    SequencePolicy adapter_expected_policy{SequencePolicy::Spot};
    NumericSpec adapter_conversion_numeric_spec;
    NumericSpec adapter_projection_numeric_spec;
    SequencePolicy adapter_projection_policy{SequencePolicy::Spot};
};

// Decodes raw fuzz bytes into a bounded structured fuzz case.
// Returns std::nullopt if the input is too truncated to produce
// an operation (at minimum a header byte and one operation needed).
[[nodiscard]] std::optional<FuzzCase> decode(const std::uint8_t* data, std::size_t size);

// Exposed for deterministic testing — the internal byte cursor that
// provides safe bounded reads over the fuzz input.
class ByteCursor final {
  public:
    ByteCursor(const std::uint8_t* data, std::size_t size) noexcept : data_{data}, size_{size} {}

    [[nodiscard]] bool exhausted() const noexcept { return offset_ >= size_; }
    [[nodiscard]] std::size_t remaining() const noexcept {
        return offset_ < size_ ? size_ - offset_ : 0;
    }

    [[nodiscard]] std::uint8_t read_u8() noexcept {
        if (offset_ >= size_)
            return 0;
        return data_[offset_++];
    }

    // Decodes a uint64 from the next bytes. Uses a compact encoding that
    // makes special values (0, 1, 2, UINT64_MAX, etc.) reachable.
    // byte: [5:7] width (1-8 bytes), [4] special flag, [0:3] special index
    [[nodiscard]] std::uint64_t read_var_u64() noexcept;

    // Reads a bounded string.
    [[nodiscard]] std::string read_string(std::size_t max_len = kMaxLevelTokenBytes) noexcept;

    // Reads an integer in [0, max_val] from the next byte.
    [[nodiscard]] std::uint32_t read_bounded(std::uint32_t max_val) noexcept;

    void skip(std::size_t n) noexcept { offset_ = std::min(offset_ + n, size_); }

    [[nodiscard]] std::size_t offset() const noexcept { return offset_; }

  private:
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t offset_{0};
};

} // namespace bmd_projection::m5::replay::fuzz_decoder
