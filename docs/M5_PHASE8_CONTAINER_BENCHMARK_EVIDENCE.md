# M5 Phase-8 Container Benchmark Evidence (PR-B / WP2)

## Status

PR-A / WP1 is accepted and merged in PR #30. PR-B / WP2 implements the
benchmark-only comparison substrate and is pending independent exact-head
review. Phase 8 remains **IN PROGRESS**; Phase 9 is **NOT STARTED**. No
candidate is selected and no production order-book migration is authorized.

## Candidate set

The fixed comparison set is exactly:

- `phase8-std-map-control-v1`
- `phase8-sorted-vector-naive-v1`
- `phase8-absl-btree-map-v1`
- `phase8-sorted-vector-batch-lww-v1`

All four use the WP1 model protocol and remain under `benchmarks/phase8`.
Abseil is linked only by test/benchmark targets. Core and installed
consumers remain Abseil-free.

## Workloads and measurements

The standard cells reuse the Phase-6 M2 workload identities and deterministic
generators for insertion, update, deletion, replacement-heavy batches, full
replacement, and top-N reads at shallow and deep depths. Phase-8 obtains the
canonical prepared state and operation sequence from the Phase-6 benchmark
support cells; it does not recreate approximate inputs while retaining their
identities. A separate
`M5_PHASE8/mixed_updates/1000` identity covers insertion, quantity update,
deletion, and duplicate-price last-write-wins input in one logical stream.

`bmd_projection_benchmarks` registers the same cells for all four candidates
through one Google Benchmark wrapper. Candidate construction, population,
state restoration, and destruction are outside the timed operation bracket.
`bmd_projection_m5_phase8_container_evidence` records raw repeated wall-clock
samples for replay/update throughput, update latency, full replacement latency,
and top-N latency.

The payload keeps timer-call calibration under
`timer_overhead_calibration`. Its `empirical_noise_floor` is instead derived
from repeated unchanged `Phase8StdMapControl` measurements for every workload
and primary metric, using the same harness and environment. Raw samples and
population-standard-deviation summaries are retained; even repetition counts
use the conventional mean-of-two-middle-values median.

The evidence producer also reuses the Phase-7 replaceable-global-new boundary
to record allocation counts/bytes and persistent live requested bytes after
population. RSS is explicitly not measured. Raw samples, deterministic
summary statistics, workload identities, source/build/dependency provenance,
and payload SHA binding are emitted in `M5_PHASE8_EVIDENCE_PAYLOAD_V1` behind
the established `M5_BENCHMARK_WRAPPER_V1` provenance wrapper.

`scripts/benchmark_phase8.py` treats the wrapper workload inventory as
authoritative and fails closed on malformed or duplicate JSON, unknown or
missing workloads/candidate cells, record-to-wrapper identity mismatches,
incompatible metric/unit pairs, malformed Git SHAs, non-finite or
negative-impossible values, repetition mismatches, digest mismatches, and
payload/binary binding failures. `scripts/benchmark-phase8.sh` provides a
structural smoke path and a manual exploratory full path; neither applies a
numeric performance threshold.

## Limitation carried forward

The pinned Abseil 20260107.1 candidate passes normal M2 observable-state
semantics, but the independently confirmed allocation-failure leak risk in
temporary B-tree storage remains an INFO candidate decision risk. WP2 does
not patch, fork, hide, or remove Abseil and does not claim general
allocator-exception safety.

Formal controlled-environment performance evidence is not generated or
claimed by this implementation PR. A later independent review and Phase 9
decision must interpret repeated evidence and apply any accepted improvement
criteria; this work package does not choose a winner.
