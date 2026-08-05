#include <binance_market_data/projection/v1/numeric/numeric_spec.hpp>
#include <binance_market_data/projection/v1/order_book/order_book.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <vector>

namespace bmd = binance_market_data::projection::v1;

class ReferenceOrderBook final {
  public:
    void apply_level(bmd::BookSide side, bmd::PriceUnits price, bmd::QuantityUnits quantity) {
        auto& levels = (side == bmd::BookSide::Bid) ? bids_ : asks_;
        auto it = find_level(levels, price);
        if (quantity.value() == 0) {
            if (it != levels.end())
                levels.erase(it);
            return;
        }
        if (it != levels.end()) {
            it->quantity = quantity;
        } else {
            levels.push_back({price, quantity});
        }
        sort_side(side);
    }

    void clear() {
        bids_.clear();
        asks_.clear();
    }
    void clear_side(bmd::BookSide side) { (side == bmd::BookSide::Bid ? bids_ : asks_).clear(); }

    [[nodiscard]] std::size_t level_count(bmd::BookSide side) const {
        return (side == bmd::BookSide::Bid) ? bids_.size() : asks_.size();
    }

    [[nodiscard]] std::vector<bmd::BookLevel> all_levels(bmd::BookSide side) const {
        return (side == bmd::BookSide::Bid) ? bids_ : asks_;
    }

    [[nodiscard]] std::optional<bmd::BookLevel> best(bmd::BookSide side) const {
        const auto& levels = (side == bmd::BookSide::Bid) ? bids_ : asks_;
        if (levels.empty()) return std::nullopt;
        return levels.front();
    }

  private:
    using LevelVec = std::vector<bmd::BookLevel>;

    [[nodiscard]] LevelVec::iterator find_level(LevelVec& v, bmd::PriceUnits p) {
        for (auto it = v.begin(); it != v.end(); ++it) {
            if (it->price == p)
                return it;
        }
        return v.end();
    }

    [[nodiscard]] LevelVec::const_iterator find_level(const LevelVec& v, bmd::PriceUnits p) const {
        for (auto it = v.begin(); it != v.end(); ++it) {
            if (it->price == p)
                return it;
        }
        return v.end();
    }

    void sort_side(bmd::BookSide side) {
        auto& levels = (side == bmd::BookSide::Bid) ? bids_ : asks_;
        if (side == bmd::BookSide::Bid) {
            std::sort(levels.begin(), levels.end(),
                      [](auto& a, auto& b) { return b.price < a.price; });
        } else {
            std::sort(levels.begin(), levels.end(),
                      [](auto& a, auto& b) { return a.price < b.price; });
        }
    }

    LevelVec bids_;
    LevelVec asks_;
};

namespace {

void verify_no_zero_quantities(const std::vector<bmd::BookLevel>& levels) {
    for (const auto& l : levels) {
        if (l.quantity.value() <= 0)
            std::abort();
    }
}

void verify_strict_ordering(const std::vector<bmd::BookLevel>& levels, bool ascending) {
    for (std::size_t i = 1; i < levels.size(); ++i) {
        if (ascending) {
            if (levels[i - 1].price.value() >= levels[i].price.value())
                std::abort();
        } else {
            if (levels[i - 1].price.value() <= levels[i].price.value())
                std::abort();
        }
    }
}

void verify_no_duplicates(const std::vector<bmd::BookLevel>& levels) {
    for (std::size_t i = 1; i < levels.size(); ++i) {
        if (levels[i - 1].price == levels[i].price)
            std::abort();
    }
}

void check_consistency(const bmd::OrderBook& book, const ReferenceOrderBook& ref) {
    for (auto side : {bmd::BookSide::Bid, bmd::BookSide::Ask}) {
        if (book.level_count(side) != ref.level_count(side))
            std::abort();
        const auto prod = book.all_levels(side);
        const auto refv = ref.all_levels(side);
        if (prod.size() != refv.size())
            std::abort();
        for (std::size_t i = 0; i < prod.size(); ++i) {
            if (!(prod[i].price == refv[i].price))
                std::abort();
            if (!(prod[i].quantity == refv[i].quantity))
                std::abort();
        }
        verify_no_zero_quantities(prod);
        const bool ascending = (side == bmd::BookSide::Ask);
        verify_strict_ordering(prod, ascending);
        verify_no_duplicates(prod);
    }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size < 4)
        return 0;

    const auto price_scale_raw = static_cast<std::uint8_t>(data[0] % 19);
    const auto quantity_scale_raw = static_cast<std::uint8_t>(data[1] % 19);
    const auto max_ops =
        static_cast<std::size_t>(data[2]) | (static_cast<std::size_t>(data[3]) << 8);
    constexpr std::size_t kMaxOperations = 512;
    const auto op_count = (max_ops == 0) ? std::size_t{1} : (max_ops % kMaxOperations) + 1;

    auto ps = bmd::DecimalScale::create(price_scale_raw);
    auto qs = bmd::DecimalScale::create(quantity_scale_raw);
    if (!ps.has_value() || !qs.has_value())
        return 0;

    bmd::NumericSpec spec{*ps, *qs};
    bmd::OrderBook book{spec};
    ReferenceOrderBook ref;
    std::vector<bmd::LevelUpdate> batch_buf;
    batch_buf.reserve(16);

    std::size_t offset = 4;
    for (std::size_t i = 0; i < op_count; ++i) {
        if (offset + 4 > size)
            break;

        const auto op_type = data[offset] % 12;
        const auto price_raw =
            static_cast<std::int64_t>((static_cast<std::int64_t>(data[offset + 1] % 128) + 1));
        auto qty_raw = static_cast<std::int64_t>(data[offset + 2]);
        const auto side_bit = (data[offset + 3] & 1);
        offset += 4;

        auto p = bmd::PriceUnits::create(price_raw);
        if (!p.has_value())
            continue;

        if (data[offset - 4] == static_cast<std::uint8_t>(0xFF)) {
            qty_raw = 0;
        }
        if (data[offset - 3] == static_cast<std::uint8_t>(0xFE)) {
            qty_raw = 1;
        }
        if (data[offset - 3] == static_cast<std::uint8_t>(0xFD)) {
            qty_raw = std::numeric_limits<std::int64_t>::max();
        }
        auto q = bmd::QuantityUnits::create(qty_raw);
        if (!q.has_value())
            continue;

        const auto side = side_bit == 0 ? bmd::BookSide::Bid : bmd::BookSide::Ask;

        switch (op_type) {
        case 0:
        case 1:
        case 2:
            static_cast<void>(book.apply_level(side, *p, *q));
            ref.apply_level(side, *p, *q);
            break;
        case 3:
            for (const auto& level : ref.all_levels(side)) {
                auto existing_q = bmd::QuantityUnits::create(level.quantity.value());
                if (existing_q.has_value()) {
                    static_cast<void>(book.apply_level(side, level.price, *existing_q));
                }
            }
            break;
        case 4:
        case 5:
            batch_buf.clear();
            for (std::size_t j = 0; j < 5 && offset + 2 < size; ++j) {
                const auto bp_raw = static_cast<std::int64_t>((data[offset] % 128) + 1);
                const auto bq_raw = static_cast<std::int64_t>(data[offset + 1]);
                offset += 2;
                auto bp = bmd::PriceUnits::create(bp_raw);
                auto bq = bmd::QuantityUnits::create(bq_raw);
                if (bp.has_value() && bq.has_value()) {
                    batch_buf.push_back({side, *bp, *bq});
                }
            }
            if (!batch_buf.empty()) {
                book.apply_updates(batch_buf);
                for (const auto& u : batch_buf) {
                    ref.apply_level(u.side, u.price, u.quantity);
                }
            }
            break;
        case 6:
            book.clear_side(side);
            ref.clear_side(side);
            break;
        case 7:
            book.clear();
            ref.clear();
            break;
        case 8: {
            std::vector<bmd::BookLevel> new_bids;
            std::vector<bmd::BookLevel> new_asks;
            for (std::size_t j = 0; j < 4 && offset + 2 < size; ++j) {
                const auto rp_raw = static_cast<std::int64_t>((data[offset] % 128) + 1);
                const auto rq_raw = static_cast<std::int64_t>(data[offset + 1]);
                offset += 2;
                auto rp = bmd::PriceUnits::create(rp_raw);
                auto rq = bmd::QuantityUnits::create(rq_raw);
                if (rp.has_value() && rq.has_value()) {
                    if (j % 2 == 0)
                        new_bids.push_back({*rp, *rq});
                    else
                        new_asks.push_back({*rp, *rq});
                }
            }
            book.replace_all(new_bids, new_asks);
            ref.clear();
            for (const auto& l : new_bids)
                ref.apply_level(bmd::BookSide::Bid, l.price, l.quantity);
            for (const auto& l : new_asks)
                ref.apply_level(bmd::BookSide::Ask, l.price, l.quantity);
            break;
        }
        case 9:
        case 10:
        case 11:
            break;
        }

        check_consistency(book, ref);
    }

    return 0;
}
