// Phase-6 benchmark entry point. A custom display reporter collects run-level
// measurements while Google Benchmark still writes the official JSON payload
// through --benchmark_out; after the run completes the M5_BENCHMARK_WRAPPER_V1
// wrapper binds the payload SHA-256 and the full provenance (OD-M5-P6-037
// through OD-M5-P6-045).

#include "benchmark_support/environment_identity.hpp"
#include "benchmark_support/workload_spec.hpp"
#include "benchmark_support/wrapper.hpp"
#include "canonical_text.hpp"

#include <benchmark/benchmark.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

namespace bm = bmd_projection::m5::benchmark;

struct RunRecord final {
    std::string name;
    std::int64_t iterations{};
    double real_time_ns{};
    double cpu_time_ns{};
    double items_per_second{};
};

class CollectingReporter final : public benchmark::BenchmarkReporter {
  public:
    bool ReportContext(const Context& context) override {
        num_cpus_ = context.cpu_info.num_cpus;
        cycles_per_second_ = context.cpu_info.cycles_per_second;
        executable_ =
            Context::executable_name == nullptr ? std::string{} : Context::executable_name;
        return true;
    }

    void ReportRuns(const std::vector<Run>& reports) override {
        for (const auto& report : reports) {
            if (report.run_type != Run::RT_Iteration) {
                continue;
            }
            RunRecord record;
            record.name = report.run_name.function_name;
            record.iterations = report.iterations;
            const auto iterations =
                static_cast<double>(report.iterations > 0 ? report.iterations : 1);
            // real_accumulated_time/cpu_accumulated_time are always nanoseconds.
            record.real_time_ns = static_cast<double>(report.real_accumulated_time) / iterations;
            record.cpu_time_ns = static_cast<double>(report.cpu_accumulated_time) / iterations;
            const auto items = report.counters.find("items_per_second");
            record.items_per_second =
                items == report.counters.end() ? 0.0 : static_cast<double>(items->second);
            records_.push_back(record);
        }
        // Minimal progress output (the JSON payload remains authoritative).
        for (const auto& report : reports) {
            std::fprintf(stdout, "[phase6] %s %s %lld ns/op\n", report.benchmark_name().c_str(),
                         report.run_name.str().c_str(),
                         static_cast<long long>(report.GetAdjustedRealTime()));
        }
    }

    void Finalize() override {}

    [[nodiscard]] const std::vector<RunRecord>& records() const noexcept { return records_; }
    [[nodiscard]] const std::string& executable() const noexcept { return executable_; }
    [[nodiscard]] double cycles_per_second() const noexcept { return cycles_per_second_; }
    [[nodiscard]] int num_cpus() const noexcept { return num_cpus_; }

  private:
    std::vector<RunRecord> records_;
    std::string executable_;
    double cycles_per_second_{};
    int num_cpus_{};
};

// Reads the --benchmark_out=... argument so the wrapper can bind the exact
// payload file Google Benchmark writes.
[[nodiscard]] std::string benchmark_out_flag(int argc, char** argv) {
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument.rfind("--benchmark_out=", 0) == 0) {
            return std::string{argument.substr(std::string_view{"--benchmark_out="}.size())};
        }
    }
    return {};
}

[[nodiscard]] std::string custom_flag(int argc, char** argv, std::string_view name,
                                      std::string_view fallback) {
    const std::string prefix = std::string{name} + "=";
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument.rfind(prefix, 0) == 0) {
            return std::string{argument.substr(prefix.size())};
        }
    }
    return std::string{fallback};
}

[[nodiscard]] std::int64_t repetitions_flag(int argc, char** argv) {
    const auto value = custom_flag(argc, argv, "--benchmark_repetitions", "1");
    char* end = nullptr;
    const auto parsed = std::strtoll(value.c_str(), &end, 10);
    if (end == value.c_str() || parsed <= 0) {
        return 1;
    }
    return parsed;
}

} // namespace

int main(int argc, char** argv) {
    // Custom Phase-6 flags must be read before benchmark::Initialize, which
    // consumes and removes the Google Benchmark flags from argv.
    const auto payload_path = benchmark_out_flag(argc, argv);
    const auto wrapper_path = custom_flag(argc, argv, "--m5_wrapper_out", "");
    const auto evidence_class = custom_flag(argc, argv, "--m5_evidence_class", "formal");
    const auto repetitions = repetitions_flag(argc, argv);

    benchmark::Initialize(&argc, argv);
    if (evidence_class != "formal" && evidence_class != "exploratory") {
        std::fprintf(stderr, "invalid --m5_evidence_class value: %s\n", evidence_class.c_str());
        return 2;
    }
    const auto binary_path = bm::current_executable_path();

    CollectingReporter collector;
    benchmark::RunSpecifiedBenchmarks(&collector);

    if (wrapper_path.empty()) {
        std::fprintf(stderr, "--m5_wrapper_out is required for Phase-6 benchmark runs\n");
        return 2;
    }
    if (payload_path.empty()) {
        std::fprintf(stderr, "--benchmark_out is required for Phase-6 benchmark runs\n");
        return 2;
    }

    bm::WrapperInput input;
    input.evidence_class = evidence_class;
    input.binary_path = binary_path;
    input.payload_path = payload_path;
    input.payload_schema = "google_benchmark_json";
    input.timer = "cpu";
    input.primary_denominator = "cpu_time";
    input.warmup_kind = "google_benchmark_internal_warmup_phase";
    input.warmup_count = 1;
    input.repetitions = static_cast<std::uint64_t>(repetitions);
    for (const auto& [name, canonical_text] : bm::registered_workloads()) {
        const auto hash_result = bmd_projection::m5::replay::sha256_hex(canonical_text);
        const auto hash = std::holds_alternative<std::string>(hash_result)
                              ? std::get<std::string>(hash_result)
                              : std::string{};
        input.workloads.push_back(bm::WorkloadRecord{name, canonical_text, hash});
    }
    for (const auto& record : collector.records()) {
        input.measurements.push_back(bm::WrapperMeasurement{
            record.name, static_cast<std::uint64_t>(record.iterations), record.real_time_ns,
            record.cpu_time_ns, "ns", record.items_per_second});
    }
    input.extra_measurement_fields.emplace_back("cpu_cycles_per_second",
                                                std::to_string(collector.cycles_per_second()));
    input.extra_measurement_fields.emplace_back("num_cpus", std::to_string(collector.num_cpus()));

    const auto wrapper_json = bm::build_wrapper_json(input);
    if (wrapper_json.empty()) {
        std::fprintf(stderr, "wrapper generation failed (payload/binary binding failure)\n");
        return 1;
    }
    std::ofstream wrapper_file{wrapper_path, std::ios::binary};
    if (!wrapper_file.is_open()) {
        std::fprintf(stderr, "cannot open wrapper output file: %s\n", wrapper_path.c_str());
        return 1;
    }
    wrapper_file << wrapper_json;
    wrapper_file.close();

    return 0;
}
