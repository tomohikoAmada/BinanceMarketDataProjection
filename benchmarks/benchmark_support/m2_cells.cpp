#include "m2_cells.hpp"

#include "canonical_text.hpp"

#include <binance_market_data/projection/v1/order_book/book_side.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace bmd_projection::m5::benchmark {
namespace {

// Large prime above the maximum depth so the quantity index can never alias
// across two applications of the same price (see m2_cells.hpp rationale).
constexpr std::uint64_t kQuantityIndexPeriod = 10'007;
constexpr std::size_t kBatchCycleSize = 64;

[[nodiscard]] core::QuantityUnits cycling_quantity(std::uint64_t index) {
    return quantity_units(BookParams{}.quantity_base + 1 +
                          static_cast<std::int64_t>(index % kQuantityIndexPeriod));
}

[[nodiscard]] core::PriceUnits absent_bid_price(std::size_t depth) {
    return price_units(BookParams{}.bid_start - static_cast<std::int64_t>(depth) - 1);
}

[[nodiscard]] std::string sha256_of(std::string_view text) {
    const auto hash = replay::sha256_hex(text);
    if (!std::holds_alternative<std::string>(hash)) {
        std::abort();
    }
    return std::get<std::string>(hash);
}

} // namespace

M2ApplyLevelCell::M2ApplyLevelCell(M2ApplyLevelKind kind, std::size_t depth)
    : kind_{kind}, depth_{depth}, book_{benchmark_numeric_spec()} {}

void M2ApplyLevelCell::prepare() {
    update_slot_ = 0;
    book_ = build_order_book(depth_);
    const BookParams params{};
    switch (kind_) {
    case M2ApplyLevelKind::Insert:
        update_ = core::LevelUpdate{core::BookSide::Bid, absent_bid_price(depth_),
                                    quantity_units(params.quantity_base + 1)};
        pool_.fill(pool_iteration_count(depth_), [this] { return build_order_book(depth_); });
        {
            const auto bids = build_bid_levels(depth_);
            generated_sha_ = sha256_of(describe_updates(std::span(&*update_, 1)) +
                                       describe_levels(std::span{bids}));
        }
        break;
    case M2ApplyLevelKind::Update:
        update_slots_.push_back({core::BookSide::Bid, price_units(params.bid_start),
                                 quantity_units(params.quantity_base + 1)});
        update_slots_.push_back({core::BookSide::Bid, price_units(params.bid_start),
                                 quantity_units(params.quantity_base + 2)});
        generated_sha_ = sha256_of(describe_updates(update_slots_));
        break;
    case M2ApplyLevelKind::Delete:
        update_ = core::LevelUpdate{core::BookSide::Bid, price_units(params.bid_start),
                                    quantity_units(0)};
        pool_.fill(pool_iteration_count(depth_), [this] { return build_order_book(depth_); });
        generated_sha_ = sha256_of(describe_updates(std::span(&*update_, 1)));
        break;
    case M2ApplyLevelKind::MissingDelete:
        update_ =
            core::LevelUpdate{core::BookSide::Bid, absent_bid_price(depth_), quantity_units(0)};
        generated_sha_ = sha256_of(describe_updates(std::span(&*update_, 1)));
        break;
    }
}

bool M2ApplyLevelCell::uses_pool() const noexcept {
    return kind_ == M2ApplyLevelKind::Insert || kind_ == M2ApplyLevelKind::Delete;
}

std::size_t M2ApplyLevelCell::pool_size() const noexcept { return pool_.size(); }

core::LevelChange M2ApplyLevelCell::execute_step(std::size_t pool_index) {
    switch (kind_) {
    case M2ApplyLevelKind::Insert:
        return pool_.at(pool_index).apply_level(update_->side, update_->price, update_->quantity);
    case M2ApplyLevelKind::Update: {
        const auto& slot_update = update_slots_.at(update_slot_);
        update_slot_ = (update_slot_ + 1) % update_slots_.size();
        return book_.apply_level(slot_update.side, slot_update.price, slot_update.quantity);
    }
    case M2ApplyLevelKind::Delete:
        return pool_.at(pool_index).apply_level(update_->side, update_->price, update_->quantity);
    case M2ApplyLevelKind::MissingDelete:
        return book_.apply_level(update_->side, update_->price, update_->quantity);
    }
    return core::LevelChange::Unchanged;
}

M2ApplyUpdatesCell::M2ApplyUpdatesCell(Config config)
    : depth_{config.depth}, batch_{config.batch}, mix_{config.mix},
      book_{benchmark_numeric_spec()} {}

void M2ApplyUpdatesCell::prepare() {
    step_ = 0;
    const BookParams params{};
    if (mix_ == M2ApplyUpdatesMix::Insertion) {
        // Empty-book insertion edge (D=0 only). Each execution inserts the same
        // `batch` bid levels into one fresh empty book from the pool.
        book_ = core::OrderBook{benchmark_numeric_spec()};
        std::vector<core::LevelUpdate> inserts;
        inserts.reserve(batch_);
        for (std::size_t index = 0; index < batch_; ++index) {
            inserts.push_back({core::BookSide::Bid,
                               price_units(params.bid_start - 1 - static_cast<std::int64_t>(index)),
                               quantity_units(params.quantity_base + 1)});
        }
        cycle_batches_.push_back(std::move(inserts));
        pool_.fill(pool_iteration_count(0),
                   [] { return core::OrderBook{benchmark_numeric_spec()}; });
        generated_sha_ = sha256_of(describe_updates(cycle_batches_.front()));
        return;
    }
    book_ = build_order_book(depth_);
    cycle_batches_.clear();
    cycle_batches_.reserve(kBatchCycleSize);
    for (std::size_t batch_index = 0; batch_index < kBatchCycleSize; ++batch_index) {
        std::vector<core::LevelUpdate> updates;
        updates.reserve(batch_);
        for (std::size_t slot = 0; slot < batch_; ++slot) {
            const auto price_index = (batch_index * batch_ + slot) % (depth_ == 0 ? 1 : depth_);
            updates.push_back(
                {core::BookSide::Bid,
                 price_units(params.bid_start - static_cast<std::int64_t>(price_index)),
                 cycling_quantity(batch_index + slot)});
        }
        cycle_batches_.push_back(std::move(updates));
    }
    std::string description;
    for (const auto& batch : cycle_batches_) {
        description += describe_updates(batch);
    }
    generated_sha_ = sha256_of(description);
}

bool M2ApplyUpdatesCell::uses_pool() const noexcept { return mix_ == M2ApplyUpdatesMix::Insertion; }

std::size_t M2ApplyUpdatesCell::pool_size() const noexcept { return pool_.size(); }

void M2ApplyUpdatesCell::execute_step(std::size_t pool_index) {
    if (mix_ == M2ApplyUpdatesMix::Insertion) {
        pool_.at(pool_index).apply_updates(cycle_batches_.front());
        return;
    }
    book_.apply_updates(cycle_batches_.at(step_ % cycle_batches_.size()));
    ++step_;
}

M2QueryCell::M2QueryCell(Config config)
    : depth_{config.depth}, query_limit_{config.query_limit}, book_{benchmark_numeric_spec()} {}

void M2QueryCell::prepare() {
    book_ = build_order_book(depth_);
    {
        const auto bids = build_bid_levels(depth_);
        const auto asks = build_ask_levels(depth_);
        generated_sha_ =
            sha256_of(describe_levels(std::span{bids}) + describe_levels(std::span{asks}));
    }
}

std::optional<core::BookLevel> M2QueryCell::best(core::BookSide side) const {
    if (side == core::BookSide::Bid) {
        return book_.best_bid();
    }
    return book_.best_ask();
}

std::optional<core::QuantityUnits> M2QueryCell::quantity_at_hit() const {
    return book_.quantity_at(core::BookSide::Bid, price_units(BookParams{}.bid_start));
}

std::optional<core::QuantityUnits> M2QueryCell::quantity_at_miss() const {
    return book_.quantity_at(core::BookSide::Bid, absent_bid_price(depth_));
}

std::vector<core::BookLevel> M2QueryCell::top_levels(core::BookSide side) const {
    return book_.top_levels(side, query_limit_);
}

std::vector<core::BookLevel> M2QueryCell::all_levels(core::BookSide side) const {
    return book_.all_levels(side);
}

M2ReplaceAllCell::M2ReplaceAllCell(std::size_t depth)
    : depth_{depth}, book_{benchmark_numeric_spec()} {}

void M2ReplaceAllCell::prepare() {
    bids_ = build_bid_levels(depth_);
    asks_ = build_ask_levels(depth_);
    // Pre-fill with a distinct old book so replace_all truly replaces state.
    book_ = core::OrderBook{benchmark_numeric_spec()};
    if (depth_ > 0) {
        std::vector<core::BookLevel> old_bids;
        std::vector<core::BookLevel> old_asks;
        old_bids.reserve(depth_);
        old_asks.reserve(depth_);
        for (std::size_t index = 0; index < depth_; ++index) {
            old_bids.push_back(
                {price_units(BookParams{}.bid_start + 20'000 - static_cast<std::int64_t>(index)),
                 quantity_units(1)});
            old_asks.push_back(
                {price_units(BookParams{}.ask_start + 30'000 + static_cast<std::int64_t>(index)),
                 quantity_units(1)});
        }
        book_.replace_all(old_bids, old_asks);
    }
    generated_sha_ = sha256_of(describe_levels(bids_) + describe_levels(asks_));
}

void M2ReplaceAllCell::execute_step() { book_.replace_all(bids_, asks_); }

M3ProxyCells::M3ProxyCells(std::size_t depth)
    : depth_{depth}, projection_{benchmark_numeric_spec(), core::SequencePolicyKind::Spot} {}

void M3ProxyCells::prepare() {
    projection_ = build_synchronized_projection(core::SequencePolicyKind::Spot, depth_);
    bids_ = build_bid_levels(depth_);
    asks_ = build_ask_levels(depth_);
    cycle_batches_.clear();
    cycle_batches_.reserve(kBatchCycleSize);
    const BookParams params{};
    for (std::size_t batch_index = 0; batch_index < kBatchCycleSize; ++batch_index) {
        std::vector<core::LevelUpdate> updates;
        updates.reserve(1);
        const auto price_index = batch_index % (depth_ == 0 ? 1 : depth_);
        updates.push_back({core::BookSide::Bid,
                           price_units(params.bid_start - static_cast<std::int64_t>(price_index)),
                           cycling_quantity(batch_index)});
        cycle_batches_.push_back(std::move(updates));
    }
    std::string description;
    for (const auto& batch : cycle_batches_) {
        description += describe_updates(batch);
    }
    generated_sha_ = sha256_of(describe_levels(bids_) + describe_levels(asks_) + description);
}

std::vector<core::BookLevel> M3ProxyCells::all_levels_both_sides() const {
    const auto book = projection_.synchronized_book();
    if (!book.has_value()) {
        std::abort();
    }
    auto bids = book->get().all_levels(core::BookSide::Bid);
    auto asks = book->get().all_levels(core::BookSide::Ask);
    bids.insert(bids.end(), asks.begin(), asks.end());
    return bids;
}

core::OrderBook M3ProxyCells::candidate_rebuild_from_vectors() const {
    core::OrderBook candidate{benchmark_numeric_spec()};
    candidate.replace_all(bids_, asks_);
    return candidate;
}

void M3ProxyCells::candidate_apply_updates(core::OrderBook& candidate, std::size_t step) const {
    candidate.apply_updates(cycle_batches_.at(step % cycle_batches_.size()));
}

void M3ProxyCells::move_commit(core::OrderBook& destination, core::OrderBook source) {
    destination = std::move(source);
}

} // namespace bmd_projection::m5::benchmark
