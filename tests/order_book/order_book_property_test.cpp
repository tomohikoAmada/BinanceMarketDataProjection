#include "reference_order_book.hpp"
#include "test_helpers.hpp"

#include <binance_market_data/projection/v1/order_book/book_level.hpp>
#include <binance_market_data/projection/v1/order_book/level_update.hpp>
#include <binance_market_data/projection/v1/order_book/order_book.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <vector>

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
        if (limit == 0)
            return 0;
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

[[nodiscard]] std::string failure_context(const std::string& seed, std::size_t transcript_idx,
                                          std::size_t op_idx, OpType type, bmd::BookSide side,
                                          Price price, Quantity quantity) {
    std::ostringstream oss;
    oss << "seed=" << seed << " transcript=" << transcript_idx << " op=" << op_idx
        << " type=" << op_name(type) << " side=" << to_string(side) << " price=" << price
        << " quantity=" << quantity;
    return oss.str();
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

    verify_no_zero_quantities(prod_bids);
    verify_no_zero_quantities(prod_asks);

    verify_strict_ordering(prod_bids, false);
    verify_strict_ordering(prod_asks, true);

    verify_no_duplicates(prod_bids);
    verify_no_duplicates(prod_asks);
}

} // namespace

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

            SCOPED_TRACE(
                failure_context(seed_str, transcript_idx, op_idx, op, bmd::BookSide::Bid, 0, 0));

            switch (op) {
            case OpType::BidInsert: {
                const auto price = bmd_test::price_units(rng.next_price());
                const auto quantity = bmd_test::quantity_units(rng.next_quantity());
                static_cast<void>(book.apply_level(bmd::BookSide::Bid, price, quantity));
                reference.apply_level(bmd::BookSide::Bid, price, quantity);
                break;
            }
            case OpType::AskInsert: {
                const auto price = bmd_test::price_units(rng.next_price());
                const auto quantity = bmd_test::quantity_units(rng.next_quantity());
                static_cast<void>(book.apply_level(bmd::BookSide::Ask, price, quantity));
                reference.apply_level(bmd::BookSide::Ask, price, quantity);
                break;
            }
            case OpType::Update: {
                const auto side = (rng.next() % 2 == 0) ? bmd::BookSide::Bid : bmd::BookSide::Ask;
                const auto levels = reference.all_levels(side);
                if (!levels.empty()) {
                    const auto idx = rng.next_size_t(levels.size());
                    const auto price = levels[idx].price;
                    const auto quantity = bmd_test::quantity_units(
                        rng.next_quantity() == 0 ? 1 : rng.next_quantity());
                    static_cast<void>(book.apply_level(side, price, quantity));
                    reference.apply_level(side, price, quantity);
                }
                break;
            }
            case OpType::Delete: {
                const auto side = (rng.next() % 2 == 0) ? bmd::BookSide::Bid : bmd::BookSide::Ask;
                const auto levels = reference.all_levels(side);
                if (!levels.empty()) {
                    const auto idx = rng.next_size_t(levels.size());
                    const auto price = levels[idx].price;
                    const auto quantity = bmd_test::quantity_units(0);
                    static_cast<void>(book.apply_level(side, price, quantity));
                    reference.apply_level(side, price, quantity);
                }
                break;
            }
            case OpType::MissingDelete: {
                const auto side = (rng.next() % 2 == 0) ? bmd::BookSide::Bid : bmd::BookSide::Ask;
                const auto price = bmd_test::price_units(rng.next_price());
                const auto quantity = bmd_test::quantity_units(0);
                const auto change = book.apply_level(side, price, quantity);
                reference.apply_level(side, price, quantity);
                EXPECT_EQ(change, bmd::LevelChange::Unchanged);
                break;
            }
            case OpType::SameValue: {
                const auto side = (rng.next() % 2 == 0) ? bmd::BookSide::Bid : bmd::BookSide::Ask;
                const auto levels = reference.all_levels(side);
                if (!levels.empty()) {
                    const auto idx = rng.next_size_t(levels.size());
                    const auto price = levels[idx].price;
                    const auto quantity = levels[idx].quantity;
                    const auto change = book.apply_level(side, price, quantity);
                    EXPECT_EQ(change, bmd::LevelChange::Unchanged);
                    reference.apply_level(side, price, quantity);
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
                book.apply_updates(batch_updates);
                for (const auto& u : batch_updates) {
                    reference.apply_level(u.side, u.price, u.quantity);
                }
                break;
            }
            case OpType::ClearSide: {
                const auto side = (rng.next() % 2 == 0) ? bmd::BookSide::Bid : bmd::BookSide::Ask;
                book.clear_side(side);
                reference.clear_side(side);
                break;
            }
            case OpType::ClearAll: {
                book.clear();
                reference.clear();
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
                book.replace_all(new_bids, new_asks);
                reference.clear();
                for (const auto& level : new_bids) {
                    reference.apply_level(bmd::BookSide::Bid, level.price, level.quantity);
                }
                for (const auto& level : new_asks) {
                    reference.apply_level(bmd::BookSide::Ask, level.price, level.quantity);
                }
                break;
            }
            case OpType::TopNQuery:
            case OpType::QuantityLookup:
                break;
            }

            verify_consistency(book, reference);
        }
    }
}
