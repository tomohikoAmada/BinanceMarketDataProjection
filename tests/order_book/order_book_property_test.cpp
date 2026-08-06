#include "reference_order_book.hpp"
#include "test_helpers.hpp"

#include <binance_market_data/projection/v1/order_book/book_level.hpp>
#include <binance_market_data/projection/v1/order_book/level_update.hpp>
#include <binance_market_data/projection/v1/order_book/order_book.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// NOLINTBEGIN(bugprone-unchecked-optional-access)
namespace bmd = binance_market_data::projection::v1;

namespace {

using Price = std::int64_t;
using Quantity = std::int64_t;

class PseudoRandomGenerator final {
  public:
    explicit PseudoRandomGenerator(std::uint64_t seed) : state_(seed) {}

    [[nodiscard]] std::uint64_t next() {
        state_ = state_ * 6364136223846793005ULL + 1442695040888963407ULL;
        return state_;
    }

    [[nodiscard]] Price next_price() {
        const auto raw = static_cast<Price>(next() % 4096);
        return (raw == 0) ? 1 : raw;
    }

    [[nodiscard]] Quantity next_quantity() { return static_cast<Quantity>(next() % 4097); }

    [[nodiscard]] std::size_t next_size_t(std::size_t limit) {
        if (limit == 0) {
            return 0;
        }
        return static_cast<std::size_t>(next() % limit);
    }

  private:
    std::uint64_t state_;
};

enum class OpType : std::uint8_t {
    BidInsert,
    AskInsert,
    Update,
    Delete,
    MissingDelete,
    SameValue,
    Batch,
    ClearSide,
    ClearAll,
    ReplaceAll,
    TopNQuery,
    QuantityLookup,
};

const char* op_name(OpType type) {
    switch (type) {
    case OpType::BidInsert:
        return "BidInsert";
    case OpType::AskInsert:
        return "AskInsert";
    case OpType::Update:
        return "Update";
    case OpType::Delete:
        return "Delete";
    case OpType::MissingDelete:
        return "MissingDelete";
    case OpType::SameValue:
        return "SameValue";
    case OpType::Batch:
        return "Batch";
    case OpType::ClearSide:
        return "ClearSide";
    case OpType::ClearAll:
        return "ClearAll";
    case OpType::ReplaceAll:
        return "ReplaceAll";
    case OpType::TopNQuery:
        return "TopNQuery";
    case OpType::QuantityLookup:
        return "QuantityLookup";
    }
    return "UNKNOWN";
}

struct OperationContext {
    std::string seed;
    std::size_t transcript_idx;
    std::size_t op_idx;
    OpType type;
    bmd::BookSide side{bmd::BookSide::Bid};
    Price price{0};
    Quantity quantity{0};
    std::optional<std::size_t> limit;
    std::optional<std::size_t> batch_size;
    std::optional<std::size_t> bid_count;
    std::optional<std::size_t> ask_count;
    bool skipped{false};
};

[[nodiscard]] OperationContext make_context(const std::string& seed, std::size_t transcript_idx,
                                            std::size_t op_idx, OpType type) {
    OperationContext ctx{};
    ctx.seed = seed;
    ctx.transcript_idx = transcript_idx;
    ctx.op_idx = op_idx;
    ctx.type = type;
    return ctx;
}

std::string format_failure_context(const OperationContext& ctx) {
    std::ostringstream oss;
    oss << "seed=" << ctx.seed << " transcript=" << ctx.transcript_idx << " op=" << ctx.op_idx
        << " type=" << op_name(ctx.type) << " side=" << to_string(ctx.side)
        << " price=" << ctx.price << " quantity=" << ctx.quantity;
    if (ctx.skipped) {
        oss << " empty=true";
    }
    if (ctx.limit.has_value()) {
        oss << " limit=" << ctx.limit.value();
    }
    if (ctx.batch_size.has_value()) {
        oss << " batch_size=" << ctx.batch_size.value();
    }
    if (ctx.bid_count.has_value() || ctx.ask_count.has_value()) {
        oss << " bid_count=" << (ctx.bid_count.has_value() ? ctx.bid_count.value() : 0)
            << " ask_count=" << (ctx.ask_count.has_value() ? ctx.ask_count.value() : 0);
    }
    return oss.str();
}

bmd::PriceUnits find_missing_price(const bmd_test::ReferenceOrderBook& reference,
                                   bmd::BookSide side, PseudoRandomGenerator& rng) {
    constexpr std::int64_t kMaxAttempts = 100;
    for (std::int64_t attempt = 0; attempt < kMaxAttempts; ++attempt) {
        const auto candidate = rng.next_price();
        if (!reference.quantity_at(side, bmd_test::price_units(candidate)).has_value()) {
            return bmd_test::price_units(candidate);
        }
    }
    // Fallback: search downward from INT64_MAX
    for (std::int64_t val = std::numeric_limits<std::int64_t>::max(); val > 0; val /= 2) {
        if (!reference.quantity_at(side, bmd_test::price_units(val)).has_value()) {
            return bmd_test::price_units(val);
        }
    }
    // Should never reach here with small test data
    std::abort();
}

void verify_no_zero_quantities(const std::vector<bmd::BookLevel>& levels) {
    for (const auto& level : levels) {
        EXPECT_GT(level.quantity.value(), 0);
    }
}

void verify_strict_ordering(const std::vector<bmd::BookLevel>& levels, bool ascending) {
    for (std::size_t i = 1; i < levels.size(); ++i) {
        if (ascending) {
            EXPECT_LT(levels[i - 1].price.value(), levels[i].price.value());
        } else {
            EXPECT_GT(levels[i - 1].price.value(), levels[i].price.value());
        }
    }
}

void verify_no_duplicates(const std::vector<bmd::BookLevel>& levels) {
    for (std::size_t i = 1; i < levels.size(); ++i) {
        EXPECT_NE(levels[i - 1].price, levels[i].price);
    }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void verify_consistency(const bmd::OrderBook& book, const bmd_test::ReferenceOrderBook& reference) {
    EXPECT_EQ(book.level_count(bmd::BookSide::Bid), reference.level_count(bmd::BookSide::Bid));
    EXPECT_EQ(book.level_count(bmd::BookSide::Ask), reference.level_count(bmd::BookSide::Ask));

    const auto prod_best_bid = book.best_bid();
    const auto ref_best_bid = reference.best_bid();
    ASSERT_EQ(prod_best_bid.has_value(), ref_best_bid.has_value());
    if (prod_best_bid.has_value()) {
        EXPECT_EQ(prod_best_bid.value().price, ref_best_bid.value().price);
        EXPECT_EQ(prod_best_bid.value().quantity, ref_best_bid.value().quantity);
    }

    const auto prod_best_ask = book.best_ask();
    const auto ref_best_ask = reference.best_ask();
    ASSERT_EQ(prod_best_ask.has_value(), ref_best_ask.has_value());
    if (prod_best_ask.has_value()) {
        EXPECT_EQ(prod_best_ask.value().price, ref_best_ask.value().price);
        EXPECT_EQ(prod_best_ask.value().quantity, ref_best_ask.value().quantity);
    }

    const auto prod_bids = book.all_levels(bmd::BookSide::Bid);
    const auto ref_bids = reference.all_levels(bmd::BookSide::Bid);
    ASSERT_EQ(prod_bids.size(), ref_bids.size());
    for (std::size_t i = 0; i < prod_bids.size(); ++i) {
        EXPECT_EQ(prod_bids[i].price, ref_bids[i].price);
        EXPECT_EQ(prod_bids[i].quantity, ref_bids[i].quantity);
    }

    const auto prod_asks = book.all_levels(bmd::BookSide::Ask);
    const auto ref_asks = reference.all_levels(bmd::BookSide::Ask);
    ASSERT_EQ(prod_asks.size(), ref_asks.size());
    for (std::size_t i = 0; i < prod_asks.size(); ++i) {
        EXPECT_EQ(prod_asks[i].price, ref_asks[i].price);
        EXPECT_EQ(prod_asks[i].quantity, ref_asks[i].quantity);
    }

    for (const auto side : {bmd::BookSide::Bid, bmd::BookSide::Ask}) {
        const auto count = book.level_count(side);
        EXPECT_EQ(book.top_levels(side, 0), reference.top_levels(side, 0));
        if (count > 0) {
            EXPECT_EQ(book.top_levels(side, 1), reference.top_levels(side, 1));
        }
        const auto half = (count > 0) ? count / 2 : 0;
        EXPECT_EQ(book.top_levels(side, half), reference.top_levels(side, half));
        EXPECT_EQ(book.top_levels(side, count), reference.top_levels(side, count));
        if (count < std::numeric_limits<std::size_t>::max()) {
            EXPECT_EQ(book.top_levels(side, count + 1), reference.top_levels(side, count + 1));
        }
    }

    verify_no_zero_quantities(prod_bids);
    verify_no_zero_quantities(prod_asks);

    verify_strict_ordering(prod_bids, false);
    verify_strict_ordering(prod_asks, true);

    verify_no_duplicates(prod_bids);
    verify_no_duplicates(prod_asks);
}

template <typename Action>
void run_checked_operation(const OperationContext& ctx, bmd::OrderBook& book,
                           bmd_test::ReferenceOrderBook& reference, Action&& action) {
    SCOPED_TRACE(format_failure_context(ctx));
    std::forward<Action>(action)();
    verify_consistency(book, reference);
}

} // namespace

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(OrderBookPropertyTest, DeterministicPropertyValidation) {
    constexpr std::uint64_t kFixedSeed = 1234567890ULL;
    constexpr std::size_t kTranscriptCount = 100;
    constexpr std::size_t kMinOpsPerTranscript = 500;
    constexpr std::size_t kMaxOpsPerTranscript = 1000;
    constexpr std::size_t kBatchMaxSize = 10;

    const auto seed_str = std::to_string(kFixedSeed);
    PseudoRandomGenerator seed_gen{kFixedSeed};

    for (std::size_t transcript_idx = 0; transcript_idx < kTranscriptCount; ++transcript_idx) {
        const auto transcript_seed = seed_gen.next();
        PseudoRandomGenerator rng{transcript_seed};
        const auto op_count =
            kMinOpsPerTranscript + rng.next_size_t(kMaxOpsPerTranscript - kMinOpsPerTranscript + 1);

        bmd::NumericSpec spec{bmd_test::scale(8), bmd_test::scale(8)};
        bmd::OrderBook book{spec};
        bmd_test::ReferenceOrderBook reference;

        for (std::size_t op_idx = 0; op_idx < op_count; ++op_idx) {
            const auto op = static_cast<OpType>(rng.next_size_t(12));

            switch (op) {
            case OpType::BidInsert: {
                const auto price_val = rng.next_price();
                const auto quantity_val = rng.next_quantity();
                const auto price = bmd_test::price_units(price_val);
                const auto quantity = bmd_test::quantity_units(quantity_val);
                auto ctx = make_context(seed_str, transcript_idx, op_idx, op);
                ctx.side = bmd::BookSide::Bid;
                ctx.price = price_val;
                ctx.quantity = quantity_val;
                run_checked_operation(ctx, book, reference, [&] {
                    static_cast<void>(book.apply_level(bmd::BookSide::Bid, price, quantity));
                    reference.apply_level(bmd::BookSide::Bid, price, quantity);
                });
                break;
            }
            case OpType::AskInsert: {
                const auto price_val = rng.next_price();
                const auto quantity_val = rng.next_quantity();
                const auto price = bmd_test::price_units(price_val);
                const auto quantity = bmd_test::quantity_units(quantity_val);
                auto ctx = make_context(seed_str, transcript_idx, op_idx, op);
                ctx.side = bmd::BookSide::Ask;
                ctx.price = price_val;
                ctx.quantity = quantity_val;
                run_checked_operation(ctx, book, reference, [&] {
                    static_cast<void>(book.apply_level(bmd::BookSide::Ask, price, quantity));
                    reference.apply_level(bmd::BookSide::Ask, price, quantity);
                });
                break;
            }
            case OpType::Update: {
                const auto side = (rng.next() % 2 == 0) ? bmd::BookSide::Bid : bmd::BookSide::Ask;
                const auto levels = reference.all_levels(side);
                auto ctx = make_context(seed_str, transcript_idx, op_idx, op);
                ctx.side = side;
                if (levels.empty()) {
                    ctx.skipped = true;
                    run_checked_operation(ctx, book, reference, [] {});
                } else {
                    const auto idx = rng.next_size_t(levels.size());
                    const auto price = levels[idx].price;
                    auto raw_quantity = rng.next_quantity();
                    if (raw_quantity == 0) {
                        raw_quantity = 1;
                    }
                    const auto existing_quantity = levels[idx].quantity.value();
                    if (raw_quantity == existing_quantity) {
                        raw_quantity = (raw_quantity == 1) ? 2 : raw_quantity - 1;
                    }
                    const auto quantity = bmd_test::quantity_units(raw_quantity);
                    ctx.price = price.value();
                    ctx.quantity = raw_quantity;
                    run_checked_operation(ctx, book, reference, [&] {
                        static_cast<void>(book.apply_level(side, price, quantity));
                        reference.apply_level(side, price, quantity);
                    });
                }
                break;
            }
            case OpType::Delete: {
                const auto side = (rng.next() % 2 == 0) ? bmd::BookSide::Bid : bmd::BookSide::Ask;
                const auto levels = reference.all_levels(side);
                auto ctx = make_context(seed_str, transcript_idx, op_idx, op);
                ctx.side = side;
                if (levels.empty()) {
                    ctx.skipped = true;
                    run_checked_operation(ctx, book, reference, [] {});
                } else {
                    const auto idx = rng.next_size_t(levels.size());
                    const auto price = levels[idx].price;
                    const auto quantity = bmd_test::quantity_units(0);
                    ctx.price = price.value();
                    ctx.quantity = 0;
                    run_checked_operation(ctx, book, reference, [&] {
                        static_cast<void>(book.apply_level(side, price, quantity));
                        reference.apply_level(side, price, quantity);
                    });
                }
                break;
            }
            case OpType::MissingDelete: {
                const auto side = (rng.next() % 2 == 0) ? bmd::BookSide::Bid : bmd::BookSide::Ask;
                const auto price = find_missing_price(reference, side, rng);
                const auto quantity = bmd_test::quantity_units(0);
                auto ctx = make_context(seed_str, transcript_idx, op_idx, op);
                ctx.side = side;
                ctx.price = price.value();
                ctx.quantity = 0;
                run_checked_operation(ctx, book, reference, [&] {
                    const auto change = book.apply_level(side, price, quantity);
                    reference.apply_level(side, price, quantity);
                    EXPECT_EQ(change, bmd::LevelChange::Unchanged);
                });
                break;
            }
            case OpType::SameValue: {
                const auto side = (rng.next() % 2 == 0) ? bmd::BookSide::Bid : bmd::BookSide::Ask;
                const auto levels = reference.all_levels(side);
                auto ctx = make_context(seed_str, transcript_idx, op_idx, op);
                ctx.side = side;
                if (levels.empty()) {
                    ctx.skipped = true;
                    run_checked_operation(ctx, book, reference, [] {});
                } else {
                    const auto idx = rng.next_size_t(levels.size());
                    const auto price = levels[idx].price;
                    const auto quantity = levels[idx].quantity;
                    ctx.price = price.value();
                    ctx.quantity = quantity.value();
                    run_checked_operation(ctx, book, reference, [&] {
                        const auto change = book.apply_level(side, price, quantity);
                        EXPECT_EQ(change, bmd::LevelChange::Unchanged);
                        reference.apply_level(side, price, quantity);
                    });
                }
                break;
            }
            case OpType::Batch: {
                std::vector<bmd::LevelUpdate> batch_updates;
                const auto batch_size = rng.next_size_t(kBatchMaxSize) + 1;
                for (std::size_t b = 0; b < batch_size; ++b) {
                    const auto bs = (rng.next() % 2 == 0) ? bmd::BookSide::Bid : bmd::BookSide::Ask;
                    const auto p = bmd_test::price_units(rng.next_price());
                    const auto q = bmd_test::quantity_units(rng.next_quantity());
                    batch_updates.push_back({bs, p, q});
                }
                auto ctx = make_context(seed_str, transcript_idx, op_idx, op);
                ctx.batch_size = batch_size;
                if (!batch_updates.empty()) {
                    ctx.side = batch_updates[0].side;
                    ctx.price = batch_updates[0].price.value();
                    ctx.quantity = batch_updates[0].quantity.value();
                } else {
                    ctx.skipped = true;
                }
                run_checked_operation(ctx, book, reference, [&] {
                    book.apply_updates(batch_updates);
                    for (const auto& u : batch_updates) {
                        reference.apply_level(u.side, u.price, u.quantity);
                    }
                });
                break;
            }
            case OpType::ClearSide: {
                const auto side = (rng.next() % 2 == 0) ? bmd::BookSide::Bid : bmd::BookSide::Ask;
                auto ctx = make_context(seed_str, transcript_idx, op_idx, op);
                ctx.side = side;
                run_checked_operation(ctx, book, reference, [&] {
                    book.clear_side(side);
                    reference.clear_side(side);
                });
                break;
            }
            case OpType::ClearAll: {
                auto ctx = make_context(seed_str, transcript_idx, op_idx, op);
                run_checked_operation(ctx, book, reference, [&] {
                    book.clear();
                    reference.clear();
                });
                break;
            }
            case OpType::ReplaceAll: {
                std::vector<bmd::BookLevel> new_bids;
                std::vector<bmd::BookLevel> new_asks;
                const auto bid_count = rng.next_size_t(8);
                const auto ask_count = rng.next_size_t(8);
                for (std::size_t i = 0; i < bid_count; ++i) {
                    const auto p = bmd_test::price_units(rng.next_price());
                    const auto q = bmd_test::quantity_units(rng.next_quantity());
                    new_bids.push_back({p, q});
                }
                for (std::size_t i = 0; i < ask_count; ++i) {
                    const auto p = bmd_test::price_units(rng.next_price());
                    const auto q = bmd_test::quantity_units(rng.next_quantity());
                    new_asks.push_back({p, q});
                }
                auto ctx = make_context(seed_str, transcript_idx, op_idx, op);
                ctx.bid_count = bid_count;
                ctx.ask_count = ask_count;
                if (!new_bids.empty()) {
                    ctx.side = bmd::BookSide::Bid;
                    ctx.price = new_bids[0].price.value();
                    ctx.quantity = new_bids[0].quantity.value();
                } else if (!new_asks.empty()) {
                    ctx.side = bmd::BookSide::Ask;
                    ctx.price = new_asks[0].price.value();
                    ctx.quantity = new_asks[0].quantity.value();
                } else {
                    ctx.skipped = true;
                }
                run_checked_operation(ctx, book, reference, [&] {
                    book.replace_all(new_bids, new_asks);
                    reference.clear();
                    for (const auto& level : new_bids) {
                        reference.apply_level(bmd::BookSide::Bid, level.price, level.quantity);
                    }
                    for (const auto& level : new_asks) {
                        reference.apply_level(bmd::BookSide::Ask, level.price, level.quantity);
                    }
                });
                break;
            }
            case OpType::TopNQuery: {
                const auto side = (rng.next() % 2 == 0) ? bmd::BookSide::Bid : bmd::BookSide::Ask;
                const auto count = reference.level_count(side);
                const auto limit_choice = rng.next_size_t(5);
                std::size_t limit = 0;
                switch (limit_choice) {
                case 0:
                    limit = 0;
                    break;
                case 1:
                    limit = 1;
                    break;
                case 2:
                    limit = (count > 0) ? count / 2 : 0;
                    break;
                case 3:
                    limit = count;
                    break;
                case 4:
                    limit = (count < std::numeric_limits<std::size_t>::max()) ? count + 1 : count;
                    break;
                }
                auto ctx = make_context(seed_str, transcript_idx, op_idx, op);
                ctx.side = side;
                ctx.limit = limit;
                run_checked_operation(ctx, book, reference, [&] {
                    const auto production = book.top_levels(side, limit);
                    const auto expected = reference.top_levels(side, limit);
                    EXPECT_EQ(production, expected);
                });
                break;
            }
            case OpType::QuantityLookup: {
                const auto side = (rng.next() % 2 == 0) ? bmd::BookSide::Bid : bmd::BookSide::Ask;
                const auto use_existing = (rng.next() % 2 == 0);
                const auto price = [&]() -> bmd::PriceUnits {
                    if (use_existing) {
                        const auto levels = reference.all_levels(side);
                        if (!levels.empty()) {
                            return levels[rng.next_size_t(levels.size())].price;
                        }
                        return bmd_test::price_units(rng.next_price());
                    }
                    return find_missing_price(reference, side, rng);
                }();
                auto ctx = make_context(seed_str, transcript_idx, op_idx, op);
                ctx.side = side;
                ctx.price = price.value();
                ctx.quantity = 0;
                run_checked_operation(ctx, book, reference, [&] {
                    const auto prod_qty = book.quantity_at(side, price);
                    const auto ref_qty = reference.quantity_at(side, price);
                    ASSERT_EQ(prod_qty.has_value(), ref_qty.has_value());
                    if (prod_qty.has_value()) {
                        EXPECT_EQ(prod_qty.value().value(), ref_qty.value().value());
                    }
                });
                break;
            }
            }
        }
    }
}

// NOLINTEND(bugprone-unchecked-optional-access)
