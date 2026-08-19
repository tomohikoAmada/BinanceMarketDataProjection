# M5 Phase-9 Container Decision

## Status

```text
DECISION=KEEP_STD_MAP
DECISION_STATUS=ACCEPTED_PENDING_GOVERNANCE_MERGE
P0=0
P1=0
P2=2
INFO=1
```

The independent Phase-9 decision review is complete. This record is the governance handoff;
on merge, Phase 9 becomes `COMPLETE / DECISION ACCEPTED`.

## Authority

The decision was made against the following immutable Phase-9 decision governance base:

```text
BASE_LABEL=pre-Phase-9-decision-record main
BASE_SHA=d6f7fbdaf425b280b549e199adc1d9951485d8d0
BASE_TREE=7e6f2af4f6789c061d260e67a5f326e57faf26b2
```

The accepted Phase-8 formal evidence identity is:

```text
SOURCE_SHA=b06fa2f5716527cc5fda3e102ba358721336246c
SOURCE_TREE=7a8e924879305b3ddba34304573c0ed81ac962bb
ARCHIVE=phase8-formal-evidence-b06fa2f.tar.gz
ARCHIVE_SHA256=2c8dd51d1e91ecdd936d32b95436c93f199f60acce24361e9ae8533c7ad48657
EVIDENCE_BINARY_SHA256=6ac0361ee8d92e782532a3df0373d660f8f9192931b1d843a20d91d1077114c1
VERDICT=FORMAL_EVIDENCE_ACCEPTED
```

## Frozen decision rule

A candidate must be semantically conformant and measured by the representative workload suite
with at least five repetitions, statistical significance, non-overlapping confidence intervals,
and at least three sigma under empirical unchanged-control noise. It must improve at least 20%
in at least two primary dimensions—replay throughput, update latency, top-N latency, and memory—
with no regression beyond 10% in any other measured primary dimension. Allocation, exception-safety,
and maintenance evidence also contribute to the decision. `KEEP std::map` is a valid result.

## Candidate dispositions

| Candidate | Disposition |
|---|---|
| `phase8-sorted-vector-naive-v1` | Semantically eligible; top-N latency and memory qualifying improvements; replay throughput failed with a material regression; does not qualify. |
| `phase8-absl-btree-map-v1` | Semantically eligible; memory qualifying improvement; replay throughput failed with a material regression; improved primary dimensions insufficient; does not qualify. |
| `phase8-sorted-vector-batch-lww-v1` | Semantically eligible; top-N latency and memory qualifying improvements; replay throughput failed with a severe regression; does not qualify. |
| `phase8-std-map-control-v1` | Control; keep. |

## Decisive evidence

The replicated deep-replay throughput regressions versus the `std::map` control were:

| Candidate | Campaign 1 | Campaign 2 | Campaign 3 |
|---|---:|---:|---:|
| Sorted-vector naive | -55.0% | -51.4% | -54.6% |
| Abseil `btree_map` | -65.4% | -62.5% | -66.0% |
| Sorted-vector batch-LWW | -88.0% | -87.4% | -88.3% |

Each regression is far beyond the frozen 10% rejection threshold. These results do not claim that
`std::map` is fastest in every workload; they establish that no alternative candidate satisfies
the complete migration gate.

## Statistical limitations

`P9-DECISION-P2-001` — P2 / NONBLOCKING: the frozen authority does not define one unique formula
for aggregating workloads into primary dimensions. No arithmetic or geometric averaging is
invented to create a migration winner. Migration eligibility must be positively demonstrated;
the KEEP decision is robust because all candidates have large, replicated replay regressions.

`P9-DECISION-P2-002` — P2 / NONBLOCKING: the authority requires non-overlapping confidence
intervals and at least three sigma but does not freeze the exact confidence level, CI estimator,
or sigma algebra. The independent reviewer used conservative sensitivity analysis. Its temporary
99.7% Student-t intervals and difference-versus-control-population-standard-deviation check are
review sensitivity checks only, not new normative repository rules. The KEEP decision does not
depend on selecting those formulas.

## Carried evidence limitation

`P8-EVIDENCE-P2-001` — P2 / NONBLOCKING: the `std::map` control for
`M5_PHASE8/mixed_updates/1000` has high within-campaign timing noise. Its favorable point estimate
cannot alone establish a replay-dimension win or average away adverse canonical `apply_updates`
workloads. This does not reject the accepted Phase-8 evidence.

## Supporting engineering evidence

The pinned Abseil candidate has an independently confirmed temporary-resource leak risk on the
allocation-failure path. It does not violate the accepted observable M2 strong-state guarantee,
and it is supporting evidence only—not the primary reason for `KEEP_STD_MAP`; the candidate already
fails the numeric migration gate independently.

Where memory results are summarized, memory means
`persistent_live_storage.measured_requested_bytes` within executable-local replaceable-global-new
requested-byte instrumentation. RSS, physical RAM, whole-process heap, and allocator-arena
footprint were not measured.

## Decision

`KEEP std::map`.

No alternative candidate qualifies under the complete frozen migration rule. No production
architecture or order-book storage change is authorized.

## Consequences

- Production `OrderBook` remains backed by `std::map`.
- No production code or production dependency changes are made.
- No production migration or migration regression campaign is required.
- On merge, Phase 10 is authorized separately for CI/reporting integration, the `m5-replay` job,
  manifest fold-in, the performance workflow, and artifact retention.

## Non-goals

This record does not implement Phase 10, modify production or benchmark code, modify tests, add
Abseil to production, change order-book storage, rerun formal evidence, or create a production
migration. No new ADR is created because the decision retains the existing production architecture.
