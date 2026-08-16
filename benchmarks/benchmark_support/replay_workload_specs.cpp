#include "replay_workload_specs.hpp"

#include "core_replay_executor.hpp"
#include "replay_checksum.hpp"
#include "workload_spec.hpp"

#include "small_workload.hpp"

#include <algorithm>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bmd_projection::m5::benchmark {
namespace {

namespace replay = bmd_projection::m5::replay;
namespace phase3 = bmd_projection::m5::phase3;

[[nodiscard]] std::string replay_spec_suffix(const replay::ReplayFixture& fixture) {
    auto identity = replay_fixture_identity(fixture);
    std::replace(identity.begin(), identity.end(), '\n', ';');
    return identity;
}

void set_replay_generated_identity(WorkloadSpecBuilder& builder,
                                   const replay::ReplayFixture& fixture) {
    const auto provenance_value = [&fixture](std::string_view key) {
        const auto found =
            std::find_if(fixture.manifest.provenance.begin(), fixture.manifest.provenance.end(),
                         [key](const auto& entry) { return entry.first == key; });
        return found == fixture.manifest.provenance.end() ? std::string{"not_applicable"}
                                                          : found->second;
    };
    builder.set("canonical_log_sha256", fixture.canonical_log_sha256);
    builder.set("generated_workload_sha256", fixture.canonical_log_sha256);
    builder.set("generator_version", provenance_value("generator"));
    builder.set("seed", provenance_value("seed"));
}

void register_core_replay_specs() {
    {
        const auto fixture = phase3::make_spot_small_workload();
        auto& builder = register_workload("CoreNormalizedReplay/Spot");
        builder.set("benchmark_name", "CoreNormalizedReplay/Spot");
        builder.set("replay_mode", "CoreOnly");
        builder.set("workload_identity", replay_spec_suffix(fixture));
        builder.set("timed_path",
                    "preloaded normalized replay -> production M1 parse -> production M3 "
                    "BookProjection -> FNV-1a checksum consumption");
        builder.set("excluded",
                    "canonical_text_parsing fixture_io hashing generation reference_model "
                    "ReplayDriver OperationObservation checkpoint");
        builder.set("throughput_denominator", "wall_time");
        builder.set("primary_timer", "wall");
        builder.set("checksum_methodology_version", kReplayChecksumMethodology);
        builder.set("generator_schema", "M5_PHASE6_REPLAY_V1");
        builder.set("logical_items_per_iteration", fixture.replay.operations.size());
        set_replay_generated_identity(builder, fixture);
    }
    {
        const auto fixture = phase3::make_usdm_small_workload();
        auto& builder = register_workload("CoreNormalizedReplay/UsdMPerpetual");
        builder.set("benchmark_name", "CoreNormalizedReplay/UsdMPerpetual");
        builder.set("replay_mode", "CoreOnly");
        builder.set("workload_identity", replay_spec_suffix(fixture));
        builder.set("timed_path",
                    "preloaded normalized replay -> production M1 parse -> production M3 "
                    "BookProjection -> FNV-1a checksum consumption");
        builder.set("excluded",
                    "canonical_text_parsing fixture_io hashing generation reference_model "
                    "ReplayDriver OperationObservation checkpoint");
        builder.set("throughput_denominator", "wall_time");
        builder.set("primary_timer", "wall");
        builder.set("checksum_methodology_version", kReplayChecksumMethodology);
        builder.set("generator_schema", "M5_PHASE6_REPLAY_V1");
        builder.set("logical_items_per_iteration", fixture.replay.operations.size());
        set_replay_generated_identity(builder, fixture);
    }
}

#if defined(BMD_PROJECTION_PHASE6_ADAPTER_ENABLED)
void register_adapter_replay_specs() {
    for (const auto& [name, fixture] :
         std::vector<std::pair<std::string, std::function<replay::ReplayFixture()>>>{
             {"AdapterWireReplay/Spot", [] { return phase3::make_spot_small_workload(); }},
             {"AdapterWireReplay/UsdMPerpetual",
              [] { return phase3::make_usdm_small_workload(); }}}) {
        const auto materialized = fixture();
        auto& builder = register_workload(name);
        builder.set("benchmark_name", name);
        builder.set("replay_mode", "AdapterEnabled");
        builder.set("workload_identity", replay_spec_suffix(materialized));
        builder.set("timed_path",
                    "preconstructed wire -> production M4 adaptation -> checked production M3 "
                    "invocation -> FNV-1a checksum consumption");
        builder.set("excluded",
                    "wire_construction fixture_parsing file_io hashing generation reference_model "
                    "ReplayDriver OperationObservation checkpoint_comparison diagnostic_rendering");
        builder.set("snapshot_serialization", "excluded");
        builder.set("throughput_denominator", "wall_time");
        builder.set("primary_timer", "wall");
        builder.set("checksum_methodology_version", kReplayChecksumMethodology);
        builder.set("generator_schema", "M5_PHASE6_REPLAY_V1");
        builder.set("logical_items_per_iteration", materialized.replay.operations.size());
        set_replay_generated_identity(builder, materialized);
    }
}
#endif

} // namespace

void register_replay_workload_specs() {
    register_core_replay_specs();
#if defined(BMD_PROJECTION_PHASE6_ADAPTER_ENABLED)
    register_adapter_replay_specs();
#endif
}

} // namespace bmd_projection::m5::benchmark
