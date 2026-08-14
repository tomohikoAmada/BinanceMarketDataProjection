#pragma once

// Benchmark-only wire preconstruction for the M4 benchmarks and
// AdapterWireReplay (OD-M5-P6-011/015). All protobuf messages are built
// before any measured region; adaptation timing excludes wire construction.

#include "book_state.hpp"
#include "replay_types.hpp"

#include <binance_market_data/market/v1/market_events.pb.h>
#include <binance_market_data/projection_adapter/v1/proto_adapter.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bmd_projection::m5::benchmark::adapter_support {

namespace adapter = binance_market_data::projection_adapter::v1;
namespace core = binance_market_data::projection::v1;
namespace market_wire = binance_market_data::market::v1;

inline constexpr std::size_t kM4UpdateLevelCount = 10;

struct WireIdentity final {
    WireIdentity(core::NumericSpec spec, adapter::ExpectedIdentity identity)
        : numeric_spec{spec}, expected{std::move(identity)} {}

    core::NumericSpec numeric_spec;
    adapter::ExpectedIdentity expected;
};

// Spot BTCUSDT identity with the benchmark numeric spec.
[[nodiscard]] WireIdentity benchmark_wire_identity();

// ExchangeDepthSnapshot with `depth` bids and `depth` asks at the benchmark
// prices/quantities.
[[nodiscard]] market_wire::ExchangeDepthSnapshot make_snapshot_wire(std::size_t depth);

// DepthUpdate with kM4UpdateLevelCount levels at existing benchmark bid
// prices. first/final are the wire update-range ids in contract order.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
[[nodiscard]] market_wire::DepthUpdate
make_update_wire(std::uint64_t first_update_id, std::uint64_t final_update_id,
                 std::optional<std::uint64_t> previous_final_update_id);

enum class PreconstructedKind : std::uint8_t {
    Baseline,
    Update,
    Rebaseline,
    Reset,
    Snapshot,
    Metadata,
    MalformedRange,
};

struct PreconstructedEntry final {
    PreconstructedEntry();

    PreconstructedKind kind{PreconstructedKind::Baseline};
    // Baseline/Rebaseline/Update: the preconstructed wire message plus
    // conversion inputs.
    market_wire::ExchangeDepthSnapshot baseline_wire;
    market_wire::DepthUpdate update_wire;
    // Snapshot: production snapshot context/options.
    adapter::SnapshotContext snapshot_context;
    adapter::SnapshotOptions snapshot_options;
    // MalformedRange: the raw ids for the core-only range validation.
    std::uint64_t malformed_first{};
    std::uint64_t malformed_final{};
    core::NumericSpec conversion_spec;
    adapter::ExpectedIdentity expected;
};

// Preconstructs the full AdapterWireReplay input sequence from a normalized
// replay fixture. Wire construction happens once, entirely outside any
// measured region.
[[nodiscard]] std::vector<PreconstructedEntry>
preconstruct_adapter_wire(const replay::ReplayFixture& fixture);

} // namespace bmd_projection::m5::benchmark::adapter_support
