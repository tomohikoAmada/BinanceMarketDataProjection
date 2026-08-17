#pragma once

// Canonical M4 workload-spec registration shared by the Phase-6 timing
// benchmark executable and the Phase-7 M4 allocation executable
// (OD-M5-P7-012: the eight accepted M4 families at depths {8, 100, 1000}
// reuse the Phase-6 family names, cell semantics, and generated-workload
// identities verbatim). Consuming targets must define
// BMD_PROJECTION_PHASE6_ADAPTER_ENABLED and link the ProtoAdapter.

namespace bmd_projection::m5::benchmark {

void register_m4_workload_specs();

} // namespace bmd_projection::m5::benchmark
