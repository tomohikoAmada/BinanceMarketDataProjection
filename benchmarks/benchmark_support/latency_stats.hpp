#pragma once

// Event-latency sample statistics (OD-M5-P6-018 / OD-M5-P6-032).
//
// Estimator: nearest-rank-v1. For ascending samples x[0..n-1]:
// Q(p) = x[ceil(p*n) - 1]. No interpolation. Calibration samples are reported
// separately and are NEVER subtracted from event latency samples.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bmd_projection::m5::benchmark {

inline constexpr std::string_view kQuantileEstimator = "nearest-rank-v1";
inline constexpr std::size_t kMinimumSamplesP50 = 1'000;
inline constexpr std::size_t kMinimumSamplesP90 = 1'000;
inline constexpr std::size_t kMinimumSamplesP99 = 10'000;
inline constexpr std::size_t kMinimumSamplesP999 = 100'000;
inline constexpr std::size_t kMinimumUniqueEventsP999 = 100'000;

struct LatencyReport final {
    std::size_t sample_count{};
    std::size_t unique_event_count{};
    std::size_t passes{};
    std::vector<std::uint64_t> sorted_samples_ns;

    [[nodiscard]] std::optional<std::uint64_t> quantile(double probability) const;
    [[nodiscard]] bool p50_eligible() const noexcept;
    [[nodiscard]] bool p90_eligible() const noexcept;
    [[nodiscard]] bool p99_eligible() const noexcept;
    [[nodiscard]] bool p999_eligible() const noexcept;
    [[nodiscard]] std::string p999_omission_reason() const;
};

struct LatencyBookkeeping final {
    std::size_t unique_event_count{};
    std::size_t passes{};
};

// Builds a LatencyReport from unsorted samples: sorts a copy, records the
// sample/pass/unique-event bookkeeping supplied by the caller.
[[nodiscard]] LatencyReport make_latency_report(std::vector<std::uint64_t> samples_ns,
                                                LatencyBookkeeping bookkeeping);

} // namespace bmd_projection::m5::benchmark
