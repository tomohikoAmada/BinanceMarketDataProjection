#include "workload_spec.hpp"

#include "canonical_text.hpp"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace bmd_projection::m5::benchmark {
namespace {

struct Registry final {
    // Finished specs, in registration order.
    std::vector<std::pair<std::string, std::string>> specs;
    // In-progress builders keyed by benchmark name. Builders are heap-owned so
    // the references returned by register_workload stay stable across further
    // registrations.
    std::vector<std::pair<std::string, std::unique_ptr<WorkloadSpecBuilder>>> builders;
    // Number of builders already folded into specs.
    std::size_t finalized_builders{0};
};

// Nothing reads the registry after main returns, so a normal function-local
// static is safe: static initialization happens on first use (after all
// static registration calls completed) and destruction runs during normal
// shutdown.
Registry& registry() {
    static Registry instance{};
    return instance;
}

[[nodiscard]] std::string canonical_text_of(const WorkloadSpecBuilder& builder) {
    std::string text;
    for (const auto& [key, value] : builder.fields()) {
        text += key;
        text += '=';
        text += value;
        text += '\n';
    }
    return text;
}

} // namespace

WorkloadSpecBuilder::WorkloadSpecBuilder(std::string benchmark_name)
    : benchmark_name_{std::move(benchmark_name)} {}

WorkloadSpecBuilder& WorkloadSpecBuilder::set(std::string_view key, std::string_view value) {
    const auto found = std::find_if(fields_.begin(), fields_.end(),
                                    [key](const auto& entry) { return entry.first == key; });
    if (found != fields_.end()) {
        found->second = std::string{value};
        return *this;
    }
    fields_.emplace_back(std::string{key}, std::string{value});
    std::sort(fields_.begin(), fields_.end(),
              [](const auto& left, const auto& right) { return left.first < right.first; });
    finalized_ = false;
    return *this;
}

WorkloadSpecBuilder& WorkloadSpecBuilder::set(std::string_view key, std::uint64_t value) {
    return set(key, std::to_string(value));
}

void WorkloadSpecBuilder::complete_generated_identity() {
    const auto has_field = [this](std::string_view key) {
        return std::any_of(fields_.begin(), fields_.end(),
                           [key](const auto& field) { return field.first == key; });
    };
    if (!has_field("generator_version")) {
        set("generator_version", "1");
    }
    if (!has_field("seed")) {
        set("seed", "not_applicable");
    }
    if (!has_field("logical_items_per_iteration") && benchmark_name_.starts_with("M1/")) {
        set("logical_items_per_iteration", 16);
    } else if (!has_field("logical_items_per_iteration") &&
               !benchmark_name_.starts_with("CoreNormalizedReplay/") &&
               !benchmark_name_.starts_with("AdapterWireReplay/")) {
        set("logical_items_per_iteration", 1);
    }
}

std::string WorkloadSpecBuilder::canonical_text() const {
    if (canonical_text_.empty() || !finalized_) {
        canonical_text_ = canonical_text_of(*this);
    }
    return canonical_text_;
}

std::string WorkloadSpecBuilder::canonical_sha256() const {
    if (canonical_sha256_.empty() || !finalized_) {
        const auto hash = replay::sha256_hex(canonical_text());
        if (!std::holds_alternative<std::string>(hash)) {
            std::abort();
        }
        canonical_sha256_ = std::get<std::string>(hash);
    }
    return canonical_sha256_;
}

WorkloadSpecBuilder& register_workload(std::string benchmark_name) {
    auto& registry_state = registry();
    auto found = std::find_if(
        registry_state.builders.begin(), registry_state.builders.end(),
        [&benchmark_name](const auto& entry) { return entry.first == benchmark_name; });
    if (found == registry_state.builders.end()) {
        registry_state.builders.emplace_back(benchmark_name,
                                             std::make_unique<WorkloadSpecBuilder>(benchmark_name));
        found = registry_state.builders.end() - 1;
    }
    return *found->second;
}

const std::vector<std::pair<std::string, std::string>>& registered_workloads() {
    auto& registry_state = registry();
    while (registry_state.finalized_builders < registry_state.builders.size()) {
        auto& builder = *registry_state.builders[registry_state.finalized_builders].second;
        builder.complete_generated_identity();
        static_cast<void>(builder.canonical_sha256());
        static_cast<void>(builder.canonical_text());
        registry_state.specs.emplace_back(builder.benchmark_name(), builder.canonical_text());
        ++registry_state.finalized_builders;
    }
    return registry_state.specs;
}

void clear_registered_workloads_for_testing() {
    auto& registry_state = registry();
    registry_state.specs.clear();
    registry_state.builders.clear();
    registry_state.finalized_builders = 0;
}

} // namespace bmd_projection::m5::benchmark
