// M5 Phase-10 recorded-medium production Core replay benchmark.
//
// The workflow performs immutable distribution verification and the existing
// Core/reference differential validation before invoking this executable. The
// executable itself only loads one already-extracted Replay_V1 fixture,
// preflights its production checksum, performs one explicit warmup, and times
// CoreReplayExecutor::run() against a fresh production BookProjection.

#include "benchmark_support/core_replay_executor.hpp"
#include "benchmark_support/environment_identity.hpp"
#include "benchmark_support/replay_checksum.hpp"
#include "benchmark_support/wrapper.hpp"
#include "canonical_text.hpp"
#include "replay_fixture.hpp"

#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>

#include <benchmark/benchmark.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {

namespace core = binance_market_data::projection::v1;
namespace bm = bmd_projection::m5::benchmark;
namespace replay = bmd_projection::m5::replay;

struct FixtureExpectation final {
    const char* fixture_id;
    const char* benchmark_name;
    std::size_t event_count;
    const char* replay_sha256;
    replay::Market market;
    const char* symbol;
    replay::SequencePolicy sequence_policy;
};

constexpr FixtureExpectation kSpotExpectation{
    "M5-REC-SPOT-BTCUSDT-V1",
    "M5RecordedReplay/Spot",
    100'002,
    "9e9831231192938ac1bd21c90b157ec17e8e2d4e8034131eb21ba57c99b2cc9d",
    replay::Market::Spot,
    "BTCUSDT",
    replay::SequencePolicy::Spot};

constexpr FixtureExpectation kUsdmExpectation{
    "M5-REC-USDM-BTCUSDT-V1",
    "M5RecordedReplay/UsdMPerpetual",
    100'002,
    "d28ffe19e134e4d5d1c4d57a60762e8884dee676c858587224aebf8afed29afc",
    replay::Market::UsdMPerpetual,
    "BTCUSDT",
    replay::SequencePolicy::UsdMPerpetual};

[[nodiscard]] const FixtureExpectation* expectation_for(std::string_view fixture_id) noexcept {
    if (fixture_id == kSpotExpectation.fixture_id) {
        return &kSpotExpectation;
    }
    if (fixture_id == kUsdmExpectation.fixture_id) {
        return &kUsdmExpectation;
    }
    return nullptr;
}

[[nodiscard]] std::string custom_flag(int argc, char** argv, std::string_view name) {
    const std::string prefix = std::string{name} + "=";
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument.rfind(prefix, 0) == 0) {
            return std::string{argument.substr(prefix.size())};
        }
    }
    return {};
}

[[nodiscard]] std::string benchmark_output_path(int argc, char** argv) {
    return custom_flag(argc, argv, "--benchmark_out");
}

[[nodiscard]] std::uint64_t repetitions_flag(int argc, char** argv) {
    const auto value = custom_flag(argc, argv, "--benchmark_repetitions");
    if (value.empty()) {
        return 1;
    }
    char* end = nullptr;
    const auto parsed = std::strtoull(value.c_str(), &end, 10);
    if (end == value.c_str() || *end != '\0' || parsed == 0) {
        return 0;
    }
    return parsed;
}

[[nodiscard]] bool matches_expectation(const replay::ReplayFixture& fixture,
                                       const FixtureExpectation& expected) noexcept {
    return fixture.identity.schema_version == replay::kReplaySchemaVersion &&
           fixture.identity.fixture_id == expected.fixture_id &&
           fixture.identity.replay_log_sha256 == expected.replay_sha256 &&
           fixture.identity.market == expected.market &&
           fixture.identity.symbol == expected.symbol &&
           fixture.identity.numeric_spec.price_scale == 8 &&
           fixture.identity.numeric_spec.quantity_scale == 8 &&
           fixture.identity.sequence_policy == expected.sequence_policy &&
           fixture.manifest.event_count == expected.event_count &&
           fixture.replay.operations.size() == expected.event_count &&
           fixture.canonical_log_sha256 == expected.replay_sha256;
}

[[nodiscard]] std::string parse_error_message(const replay::ParseError& error) {
    return error.message.empty() ? "unknown Replay_V1 fixture error" : error.message;
}

[[nodiscard]] std::string medium_workload_spec(const replay::ReplayFixture& fixture,
                                               const FixtureExpectation& expected) {
    std::map<std::string, std::string> fields;
    const auto identity = bm::replay_fixture_identity(fixture);
    std::size_t start = 0;
    while (start < identity.size()) {
        const auto end = identity.find('\n', start);
        const auto line =
            identity.substr(start, end == std::string::npos ? std::string::npos : end - start);
        const auto separator = line.find('=');
        if (separator != std::string::npos) {
            fields[std::string{line.substr(0, separator)}] =
                std::string{line.substr(separator + 1)};
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }

    fields["benchmark_name"] = expected.benchmark_name;
    fields["replay_mode"] = "CoreOnly";
    fields["tier"] = "recorded_medium_v1";
    fields["fixture_id"] = expected.fixture_id;
    fields["event_count"] = std::to_string(expected.event_count);
    fields["market"] = expected.market == replay::Market::Spot ? "Spot" : "UsdMPerpetual";
    fields["symbol"] = expected.symbol;
    fields["price_scale"] = "8";
    fields["quantity_scale"] = "8";
    fields["policy"] =
        expected.sequence_policy == replay::SequencePolicy::Spot ? "Spot" : "UsdMPerpetual";
    fields["canonical_log_sha256"] = expected.replay_sha256;
    fields["distribution_schema"] = "M5_MEDIUM_CORPUS_DISTRIBUTION_V1";
    fields["distribution_package_id"] = "M5-MEDIUM-RECORDED-V1";
    fields["distribution_release_tag"] = "m5-medium-corpus-v1";
    fields["distribution_asset_name"] = "m5-medium-recorded-v1.tar.gz";
    fields["distribution_outer_sha256"] =
        "5143521fe9728a7c2ce03522b78be4ba2fd91388cdabac800f4a87e970e4adfb";
    fields["distribution_manifest_sha256"] =
        "13e4c37119e26f32c60f64f73565363d51ef58f59245f4a6678f4bf016cdba65";
    fields["timed_path"] = "CoreReplayExecutor::run fresh BookProjection production Core replay";
    fields["excluded"] =
        "network_download archive_hashing tar_decompression manifest_parsing fixture_file_io "
        "Replay_V1_text_parsing distribution_validation reference_model differential_validation "
        "diagnostic_rendering wrapper_generation payload_hashing";
    fields["throughput_denominator"] = "wall_time";
    fields["primary_timer"] = "wall";
    fields["checksum_methodology_version"] = std::string{bm::kReplayChecksumMethodology};
    fields["logical_items_per_iteration"] = std::to_string(expected.event_count);

    std::string canonical;
    for (const auto& [key, value] : fields) {
        canonical += key;
        canonical += '=';
        canonical += value;
        canonical += '\n';
    }
    return canonical;
}

struct RunContext final {
    explicit RunContext(std::shared_ptr<const replay::ReplayFixture> fixture_value)
        : fixture{std::move(fixture_value)}, executor{*fixture} {}

    std::shared_ptr<const replay::ReplayFixture> fixture;
    bm::CoreReplayExecutor executor;
};

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
            const auto iterations = report.iterations > 0 ? report.iterations : 1;
            records_.push_back(
                RunRecord{report.run_name.function_name, report.iterations,
                          static_cast<double>(report.real_accumulated_time) * 1e9 /
                              static_cast<double>(iterations),
                          static_cast<double>(report.cpu_accumulated_time) * 1e9 /
                              static_cast<double>(iterations),
                          report.counters.contains("items_per_second")
                              ? static_cast<double>(report.counters.at("items_per_second"))
                              : 0.0});
        }
        for (const auto& report : reports) {
            std::fprintf(stdout, "[phase10] %s %s ns/op\n", report.benchmark_name().c_str(),
                         report.run_name.str().c_str());
        }
    }

    void Finalize() override {}

    [[nodiscard]] const std::vector<RunRecord>& records() const noexcept { return records_; }
    [[nodiscard]] double cycles_per_second() const noexcept { return cycles_per_second_; }
    [[nodiscard]] int num_cpus() const noexcept { return num_cpus_; }

  private:
    std::vector<RunRecord> records_;
    std::string executable_;
    double cycles_per_second_{};
    int num_cpus_{};
};

void run_recorded_replay(benchmark::State& state, const std::shared_ptr<RunContext>& context,
                         std::string_view benchmark_name) {
    {
        core::BookProjection warmup_projection{context->executor.numeric_spec(),
                                               context->executor.policy()};
        if (context->executor.run(warmup_projection) != context->executor.expected_checksum()) {
            state.SkipWithError("explicit warmup checksum mismatch");
            return;
        }
    }

    std::uint64_t accumulator = 0;
    for ([[maybe_unused]] auto _ : state) {
        core::BookProjection projection{context->executor.numeric_spec(),
                                        context->executor.policy()};
        const auto checksum = context->executor.run(projection);
        if (checksum != context->executor.expected_checksum()) {
            state.SkipWithError("replay checksum mismatch");
            break;
        }
        accumulator += checksum;
        benchmark::DoNotOptimize(accumulator);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                            static_cast<std::int64_t>(context->executor.event_count()));
    state.SetLabel(std::string{benchmark_name});
}

} // namespace

int main(int argc, char** argv) {
    const auto fixture_directory = custom_flag(argc, argv, "--m5_fixture_dir");
    const auto payload_path = benchmark_output_path(argc, argv);
    const auto wrapper_path = custom_flag(argc, argv, "--m5_wrapper_out");
    const auto evidence_class = custom_flag(argc, argv, "--m5_evidence_class");
    const auto repetitions = repetitions_flag(argc, argv);
    if (fixture_directory.empty() || payload_path.empty() || wrapper_path.empty() ||
        evidence_class != "exploratory" || repetitions == 0) {
        std::fprintf(stderr, "required flags: --m5_fixture_dir, --benchmark_out, "
                             "--benchmark_repetitions, --m5_wrapper_out, "
                             "--m5_evidence_class=exploratory\n");
        return 2;
    }

    const auto loaded = replay::load_fixture(fixture_directory);
    if (std::holds_alternative<replay::ParseError>(loaded)) {
        std::fprintf(stderr, "Replay_V1 fixture load failed: %s\n",
                     parse_error_message(std::get<replay::ParseError>(loaded)).c_str());
        return 1;
    }
    const auto fixture =
        std::make_shared<replay::ReplayFixture>(std::get<replay::ReplayFixture>(loaded));
    const auto* expected = expectation_for(fixture->identity.fixture_id);
    if (expected == nullptr || !matches_expectation(*fixture, *expected)) {
        std::fprintf(stderr, "unknown or non-authorized recorded medium fixture: %s\n",
                     fixture->identity.fixture_id.c_str());
        return 1;
    }

    auto context = std::make_shared<RunContext>(fixture);
    if (!context->executor.prepared_inputs_valid()) {
        std::fprintf(stderr, "recorded medium fixture contains invalid production inputs\n");
        return 1;
    }
    core::BookProjection preflight_projection{context->executor.numeric_spec(),
                                              context->executor.policy()};
    context->executor.set_expected_checksum(context->executor.run(preflight_projection));

    benchmark::RegisterBenchmark(
        expected->benchmark_name,
        [context, name = std::string{expected->benchmark_name}](benchmark::State& state) {
            run_recorded_replay(state, context, name);
        })
        ->Unit(benchmark::kMicrosecond)
        ->UseRealTime();

    benchmark::Initialize(&argc, argv);
    CollectingReporter collector;
    benchmark::RunSpecifiedBenchmarks(&collector);

    bm::WrapperInput wrapper_input;
    wrapper_input.evidence_class = evidence_class;
    wrapper_input.binary_path = bm::current_executable_path();
    wrapper_input.payload_path = payload_path;
    wrapper_input.payload_schema = "google_benchmark_json";
    wrapper_input.timer = "wall";
    wrapper_input.primary_denominator = "wall_time";
    wrapper_input.warmup_kind = "explicit_workload_pass_v1";
    wrapper_input.warmup_count = 1;
    wrapper_input.repetitions = repetitions;
    wrapper_input.checksum_methodology = std::string{bm::kReplayChecksumMethodology};

    const auto canonical = medium_workload_spec(*fixture, *expected);
    const auto hash = replay::sha256_hex(canonical);
    if (!std::holds_alternative<std::string>(hash)) {
        std::fprintf(stderr, "cannot hash recorded workload identity\n");
        return 1;
    }
    wrapper_input.workloads.push_back(
        bm::WorkloadRecord{expected->benchmark_name, canonical, std::get<std::string>(hash)});
    for (const auto& record : collector.records()) {
        wrapper_input.measurements.push_back(bm::WrapperMeasurement{
            record.name, static_cast<std::uint64_t>(record.iterations), record.real_time_ns,
            record.cpu_time_ns, "ns", record.items_per_second});
    }

    const auto wrapper_json = bm::build_wrapper_json(wrapper_input);
    if (wrapper_json.empty()) {
        std::fprintf(stderr, "recorded benchmark wrapper generation failed\n");
        return 1;
    }
    std::ofstream output{wrapper_path, std::ios::binary};
    if (!output.is_open()) {
        std::fprintf(stderr, "cannot open wrapper output: %s\n", wrapper_path.c_str());
        return 1;
    }
    output << wrapper_json;
    if (!output.good()) {
        std::fprintf(stderr, "cannot write wrapper output: %s\n", wrapper_path.c_str());
        return 1;
    }
    return 0;
}
