#pragma once

// M5_BENCHMARK_WRAPPER_V1 metadata/provenance wrapper
// (OD-M5-P6-021/022/023/029). The wrapper separates source/binary provenance,
// build identity, environment identity, workload identity, measurement
// identity, and result-payload binding, and fails closed: the payload SHA-256
// binds the wrapper to the exact payload file it describes.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace bmd_projection::m5::benchmark {

inline constexpr const char* kWrapperSchema = "M5_BENCHMARK_WRAPPER_V1";
inline constexpr const char* kMeasurementContract = "M5_PHASE6_MEASUREMENT_CONTRACT_V1";
inline constexpr const char* kLatencySchema = "M5_REPLAY_LATENCY_V1";

struct WorkloadRecord final {
    std::string benchmark_name;
    std::string canonical_text;
    std::string canonical_sha256;
};

struct WrapperMeasurement final {
    std::string name;
    std::uint64_t iterations{};
    double real_time_ns{};
    double cpu_time_ns{};
    std::string time_unit;
    double items_per_second{};
};

struct WrapperInput final {
    std::string evidence_class{"formal"};
    std::string binary_path;
    std::string payload_path;
    std::string payload_schema{"google_benchmark_json"};
    std::vector<WorkloadRecord> workloads;
    std::vector<WrapperMeasurement> measurements;
    std::string timer{"cpu"};
    std::string primary_denominator{"cpu_time"};
    std::string warmup_kind;
    std::uint64_t warmup_count{};
    std::uint64_t repetitions{};
    std::uint64_t sample_count{};
    std::uint64_t unique_event_count{};
    std::uint64_t passes{};
    std::string quantile_estimator;
    std::string checksum_methodology;
    std::vector<std::pair<std::string, std::string>> extra_measurement_fields;
};

// Computes binary/payload SHA-256, collects runtime environment identity, and
// serializes the wrapper. Returns an empty string on fatal binding failures
// (e.g. unreadable payload), so callers fail closed.
[[nodiscard]] std::string build_wrapper_json(const WrapperInput& input);

} // namespace bmd_projection::m5::benchmark
