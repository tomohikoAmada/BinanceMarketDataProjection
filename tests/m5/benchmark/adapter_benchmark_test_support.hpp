#pragma once

#include <cstddef>
#include <cstdint>

namespace bmd_projection::m5::benchmark::test_support {

struct RepeatObservation final {
    bool full_preconstruction;
    bool first_baseline_adapts;
    bool rebaseline_uses_adapter_wire;
    bool corrupt_rebaseline_is_rejected;
    std::size_t event_count;
    std::uint64_t first_checksum;
    std::uint64_t second_checksum;
    bool first_synchronized;
    bool second_synchronized;
};

struct MarketObservation final {
    std::uint64_t spot_checksum;
    std::uint64_t usdm_checksum;
    bool usdm_synchronized;
};

[[nodiscard]] RepeatObservation observe_spot_replay_repeat();
[[nodiscard]] MarketObservation observe_spot_usdm_replays();

} // namespace bmd_projection::m5::benchmark::test_support
