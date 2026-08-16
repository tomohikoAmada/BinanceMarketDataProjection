#pragma once

// Canonical M2 workload-spec registration shared by the Phase-6 timing
// benchmark executable and the Phase-7 allocation measurement executables
// (OD-M5-P7-008: Phase-7 reuses the accepted Phase-6 workload identities
// verbatim; ONE identity source, no second generator).
//
// The registration sequence and canonical field content are bit-for-bit
// identical to the Phase-6 registration that previously lived inside
// benchmarks/m2_benchmarks.cpp. include_update_mix selects the Phase-6
// scaling family M2/apply_updates/update_mix/*, which is a Phase-6 timing
// family and is deliberately NOT part of the Phase-7 formal M2 inventory
// (OD-M5-P7-008).

#include <cstddef>

namespace bmd_projection::m5::benchmark {

// Phase 6: include_update_mix=true. Phase 7: include_update_mix=false.
void register_m2_workload_specs(bool include_update_mix);

} // namespace bmd_projection::m5::benchmark
