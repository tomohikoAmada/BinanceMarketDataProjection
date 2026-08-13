#pragma once

// M3 benchmark cell machinery (OD-M5-P6-006 through OD-M5-P6-009).
//
// Accepted live-apply cells: every measured execution begins Synchronized and
// every intended accepted operation actually returns Applied. D>0 cells use
// existing-price quantity updates so book depth stays fixed while mutation is
// real; B=0 cells traverse the real accepted production transaction with an
// empty level span; D=0/B>0 is the explicitly labelled empty-book insertion
// edge, measured against a bounded pool of freshly prepared empty books.
//
// Classification cells: stale/duplicate use stable repeated non-mutating
// state; gap/reset/baseline-install are one-shot state changes measured
// against freshly prepared state pools (never "first change + subsequent
// RejectedWrongState" under one label).

#include "book_state.hpp"

#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace bmd_projection::m5::benchmark {

inline constexpr std::size_t kM3ClassificationDepth = 100;

class M3AcceptedCell final {
  public:
    struct Config final {
        core::SequencePolicyKind policy;
        std::size_t depth;
        std::size_t batch;
    };

    explicit M3AcceptedCell(Config config);

    // Rebuilds all prepared state; runs before each Google Benchmark
    // invocation, entirely outside the measured region.
    void prepare();

    [[nodiscard]] bool uses_pool() const noexcept;
    [[nodiscard]] std::size_t pool_size() const noexcept;

    // Executes one measured accepted live apply. The disposition must be
    // Applied for every execution; benchmarks/tests enforce this.
    [[nodiscard]] core::ApplyResult execute_step(std::size_t pool_index = 0);

    [[nodiscard]] const Config& config() const noexcept { return config_; }
    [[nodiscard]] const core::BookProjection& projection() const noexcept { return projection_; }
    [[nodiscard]] core::UpdateId current_update_id() const noexcept {
        return core::UpdateId{current_id_};
    }
    [[nodiscard]] const std::string& generated_workload_sha256() const noexcept {
        return generated_sha_;
    }

  private:
    Config config_;
    core::BookProjection projection_;
    StatePool<core::BookProjection> pool_;
    std::vector<std::vector<core::LevelUpdate>> cycle_batches_;
    std::vector<core::LevelUpdate> insert_batch_;
    std::vector<core::LevelUpdate> empty_levels_;
    std::size_t step_{0};
    std::uint64_t current_id_{0};
    std::string generated_sha_;
};

enum class M3ClassificationKind : std::uint8_t {
    Stale,
    Duplicate,
    Gap,
    Reset,
    BaselineInstall,
};

struct M3ClassificationResult final {
    std::optional<core::ApplyDisposition> apply_disposition;
    std::optional<core::InstallDisposition> install_disposition;
    core::ProjectionStatus status_after{core::ProjectionStatus::AwaitingBaseline};
};

class M3ClassificationCell final {
  public:
    struct Config final {
        M3ClassificationKind kind;
        core::SequencePolicyKind policy;
        std::size_t depth;
    };

    explicit M3ClassificationCell(Config config);

    void prepare();

    [[nodiscard]] bool uses_pool() const noexcept;
    [[nodiscard]] std::size_t pool_size() const noexcept;

    [[nodiscard]] M3ClassificationResult execute_step(std::size_t pool_index = 0);

    [[nodiscard]] const Config& config() const noexcept { return config_; }
    [[nodiscard]] const core::BookProjection& projection() const noexcept { return projection_; }
    [[nodiscard]] const std::string& generated_workload_sha256() const noexcept {
        return generated_sha_;
    }

  private:
    Config config_;
    core::BookProjection projection_;
    StatePool<core::BookProjection> pool_;
    std::vector<core::BookLevel> baseline_bids_;
    std::vector<core::BookLevel> baseline_asks_;
    std::optional<core::DepthBatch> stale_batch_;
    std::optional<core::DepthBatch> duplicate_batch_;
    std::optional<core::DepthBatch> gap_batch_;
    std::string generated_sha_;
};

// The 48 accepted live-apply cell names, generated in the canonical order
// (policy x depth x batch) used by the inventory validator.
[[nodiscard]] std::vector<std::string> expected_m3_accepted_cell_names();

} // namespace bmd_projection::m5::benchmark
