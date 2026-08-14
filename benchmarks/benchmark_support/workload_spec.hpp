#pragma once

// M5 Phase-6 synthetic workload identity (OD-M5-P6-023).
//
// Every registered benchmark owns a canonical workload-spec representation.
// The canonical text is a deterministic key-sorted "key=value" document; its
// SHA-256 is the workload-spec identity. Seed alone is never sufficient:
// generator schema/version, seed, and all parameters are part of the spec.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bmd_projection::m5::benchmark {

inline constexpr std::string_view kWorkloadSpecSchema = "M5_BENCHMARK_WORKLOAD_SPEC_V1";

// Builds the canonical workload-spec text for one benchmark. Keys are stored
// sorted by key; a duplicate key overwrites the previous value. The canonical
// text and its SHA-256 are computed once by build().
class WorkloadSpecBuilder final {
  public:
    explicit WorkloadSpecBuilder(std::string benchmark_name);

    WorkloadSpecBuilder& set(std::string_view key, std::string_view value);
    WorkloadSpecBuilder& set(std::string_view key, std::uint64_t value);

    [[nodiscard]] std::string canonical_text() const;
    [[nodiscard]] std::string canonical_sha256() const;

    [[nodiscard]] const std::string& benchmark_name() const noexcept { return benchmark_name_; }
    [[nodiscard]] const std::vector<std::pair<std::string, std::string>>& fields() const noexcept {
        return fields_;
    }

    // Completes the generated-workload identity from the deterministic input
    // description before the registry is frozen. This deliberately happens
    // independently of the active benchmark filter.
    void complete_generated_identity();

  private:
    std::string benchmark_name_;
    std::vector<std::pair<std::string, std::string>> fields_;
    bool finalized_{false};
    mutable std::string canonical_text_;
    mutable std::string canonical_sha256_;
};

// Builds and registers a workload spec at static-initialization time. The
// registry is complete regardless of the active Google Benchmark filter, which
// is what lets the inventory validator prove that all required benchmark
// families are REGISTERED even when CI smoke executes only a subset.
WorkloadSpecBuilder& register_workload(std::string benchmark_name);

// Frozen view of the static registration registry.
[[nodiscard]] const std::vector<std::pair<std::string, std::string>>& registered_workloads();

// Clears the registry (test support only).
void clear_registered_workloads_for_testing();

} // namespace bmd_projection::m5::benchmark
