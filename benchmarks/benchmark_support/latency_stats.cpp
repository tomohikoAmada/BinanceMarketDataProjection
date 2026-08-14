#include "latency_stats.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bmd_projection::m5::benchmark {

std::optional<std::uint64_t> LatencyReport::quantile(double probability) const {
    if (sorted_samples_ns.empty() || probability <= 0.0 || probability > 1.0) {
        return std::nullopt;
    }
    const auto scaled = std::ceil(probability * static_cast<double>(sorted_samples_ns.size()));
    if (scaled < 1.0) {
        return std::nullopt;
    }
    const auto rank = static_cast<std::size_t>(scaled);
    return sorted_samples_ns.at(rank - 1);
}

bool LatencyReport::p50_eligible() const noexcept { return sample_count >= kMinimumSamplesP50; }

bool LatencyReport::p90_eligible() const noexcept { return sample_count >= kMinimumSamplesP90; }

bool LatencyReport::p99_eligible() const noexcept { return sample_count >= kMinimumSamplesP99; }

bool LatencyReport::p999_eligible() const noexcept {
    return sample_count >= kMinimumSamplesP999 && unique_event_count >= kMinimumUniqueEventsP999;
}

std::string LatencyReport::p999_omission_reason() const {
    if (p999_eligible()) {
        return {};
    }
    std::string reason = "ineligible: ";
    if (sample_count < kMinimumSamplesP999) {
        reason += "sample_count " + std::to_string(sample_count) + " < " +
                  std::to_string(kMinimumSamplesP999);
    }
    if (unique_event_count < kMinimumUniqueEventsP999) {
        if (sample_count < kMinimumSamplesP999) {
            reason += "; ";
        }
        reason += "unique_event_count " + std::to_string(unique_event_count) + " < " +
                  std::to_string(kMinimumUniqueEventsP999);
    }
    return reason;
}

LatencyReport make_latency_report(std::vector<std::uint64_t> samples_ns,
                                  LatencyBookkeeping bookkeeping) {
    LatencyReport report;
    report.sample_count = samples_ns.size();
    report.unique_event_count = bookkeeping.unique_event_count;
    report.passes = bookkeeping.passes;
    report.sorted_samples_ns = std::move(samples_ns);
    std::sort(report.sorted_samples_ns.begin(), report.sorted_samples_ns.end());
    return report;
}

} // namespace bmd_projection::m5::benchmark
