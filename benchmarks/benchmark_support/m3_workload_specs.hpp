#pragma once

// Canonical M3 workload-spec registration shared by the Phase-6 timing
// benchmark executable and the Phase-7 allocation measurement executables
// (OD-M5-P7-009/010: the complete 48-cell accepted live-apply matrix, the
// classification cells, and the Component/Proxy diagnostic cells reuse the
// accepted Phase-6 workload identities verbatim).

namespace bmd_projection::m5::benchmark {

void register_m3_workload_specs();

} // namespace bmd_projection::m5::benchmark
