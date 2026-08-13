#include "m3_cells.hpp"

#include "canonical_text.hpp"

#include <binance_market_data/projection/v1/order_book/book_side.hpp>
#include <binance_market_data/projection/v1/order_book/level_update.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace bmd_projection::m5::benchmark {
namespace {

constexpr std::uint64_t kQuantityIndexPeriod = 10'007;
constexpr std::size_t kBatchCycleSize = 64;
inline constexpr std::array<std::size_t, 6> kDepthSet{0, 8, 100, 1'000, 5'000, 10'000};
inline constexpr std::array<std::size_t, 4> kBatchSet{0, 1, 10, 100};

[[nodiscard]] core::QuantityUnits cycling_quantity(std::uint64_t index) {
    return quantity_units(BookParams{}.quantity_base + 1 +
                          static_cast<std::int64_t>(index % kQuantityIndexPeriod));
}

[[nodiscard]] std::string policy_label(core::SequencePolicyKind policy) {
    return policy == core::SequencePolicyKind::Spot ? "Spot" : "UsdMPerpetual";
}

[[nodiscard]] std::string sha256_of(std::string_view text) {
    const auto hash = replay::sha256_hex(text);
    if (!std::holds_alternative<std::string>(hash)) {
        std::abort();
    }
    return std::get<std::string>(hash);
}

// C = base_update_id + 1 is the synchronized update ID after the bootstrap
// bridge in build_synchronized_projection.
[[nodiscard]] core::UpdateId synchronized_id() noexcept {
    return core::UpdateId{BookParams{}.base_update_id + 1};
}

} // namespace

M3AcceptedCell::M3AcceptedCell(Config config)
    : config_{config}, projection_{benchmark_numeric_spec(), config.policy} {}

void M3AcceptedCell::prepare() {
    const BookParams params{};
    step_ = 0;
    current_id_ = params.base_update_id + 1;
    std::string description;
    if (config_.depth == 0 && config_.batch > 0) {
        // Empty-book insertion edge: one bounded pool of empty synchronized
        // books; each measured execution inserts the same `batch` bid levels.
        projection_ = build_synchronized_projection(config_.policy, 0);
        insert_batch_.clear();
        insert_batch_.reserve(config_.batch);
        for (std::size_t index = 0; index < config_.batch; ++index) {
            insert_batch_.push_back(
                {core::BookSide::Bid,
                 price_units(params.bid_start - 1 - static_cast<std::int64_t>(index)),
                 quantity_units(params.quantity_base + 1)});
        }
        pool_.fill(pool_iteration_count(0),
                   [this] { return build_synchronized_projection(config_.policy, 0); });
        description = describe_updates(insert_batch_);
    } else if (config_.depth > 0 && config_.batch > 0) {
        // Existing-price replacement updates: depth stays fixed, mutation is
        // real, and every execution returns Applied.
        projection_ = build_synchronized_projection(config_.policy, config_.depth);
        cycle_batches_.clear();
        cycle_batches_.reserve(kBatchCycleSize);
        for (std::size_t batch_index = 0; batch_index < kBatchCycleSize; ++batch_index) {
            std::vector<core::LevelUpdate> updates;
            updates.reserve(config_.batch);
            for (std::size_t slot = 0; slot < config_.batch; ++slot) {
                const auto price_index = (batch_index * config_.batch + slot) % config_.depth;
                updates.push_back(
                    {core::BookSide::Bid,
                     price_units(params.bid_start - static_cast<std::int64_t>(price_index)),
                     cycling_quantity(batch_index + slot)});
            }
            description += describe_updates(updates);
            cycle_batches_.push_back(std::move(updates));
        }
    } else {
        // B=0: the mandatory advancing empty-level batch. It traverses the
        // real accepted production transaction (both-side copies, candidate
        // construction, replace_all, empty apply_updates, move commit) and
        // returns Applied while executing the logical copy transaction.
        projection_ = build_synchronized_projection(config_.policy, config_.depth);
    }
    generated_sha_ = sha256_of(description);
}

bool M3AcceptedCell::uses_pool() const noexcept { return config_.depth == 0 && config_.batch > 0; }

std::size_t M3AcceptedCell::pool_size() const noexcept { return pool_.size(); }

core::ApplyResult M3AcceptedCell::execute_step(std::size_t pool_index) {
    const auto next = current_id_ + 1;
    if (uses_pool()) {
        auto& target = pool_.at(pool_index);
        const auto range =
            core::UpdateRange::try_create(core::UpdateId{next}, core::UpdateId{next});
        if (!range.has_value()) {
            std::abort();
        }
        return target.apply({*range,
                             config_.policy == core::SequencePolicyKind::UsdMPerpetual
                                 ? std::optional<core::UpdateId>{core::UpdateId{current_id_}}
                                 : std::nullopt,
                             insert_batch_});
    }
    const auto range = core::UpdateRange::try_create(core::UpdateId{next}, core::UpdateId{next});
    if (!range.has_value()) {
        std::abort();
    }
    if (config_.batch == 0) {
        const auto result =
            projection_.apply({*range,
                               config_.policy == core::SequencePolicyKind::UsdMPerpetual
                                   ? std::optional<core::UpdateId>{core::UpdateId{current_id_}}
                                   : std::nullopt,
                               {}});
        current_id_ = next;
        return result;
    }
    const auto& updates = cycle_batches_.at(step_ % cycle_batches_.size());
    const auto result =
        projection_.apply({*range,
                           config_.policy == core::SequencePolicyKind::UsdMPerpetual
                               ? std::optional<core::UpdateId>{core::UpdateId{current_id_}}
                               : std::nullopt,
                           updates});
    ++step_;
    current_id_ = next;
    return result;
}

M3ClassificationCell::M3ClassificationCell(Config config)
    : config_{config}, projection_{benchmark_numeric_spec(), config.policy} {
    const auto current = synchronized_id();
    const auto current_value = current.value();
    auto stale_range = core::UpdateRange::try_create(core::UpdateId{current_value - 3},
                                                     core::UpdateId{current_value - 1});
    auto duplicate_range =
        core::UpdateRange::try_create(core::UpdateId{current_value - 1}, current);
    std::optional<core::UpdateRange> gap_range;
    if (config.kind == M3ClassificationKind::Gap) {
        if (config.policy == core::SequencePolicyKind::Spot) {
            gap_range = core::UpdateRange::try_create(core::UpdateId{current_value + 2},
                                                      core::UpdateId{current_value + 2});
        } else {
            gap_range = core::UpdateRange::try_create(core::UpdateId{current_value + 1},
                                                      core::UpdateId{current_value + 1});
        }
    } else {
        gap_range = core::UpdateRange::try_create(current, current);
    }
    if (!stale_range.has_value() || !duplicate_range.has_value() || !gap_range.has_value()) {
        std::abort();
    }
    const auto usdm_previous = config.policy == core::SequencePolicyKind::UsdMPerpetual
                                   ? std::optional<core::UpdateId>{current}
                                   : std::nullopt;
    stale_batch_ = {*stale_range, usdm_previous, {}};
    duplicate_batch_ = {*duplicate_range, usdm_previous, {}};
    gap_batch_ = {*gap_range,
                  config.policy == core::SequencePolicyKind::UsdMPerpetual
                      ? std::optional<core::UpdateId>{core::UpdateId{current_value + 3}}
                      : std::nullopt,
                  {}};
    baseline_bids_ = build_bid_levels(config.depth);
    baseline_asks_ = build_ask_levels(config.depth);
}

void M3ClassificationCell::prepare() {
    projection_ = build_synchronized_projection(config_.policy, config_.depth);
    generated_sha_ = sha256_of(describe_levels(baseline_bids_) + describe_levels(baseline_asks_));
    switch (config_.kind) {
    case M3ClassificationKind::Stale:
    case M3ClassificationKind::Duplicate:
        // Stable repeated non-mutating state: one synchronized projection is
        // reused; every execution sees the identical semantic precondition.
        break;
    case M3ClassificationKind::Gap:
    case M3ClassificationKind::Reset:
        // One-shot state changes: fresh prepared synchronized state per
        // measured execution.
        pool_.fill(pool_iteration_count(config_.depth),
                   [this] { return build_synchronized_projection(config_.policy, config_.depth); });
        break;
    case M3ClassificationKind::BaselineInstall:
        // Fresh AwaitingBaseline state per measured execution.
        pool_.fill(pool_iteration_count(0), [this] {
            return core::BookProjection{benchmark_numeric_spec(), config_.policy};
        });
        break;
    }
}

bool M3ClassificationCell::uses_pool() const noexcept {
    return config_.kind == M3ClassificationKind::Gap ||
           config_.kind == M3ClassificationKind::Reset ||
           config_.kind == M3ClassificationKind::BaselineInstall;
}

std::size_t M3ClassificationCell::pool_size() const noexcept { return pool_.size(); }

M3ClassificationResult M3ClassificationCell::execute_step(std::size_t pool_index) {
    M3ClassificationResult result;
    result.status_after = projection_.status();
    switch (config_.kind) {
    case M3ClassificationKind::Stale: {
        if (!stale_batch_.has_value()) {
            std::abort();
        }
        const auto applied = projection_.apply(*stale_batch_);
        result.apply_disposition = applied.disposition;
        result.status_after = applied.status_after;
        return result;
    }
    case M3ClassificationKind::Duplicate: {
        if (!duplicate_batch_.has_value()) {
            std::abort();
        }
        const auto applied = projection_.apply(*duplicate_batch_);
        result.apply_disposition = applied.disposition;
        result.status_after = applied.status_after;
        return result;
    }
    case M3ClassificationKind::Gap: {
        if (!gap_batch_.has_value()) {
            std::abort();
        }
        auto& target = pool_.at(pool_index);
        const auto applied = target.apply(*gap_batch_);
        result.apply_disposition = applied.disposition;
        result.status_after = applied.status_after;
        return result;
    }
    case M3ClassificationKind::Reset: {
        auto& target = pool_.at(pool_index);
        target.reset();
        result.status_after = target.status();
        return result;
    }
    case M3ClassificationKind::BaselineInstall: {
        auto& target = pool_.at(pool_index);
        const auto installed = target.install_baseline(
            {core::UpdateId{BookParams{}.base_update_id}, baseline_bids_, baseline_asks_});
        result.install_disposition = installed.disposition;
        result.status_after = installed.status_after;
        return result;
    }
    }
    return result;
}

std::vector<std::string> expected_m3_accepted_cell_names() {
    std::vector<std::string> names;
    names.reserve(48);
    for (const auto policy :
         {core::SequencePolicyKind::Spot, core::SequencePolicyKind::UsdMPerpetual}) {
        for (const auto depth : kDepthSet) {
            for (const auto batch : kBatchSet) {
                names.push_back("M3/LiveApply/Accepted/" + policy_label(policy) + "/D" +
                                std::to_string(depth) + "/B" + std::to_string(batch));
            }
        }
    }
    return names;
}

} // namespace bmd_projection::m5::benchmark
