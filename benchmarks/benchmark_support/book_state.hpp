#pragma once

// Deterministic book/projection state builders shared by the M2 and M3
// benchmark families. All prepared state is constructed outside the measured
// region; the semantic preconditions and dispositions are fixed per cell.

#include <binance_market_data/projection/v1/numeric/numeric_spec.hpp>
#include <binance_market_data/projection/v1/numeric/price_units.hpp>
#include <binance_market_data/projection/v1/numeric/quantity_units.hpp>
#include <binance_market_data/projection/v1/order_book/book_level.hpp>
#include <binance_market_data/projection/v1/order_book/book_side.hpp>
#include <binance_market_data/projection/v1/order_book/level_update.hpp>
#include <binance_market_data/projection/v1/order_book/order_book.hpp>
#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace bmd_projection::m5::benchmark {

namespace core = binance_market_data::projection::v1;

inline constexpr std::uint32_t kBenchmarkPriceScale = 2;
inline constexpr std::uint32_t kBenchmarkQuantityScale = 3;

struct BookParams final {
    std::int64_t bid_start{20'000};
    std::int64_t ask_start{20'001};
    std::int64_t quantity_base{1'000};
    std::uint64_t base_update_id{1'000'000};
    std::uint32_t quantity_cycle{16};
};

[[nodiscard]] core::NumericSpec benchmark_numeric_spec();
[[nodiscard]] core::PriceUnits price_units(std::int64_t value);
[[nodiscard]] core::QuantityUnits quantity_units(std::int64_t value);

// Bid level i: price = bid_start - i (i in [0, depth)); ask level i:
// price = ask_start + i. All levels carry quantity_base.
[[nodiscard]] std::vector<core::BookLevel> build_bid_levels(std::size_t depth);
[[nodiscard]] std::vector<core::BookLevel> build_ask_levels(std::size_t depth);
[[nodiscard]] core::OrderBook build_order_book(std::size_t depth);

// Quantity cycling table: quantity_base + 1 + (i % quantity_cycle). No entry
// ever equals quantity_base, so a replacement at the same price is always a
// real mutation.
[[nodiscard]] std::vector<core::QuantityUnits> build_quantity_table();

[[nodiscard]] core::UpdateId update_id(std::uint64_t value);

// Installs the baseline at base_update_id and applies the market bootstrap
// bridge, leaving the projection Synchronized at depth D levels per side.
// Spot bridge: [base+1, base+1] with empty levels. USD-M bridge:
// [base-1, base+1] with previous_final == base. Both return Applied.
[[nodiscard]] core::BookProjection build_synchronized_projection(core::SequencePolicyKind policy,
                                                                 std::size_t depth);

// Canonical description of prepared level data used for the generated-workload
// SHA-256 identity of stateful cells.
[[nodiscard]] std::string describe_levels(std::span<const core::BookLevel> levels);
[[nodiscard]] std::string describe_updates(std::span<const core::LevelUpdate> updates);

// Bounded pool of identically prepared states. Pool entries are consumed once
// per measured execution; the pool is rebuilt outside the measured region and
// its destruction (including any book destruction) happens after the loop.
template <typename T> class StatePool final {
  public:
    StatePool() = default;

    void reserve(std::size_t size) { entries_.reserve(size); }

    template <typename Factory> void fill(std::size_t size, Factory factory) {
        entries_.clear();
        entries_.reserve(size);
        for (std::size_t index = 0; index < size; ++index) {
            entries_.push_back(factory());
        }
    }

    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] T& at(std::size_t index) { return entries_.at(index); }
    [[nodiscard]] const T& at(std::size_t index) const { return entries_.at(index); }

  private:
    std::vector<T> entries_;
};

// Fixed measured-iteration counts for pool-based cells, scaled by depth so the
// prepared-state memory stays bounded while every measured execution keeps an
// identical fresh semantic precondition.
[[nodiscard]] std::size_t pool_iteration_count(std::size_t depth);

} // namespace bmd_projection::m5::benchmark
