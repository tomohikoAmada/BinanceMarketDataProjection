#include "wrapper.hpp"

#include "benchmark_build_identity.hpp"
#include "environment_identity.hpp"
#include "phase6_json.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace bmd_projection::m5::benchmark {
namespace {

[[nodiscard]] std::vector<std::string> split_conan_references() {
    std::vector<std::string> references;
    std::istringstream stream{std::string{BMD_P6_CONAN_REFERENCES}};
    std::string reference;
    while (std::getline(stream, reference, ';')) {
        if (!reference.empty()) {
            references.push_back(reference);
        }
    }
    return references;
}

} // namespace

std::string standard_library_name() noexcept {
#if defined(__GLIBCXX__)
    return "libstdc++";
#elif defined(_LIBCPP_VERSION)
    return "libc++";
#else
    return "unavailable";
#endif
}

std::string standard_library_version() noexcept {
#if defined(__GLIBCXX__)
    return std::to_string(__GLIBCXX__);
#elif defined(_LIBCPP_VERSION)
    return std::to_string(_LIBCPP_VERSION);
#else
    return "unavailable";
#endif
}

std::string standard_library_detection_status() noexcept {
#if defined(__GLIBCXX__)
    return "detected_via___GLIBCXX__";
#elif defined(_LIBCPP_VERSION)
    return "detected_via__LIBCPP_VERSION";
#else
    return "preprocessor_undetected";
#endif
}

bool valid_git_sha(std::string_view value) {
    return (value.size() == 40 || value.size() == 64) &&
           std::all_of(value.begin(), value.end(), [](char character) {
               return std::isxdigit(static_cast<unsigned char>(character)) != 0;
           });
}

SourceProvenanceState compute_source_provenance_state() {
    SourceProvenanceState state;
    state.known = std::string_view{BMD_P6_GIT_PROVENANCE_STATUS} == "known" &&
                  valid_git_sha(BMD_P6_GIT_SHA) &&
                  (std::string_view{BMD_P6_GIT_DIRTY_AT_CONFIGURE} == "true" ||
                   std::string_view{BMD_P6_GIT_DIRTY_AT_CONFIGURE} == "false");
    state.dirty = !state.known || std::string_view{BMD_P6_GIT_DIRTY_AT_CONFIGURE} == "true";
    return state;
}

void write_build_identity_section(json::Writer& writer) {
    writer.key("compiler");
    writer.begin_object();
    writer.key("id");
    writer.value(BMD_P6_COMPILER_ID);
    writer.key("version");
    writer.value(BMD_P6_COMPILER_VERSION);
    writer.end_object();
    writer.key("cxx_standard");
    writer.value(BMD_P6_CXX_STANDARD);
    writer.key("build_type");
    writer.value(BMD_P6_BUILD_TYPE);
    writer.key("sanitizer_state");
    writer.value(BMD_P6_SANITIZER_STATE);
    writer.key("lto_state");
    writer.value(BMD_P6_LTO_STATE);
    writer.key("standard_library");
    writer.begin_object();
    writer.key("name");
    writer.value(standard_library_name());
    writer.key("version");
    writer.value(standard_library_version());
    writer.key("detection_status");
    writer.value(standard_library_detection_status());
    writer.end_object();
    writer.key("conan_lock_sha256");
    writer.value(BMD_P6_CONAN_LOCK_SHA256);
    writer.key("conan_references");
    writer.begin_array();
    for (const auto& reference : split_conan_references()) {
        writer.value(reference);
    }
    writer.end_array();
    writer.key("google_benchmark_version");
    writer.value(BMD_P6_GOOGLE_BENCHMARK_VERSION);
}

void write_m4_dependency_section(json::Writer& writer) {
    writer.key("m4_dependency_identity");
    writer.begin_object();
    writer.key("status");
    writer.value(BMD_P6_ADAPTER_ENABLED);
    if (std::string{BMD_P6_ADAPTER_ENABLED} == "ON") {
        writer.key("contracts_source_revision");
        writer.value(BMD_P6_CONTRACTS_SOURCE_REVISION);
        writer.key("contracts_conan_reference");
        writer.value(BMD_P6_CONTRACTS_CONAN_REFERENCE);
        writer.key("contracts_recipe_revision");
        writer.value(BMD_P6_CONTRACTS_RECIPE_REVISION);
        writer.key("contracts_package_id");
        writer.value(BMD_P6_CONTRACTS_PACKAGE_ID);
        writer.key("protobuf_runtime_version");
        writer.value(BMD_P6_PROTOBUF_RUNTIME_VERSION);
        writer.key("protobuf_runtime_rrev");
        writer.value(BMD_P6_PROTOBUF_RUNTIME_RREV);
    } else {
        writer.key("reason");
        writer.value("not_applicable_core_only_payload");
    }
    writer.end_object();
}

void write_environment_section(json::Writer& writer) {
    const auto environment = collect_environment_identity();
    writer.key("environment_identity");
    writer.begin_object();
    writer.key("os_name");
    writer.value(environment.os_name);
    writer.key("os_version");
    writer.value(environment.os_version);
    writer.key("architecture");
    writer.value(environment.architecture);
    writer.key("cpu_model");
    writer.value(environment.cpu_model);
    writer.key("logical_core_count");
    writer.value(environment.logical_core_count);
    writer.end_object();
}

void write_source_provenance_section(json::Writer& writer, const SourceProvenanceState& state) {
    writer.key("source_provenance");
    writer.begin_object();
    writer.key("git_sha");
    writer.value(BMD_P6_GIT_SHA);
    writer.key("status");
    writer.value(state.known ? "known" : "unavailable");
    writer.key("dirty_at_configure");
    writer.value(state.dirty);
    writer.end_object();
}

void write_binary_provenance_section(json::Writer& writer, std::string_view path,
                                     std::string_view sha256) {
    writer.key("binary_provenance");
    writer.begin_object();
    writer.key("path");
    writer.value(path);
    writer.key("sha256");
    writer.value(sha256);
    writer.end_object();
}

void write_workload_identities_section(json::Writer& writer,
                                       const std::vector<WorkloadRecord>& workloads) {
    writer.key("workload_identities");
    writer.begin_array();
    for (const auto& workload : workloads) {
        writer.begin_object();
        writer.key("benchmark_name");
        writer.value(workload.benchmark_name);
        writer.key("workload_spec_schema");
        writer.value("M5_BENCHMARK_WORKLOAD_SPEC_V1");
        writer.key("workload_spec_sha256");
        writer.value(workload.canonical_sha256);
        writer.key("generator_schema");
        writer.value(workload_field(workload, "generator_schema"));
        writer.key("generator_version");
        writer.value(workload_field(workload, "generator_version"));
        writer.key("seed");
        writer.value(workload_field(workload, "seed"));
        writer.key("generated_workload_sha256");
        writer.value(workload_field(workload, "generated_workload_sha256"));
        writer.key("canonical_spec_text");
        writer.value(workload.canonical_text);
        writer.end_object();
    }
    writer.end_array();
}

std::string workload_field(const WorkloadRecord& workload, std::string_view key) {
    const auto prefix = std::string{key} + "=";
    std::istringstream stream{workload.canonical_text};
    std::string line;
    while (std::getline(stream, line)) {
        if (line.starts_with(prefix)) {
            return line.substr(prefix.size());
        }
    }
    return {};
}

std::string build_wrapper_json(const WrapperInput& input) {
    const auto binary_sha = sha256_file_hex(input.binary_path);
    const auto payload_sha = sha256_file_hex(input.payload_path);
    if (binary_sha.empty() || payload_sha.empty()) {
        return {};
    }

    auto effective_class = input.evidence_class;
    std::string downgrade_reason;
    const auto source_state = compute_source_provenance_state();
    if (!source_state.known && input.evidence_class == "formal") {
        return {};
    }
    if (source_state.dirty && effective_class == "formal") {
        effective_class = "exploratory";
        downgrade_reason = "dirty_at_configure";
    }

    json::Writer writer;
    writer.begin_object();
    writer.key("schema");
    writer.value(kWrapperSchema);
    writer.key("measurement_contract_version");
    writer.value(kMeasurementContract);
    writer.key("evidence_class");
    writer.value(effective_class);
    writer.key("requested_evidence_class");
    writer.value(input.evidence_class);
    writer.key("evidence_class_downgrade_reason");
    if (downgrade_reason.empty()) {
        writer.value_null();
    } else {
        writer.value(downgrade_reason);
    }

    write_source_provenance_section(writer, source_state);
    write_binary_provenance_section(writer, input.binary_path, binary_sha);

    writer.key("build_identity");
    writer.begin_object();
    write_build_identity_section(writer);
    writer.end_object();

    write_environment_section(writer);
    write_m4_dependency_section(writer);
    write_workload_identities_section(writer, input.workloads);

    writer.key("measurement_identity");
    writer.begin_object();
    writer.key("timer");
    writer.value(input.timer);
    writer.key("primary_denominator");
    writer.value(input.primary_denominator);
    writer.key("warmup");
    writer.begin_object();
    writer.key("kind");
    writer.value(input.warmup_kind);
    writer.key("count");
    writer.value(input.warmup_count);
    writer.end_object();
    writer.key("repetitions");
    writer.value(input.repetitions);
    if (input.sample_count > 0 || input.unique_event_count > 0 || input.passes > 0) {
        writer.key("sample_count");
        writer.value(input.sample_count);
        writer.key("unique_event_count");
        writer.value(input.unique_event_count);
        writer.key("passes");
        writer.value(input.passes);
    }
    if (!input.quantile_estimator.empty()) {
        writer.key("quantile_estimator");
        writer.value(input.quantile_estimator);
    }
    if (!input.checksum_methodology.empty()) {
        writer.key("checksum_methodology_version");
        writer.value(input.checksum_methodology);
    }
    for (const auto& [key, value] : input.extra_measurement_fields) {
        writer.key(key);
        writer.value(value);
    }
    writer.end_object();

    writer.key("measurements");
    writer.begin_array();
    for (const auto& measurement : input.measurements) {
        writer.begin_object();
        writer.key("name");
        writer.value(measurement.name);
        writer.key("iterations");
        writer.value(measurement.iterations);
        writer.key("real_time_ns");
        writer.value(measurement.real_time_ns);
        writer.key("cpu_time_ns");
        writer.value(measurement.cpu_time_ns);
        writer.key("time_unit");
        writer.value(measurement.time_unit);
        writer.key("items_per_second");
        writer.value(measurement.items_per_second);
        writer.end_object();
    }
    writer.end_array();

    writer.key("result_payload");
    writer.begin_object();
    writer.key("path");
    writer.value(input.payload_path);
    writer.key("sha256");
    writer.value(payload_sha);
    writer.key("schema");
    writer.value(input.payload_schema);
    writer.end_object();

    writer.end_object();
    return writer.str();
}

} // namespace bmd_projection::m5::benchmark
