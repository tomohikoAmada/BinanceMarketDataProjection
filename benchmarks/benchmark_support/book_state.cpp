#include "book_state.hpp"

#include <binance_market_data/projection/v1/numeric/decimal_scale.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace bmd_projection::m5::benchmark {
namespace {

[[nodiscard]] core::DecimalScale required_scale(std::uint32_t value) {
    const auto scale = core::DecimalScale::create(value);
    if (!scale.has_value()) {
        std::abort();
    }
    return *scale;
}

} // namespace

core::NumericSpec benchmark_numeric_spec() {
    return {required_scale(kBenchmarkPriceScale), required_scale(kBenchmarkQuantityScale)};
}

core::PriceUnits price_units(std::int64_t value) {
    const auto result = core::PriceUnits::create(value);
    if (!result.has_value()) {
        std::abort();
    }
    return *result;
}

core::QuantityUnits quantity_units(std::int64_t value) {
    const auto result = core::QuantityUnits::create(value);
    if (!result.has_value()) {
        std::abort();
    }
    return *result;
}

std::vector<core::BookLevel> build_bid_levels(std::size_t depth) {
    const BookParams params{};
    std::vector<core::BookLevel> levels;
    levels.reserve(depth);
    for (std::size_t index = 0; index < depth; ++index) {
        levels.push_back({price_units(params.bid_start - static_cast<std::int64_t>(index)),
                          quantity_units(params.quantity_base)});
    }
    return levels;
}

std::vector<core::BookLevel> build_ask_levels(std::size_t depth) {
    const BookParams params{};
    std::vector<core::BookLevel> levels;
    levels.reserve(depth);
    for (std::size_t index = 0; index < depth; ++index) {
        levels.push_back({price_units(params.ask_start + static_cast<std::int64_t>(index)),
                          quantity_units(params.quantity_base)});
    }
    return levels;
}

core::OrderBook build_order_book(std::size_t depth) {
    core::OrderBook book{benchmark_numeric_spec()};
    const auto bids = build_bid_levels(depth);
    const auto asks = build_ask_levels(depth);
    book.replace_all(bids, asks);
    return book;
}

std::vector<core::QuantityUnits> build_quantity_table() {
    const BookParams params{};
    std::vector<core::QuantityUnits> table;
    table.reserve(params.quantity_cycle);
    for (std::uint32_t index = 0; index < params.quantity_cycle; ++index) {
        table.push_back(quantity_units(params.quantity_base + 1 + index));
    }
    return table;
}

core::UpdateId update_id(std::uint64_t value) { return core::UpdateId{value}; }

core::BookProjection build_synchronized_projection(core::SequencePolicyKind policy,
                                                   std::size_t depth) {
    const BookParams params{};
    core::BookProjection projection{benchmark_numeric_spec(), policy};
    const auto bids = build_bid_levels(depth);
    const auto asks = build_ask_levels(depth);
    const auto installed =
        projection.install_baseline({update_id(params.base_update_id), bids, asks});
    if (installed.disposition != core::InstallDisposition::Installed) {
        std::abort();
    }
    if (policy == core::SequencePolicyKind::Spot) {
        const auto bridge = core::UpdateRange::try_create(update_id(params.base_update_id + 1),
                                                          update_id(params.base_update_id + 1));
        if (!bridge.has_value()) {
            std::abort();
        }
        const auto applied = projection.apply({*bridge, std::nullopt, {}});
        if (applied.disposition != core::ApplyDisposition::Applied ||
            applied.status_after != core::ProjectionStatus::Synchronized) {
            std::abort();
        }
    } else {
        const auto bridge = core::UpdateRange::try_create(update_id(params.base_update_id - 1),
                                                          update_id(params.base_update_id + 1));
        if (!bridge.has_value()) {
            std::abort();
        }
        const auto applied = projection.apply({*bridge, update_id(params.base_update_id), {}});
        if (applied.disposition != core::ApplyDisposition::Applied ||
            applied.status_after != core::ProjectionStatus::Synchronized) {
            std::abort();
        }
    }
    return projection;
}

std::string describe_levels(std::span<const core::BookLevel> levels) {
    std::string text;
    for (const auto& level : levels) {
        text += std::to_string(level.price.value());
        text += ',';
        text += std::to_string(level.quantity.value());
        text += ';';
    }
    return text;
}

std::string describe_updates(std::span<const core::LevelUpdate> updates) {
    std::string text;
    for (const auto& update : updates) {
        text += std::to_string(static_cast<std::uint8_t>(update.side));
        text += ':';
        text += std::to_string(update.price.value());
        text += ',';
        text += std::to_string(update.quantity.value());
        text += ';';
    }
    return text;
}

std::size_t pool_iteration_count(std::size_t depth) {
    if (depth <= 8) {
        return 4'096;
    }
    if (depth <= 100) {
        return 2'048;
    }
    if (depth <= 1'000) {
        return 1'024;
    }
    if (depth <= 5'000) {
        return 512;
    }
    return 256;
}

} // namespace bmd_projection::m5::benchmark
