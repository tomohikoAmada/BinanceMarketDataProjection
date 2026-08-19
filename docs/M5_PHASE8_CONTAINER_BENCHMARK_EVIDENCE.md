# M5 Phase-8 Container Benchmark Evidence (PR-B / WP2)

## Status

PR-A / WP1 is accepted and merged in PR #30. PR-B / WP2 was independently
accepted and merged in PR #31. Phase 8 is **COMPLETE / EVIDENCE ACCEPTED**.
The later Phase-9 decision is **COMPLETE / DECISION ACCEPTED** with `KEEP_STD_MAP`;
no production order-book migration is authorized. The explicit decision record is
`docs/M5_PHASE9_CONTAINER_DECISION.md`.

PR #31 approved Head: `f1bbe499f7179094cfefae796f454951e1736add`.
PR #31 squash merge / Phase-8 formal-evidence source revision:
`b06fa2f5716527cc5fda3e102ba358721336246c`.
Post-merge main CI: run `32203582370`, workflow `ci`, event `push`, completed
`success`, 19/19 jobs PASS.

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

Historical implementation-PR scope: that PR did not itself generate or claim
formal controlled-environment performance evidence. The later formal cohort
and independent acceptance are recorded below; the Phase-9 decision record interprets that
accepted evidence and applies the frozen decision criteria. This document does not choose a winner.

## Formal evidence acceptance

The authoritative formal evidence was collected from:

```text
SOURCE_SHA=b06fa2f5716527cc5fda3e102ba358721336246c
SOURCE_TREE=7a8e924879305b3ddba34304573c0ed81ac962bb
ARCHIVE=phase8-formal-evidence-b06fa2f.tar.gz
ARCHIVE_SHA256=2c8dd51d1e91ecdd936d32b95436c93f199f60acce24361e9ae8533c7ad48657
EVIDENCE_BINARY_SHA256=6ac0361ee8d92e782532a3df0373d660f8f9192931b1d843a20d91d1077114c1
VERDICT=FORMAL_EVIDENCE_ACCEPTED
P0=0
P1=0
P2=1
INFO=2
```

Protocol and integrity summary:

- Three independent process campaigns, seven repetitions, exactly ten workloads, exactly four
  candidates, forty records per campaign, and no filtering.
- Archive integrity PASS; SHA256SUMS had 62/62 retained entries PASS; provenance, binary, payload,
  and dependency binding PASS.
- Formal effective evidence class in all campaigns; semantic digest consistency PASS; empirical
  `std::map` control noise retained; allocation and persistent requested-byte evidence retained;
  RSS not measured.
- Matrix had ten workloads, four candidates, forty cells per campaign, with no duplicates, missing
  cells, or extras. Cross-campaign cohort homogeneity was ACCEPTABLE.

Controlled host and build userspace:

```text
Host: Ubuntu 22.04.5 LTS, Linux x86_64, Intel Core i5-11320H
Measurement CPU: logical CPU 3
Toolchain: repository canonical amd64 toolchain image, Clang 18.1.3, Release, C++20
ProtoAdapter: OFF
Sanitizers: OFF
LTO: OFF
Conan: 2.31.2
```

The host is the controlled Phase-8 comparison host; it is not claimed to be universally
representative hardware.

### Retained nonblocking finding

`P8-EVIDENCE-P2-001` is P2 / NONBLOCKING. The `std::map` control for
`M5_PHASE8/mixed_updates/1000` showed materially high within-campaign timing noise: approximately
13.605M, 12.170M, and 9.950M updates/s campaign medians, with within-campaign CV approximately
21.65%, 22.11%, and 20.57%. This does not invalidate the Phase-8 cohort. Phase 9 must apply the
frozen significance and empirical-noise requirements before this cell contributes to a migration
decision. No new benchmark campaign is required solely to remove this P2.

### INFO limitations

- Effective Intel P-state/HWP controls and stable governor/EPP/turbo/frequency policy were retained,
  but not a direct `scaling_driver` or `intel_pstate/status` string.
- Container affinity was bound to logical CPU 3, but its physical core/SMT sibling was not
  exclusively isolated from every normal host process. No material sustained competing load or
  cohort-wide timing drift was found.

Both INFO items are nonblocking.

## Governance boundary

On merge, this record establishes:

```text
PHASE7=COMPLETE / EVIDENCE ACCEPTED
PHASE8_WP1=ACCEPTED / MERGED
PHASE8_WP2=ACCEPTED / MERGED
PHASE8_FORMAL_EVIDENCE=ACCEPTED
PHASE8=COMPLETE / EVIDENCE ACCEPTED
PHASE9=AUTHORIZED / NOT STARTED
CONTAINER_WINNER=NONE
PRODUCTION_MIGRATION=NO
PHASE9_STARTED=NO
```

Later Phase-9 decision:

The separately conducted Phase-9 decision review concluded `KEEP_STD_MAP`. The durable current
governance state recorded by `docs/M5_PHASE9_CONTAINER_DECISION.md` is:

```text
PHASE9=COMPLETE / DECISION ACCEPTED
CONTAINER_DECISION=KEEP_STD_MAP
CANDIDATE_WINNER=NONE
PRODUCTION_CONTAINER=std::map
PRODUCTION_MIGRATION=NO
MIGRATION_IMPLEMENTED=NO
PHASE10=AUTHORIZED / NOT STARTED
```

The Phase-8 evidence record did not itself make the Phase-9 decision; it remains the immutable
numeric evidence record.
