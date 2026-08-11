#include "canonical_observation.hpp"
#include "semantic_digest.hpp"
#include "semantic_manifest.hpp"

#include "../oracle/adapter_production_side.hpp"
#include "../oracle/core_production_side.hpp"
#include "../oracle/reference_side.hpp"
#include "../oracle/replay_driver.hpp"
#include "../phase3/small_workload.hpp"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace oracle = bmd_projection::m5::oracle;
namespace phase3 = bmd_projection::m5::phase3;
namespace replay = bmd_projection::m5::replay;
namespace semantic = bmd_projection::m5::semantic;

struct WorkloadConfig final {
    std::string workload_id;
    std::string fixture_id;
    std::string fixture_hash;
    oracle::ReplayMode mode;
    const replay::ReplayFixture& fixture;
};

struct CliArgs final {
    std::string output_path;
    std::string head_sha;
    bool valid{false};
};

[[nodiscard]] CliArgs parse_args(int argc, char** argv) {
    CliArgs args;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--output" && i + 1 < argc) {
            args.output_path = argv[++i];
        } else if (arg == "--head-sha" && i + 1 < argc) {
            args.head_sha = argv[++i];
        }
    }
    args.valid = !args.output_path.empty() && !args.head_sha.empty();
    return args;
}

[[nodiscard]] oracle::ReplayOutcome run_core_replay(const replay::ReplayFixture& fixture) {
    oracle::ReplayDriver driver{fixture, oracle::make_core_production_side(fixture),
                                oracle::make_reference_side(fixture, oracle::ReplayMode::CoreOnly),
                                oracle::ObservationRetention::RetainAll};
    return driver.run();
}

[[nodiscard]] oracle::ReplayOutcome run_adapter_replay(const replay::ReplayFixture& fixture) {
    oracle::ReplayDriver driver{
        fixture, oracle::make_adapter_production_side(fixture),
        oracle::make_reference_side(fixture, oracle::ReplayMode::AdapterEnabled),
        oracle::ObservationRetention::RetainAll};
    return driver.run();
}

[[nodiscard]] semantic::ManifestWorkloadEntry process_workload(const WorkloadConfig& config) {
    semantic::ManifestWorkloadEntry entry;
    entry.workload_id = config.workload_id;
    entry.fixture_id = config.fixture_id;
    entry.fixture_hash = config.fixture_hash;

    oracle::ReplayOutcome outcome;
    if (config.mode == oracle::ReplayMode::CoreOnly) {
        outcome = run_core_replay(config.fixture);
    } else {
        outcome = run_adapter_replay(config.fixture);
    }

    if (outcome.first_divergence.has_value()) {
        std::cerr << "ERROR: differential divergence in workload " << config.workload_id
                  << " at event " << outcome.first_divergence->event_index << '\n'
                  << oracle::render_divergence(*outcome.first_divergence) << '\n';
        std::exit(1);
    }

    if (outcome.processed_events != phase3::kSmallWorkloadEventCount) {
        std::cerr << "ERROR: workload " << config.workload_id << " processed "
                  << outcome.processed_events << " events, expected "
                  << phase3::kSmallWorkloadEventCount << '\n';
        std::exit(1);
    }

    if (outcome.observations.empty()) {
        std::cerr << "ERROR: no observations retained for workload " << config.workload_id << '\n';
        std::exit(1);
    }

    entry.semantic_digest =
        semantic::compute_semantic_digest_from_observations(outcome.observations);

    if (entry.semantic_digest.empty()) {
        std::cerr << "ERROR: digest computation failed for workload " << config.workload_id << '\n';
        std::exit(1);
    }

    std::cout << "  " << config.workload_id << ": digest=" << entry.semantic_digest << '\n';

    return entry;
}

[[nodiscard]] semantic::ManifestToolchain get_toolchain() {
    semantic::ManifestToolchain tc;
#ifdef BMD_PROJECTION_COMPILER_ID
    tc.compiler = BMD_PROJECTION_COMPILER_ID;
#else
    tc.compiler = "UNKNOWN";
#endif
#ifdef BMD_PROJECTION_COMPILER_VERSION
    tc.compiler_version = BMD_PROJECTION_COMPILER_VERSION;
#else
    tc.compiler_version = "UNKNOWN";
#endif
#ifdef BMD_PROJECTION_SYSTEM_NAME
    tc.os = BMD_PROJECTION_SYSTEM_NAME;
#else
    tc.os = "UNKNOWN";
#endif
#ifdef BMD_PROJECTION_SYSTEM_PROCESSOR
    tc.architecture = BMD_PROJECTION_SYSTEM_PROCESSOR;
#else
    tc.architecture = "UNKNOWN";
#endif
    return tc;
}

[[nodiscard]] std::string get_build_type() {
#ifdef BMD_PROJECTION_BUILD_TYPE
    return BMD_PROJECTION_BUILD_TYPE;
#else
    return "UNKNOWN";
#endif
}

} // namespace

int main(int argc, char** argv) {
    const auto args = parse_args(argc, argv);
    if (!args.valid) {
        std::cerr
            << "Usage: bmd_projection_m5_semantic_manifest --output <path> --head-sha <sha>\n";
        return 1;
    }

    if (args.head_sha == "LOCAL") {
        std::cerr << "WARNING: running with head-sha=LOCAL (non-CI mode)\n";
    } else if (args.head_sha.size() != 40) {
        std::cerr << "ERROR: --head-sha must be a 40-character SHA-1 hex string or LOCAL\n";
        return 1;
    }

    std::cout << "M5 Semantic Manifest Producer v1\n";
    std::cout << "HEAD: " << args.head_sha << '\n';

    std::cout << "Generating Spot workload...\n";
    const auto spot_fixture = phase3::make_spot_small_workload();

    std::cout << "Generating USD-M workload...\n";
    const auto usdm_fixture = phase3::make_usdm_small_workload();

    const std::string spot_hash = spot_fixture.canonical_log_sha256;
    const std::string usdm_hash = usdm_fixture.canonical_log_sha256;

    std::cout << "Spot fixture hash: " << spot_hash << '\n';
    std::cout << "USD-M fixture hash: " << usdm_hash << '\n';

    std::cout << "Processing workloads...\n";

    std::vector<WorkloadConfig> configs = {
        {"m5-small-core-spot-v1", spot_fixture.identity.fixture_id, spot_hash,
         oracle::ReplayMode::CoreOnly, spot_fixture},
        {"m5-small-core-usdm-v1", usdm_fixture.identity.fixture_id, usdm_hash,
         oracle::ReplayMode::CoreOnly, usdm_fixture},
        {"m5-small-adapter-spot-v1", spot_fixture.identity.fixture_id, spot_hash,
         oracle::ReplayMode::AdapterEnabled, spot_fixture},
        {"m5-small-adapter-usdm-v1", usdm_fixture.identity.fixture_id, usdm_hash,
         oracle::ReplayMode::AdapterEnabled, usdm_fixture},
    };

    std::vector<semantic::ManifestWorkloadEntry> entries;
    for (const auto& config : configs) {
        entries.push_back(process_workload(config));
    }

    semantic::SemanticManifest manifest;
    manifest.schema_version = std::string(semantic::kManifestSchemaV1);
    manifest.head_sha = args.head_sha;
    manifest.toolchain = get_toolchain();
    manifest.build_type = get_build_type();
    manifest.fixture_set_id = semantic::compute_fixture_set_id(entries);
    manifest.workloads = std::move(entries);

    const auto json = semantic::render_manifest_json(manifest);

    std::ofstream out(args.output_path);
    if (!out) {
        std::cerr << "ERROR: cannot write manifest to " << args.output_path << '\n';
        return 1;
    }
    out << json;
    if (!out) {
        std::cerr << "ERROR: I/O failure writing manifest to " << args.output_path << '\n';
        return 1;
    }
    out.close();

    std::cout << "Manifest written to " << args.output_path << '\n';
    std::cout << "Fixture set ID: " << manifest.fixture_set_id << '\n';
    std::cout << "DONE\n";
    return 0;
}
