// Dedicated production-only event-latency executable (OD-M5-P6-017/018/020/029).
// Event percentiles are never derived from Google
// Benchmark repetitions. The required process:
//
//   1. preload and pre-touch the workload
//   2. build immutable inputs
//   3. run one full untimed warmup pass
//   4. discard warmup production state
//   5. construct fresh production state
//   6. preallocate latency sample storage
//   7. per event: t0 = steady_clock::now(); execute exactly one production
//      event; t1 = steady_clock::now(); store t1-t0
//   8. no oracle/logging/formatting/vector growth/parsing/hashing inside the
//      bracket (typed evidence capture only; folding happens outside)
//   9. post-run final-state/checksum validation
//  10. statistics computed after measurement
//
// Calibration samples use the same storage pattern in a separate empty
// bracket distribution and are reported separately; they are NEVER subtracted
// from event latency samples.

#include "benchmark_support/core_replay_executor.hpp"
#include "benchmark_support/environment_identity.hpp"
#include "benchmark_support/latency_stats.hpp"
#include "benchmark_support/phase6_json.hpp"
#include "benchmark_support/workload_spec.hpp"
#include "benchmark_support/wrapper.hpp"
#include "canonical_text.hpp"
#include "small_workload.hpp"

#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

namespace core = binance_market_data::projection::v1;
namespace bm = bmd_projection::m5::benchmark;
namespace phase3 = bmd_projection::m5::phase3;
namespace replay = bmd_projection::m5::replay;

using Clock = std::chrono::steady_clock;

[[nodiscard]] std::string checksum_hex(std::uint64_t value) {
    char buffer[17]{};
    std::snprintf(buffer, sizeof(buffer), "%016llx", static_cast<unsigned long long>(value));
    return std::string{buffer};
}

struct LatencyOptions final {
    std::string workload{"spot"};
    std::size_t passes{5};
    std::size_t calibration_samples{100'000};
    std::string output_path;
    std::string wrapper_path;
    std::string evidence_class{"formal"};
};

[[nodiscard]] LatencyOptions parse_options(int argc, char** argv) {
    LatencyOptions options;
    const auto flag_value = [&](std::string_view name, std::string_view fallback) {
        const std::string prefix = std::string{name} + "=";
        for (int index = 1; index < argc; ++index) {
            const std::string_view argument{argv[index]};
            if (argument.rfind(prefix, 0) == 0) {
                return std::string{argument.substr(prefix.size())};
            }
        }
        return std::string{fallback};
    };
    const auto parse_size = [](const std::string& text, std::size_t fallback) {
        if (text.empty()) {
            return fallback;
        }
        char* end = nullptr;
        const auto value = std::strtoull(text.c_str(), &end, 10);
        if (end == text.c_str() || value == 0) {
            return fallback;
        }
        return static_cast<std::size_t>(value);
    };
    options.workload = flag_value("--m5_workload", "spot");
    options.passes = parse_size(flag_value("--m5_passes", "5"), 5);
    options.calibration_samples =
        parse_size(flag_value("--m5_calibration_samples", "100000"), 100'000);
    options.output_path = flag_value("--m5_output", "");
    options.wrapper_path = flag_value("--m5_wrapper_out", "");
    options.evidence_class = flag_value("--m5_evidence_class", "formal");
    return options;
}

[[nodiscard]] replay::ReplayFixture make_fixture(std::string_view workload) {
    if (workload == "spot") {
        return phase3::make_spot_small_workload();
    }
    if (workload == "usdm") {
        return phase3::make_usdm_small_workload();
    }
    std::fprintf(stderr, "unknown --m5_workload value: %s (expected spot|usdm)\n",
                 std::string{workload}.c_str());
    std::exit(2);
}

void write_quantiles(bm::json::Writer& writer, const bm::LatencyReport& report) {
    const auto write_gated = [&writer, &report](double probability, std::string_view key,
                                                bool eligible) {
        writer.key(key);
        const auto value = report.quantile(probability);
        if (eligible && value.has_value()) {
            writer.value(*value);
        } else {
            writer.value_null();
        }
    };
    write_gated(0.5, "p50", report.p50_eligible());
    write_gated(0.9, "p90", report.p90_eligible());
    write_gated(0.99, "p99", report.p99_eligible());
    write_gated(0.999, "p99_9", report.p999_eligible());
}

void write_raw_samples(bm::json::Writer& writer, const std::vector<std::uint64_t>& samples) {
    writer.begin_array();
    for (const auto sample : samples) {
        writer.value(sample);
    }
    writer.end_array();
}

} // namespace

int main(int argc, char** argv) {
    const auto options = parse_options(argc, argv);
    if (options.output_path.empty() || options.wrapper_path.empty()) {
        std::fprintf(stderr, "--m5_output and --m5_wrapper_out are required\n");
        return 2;
    }
    if (options.evidence_class != "formal" && options.evidence_class != "exploratory") {
        std::fprintf(stderr, "invalid --m5_evidence_class value: %s\n",
                     options.evidence_class.c_str());
        return 2;
    }

    // 1. Preload and pre-touch the workload; 2. immutable inputs.
    const auto fixture = make_fixture(options.workload);
    bm::CoreReplayExecutor executor{fixture};
    const auto event_count = executor.event_count();
    const auto expected_pass_checksum = [&] {
        core::BookProjection projection{executor.numeric_spec(), executor.policy()};
        return executor.run(projection);
    }();

    // 3. One full untimed warmup pass; 4. discard its state.
    {
        core::BookProjection projection{executor.numeric_spec(), executor.policy()};
        const auto warmup_checksum = executor.run(projection);
        if (warmup_checksum != expected_pass_checksum) {
            std::fprintf(stderr, "latency warmup checksum mismatch\n");
            return 1;
        }
    }

    // 5./6. Fresh production state per pass; preallocated sample storage.
    const auto total_samples = options.passes * event_count;
    std::vector<std::uint64_t> samples;
    samples.reserve(total_samples);
    std::vector<std::uint64_t> pass_checksums;
    pass_checksums.reserve(options.passes);
    std::vector<bm::EventEvidence> pass_evidence;
    pass_evidence.reserve(event_count);

    // 7./8. Per-event steady_clock brackets.
    for (std::size_t pass = 0; pass < options.passes; ++pass) {
        core::BookProjection projection{executor.numeric_spec(), executor.policy()};
        pass_evidence.clear();
        for (std::size_t index = 0; index < event_count; ++index) {
            const auto t0 = Clock::now();
            const auto evidence = executor.execute_event(projection, index);
            const auto t1 = Clock::now();
            samples.push_back(static_cast<std::uint64_t>((t1 - t0).count()));
            pass_evidence.push_back(evidence);
        }
        // Checksum folding happens outside the per-event brackets.
        std::uint64_t checksum = bm::kReplayChecksumSeed;
        for (const auto& evidence : pass_evidence) {
            checksum = bm::fold_evidence(checksum, evidence);
        }
        checksum = executor.finalize_checksum(projection, checksum);
        pass_checksums.push_back(checksum);
    }

    // 9./10. Post-run validation and statistics.
    for (const auto checksum : pass_checksums) {
        if (checksum != expected_pass_checksum) {
            std::fprintf(stderr, "latency pass checksum mismatch: got %llu expected %llu\n",
                         static_cast<unsigned long long>(checksum),
                         static_cast<unsigned long long>(expected_pass_checksum));
            return 1;
        }
    }

    // Calibration: separate empty-bracket distribution, same storage pattern.
    std::vector<std::uint64_t> calibration_samples;
    calibration_samples.reserve(options.calibration_samples);
    for (std::size_t index = 0; index < options.calibration_samples; ++index) {
        const auto t0 = Clock::now();
        const auto t1 = Clock::now();
        calibration_samples.push_back(static_cast<std::uint64_t>((t1 - t0).count()));
    }

    const auto report = bm::make_latency_report(
        std::move(samples), bm::LatencyBookkeeping{event_count, options.passes});
    const auto calibration =
        bm::make_latency_report(std::move(calibration_samples), bm::LatencyBookkeeping{0, 1});

    // Workload identity for the payload and wrapper.
    const auto workload_spec_text = bm::replay_fixture_identity(fixture);
    const auto workload_spec_sha = [&] {
        const auto hash = replay::sha256_hex(workload_spec_text);
        return std::holds_alternative<std::string>(hash) ? std::get<std::string>(hash)
                                                         : std::string{};
    }();

    // M5_REPLAY_LATENCY_V1 payload.
    {
        bm::json::Writer writer;
        writer.begin_object();
        writer.key("schema");
        writer.value(bm::kLatencySchema);
        writer.key("measurement_contract_version");
        writer.value(bm::kMeasurementContract);
        writer.key("workload");
        writer.begin_object();
        writer.key("workload_id");
        writer.value(fixture.identity.fixture_id);
        writer.key("market");
        writer.value(fixture.identity.market == replay::Market::Spot ? "Spot" : "UsdMPerpetual");
        writer.key("symbol");
        writer.value(fixture.identity.symbol);
        writer.key("price_scale");
        writer.value(static_cast<std::uint64_t>(fixture.identity.numeric_spec.price_scale));
        writer.key("quantity_scale");
        writer.value(static_cast<std::uint64_t>(fixture.identity.numeric_spec.quantity_scale));
        writer.key("sequence_policy");
        writer.value(fixture.identity.sequence_policy == replay::SequencePolicy::Spot
                         ? "Spot"
                         : "UsdMPerpetual");
        writer.key("event_count");
        writer.value(static_cast<std::uint64_t>(event_count));
        writer.key("canonical_log_sha256");
        writer.value(fixture.canonical_log_sha256);
        writer.key("workload_spec_schema");
        writer.value("M5_BENCHMARK_WORKLOAD_SPEC_V1");
        writer.key("workload_spec_sha256");
        writer.value(workload_spec_sha);
        writer.key("workload_spec_text");
        writer.value(workload_spec_text);
        writer.end_object();
        writer.key("timer");
        writer.begin_object();
        writer.key("type");
        writer.value("steady_clock");
        writer.key("primary_denominator");
        writer.value("wall_time");
        writer.end_object();
        writer.key("warmup");
        writer.begin_object();
        writer.key("kind");
        writer.value("full_workload_pass");
        writer.key("count");
        writer.value(static_cast<std::uint64_t>(1));
        writer.key("state_isolation");
        writer.value("fresh_production_state_after_warmup");
        writer.end_object();
        writer.key("passes");
        writer.value(static_cast<std::uint64_t>(report.passes));
        writer.key("sample_count");
        writer.value(static_cast<std::uint64_t>(report.sample_count));
        writer.key("unique_event_count");
        writer.value(static_cast<std::uint64_t>(report.unique_event_count));
        writer.key("quantile_estimator");
        writer.value(bm::kQuantileEstimator);
        writer.key("checksum");
        writer.begin_object();
        writer.key("methodology_version");
        writer.value(bm::kReplayChecksumMethodology);
        writer.key("expected");
        writer.value(checksum_hex(expected_pass_checksum));
        writer.key("per_pass");
        writer.begin_array();
        for (const auto checksum : pass_checksums) {
            writer.value(checksum_hex(checksum));
        }
        writer.end_array();
        writer.key("validated");
        writer.value(true);
        writer.end_object();
        writer.key("eligibility");
        writer.begin_object();
        writer.key("p50");
        writer.value(report.p50_eligible());
        writer.key("p90");
        writer.value(report.p90_eligible());
        writer.key("p99");
        writer.value(report.p99_eligible());
        writer.key("p99_9");
        writer.value(report.p999_eligible());
        if (!report.p999_eligible()) {
            writer.key("p99_9_reason");
            writer.value(report.p999_omission_reason());
        }
        writer.end_object();
        writer.key("quantiles_ns");
        writer.begin_object();
        write_quantiles(writer, report);
        writer.end_object();
        writer.key("calibration");
        writer.begin_object();
        writer.key("sample_count");
        writer.value(static_cast<std::uint64_t>(calibration.sample_count));
        writer.key("quantiles_ns");
        writer.begin_object();
        write_quantiles(writer, calibration);
        writer.end_object();
        writer.key("subtracted_from_event_samples");
        writer.value(false);
        writer.key("calibration_samples_ns");
        write_raw_samples(writer, calibration.sorted_samples_ns);
        writer.end_object();
        writer.key("raw_samples_ns");
        write_raw_samples(writer, report.sorted_samples_ns);
        writer.end_object();

        std::ofstream output{options.output_path, std::ios::binary};
        if (!output.is_open()) {
            std::fprintf(stderr, "cannot open latency output file: %s\n",
                         options.output_path.c_str());
            return 1;
        }
        output << writer.str();
        output.close();
    }

    // M5_BENCHMARK_WRAPPER_V1 binding for the latency payload.
    {
        bm::WrapperInput input;
        input.evidence_class = options.evidence_class;
        input.binary_path = bm::current_executable_path();
        input.payload_path = options.output_path;
        input.payload_schema = bm::kLatencySchema;
        input.timer = "steady_clock";
        input.primary_denominator = "wall_time";
        input.warmup_kind = "full_workload_pass";
        input.warmup_count = 1;
        input.repetitions = 1;
        input.sample_count = report.sample_count;
        input.unique_event_count = report.unique_event_count;
        input.passes = report.passes;
        input.quantile_estimator = std::string{bm::kQuantileEstimator};
        input.checksum_methodology = std::string{bm::kReplayChecksumMethodology};
        input.workloads.push_back(
            bm::WorkloadRecord{"M5/CoreEventLatency/" + fixture.identity.fixture_id,
                               workload_spec_text, workload_spec_sha});
        input.extra_measurement_fields.emplace_back("latency_schema",
                                                    std::string{bm::kLatencySchema});
        const auto wrapper_json = bm::build_wrapper_json(input);
        if (wrapper_json.empty()) {
            std::fprintf(stderr, "latency wrapper generation failed\n");
            return 1;
        }
        std::ofstream wrapper{options.wrapper_path, std::ios::binary};
        if (!wrapper.is_open()) {
            std::fprintf(stderr, "cannot open wrapper output file: %s\n",
                         options.wrapper_path.c_str());
            return 1;
        }
        wrapper << wrapper_json;
        wrapper.close();
    }

    return 0;
}
