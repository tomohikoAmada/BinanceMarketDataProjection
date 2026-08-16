#include "m4_workload_specs.hpp"

#include "adapter_wire_support.hpp"
#include "canonical_text.hpp"
#include "workload_spec.hpp"

#include <cstddef>
#include <cstdlib>
#include <string>
#include <string_view>
#include <variant>

namespace bmd_projection::m5::benchmark {
namespace {

namespace replay = bmd_projection::m5::replay;
namespace wire_support = bmd_projection::m5::benchmark::adapter_support;

constexpr std::size_t kM4DepthSet[] = {8, 100, 1'000};

[[nodiscard]] std::string m4_generated_sha256(std::string_view family, std::size_t depth) {
    const auto hash = replay::sha256_hex(
        wire_support::m4_generated_workload_description(family, depth));
    if (!std::holds_alternative<std::string>(hash)) {
        std::abort();
    }
    return std::get<std::string>(hash);
}

} // namespace

void register_m4_workload_specs() {
    const std::string_view families[] = {"AdaptExchangeDepthSnapshot/Spot",
                                         "AdaptDepthUpdate/Spot",
                                         "CheckedInstall",
                                         "CheckedApply",
                                         "MakeLocalOrderBookSnapshot/Unlimited",
                                         "MakeLocalOrderBookSnapshot/Limited",
                                         "SerializeSnapshot/FreshBuffer",
                                         "SerializeSnapshot/ReusedBuffer"};
    for (const auto family : families) {
        for (const auto depth : kM4DepthSet) {
            const auto name = "M4/" + std::string{family} + "/" + std::to_string(depth);
            auto& builder = register_workload(name);
            builder.set("benchmark_name", name);
            builder.set("operation", family);
            builder.set("depth_per_side", depth);
            builder.set("market", "Spot");
            builder.set("generator_schema", "M5_PHASE6_M4_CELLS_V1");
            builder.set("generated_workload_sha256", m4_generated_sha256(family, depth));
            builder.set("primary_timer", "cpu");
            builder.set("primary_denominator", "cpu_time");
            if (std::string{family} == "SerializeSnapshot/ReusedBuffer") {
                builder.set("serialization_buffer_mode", "reused");
                builder.set("diagnostic", true);
            } else if (std::string{family} == "SerializeSnapshot/FreshBuffer") {
                builder.set("serialization_buffer_mode", "fresh");
            }
            if (std::string{family} == "AdaptDepthUpdate/Spot" ||
                std::string{family} == "CheckedApply") {
                builder.set("update_level_count", wire_support::kM4UpdateLevelCount);
            }
            if (std::string{family} == "CheckedApply") {
                for (const auto& [key, value] :
                     wire_support::checked_apply_canonical_sequence_fields()) {
                    builder.set(key, value);
                }
            }
        }
    }
}

} // namespace bmd_projection::m5::benchmark
