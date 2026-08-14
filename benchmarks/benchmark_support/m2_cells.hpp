#pragma once

// M2 stateful benchmark cells (OD-M5-P6-004/005).
//
// Every measured execution of a cell has the same semantic precondition and
// expected disposition as the first execution. Pool-based cells (insert and
// delete) consume one freshly prepared state per measured execution; the pool
// is built and rebuilt entirely outside the measured region. Non-pool cells
// reuse one state whose disposition is provably identical on every execution
// (quantity alternation, absent targets, or read-only queries).

#include "book_state.hpp"

#include <binance_market_data/projection/v1/order_book/order_book.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bmd_projection::m5::benchmark {

enum class M2ApplyLevelKind : std::uint8_t {
    Insert,
    Update,
    Delete,
    MissingDelete,
};

class M2ApplyLevelCell final {
  public:
    M2ApplyLevelCell(M2ApplyLevelKind kind, std::size_t depth);

    // Rebuilds all prepared state; must run before each Google Benchmark
    // invocation (setup is outside the measured region).
    void prepare();

    [[nodiscard]] bool uses_pool() const noexcept;
    [[nodiscard]] std::size_t pool_size() const noexcept;

    // Executes one measured logical operation. For pool cells, pool_index
    // selects the fresh state consumed by this execution.
    [[nodiscard]] core::LevelChange execute_step(std::size_t pool_index = 0);

    [[nodiscard]] M2ApplyLevelKind kind() const noexcept { return kind_; }
    [[nodiscard]] std::size_t depth() const noexcept { return depth_; }
    [[nodiscard]] const core::OrderBook& book() const noexcept { return book_; }
    [[nodiscard]] std::size_t prepared_update_slot_count() const noexcept {
        return update_slots_.size();
    }

    // Guarded access to the prepared single update used by the pool and
    // idempotent cells (never by the alternating Update cell).
    [[nodiscard]] const core::LevelUpdate& prepared_update() const noexcept {
        if (!update_.has_value()) {
            std::abort();
        }
        return *update_;
    }
    [[nodiscard]] const std::string& generated_workload_sha256() const noexcept {
        return generated_sha_;
    }

  private:
    M2ApplyLevelKind kind_;
    std::size_t depth_;
    core::OrderBook book_;
    std::optional<core::LevelUpdate> update_;
    std::vector<core::LevelUpdate> update_slots_;
    std::size_t update_slot_{0};
    StatePool<core::OrderBook> pool_;
    std::string generated_sha_;
};

enum class M2ApplyUpdatesMix : std::uint8_t {
    ReplacementHeavy,
    Insertion,
};

// Pure identity generators. They hash the concrete prepared state and update
// sequence without constructing the bounded execution pools.
[[nodiscard]] std::string m2_apply_level_generated_sha256(M2ApplyLevelKind kind, std::size_t depth);
[[nodiscard]] std::string m2_replace_all_generated_sha256(std::size_t depth);

class M2ApplyUpdatesCell final {
  public:
    struct Config final {
        std::size_t depth;
        std::size_t batch;
        M2ApplyUpdatesMix mix;
    };

    // For batch updates: depth levels per side, batch of `batch` replacement
    // updates per execution. For the primary scaling workload (update_mix),
    // depth runs over the full {0, 8, 100, 1000, 5000, 10000} set and the
    // D=0 cell is explicitly labelled as the empty-book insertion edge.
    explicit M2ApplyUpdatesCell(Config config);

    void prepare();

    [[nodiscard]] bool uses_pool() const noexcept;
    [[nodiscard]] std::size_t pool_size() const noexcept;

    void execute_step(std::size_t pool_index = 0);

    [[nodiscard]] std::size_t depth() const noexcept { return depth_; }
    [[nodiscard]] std::size_t batch() const noexcept { return batch_; }
    [[nodiscard]] M2ApplyUpdatesMix mix() const noexcept { return mix_; }
    [[nodiscard]] const core::OrderBook& book() const noexcept { return book_; }
    [[nodiscard]] const core::OrderBook& pooled_book(std::size_t index) const {
        return pool_.at(index);
    }
    [[nodiscard]] std::size_t prepared_batch_count() const noexcept {
        return cycle_batches_.size();
    }
    [[nodiscard]] const std::string& generated_workload_sha256() const noexcept {
        return generated_sha_;
    }

  private:
    std::size_t depth_;
    std::size_t batch_;
    M2ApplyUpdatesMix mix_;
    core::OrderBook book_;
    std::vector<std::vector<core::LevelUpdate>> cycle_batches_;
    std::size_t step_{0};
    StatePool<core::OrderBook> pool_;
    std::string generated_sha_;
};

[[nodiscard]] std::string
m2_apply_updates_generated_sha256(const M2ApplyUpdatesCell::Config& config);

// Prepared read-only query state. All query cells are idempotent over a fixed
// book, so a single prepared book is shared across every measured execution.
class M2QueryCell final {
  public:
    struct Config final {
        std::size_t depth;
        std::size_t query_limit;
    };

    explicit M2QueryCell(Config config);

    void prepare();

    // best_bid / best_ask.
    [[nodiscard]] std::optional<core::BookLevel> best(core::BookSide side) const;
    // quantity_at: hit reads the best bid price, miss reads an absent price.
    [[nodiscard]] std::optional<core::QuantityUnits> quantity_at_hit() const;
    [[nodiscard]] std::optional<core::QuantityUnits> quantity_at_miss() const;
    // top_levels with the recorded query limit.
    [[nodiscard]] std::vector<core::BookLevel> top_levels(core::BookSide side) const;
    // all_levels copy of both sides.
    [[nodiscard]] std::vector<core::BookLevel> all_levels(core::BookSide side) const;

    [[nodiscard]] std::size_t depth() const noexcept { return depth_; }
    [[nodiscard]] std::size_t query_limit() const noexcept { return query_limit_; }
    [[nodiscard]] const core::OrderBook& book() const noexcept { return book_; }
    [[nodiscard]] const std::string& generated_workload_sha256() const noexcept {
        return generated_sha_;
    }

  private:
    std::size_t depth_;
    std::size_t query_limit_;
    core::OrderBook book_;
    std::string generated_sha_;
};

[[nodiscard]] std::string m2_query_generated_sha256(std::string_view operation,
                                                    const M2QueryCell::Config& config);

class M2ReplaceAllCell final {
  public:
    explicit M2ReplaceAllCell(std::size_t depth);

    void prepare();

    // Replaces the book with the prebuilt canonical vectors. The post-state is
    // exactly the canonical expected book after every execution, so one book
    // is reused forever.
    void execute_step();

    [[nodiscard]] std::size_t depth() const noexcept { return depth_; }
    [[nodiscard]] const core::OrderBook& book() const noexcept { return book_; }
    [[nodiscard]] const std::string& generated_workload_sha256() const noexcept {
        return generated_sha_;
    }

  private:
    std::size_t depth_;
    core::OrderBook book_;
    std::vector<core::BookLevel> bids_;
    std::vector<core::BookLevel> asks_;
    std::string generated_sha_;
};

// M3 component/proxy measurements (OD-M5-P6-009): public API operations
// analogous to internal transaction stages. They are approximate proxy
// measurements, never claimed as an exact BookProjection::apply decomposition.
class M3ProxyCells final {
  public:
    explicit M3ProxyCells(std::size_t depth);

    void prepare();

    // M3/Component/AllLevelsBothSides: both-side all_levels copies from a
    // synchronized production projection.
    [[nodiscard]] std::vector<core::BookLevel> all_levels_both_sides() const;
    // M3/Proxy/CandidateRebuildFromVectors: fresh OrderBook + replace_all from
    // prebuilt vectors.
    [[nodiscard]] core::OrderBook candidate_rebuild_from_vectors() const;
    // M3/Proxy/CandidateApplyUpdates: apply a prebuilt batch to a candidate
    // book rebuilt from the canonical vectors.
    void candidate_apply_updates(core::OrderBook& candidate, std::size_t step) const;
    // M3/Proxy/OrderBookMoveCommit: destination must hold the populated old
    // book; measurement includes destination destruction (OD-M5-P6-009).
    static void move_commit(core::OrderBook& destination, core::OrderBook source);

    [[nodiscard]] std::size_t depth() const noexcept { return depth_; }
    [[nodiscard]] const core::BookProjection& projection() const noexcept { return projection_; }
    [[nodiscard]] const std::string& generated_workload_sha256() const noexcept {
        return generated_sha_;
    }

  private:
    std::size_t depth_;
    core::BookProjection projection_;
    std::vector<core::BookLevel> bids_;
    std::vector<core::BookLevel> asks_;
    std::vector<std::vector<core::LevelUpdate>> cycle_batches_;
    std::string generated_sha_;
};

} // namespace bmd_projection::m5::benchmark
