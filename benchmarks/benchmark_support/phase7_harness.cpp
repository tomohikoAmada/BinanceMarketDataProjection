#include "phase7_harness.hpp"

#include "canonical_text.hpp"
#include "environment_identity.hpp"
#include "workload_spec.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace bmd_projection::m5::benchmark {
namespace {

[[nodiscard]] std::string flag_value(int argc, char** argv, std::string_view name,
                                     std::string_view fallback) {
    const auto prefix = std::string{name} + "=";
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument.rfind(prefix, 0) == 0) {
            return std::string{argument.substr(prefix.size())};
        }
    }
    return std::string{fallback};
}

[[nodiscard]] std::size_t parse_size(const std::string& text, std::size_t fallback) {
    if (text.empty()) {
        return fallback;
    }
    char* end = nullptr;
    const auto value = std::strtoull(text.c_str(), &end, 10);
    if (end == text.c_str() || value == 0) {
        return fallback;
    }
    return static_cast<std::size_t>(value);
}

} // namespace

Phase7RunOptions parse_phase7_options(int argc, char** argv) {
    Phase7RunOptions options;
    options.output_path = flag_value(argc, argv, "--m5_output", "");
    options.wrapper_path = flag_value(argc, argv, "--m5_wrapper_out", "");
    options.evidence_class = flag_value(argc, argv, "--m5_evidence_class", "exploratory");
    options.repetitions = parse_size(flag_value(argc, argv, "--m5_repetitions", "3"), 3);
    options.filter = flag_value(argc, argv, "--m5_filter", "");
    return options;
}

Phase7NormalizedMetrics
normalized_metrics_of(const m5::allocation::MeasurementResult& result) noexcept {
    return Phase7NormalizedMetrics{result.allocation_count,
                                   result.total_allocated_bytes,
                                   result.deallocation_count,
                                   result.deallocated_bytes,
                                   result.persistent_live_delta,
                                   result.peak_above_entry,
                                   result.transient_excess_over_persistent};
}

Phase7Harness::Phase7Harness(Phase7RunOptions options) : options_{std::move(options)} {}

Phase7CellOutcome Phase7Harness::measure_cell(const std::function<void()>& operation,
                                              const std::function<void()>& destroy_owner) {
    Phase7CellOutcome outcome;
    outcome.determinism_confirmed = true;
    bool first_rep = true;
    for (std::size_t rep = 0; rep < options_.repetitions; ++rep) {
        m5::allocation::MeasurementResult rep_result;
        {
            m5::allocation::MeasurementScope scope;
            operation();
            scope.finish();
            rep_result = scope.result();
        }
        if (rep_result.operation_aborted || rep_result.allocation_failure_observed) {
            outcome.operation_ok = false;
        }
        // Owning-result destruction happens strictly AFTER the bracket: B was
        // taken with the owner alive (OD-M5-P7-005/007).
        if (destroy_owner) {
            destroy_owner();
            const auto d = m5::allocation::live_bytes_snapshot();
            outcome.has_post_destroy = true;
            outcome.post_destroy_live_bytes = d;
            if (rep_result.live_metrics_eligible && d == rep_result.live_bytes_before) {
                outcome.post_destroy_lifecycle_status = "destroyed";
            } else if (rep_result.live_metrics_eligible) {
                outcome.post_destroy_lifecycle_status =
                    "destroyed_mismatch:post_destroy_live_bytes_differ_from_bracket_entry";
            } else {
                outcome.post_destroy_lifecycle_status =
                    "retained:live_metrics_ineligible_for_lifecycle_check";
            }
        }
        if (first_rep) {
            outcome.measurement = rep_result;
            first_rep = false;
        } else if (normalized_metrics_of(rep_result) != normalized_metrics_of(outcome.measurement)) {
            outcome.determinism_confirmed = false;
            std::fprintf(stderr,
                         "determinism mismatch rep %zu: allocs %llu/%llu bytes %llu/%llu "
                         "deallocs %llu/%llu freed %llu/%llu delta %u:%llu/%u:%llu peak_above "
                         "%llu/%llu transient %llu/%llu A %llu/%llu\n",
                         rep,
                         static_cast<unsigned long long>(rep_result.allocation_count),
                         static_cast<unsigned long long>(outcome.measurement.allocation_count),
                         static_cast<unsigned long long>(rep_result.total_allocated_bytes),
                         static_cast<unsigned long long>(
                             outcome.measurement.total_allocated_bytes),
                         static_cast<unsigned long long>(rep_result.deallocation_count),
                         static_cast<unsigned long long>(outcome.measurement.deallocation_count),
                         static_cast<unsigned long long>(rep_result.deallocated_bytes),
                         static_cast<unsigned long long>(outcome.measurement.deallocated_bytes),
                         static_cast<unsigned int>(rep_result.persistent_live_delta.sign),
                         static_cast<unsigned long long>(rep_result.persistent_live_delta.magnitude),
                         static_cast<unsigned int>(outcome.measurement.persistent_live_delta.sign),
                         static_cast<unsigned long long>(
                             outcome.measurement.persistent_live_delta.magnitude),
                         static_cast<unsigned long long>(rep_result.peak_above_entry),
                         static_cast<unsigned long long>(outcome.measurement.peak_above_entry),
                         static_cast<unsigned long long>(
                             rep_result.transient_excess_over_persistent),
                         static_cast<unsigned long long>(
                             outcome.measurement.transient_excess_over_persistent),
                         static_cast<unsigned long long>(rep_result.live_bytes_before),
                         static_cast<unsigned long long>(outcome.measurement.live_bytes_before));
        }
    }
    return outcome;
}

m5::allocation::MeasurementResult Phase7Harness::measure_calibration() {
    m5::allocation::MeasurementResult result;
    {
        m5::allocation::MeasurementScope scope;
        scope.finish();
        result = scope.result();
    }
    return result;
}

const std::pair<std::string, std::string>* Phase7Harness::find_workload(
    std::string_view name) const {
    const auto& workloads = registered_workloads();
    for (const auto& entry : workloads) {
        if (entry.first == name) {
            return &entry;
        }
    }
    return nullptr;
}

bool Phase7Harness::emit() {
    if (records_.empty()) {
        std::fprintf(stderr, "Phase-7 measurement produced no records\n");
        return false;
    }
    const auto payload = build_allocation_payload_json(records_, calibration_records_);
    {
        std::ofstream output{options_.output_path, std::ios::binary};
        if (!output.is_open()) {
            std::fprintf(stderr, "cannot open Phase-7 payload output file: %s\n",
                         options_.output_path.c_str());
            return false;
        }
        output << payload;
        output.close();
    }
    AllocationWrapperInput wrapper_input;
    wrapper_input.evidence_class = options_.evidence_class;
    wrapper_input.binary_path = current_executable_path();
    wrapper_input.payload_path = options_.output_path;
    wrapper_input.repetitions = options_.repetitions;
    for (const auto& [name, canonical_text] : registered_workloads()) {
        const auto hash = bmd_projection::m5::replay::sha256_hex(canonical_text);
        wrapper_input.workloads.push_back(
            WorkloadRecord{name, canonical_text,
                           std::holds_alternative<std::string>(hash)
                               ? std::get<std::string>(hash)
                               : std::string{}});
    }
    const auto wrapper_json = build_allocation_wrapper_json(wrapper_input);
    if (wrapper_json.empty()) {
        std::fprintf(stderr, "Phase-7 allocation wrapper generation failed\n");
        return false;
    }
    std::ofstream wrapper{options_.wrapper_path, std::ios::binary};
    if (!wrapper.is_open()) {
        std::fprintf(stderr, "cannot open Phase-7 wrapper output file: %s\n",
                     options_.wrapper_path.c_str());
        return false;
    }
    wrapper << wrapper_json;
    wrapper.close();
    return true;
}

} // namespace bmd_projection::m5::benchmark
