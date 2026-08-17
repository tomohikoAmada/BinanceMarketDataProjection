#pragma once

// Canonical replay workload-spec registration shared by the Phase-6 timing
// benchmark executable and the Phase-7 replay allocation executable
// (OD-M5-P7-013: the accepted small-tier Spot/USD-M workloads with their
// canonical replay-log SHA-256 identities; no new fixture is invented).
//
// The adapter identities register only when BMD_PROJECTION_PHASE6_ADAPTER_ENABLED
// is defined for the consuming target (mirroring the Phase-6 availability
// rule).

namespace bmd_projection::m5::benchmark {

void register_replay_workload_specs();

} // namespace bmd_projection::m5::benchmark
