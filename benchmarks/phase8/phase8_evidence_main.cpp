// M5 Phase-8 PR-B evidence producer.
//
// This executable is deliberately separate from Google Benchmark. It emits
// raw per-repetition wall-clock samples plus the Phase-7 global-new
// allocation/live-storage measurements. No oracle, JSON, or logging work is
// performed inside a measured candidate operation.

#include "phase8_absl_btree_map.hpp"
#include "phase8_sorted_vector_batch_lww.hpp"
#include "phase8_sorted_vector_naive.hpp"
#include "phase8_std_map_control.hpp"
#include "phase8_workload.hpp"

#include "../benchmark_support/allocation_instrumentation.hpp"
#include "../benchmark_support/book_state.hpp"
#include "../benchmark_support/environment_identity.hpp"
#include "../benchmark_support/phase6_json.hpp"
#include "../benchmark_support/replay_checksum.hpp"
#include "../benchmark_support/workload_spec.hpp"
#include "../benchmark_support/wrapper.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <numeric>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bmd_projection::m5::phase8 {
namespace {

namespace bm = bmd_projection::m5::benchmark;
namespace alloc = bmd_projection::m5::allocation;
using Clock = std::chrono::steady_clock;

inline constexpr std::string_view kEvidenceSchema = "M5_PHASE8_EVIDENCE_PAYLOAD_V1";
inline constexpr std::string_view kRecordSchema = "M5_PHASE8_MEASUREMENT_RECORD_V1";
inline constexpr std::string_view kContract = "M5_PHASE8_MEASUREMENT_CONTRACT_V1";
inline constexpr std::size_t kDefaultRepetitions = 5;
inline constexpr std::size_t kStandardUpdatePasses = 16;

volatile std::uint64_t g_sink = 0;

struct Options final {
    std::string output;
    std::string wrapper;
    std::string filter;
    std::string evidence_class{"exploratory"};
    std::size_t repetitions{kDefaultRepetitions};
};

struct Stats final {
    double mean{};
    double median{};
    double minimum{};
    double maximum{};
    double standard_deviation{};
    double coefficient_of_variation{};
};

struct Record final {
    std::string candidate;
    const Phase8Workload* workload{};
    std::string metric;
    std::string unit;
    std::vector<double> raw_values;
    std::vector<std::uint64_t> allocation_counts;
    std::vector<std::uint64_t> allocated_bytes;
    std::vector<std::uint64_t> persistent_live_bytes;
    std::string final_digest;
    bool post_destroy_consistent{true};
};

[[nodiscard]] std::string argument_value(int argc, char** argv, std::string_view name) {
    const std::string prefix = std::string{name} + '=';
    for (int index = 1; index < argc; ++index) {
        const std::string_view arg{argv[index]};
        if (arg.starts_with(prefix)) {
            return std::string{arg.substr(prefix.size())};
        }
    }
    return {};
}

[[nodiscard]] std::size_t parse_count(std::string_view value, std::size_t fallback) {
    if (value.empty()) {
        return fallback;
    }
    char* end = nullptr;
    const auto parsed = std::strtoull(std::string{value}.c_str(), &end, 10);
    if (end == nullptr || *end != '\0' || parsed == 0) {
        return 0;
    }
    return static_cast<std::size_t>(parsed);
}

[[nodiscard]] Options parse_options(int argc, char** argv) {
    Options options;
    options.output = argument_value(argc, argv, "--m5_output");
    options.wrapper = argument_value(argc, argv, "--m5_wrapper_out");
    options.filter = argument_value(argc, argv, "--m5_filter");
    const auto requested_class = argument_value(argc, argv, "--m5_evidence_class");
    if (!requested_class.empty()) {
        options.evidence_class = requested_class;
    }
    options.repetitions =
        parse_count(argument_value(argc, argv, "--m5_repetitions"), kDefaultRepetitions);
    return options;
}

template <typename Model> void populate(Model& model, const Phase8Workload& workload) {
    model.replace_all(std::span{workload.bids}, std::span{workload.asks});
}

template <typename Model> void consume_operation(Model& model, const Phase8Workload& workload) {
    switch (workload.operation) {
    case Phase8Operation::apply_level: {
        const auto& update = workload.updates.front();
        g_sink ^= static_cast<std::uint64_t>(
            model.apply_level(update.side, update.price, update.quantity));
        break;
    }
    case Phase8Operation::apply_updates: {
        const auto passes = workload.id.starts_with("M5_PHASE8/") ? 1U : kStandardUpdatePasses;
        for (std::size_t pass = 0; pass < passes; ++pass) {
            for (const auto& updates : workload.update_batches) {
                model.apply_updates(std::span{updates});
                g_sink ^= model.level_count(core::BookSide::Bid);
                g_sink ^= model.level_count(core::BookSide::Ask);
            }
        }
        break;
    }
    case Phase8Operation::replace_all:
        model.replace_all(std::span{workload.bids}, std::span{workload.asks});
        g_sink ^= model.level_count(core::BookSide::Bid);
        break;
    case Phase8Operation::top_levels: {
        const auto levels = model.top_levels(core::BookSide::Bid, workload.query_limit);
        g_sink ^= levels.size();
        if (!levels.empty()) {
            g_sink ^= static_cast<std::uint64_t>(levels.front().price.value());
        }
        break;
    }
    }
}

template <typename Model>
[[nodiscard]] std::string digest_after_operation(const Phase8Workload& workload) {
    Model model{bm::benchmark_numeric_spec()};
    populate(model, workload);
    consume_operation(model, workload);
    return phase8_digest(model);
}

template <typename Model> [[nodiscard]] double timed_operation(const Phase8Workload& workload) {
    Model model{bm::benchmark_numeric_spec()};
    populate(model, workload);
    const auto start = Clock::now();
    consume_operation(model, workload);
    const auto stop = Clock::now();
    const auto nanos = std::chrono::duration<double, std::nano>{stop - start}.count();
    return std::max(1.0, nanos);
}

template <typename Model>
[[nodiscard]] alloc::MeasurementResult measured_allocation(const Phase8Workload& workload) {
    Model model{bm::benchmark_numeric_spec()};
    populate(model, workload);
    alloc::MeasurementResult result;
    {
        alloc::MeasurementScope scope;
        consume_operation(model, workload);
        scope.finish();
        result = scope.result();
    }
    return result;
}

template <typename Model>
[[nodiscard]] std::pair<std::uint64_t, bool> persistent_footprint(const Phase8Workload& workload) {
    const auto before = alloc::live_bytes_snapshot();
    std::uint64_t after{};
    {
        Model model{bm::benchmark_numeric_spec()};
        populate(model, workload);
        after = alloc::live_bytes_snapshot();
    }
    const auto post = alloc::live_bytes_snapshot();
    return {after >= before ? after - before : 0, post == before};
}

template <typename Model> void warmup(const std::vector<Phase8Workload>& workloads) {
    for (const auto& workload : workloads) {
        Model model{bm::benchmark_numeric_spec()};
        populate(model, workload);
        consume_operation(model, workload);
    }
}

[[nodiscard]] Stats summarize(std::vector<double> values) {
    Stats result;
    if (values.empty()) {
        return result;
    }
    std::sort(values.begin(), values.end());
    result.minimum = values.front();
    result.maximum = values.back();
    result.median = values[values.size() / 2U];
    result.mean =
        std::accumulate(values.begin(), values.end(), 0.0) / static_cast<double>(values.size());
    double squared = 0.0;
    for (const auto value : values) {
        const auto difference = value - result.mean;
        squared += difference * difference;
    }
    result.standard_deviation = std::sqrt(squared / static_cast<double>(values.size()));
    result.coefficient_of_variation =
        result.mean == 0.0 ? 0.0 : result.standard_deviation / result.mean;
    return result;
}

void write_stats(bm::json::Writer& writer, const std::vector<double>& raw) {
    const auto stats = summarize(raw);
    writer.key("raw");
    writer.begin_array();
    for (const auto value : raw) {
        writer.value(value);
    }
    writer.end_array();
    writer.key("summary");
    writer.begin_object();
    writer.key("mean");
    writer.value(stats.mean);
    writer.key("median");
    writer.value(stats.median);
    writer.key("minimum");
    writer.value(stats.minimum);
    writer.key("maximum");
    writer.value(stats.maximum);
    writer.key("standard_deviation");
    writer.value(stats.standard_deviation);
    writer.key("coefficient_of_variation");
    writer.value(stats.coefficient_of_variation);
    writer.end_object();
}

void write_uint64_array(bm::json::Writer& writer, const std::vector<std::uint64_t>& values) {
    writer.begin_array();
    for (const auto value : values) {
        writer.value(value);
    }
    writer.end_array();
}

template <typename Model>
void collect_model_records(const std::vector<Phase8Workload>& workloads, std::size_t repetitions,
                           std::vector<Record>& records,
                           std::vector<std::string>& reference_digests, bool set_reference,
                           bool& valid) {
    for (std::size_t workload_index = 0; workload_index < workloads.size(); ++workload_index) {
        const auto& workload = workloads[workload_index];
        Record record;
        record.candidate = std::string{Model::model_id()};
        record.workload = &workload;
        record.metric =
            workload.operation == Phase8Operation::apply_updates ? "replay_update_throughput"
            : workload.operation == Phase8Operation::top_levels  ? "top_n_read_latency"
            : workload.operation == Phase8Operation::replace_all ? "full_replacement_latency"
                                                                 : "update_latency";
        record.unit = record.metric == "replay_update_throughput" ? "updates_per_second" : "ns";
        const auto baseline_digest = digest_after_operation<Model>(workload);
        if (set_reference) {
            reference_digests[workload_index] = baseline_digest;
        } else if (baseline_digest != reference_digests[workload_index]) {
            valid = false;
        }
        for (std::size_t repetition = 0; repetition < repetitions; ++repetition) {
            const auto elapsed = timed_operation<Model>(workload);
            const auto allocation = measured_allocation<Model>(workload);
            const auto [persistent, post_destroy_ok] = persistent_footprint<Model>(workload);
            record.raw_values.push_back(
                record.metric == "replay_update_throughput"
                    ? static_cast<double>(
                          (workload.id.starts_with("M5_PHASE8/") ? 1U : kStandardUpdatePasses) *
                          workload.update_batches.size() * workload.batch) /
                          (elapsed / 1'000'000'000.0)
                    : elapsed);
            record.allocation_counts.push_back(allocation.allocation_count);
            record.allocated_bytes.push_back(allocation.total_allocated_bytes);
            record.persistent_live_bytes.push_back(persistent);
            record.post_destroy_consistent = record.post_destroy_consistent && post_destroy_ok &&
                                             allocation.live_metrics_eligible &&
                                             allocation.allocation_count_valid &&
                                             allocation.total_allocated_bytes_valid;
            if (repetition == 0) {
                record.final_digest = baseline_digest;
            } else if (record.final_digest != digest_after_operation<Model>(workload)) {
                valid = false;
            }
        }
        records.push_back(std::move(record));
    }
}

void write_record(bm::json::Writer& writer, const Record& record, std::size_t repetitions) {
    const auto& workload = *record.workload;
    writer.begin_object();
    writer.key("schema");
    writer.value(kRecordSchema);
    writer.key("candidate_model_id");
    writer.value(record.candidate);
    writer.key("workload_id");
    writer.value(workload.id);
    writer.key("workload_spec_schema");
    writer.value(bm::kWorkloadSpecSchema);
    writer.key("workload_spec_sha256");
    writer.value(workload.workload_spec_sha256);
    writer.key("generated_workload_sha256");
    writer.value(workload.generated_workload_sha256);
    writer.key("operation");
    writer.value(phase8_operation_name(workload.operation));
    writer.key("depth_per_side");
    writer.value(static_cast<std::uint64_t>(workload.depth));
    writer.key("batch");
    writer.value(static_cast<std::uint64_t>(workload.batch));
    writer.key("query_limit");
    writer.value(static_cast<std::uint64_t>(workload.query_limit));
    writer.key("metric");
    writer.value(record.metric);
    writer.key("unit");
    writer.value(record.unit);
    writer.key("repetitions");
    writer.value(static_cast<std::uint64_t>(repetitions));
    writer.key("measurement");
    writer.begin_object();
    write_stats(writer, record.raw_values);
    writer.end_object();
    writer.key("allocation_supporting_evidence");
    writer.begin_object();
    writer.key("boundary");
    writer.value(alloc::kAllocationBoundary);
    writer.key("allocation_count");
    write_uint64_array(writer, record.allocation_counts);
    writer.key("allocated_bytes");
    write_uint64_array(writer, record.allocated_bytes);
    writer.end_object();
    writer.key("persistent_live_storage");
    writer.begin_object();
    writer.key("measured_requested_bytes");
    write_uint64_array(writer, record.persistent_live_bytes);
    writer.key("post_destroy_consistent");
    writer.value(record.post_destroy_consistent);
    writer.key("rss");
    writer.value("not_measured");
    writer.end_object();
    writer.key("final_state_digest");
    writer.value(record.final_digest);
    writer.end_object();
}

[[nodiscard]] std::string build_payload(const std::vector<Record>& records, std::size_t repetitions,
                                        const std::vector<double>& noise_floor) {
    bm::json::Writer writer;
    writer.begin_object();
    writer.key("schema");
    writer.value(kEvidenceSchema);
    writer.key("measurement_contract_version");
    writer.value(kContract);
    writer.key("candidate_models");
    writer.begin_array();
    for (const auto id : {kPhase8StdMapControlId, kPhase8SortedVectorNaiveId, kPhase8AbslBtreeMapId,
                          kPhase8SortedVectorBatchLwwId}) {
        writer.value(id);
    }
    writer.end_array();
    writer.key("repetitions");
    writer.value(static_cast<std::uint64_t>(repetitions));
    writer.key("noise_floor");
    writer.begin_object();
    writer.key("timer");
    writer.value("steady_clock");
    writer.key("unit");
    writer.value("ns");
    write_stats(writer, noise_floor);
    writer.end_object();
    writer.key("records");
    writer.begin_array();
    for (const auto& record : records) {
        write_record(writer, record, repetitions);
    }
    writer.end_array();
    writer.end_object();
    return writer.str();
}

[[nodiscard]] std::vector<bm::WorkloadRecord>
workload_records(const std::vector<Phase8Workload>& workloads) {
    std::vector<bm::WorkloadRecord> result;
    result.reserve(workloads.size());
    for (const auto& workload : workloads) {
        result.push_back({workload.id, workload.workload_spec_text, workload.workload_spec_sha256});
    }
    return result;
}

int run(int argc, char** argv) {
    const auto options = parse_options(argc, argv);
    if (options.output.empty() || options.wrapper.empty() || options.repetitions < 5 ||
        (options.evidence_class != "formal" && options.evidence_class != "exploratory")) {
        std::fprintf(stderr, "usage: --m5_output=PATH --m5_wrapper_out=PATH "
                             "[--m5_repetitions=N>=5] [--m5_filter=SUBSTRING] "
                             "[--m5_evidence_class=formal|exploratory]\n");
        return 2;
    }
    const auto& all_workloads = phase8_workloads();
    std::vector<Phase8Workload> workloads;
    for (const auto& workload : all_workloads) {
        if (options.filter.empty() || workload.id.find(options.filter) != std::string::npos) {
            workloads.push_back(workload);
        }
    }
    if (workloads.empty()) {
        std::fprintf(stderr, "Phase-8 workload filter matched no workload\n");
        return 2;
    }

    warmup<Phase8StdMapControl<>>(workloads);
    warmup<Phase8SortedVectorNaive<>>(workloads);
    warmup<Phase8AbslBtreeMap<>>(workloads);
    warmup<Phase8SortedVectorBatchLww<>>(workloads);

    std::vector<double> noise_floor;
    noise_floor.reserve(options.repetitions);
    for (std::size_t repetition = 0; repetition < options.repetitions; ++repetition) {
        const auto start = Clock::now();
        const auto stop = Clock::now();
        noise_floor.push_back(
            std::max(1.0, std::chrono::duration<double, std::nano>{stop - start}.count()));
    }

    std::vector<Record> records;
    records.reserve(workloads.size() * 4U);
    bool valid = true;
    std::vector<std::string> reference_digests(workloads.size());
    collect_model_records<Phase8StdMapControl<>>(workloads, options.repetitions, records,
                                                 reference_digests, true, valid);
    collect_model_records<Phase8SortedVectorNaive<>>(workloads, options.repetitions, records,
                                                     reference_digests, false, valid);
    collect_model_records<Phase8AbslBtreeMap<>>(workloads, options.repetitions, records,
                                                reference_digests, false, valid);
    collect_model_records<Phase8SortedVectorBatchLww<>>(workloads, options.repetitions, records,
                                                        reference_digests, false, valid);
    for (const auto& record : records) {
        valid = valid && record.post_destroy_consistent;
    }
    if (!valid) {
        std::fprintf(stderr, "Phase-8 candidate evidence preflight failed\n");
        return 1;
    }

    const auto payload = build_payload(records, options.repetitions, noise_floor);
    std::ofstream payload_stream(options.output, std::ios::binary | std::ios::trunc);
    if (!payload_stream.is_open()) {
        return 1;
    }
    payload_stream << payload;
    payload_stream.close();

    bm::WrapperInput wrapper_input;
    wrapper_input.evidence_class = options.evidence_class;
    wrapper_input.binary_path = bm::current_executable_path();
    wrapper_input.payload_path = options.output;
    wrapper_input.payload_schema = std::string{kEvidenceSchema};
    wrapper_input.workloads = workload_records(workloads);
    wrapper_input.timer = "steady_clock";
    wrapper_input.primary_denominator = "wall_time";
    wrapper_input.warmup_kind = "one_complete_candidate_workload_pass_v1";
    wrapper_input.warmup_count = 1;
    wrapper_input.repetitions = options.repetitions;
    wrapper_input.extra_measurement_fields = {
        {"candidate_set", "phase8-four-approved-models"},
        {"memory_method", "persistent_live_requested_bytes"},
        {"allocation_method", std::string{alloc::kAllocationBoundary}},
        {"rss_method", "not_measured"},
    };
    const auto wrapper = bm::build_wrapper_json(wrapper_input);
    if (wrapper.empty()) {
        return 1;
    }
    std::ofstream wrapper_stream(options.wrapper, std::ios::binary | std::ios::trunc);
    if (!wrapper_stream.is_open()) {
        return 1;
    }
    wrapper_stream << wrapper;
    std::printf("Phase-8 evidence emitted: %s\n", options.output.c_str());
    return 0;
}

} // namespace
} // namespace bmd_projection::m5::phase8

int main(int argc, char** argv) { return bmd_projection::m5::phase8::run(argc, argv); }
