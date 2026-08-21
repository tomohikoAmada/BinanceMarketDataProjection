# Milestones

## M0 Repository Foundation — COMPLETE

### Goal

Create a private, repeatably buildable, testable, installable C++20 library baseline that future
Gateway and History/Replay consumers can reuse.

### Inputs

None — this is the bootstrap milestone.

### Outputs

- CMake/Ninja presets and target-local warnings, sanitizers, clang-tidy, and coverage options.
- Repository-local Conan 2 workflow locked to GoogleTest 1.17.0 and Google Benchmark 1.9.5.
- Minimal version API, unit and header self-containment tests, and benchmark smoke target.
- Install/export package and an isolated staged-install consumer test.
- GitHub Actions across Ubuntu GCC, Ubuntu Clang, and macOS AppleClang, plus ASan/UBSan and benchmark
  jobs.
- README, architecture boundaries, agent rules, milestones, open questions, and four ADRs.

### Acceptance gates

Debug and Release builds/tests, ASan, UBSan, format, tidy, benchmark smoke, package installation, and
the independent consumer must pass. Required GitHub Actions jobs must be green, the Draft PR must
remain unmerged, the worktree must be clean, and M0 must contain no projection business behavior.
TSan must be exercised when the platform runtime supports it or its real limitation documented.

### Non-goals

No decimal parsing, fixed-point domain types, order book, snapshot/diff application, sequence policy,
gap/resync logic, market-state calculations, protobuf adapter, gRPC, network transport, JSON parser,
Python binding, persistence, Gateway/History runtime, strategy, risk, or trading behavior.

### Definition of done

All M0 deliverables are committed to `feat/m0-repository-foundation`, local mandatory checks pass,
required CI is green, a Draft PR targets the bootstrap-only `main`, and this status is changed to
`COMPLETE` in a separate final commit followed by another green CI run.

Local clang-tidy may be skipped when the host toolchain lacks it; CI clang-tidy is the mandatory
gate. The version header is generated from CMake; no hand-maintained duplicate version string exists.

## M1 Numeric and Domain Primitives — COMPLETE

### Goal

Establish the exact, deterministic numeric boundary used by future projection behavior without
implementing order-book or market-state logic.

### Outputs

- Explicit decimal scales from 0 through 18 and distinct signed 64-bit price and quantity units.
- Strict Contracts-compatible parsing for prices, quantities, and positive quantities.
- Exact rescaling and deterministic formatting with retained source fractional digit counts.
- Stable error codes and prechecked arithmetic that never relies on signed overflow or rounding.
- Boundary, roundtrip, public-header, and 20,000-case deterministic property validation.
- Clang-only libFuzzer harness, seed corpus, blocking CI smoke job, and staged-install consumer use.
- M1 numeric semantics documentation and accepted fixed-point representation decision.

### Acceptance status

Implementation, required local checks, and external code review are complete. External code review
concluded with no blocking correctness findings. M1 is approved for merge and is marked complete.

### Non-goals

No order book, price-level map, snapshots, diff application, sequencing, gap/resync policy,
market-state calculations, Protobuf adapter, networking, host runtime, Python binding, symbol
filters, signed decimals, persistence, threading, logging, strategy, or trading behavior.

## M2 Order Book Core — COMPLETE

### Goal

Implement a deterministic, single-writer, market-by-price order book with absolute-quantity
semantics, zero-quantity deletion, bid/ask ordering, best-level queries, top-N output, batch
updates, and atomic full-book replacement.

### Outputs

- `BookSide`, `BookLevel`, `LevelUpdate`, `LevelChange`, and `OrderBook` public API.
- PIMPL-isolated `std::map` storage with bid descending / ask ascending ordering.
- Absolute quantity replacement, zero-quantity deletion, missing-delete no-op.
- Batch update with ordered application and same-price last-write-wins.
- Atomic `replace_all` with last-write-wins per side and strong exception guarantee.
- Best bid/ask, quantity-at-price, top-N, and full-level-copy queries.
- Crossed and locked book acceptance without rejection or auto-resolution.
- Independent vector-based reference model for property and fuzz validation.
- Model-based libFuzzer harness and deterministic property tests.
- M2 semantics documentation and updated ADR-0003.

### Acceptance status

Implementation, unit tests, property tests, fuzzing, and external code review are complete.
M2 passed external code review with no blocking correctness, determinism, portability, or
validation findings. M2 is approved for merge and is marked complete.

### Non-goals

No sequence validation, update IDs, gap detection, resynchronization, snapshot contract, protobuf
adapter, networking, Gateway runtime, History runtime, Python binding, matching engine, strategy,
persistence, or trading behavior.

## M3 Sequence and Projection State — COMPLETE

Implementation status: **COMPLETE**

### Goal

Define market-specific sequencing, bootstrap validation, gap detection, reset, and the synchronized
projection lifecycle while deterministically applying normalized absolute-quantity depth batches to
the M2 order book.

### Implementation deliverables

- Strongly typed unsigned Binance update IDs and valid-by-construction inclusive update ranges.
- Explicit Spot and USD(S)-M perpetual sequence-policy kinds, matching the frozen Contracts market
  identifiers.
- Separate Spot interval continuity and USD-M previous-final (`pu`) continuity classifiers.
- Domain-only baseline and depth-batch views with no wire or runtime dependency.
- `AwaitingBaseline`, `AwaitingBridge`, `Synchronized`, and `NeedsResync` lifecycle states.
- Stable apply results, deterministic gap evidence, reset, and synchronization-aware const queries.
- Strong exception safety for baseline installation and incremental sequence/book commit.
- Complete transition/unit matrices, an independent property model, deterministic replay tests,
  allocation-failure tests, and a model-based M3 fuzz harness.
- M3 architecture design and ACCEPTED ADR-0005, with the independent external review result
  recorded in the design documents.

COIN-M was reviewed as future-compatibility evidence but is not in the current M3 public surface
because the frozen Contracts baseline does not identify a COIN-M market.

### Design status

Architecture review: **APPROVED**.

Implementation review: **APPROVED**. Blocking findings: **0**.

ADR-0005 is **ACCEPTED**.

The M3 public API, production source, unit/state/property/replay/allocation-failure validation,
model-based fuzz harness, seed corpus, and staged-install consumer are implemented on `main`.
External implementation code review approved head
`7606a60bbb2d2a192f6c0259942174fbd49847ba` with no blocking findings, and CI run `31083008166`
passed 8/8 jobs for that exact head.

PR #6 is **MERGED** with squash merge
`39b34dc3a2fd5784f6a53d5e39c80e56be42355c`. Main CI run `31088396997` passed 8/8 jobs.
M3 is complete on `main`; M4 and later milestones remain separate future work.

### Non-goals

M3 does not own snapshot download, WebSocket transport, network-event buffering, reconnect, Host
queues, Gateway session identity, timestamps, threads, locks, logging, telemetry, persistence, or
History storage. M4 owns Protobuf/wire adaptation and snapshot contract production; M6 owns Gateway
runtime integration. Derived market prices, matching, orders, strategy, risk, and trading remain out
of scope.

## M4 Snapshots and Protobuf Boundary — COMPLETE

Implementation status: **COMPLETE**

### Acceptance status

Design status: **APPROVED / MERGED**.

ADR-0006: **ACCEPTED**.

External architecture review: **APPROVED**. Round 1 findings: **3 CLOSED**. Architecture blockers:
**0**.

Independent Implementation Review: **APPROVED**.

Implementation blockers: **0**. C-M4-001: **CLOSED / SATISFIED**. OD-M4-001: **CLOSED**.
Contracts integration: **VERIFIED**. M4 is **COMPLETE** on `main`.

Reviewed Head: `390cdceb013bc05878db090bdedc40068c03c79c`.
Reviewed CI: `31193311386` — 16/16 PASS.

### Merge completion record

- Pull request: #8 — Implement M4 snapshots and Protobuf boundary
- Final approved PR head: `efddcd2fbe6901a7506ae84aee5586be773a2835`
- Squash merge commit: `ac780d9eb7b49ff20a6b3b4bee6a993b51b70af4`
- Merge time: `2026-08-08T05:39:08Z`
- Main CI run: `31242162782`
- Main CI event: `push`
- Main CI result: `16/16 PASS`
- M4 implementation status: `COMPLETE`

The three deferred P2 findings below remain DEFERRED / NON-BLOCKING. M5 design assigns
M4-IIR-3 to M5 corpus work and defers M4-IIR-1/M4-IIR-2 to maintenance; none are closed here.

### Deferred P2 findings

| ID | Area | Finding | Status |
|---|---|---|---|
| M4-IIR-1 | `tests/cmake/check_m4_lock.cmake` | Lock-drift test proves required Contracts and Protobuf identities but does not assert the entire allowable dependency recipe set | DEFERRED / NON-BLOCKING |
| M4-IIR-2 | shared duplicate-symbol audit | Shared-mode audit checks consumer and Contracts shared library but does not separately `nm` the installed ProtoAdapter library | DEFERRED / NON-BLOCKING |
| M4-IIR-3 | M4 fuzz corpus / quality-input coverage | Initial corpus seeds are simple labels; fuzz bytes do not currently drive quality flags / HostQualityFact inputs | DEFERRED / NON-BLOCKING |

These P2 items do not block M4 merge. They may be addressed later as test hardening,
maintenance, M5 preparation, or future quality follow-up.

### Goal

Define an optional Protobuf adapter boundary that converts pinned Contracts wire messages into
owning M1/M2/M3 inputs and converts `BookProjection` plus explicit Host context into deterministic
`LocalOrderBookSnapshot` messages without introducing Protobuf into Core.

### Design deliverables

- Separate optional adapter target and one-way dependency direction into Core.
- Audited Contracts C++ artifact acquisition with separate schema fingerprint and package
  revision/version identities.
- Explicit inbound `ExchangeDepthSnapshot` and `DepthUpdate` conversion.
- Owning wrappers bound to `NumericSpec` and sequence policy that expose checked M3 invocation and
  keep span-based views private.
- Typed, deterministic validation and projection-binding errors.
- Explicit Host-owned identity, producer, time, depth-limit, gap, and quality context.
- State-specific `LocalOrderBookSnapshot` eligibility and visibility rules.
- Deterministic `GapInfo` to `GapDescriptor` mapping and historical-gap policy.
- Separate Host/Core quality domains plus owning inbound wire-quality sidecars.
- Compatibility, packaging/install, downstream-consumer, test, property, and fuzz plans.
- Accepted ADR-0006 for the separate Core/Protobuf adapter boundary.

### Design status

External architecture review Round 2: **APPROVED**. Round 1 findings are **3 CLOSED** and
architecture blocking findings are **0**.

The merged C-M4-001 prerequisite at `67ee1bf69fad980d114cfa278c3a6ffe310a4d7a`
provides `binance-market-data-contracts-cpp/0.1.0` and
`BinanceMarketDataContracts::Protobuf`. Projection pins Conan RREV
`7fd3efe3d289462fb16c78ffeced1682`, validates the approved schema/runtime metadata at configure
time, and implements the optional `BinanceMarketDataProjection::ProtoAdapter` component without
changing Core. The formal Contracts Package Revision remains `NOT_FORMALLY_ASSIGNED` until release.

### Non-goals

No Contracts change, gRPC runtime, Gateway lifecycle, network ownership, system-time read,
generated-code copy, or M5/M6 work is included. `MarketStateSnapshot` generation and derived market
calculations remain deferred unless separately designed and approved.

## M5 Differential Validation and Performance — APPROVED / MERGED / IN PROGRESS

Implementation status: **IN PROGRESS**

### Design status

The M5 architecture is designed in
`docs/M5_DIFFERENTIAL_VALIDATION_AND_PERFORMANCE_DESIGN.md` and ADR-0007 is **ACCEPTED**.
The design covers layered differential validation with operation-result observation
(OperationObservation model), canonical replay fixtures with explicit canonical text format
rules, determinism and cross-compiler semantic manifests with artifact fan-in transport,
replay/differential fuzzing, benchmark methodology, allocation/memory characterization, and the
benchmark-only container spike with explicit decision criteria.

Independent architecture review: **APPROVED** (focused re-review of corrected head
`9fff05ca8333d89d28d89c794d65255b56578715`; reviewed CI `31245814229` — PASS).
Findings M5-AR-001 through M5-AR-010: **CLOSED**. P0: **0**. P1 Design: **0**.
No M5 production change or container migration is authorized by the design.

### Pre-implementation decisions

OD-M5-001 and OD-M5-002 are **CLOSED**. Implementation authorization decisions and CI policy
are recorded in `docs/M5_PREIMPLEMENTATION_DECISIONS.md`. OD-M5-003 remains SPIKE-RESOLVABLE.
M5 implementation is authorized. Phases 1, 2, 3, 4, 5, and 6 are complete and merged; Phase 5
is COMPLETE / MERGED (evidence in the Phase-5 record below).
Phase 6: decision record APPROVED / MERGED; implementation COMPLETE / MERGED (evidence in the
Phase-6 record below; see `docs/M5_PHASE6_REPRESENTATIVE_BENCHMARKS.md`).

Phase 1 canonical replay infrastructure is **COMPLETE / MERGED** (PR #11, merge
`5e8629a7ff825f8ea941304d9b09be1670643e8a`, post-merge main CI `31264500905` — PASS 16/16). It
covers canonical replay grammar and byte/UTF-8 validation, manifest and exact-log SHA-256
identity, normalized operation loading, deterministic tiny fixtures, structured parser
diagnostics, and the explicit offline Recorder materializer/bootstrap contract.

Phase 2 (independent differential oracle: R1 ReferenceDecimal promoted, R4 ReferenceAdapter,
neutral ReplayDriver, OperationObservation, semantic checkpoints, first-divergence diagnostics
with layer attribution) is **COMPLETE / MERGED** (PR #12, merge
`75c619dd683ff2a3893f9535e206231e7bfecc41`, final review APPROVED, P0: 0, P1: 0, post-merge main
CI `31315421548` — PASS 16/16). M5-P2-IR-001 through M5-P2-IR-007 and M5-P2-RR-001 are CLOSED;
M5-P2-IR-008 and M5-P2-IR-009 remain DEFERRED / NON-BLOCKING.

Phase 3 is **COMPLETE / MERGED** via PR #13 (final approved Head
`a8e4ccfc31efd4e67bc10cf0ac9ad2a99faa8354`; exact-head CI `31491615547` — PASS 16/16; P0: 0;
P1: 0; squash merge `473a907eba2001d18926c57d6c8d16b10c7505be`), rebased onto the accepted M3
Spot successor-coverage semantics (ADR-0008). Deterministic 2,048-event
small-tier Spot and USD-M workloads, per-event scaled differential comparison, bounded observation
retention, stable first-divergence diagnostics, the independent offline Recorder Raw-v1
materializer, and validator-enforced medium lifecycle validity are implemented. The mandatory
Spot 100k corpus (`M5-REC-SPOT-BTCUSDT-V1`) is validated PASS (exact-next bridge
`U=98288147168 u=98288147175` against `L=98288147167` under ADR-0008; 100,001 Applied, final
Synchronized, zero gaps). The mandatory
USD-M 100k corpus (`M5-REC-USDM-BTCUSDT-V1`) is validated PASS (bridge Applied / Synchronized,
100,001 Applied, final Synchronized, zero gaps). The Phase-3 corrections (M5-P3-IR-001 through
M5-P3-IR-005) are recorded; M5-P3-IR-001's historical contains-`L` restoration was itself
superseded by ADR-0008 acceptance. Residual M5-P3-RR2-002 remains PARTIALLY CLOSED / ACCEPTED
NON-BLOCKING P2. See `docs/M5_PHASE3_DETERMINISTIC_REPLAY.md`.
Phase 4 is **COMPLETE / MERGED** via PR #16 (final approved Head
`b612f85d281315346c0ccf6f599d51af538e3cf4`; exact-head CI `31571166506` — PASS 18/18; P0: 0;
P1: 0; squash merge `7b6d9ef3b222675138fdd34f3fed381216fe9d02`; post-merge main CI
`31576511096` — PASS 18/18): canonical OperationObservation serialization (schema v1),
semantic SHA-256 digests, portable manifest v1, manifest producer, fail-closed shared
Python evidence validation, metadata-validated three-cross-compiler artifact fan-in, full-evidence
`m5-replay` Debug/Release determinism, and exact-Head `m5-semantic-compare` blocking job. The seven
P1 findings from rejected Head `bf2239206ff74e11e3ce73de73f28465b033f808` were corrected and
independently re-reviewed; post-merge GNU/Clang/AppleClang Release manifests agree on the
fixture-set identity, the four authoritative workloads, fixture hashes, and semantic digests. See
`docs/M5_PHASE4_CROSS_COMPILER_SEMANTIC_MANIFESTS.md`.
No production-code change.

Phase 5 (differential replay fuzzing) is **COMPLETE / MERGED** via PR #18 (final approved Head
`e56f5dbd12b9e66946343467221e8e3ba9984531`; exact-head CI `31668465623` — PASS 18/18; final
focused independent review APPROVED, P0: 0, P1: 0, P2: 0; squash merge
`53268d5cd2090f4779ffdc14c070184f470cc899`; post-merge main CI `31671708958` — PASS 18/18):
direct structured byte decoder, bounded deterministic
`LLVMFuzzerTestOneInput`, production/reference ReplayDriver comparison, Core and Adapter mode
coverage, Spot and USD-M market coverage, libFuzzer abort on first divergence, 10 mandatory
checked-in seed categories with structural validation, fifth fuzz-smoke target (10,000-run
CI smoke), and shared `cmake/M5Support.cmake` for single-source replay/oracle/reference
reuse. Existing four fuzzers preserved; production `src/`/`include/` unchanged;
the corrected ten-seed corpus proves exact structural intent, including complete recovery,
duplicate plus stale ranges, exact decimal token categories, and AdapterEnabled depth/quality
paths. Snapshot comparison observes actual venue, market, and schema through explicit observation
V2 / manifest V2 evolution while retaining historical V1 bytes. Reused M5 support translation
units receive fuzz-only `fuzzer-no-link` + ASan + UBSan instrumentation verified from compile
commands. Snapshot source/current-gap wire enum extraction fails closed rather than legalizing
unspecified or unknown values, and manifest rendering accepts only V1/V1 or V2/V2 while preserving
the exact historical V1 JSON shape. M4-IIR-3 is CLOSED by final independent Phase-5 review evidence.
Post-merge GNU/Clang/AppleClang Release manifests record `head_sha`
`53268d5cd2090f4779ffdc14c070184f470cc899` and agree on all four authoritative semantic digests.
`docs/M5_PHASE5_DIFFERENTIAL_FUZZING.md`.

Phase 6 (representative benchmarks) is **COMPLETE / MERGED** via PR #21 (final independently
accepted implementation Head `9776ba6b93990c44e550f289b69127ca721b0d00`; accepted exact-head CI
`31803322848` — PASS 18/18; final independent review APPROVED, P0: 0, P1: 0, P2: 1; formal
exact-head Phase-6 evidence ACCEPTED; squash merge `227524e6d17cce77813c6f26cd65bb8d996f5677`;
post-merge main CI `31809917018` — PASS 18/18). The accepted Head and the squash merge commit
share the same repository tree (`79aa0151d65d42a1c63e6649bf24bf9053105667`). Delivered:
normative M1 representative benchmarks; M2 stateful/query workloads; the complete 48-cell M3
accepted live-apply matrix plus classification and component/proxy measurements; M4
adaptation/checked/snapshot/serialization benchmarks; CoreNormalizedReplay and AdapterWireReplay
(Spot + USD-M); the production event-latency sampler; `M5_BENCHMARK_WORKLOAD_SPEC_V1`,
`M5_BENCHMARK_WRAPPER_V1`, and `M5_REPLAY_LATENCY_V1`; fail-closed
inventory/smoke/wrapper/latency validation; the formal/manual benchmark-full evidence workflow;
the benchmark-smoke CI structural gate; and truthful source/binary/dependency/workload/result
provenance. Final provenance corrections closed the two P1s: P6-FINAL-001 (CheckedApply
canonical workload identity explicitly records the locked Spot successor `initial_update_id=
1000001`, `first/final_update_id=1000002`, `previous_final_update_id=not_applicable`, bound to
the actual timed workload with fail-closed regression/validation) and P6-FINAL-002
(`contracts_package_id` records the real Conan binary package ID derived from the exact consumed
package's conaninfo.txt rather than the cache-directory locator). Production `src/**` and public
`include/**` were unchanged by Phase 6. See
`docs/M5_PHASE6_REPRESENTATIVE_BENCHMARKS.md`.

Phase 7 (allocation/memory characterization) is **COMPLETE / EVIDENCE ACCEPTED**.
Its pre-implementation decision record is **APPROVED / MERGED** via PR #25:
`docs/M5_PHASE7_PREIMPLEMENTATION_DECISIONS.md` records decisions OD-M5-P7-001
through OD-M5-P7-023 (attribution model, instrumentation isolation with a two-lifetime
tracking/measurement model, the complete corrected C++ replaceable operator surface with
checked aligned backing-size arithmetic, raw requested allocation metrics over the
`cxx_replaceable_global_new` boundary, the A/P/B live model with
`persistent_live_delta`/`peak_above_entry`/`transient_excess_over_persistent` normalized
metrics and owning-output post-destroy lifecycles, footprint semantics with non-additive
node and estimated allocator models, baseline-snapshot discipline, M2/M3/M4/replay
inventories with exact-rational replay per-event values, warmup, determinism on normalized
metrics, machine-readable evidence schemas `M5_ALLOCATION_WRAPPER_V1` /
`M5_PHASE7_ALLOCATION_RECORD_V1` / `M5_PHASE7_FOOTPRINT_RECORD_V1` with contract
`M5_PHASE7_MEASUREMENT_CONTRACT_V1`, formal evidence eligibility with the fixed-purpose
Phase-7 canonical Release runner, toolchain interpretation, overflow/failure behavior, a
30-case adversarial validation suite, Phase-8 handoff, and the Phase-10 CI boundary). The
final independent methodology review was **APPROVED** (P0: 0, P1: 0, P2: 0); all findings
M5-P7-MR-001..010 and M5-P7-RR-001 are **CLOSED**; approved Head
`92288927165f2d7486491371a11a7a586c645565`; squash merge
`c2ad198677130d05ad054ea48ade3a1d8021c153` (merged tree == approved Head tree).
Phase-7 implementation and formal evidence acceptance are complete:
- PR-A (work package WP1, PR #27, MERGED at squash merge
  `4d6d5b9b0100f91e6cd3cf1bf9388cb1095eba63`): the executable-local allocation instrumentation
  substrate (constant-initialized process-lifetime state, fixed-capacity
  non-allocating provenance table, complete 20-form replaceable global
  new/delete surface with checked aligned backing arithmetic,
  MeasurementScope A/P/B semantics, fail-closed overflow/failure
  behavior, test-only seams) and its adversarial validation executable
  `bmd_projection_allocation_instrumentation_tests`.
- PR-B (work packages WP2–WP4, PR #28, MERGED at squash merge
  `c15bfa6410d68d5c7898e7639601c267b82bfeba`; post-merge main CI
  `31987557259` — 19/19 PASS): the measurement/evidence machinery — the
  Phase-7 evidence schemas (`M5_ALLOCATION_WRAPPER_V1`,
  `M5_PHASE7_MEASUREMENT_CONTRACT_V1`, `M5_PHASE7_ALLOCATION_RECORD_V1`,
  `M5_PHASE7_FOOTPRINT_RECORD_V1`), the independent fail-closed Python
  validator, shared Phase-6 workload identities with bit-for-bit golden
  regression, the M2/M3/M4/replay allocation characterization
  executables, the persistent footprint experiment, and the EXPLORATORY
  local driver `scripts/benchmark-allocation.sh`.
- PR-C (work package WP5): the canonical formal Release runner
  `scripts/benchmark-allocation-formal.sh` — the sole public host command
  for formal Phase-7 evidence, with the INFRA-TC-001-style fail-closed
  host/internal trust boundary, the single formal source model (read-only
  mounted `/src` with `.git`, fresh ephemeral `/work`; no copied source),
  Release/sanitizers-off/explicit-LTO-off configuration, exact
  binary-SHA-256 binding, three process invocations per measurement
  family with cross-invocation normalized-metric determinism, formal
  evidence class validated without `--allow-exploratory`, and
  deterministic trust-boundary tests
  (`scripts/test-benchmark-allocation-formal.sh`). The formal Release runner and its
  trust-boundary tests were independently accepted.
- WP6 (formal evidence acceptance): formal Phase-7 evidence generated
  fresh from merged main `bb1d62ff…` and independently accepted — Phase-7
  is COMPLETE / EVIDENCE ACCEPTED.
- Phase 8 (container spike) is **COMPLETE / EVIDENCE ACCEPTED** — PR-A / WP1 is accepted and
  merged in PR #30; PR-B / WP2 was independently accepted and squash-merged at the Phase-8
  accepted baseline `b06fa2f5716527cc5fda3e102ba358721336246c` (approved Head
  `f1bbe499f7179094cfefae796f454951e1736add`; post-merge CI `32203582370`, 19/19 PASS). The
  benchmark-only same-interface container-model substrate
  (Phase8StdMapControl, Phase8SortedVectorNaive, Phase8AbslBtreeMap,
  Phase8SortedVectorBatchLww) behind one static-polymorphism model
  protocol with a semantic conformance gate (ReferenceOrderBook and
  production core::OrderBook differential checks, replace_all strong
  exception guarantee, deterministic property sequences) and Abseil as an
  explicit TEST/BENCHMARK-only requirement. WP2 adds repeated timing,
  allocation, persistent-footprint, noise-floor, provenance, and fail-closed
  comparison evidence. Formal evidence was independently accepted; Phase 9 owns the decision
  record and no production migration is authorized. See
  `docs/M5_PHASE8_CONTAINER_BENCHMARK_EVIDENCE.md`.

### Phase-8 formal evidence acceptance

The formal cohort was collected from source SHA
`b06fa2f5716527cc5fda3e102ba358721336246c` and source tree
`7a8e924879305b3ddba34304573c0ed81ac962bb`. The authoritative archive is
`phase8-formal-evidence-b06fa2f.tar.gz` with SHA-256
`2c8dd51d1e91ecdd936d32b95436c93f199f60acce24361e9ae8533c7ad48657`; the evidence binary SHA-256
is `6ac0361ee8d92e782532a3df0373d660f8f9192931b1d843a20d91d1077114c1`.

Independent verdict: **FORMAL_EVIDENCE_ACCEPTED**. P0=0, P1=0, P2=1, INFO=2. Archive integrity
passed; all 62 retained SHA-256 entries passed; provenance, binary, payload, and dependency
binding passed. The cohort comprised three independent process campaigns, seven repetitions, ten
workloads, four candidates, forty records per campaign, no filtering, semantic digest consistency
PASS, and ACCEPTABLE cross-campaign cohort homogeneity. RSS was not measured; allocation and
persistent requested-byte evidence was retained.

The controlled comparison host was Ubuntu 22.04.5 LTS, Linux x86_64, Intel Core i5-11320H, with
measurement pinned to logical CPU 3. This is the controlled Phase-8 comparison host, not a
universally representative hardware claim.

Retained finding `P8-EVIDENCE-P2-001` is P2 / NONBLOCKING: the `std::map` control for
`M5_PHASE8/mixed_updates/1000` has materially high within-campaign timing noise. Phase 9 must
apply the frozen significance and empirical-noise requirements before this cell contributes to a
migration decision. The finding does not invalidate the Phase-8 cohort and does not require a new
campaign solely to remove it. INFO limitations were retained concisely: no direct
`scaling_driver`/`intel_pstate/status` string, and no exclusive isolation of the physical core/SMT
sibling from every normal host process; neither was blocking.

The durable governance result is: PHASE7=COMPLETE / EVIDENCE ACCEPTED;
PHASE8=COMPLETE / EVIDENCE ACCEPTED; PHASE9=COMPLETE / DECISION ACCEPTED;
CONTAINER_DECISION=KEEP_STD_MAP; CANDIDATE_WINNER=NONE; PRODUCTION_CONTAINER=std::map;
PRODUCTION_MIGRATION=NO; MIGRATION_IMPLEMENTED=NO;
PHASE10=COMPLETE / WP-A COMPLETE / WP-B COMPLETE /
WEEKLY_M5_PERFORMANCE=ACTIVE / ACTIVATION_PROOF=ACCEPTED. The decision record
is `docs/M5_PHASE9_CONTAINER_DECISION.md`.

### M3 Spot successor-coverage correction (2026-08-10)

The M3 Spot bootstrap successor-coverage correction (PR #14, ADR-0008) is **MERGED**.
Independent focused re-review of implementation head
`5195a5cf639989ef073d908dfbf5ec5be1e3cc40` was **APPROVED** (P0: 0, P1: 0, blocking findings:
0; findings M3-SC-RR-001 through M3-SC-RR-005: **CLOSED**); exact-head CI run `31446514958`
passed 16/16. ADR-0008 is **ACCEPTED**, superseding only the Spot-bootstrap contains-`L`
portion of ADR-0005; USD-M semantics are unchanged. PR #14 merged at main
`8bc71f2ae457cf3d15a9dcb4ea659a9c3f85a569` (squash merge, 2026-08-11).

The M5 Phase-3 continuation (PR #13) was rebased onto the corrected main and the mandatory
Spot corpus gate was re-evaluated: under the accepted successor-coverage rule the pinned
in-window Spot archive's exact-next candidate (`U = L + 1`) is a valid bootstrap bridge, and the
Spot 100k corpus is validated PASS. At that time PR #13 remained OPEN / DRAFT pending independent
review; it has since been independently approved and merged (see Phase 3 acceptance above). This
branch changed no M5 Phase-3 production materializer dependency: the correction is test/tool-only
on top of the accepted main semantics.

### Deferred M4 P2 findings

The three deferred M4 P2 findings remain open and non-blocking. M5 design dispositions:
M4-IIR-1 and M4-IIR-2 are deferred to maintenance; M4-IIR-3 is incorporated into M5 corpus work.
These are design decisions only and close nothing.

### Non-goals

M5 does not implement live networking, Gateway runtime, gRPC, queues, backpressure, reconnect,
strategy, risk, trading, persistence, Python binding, or production container migration.

## Development map

1. **M0 Repository Foundation** — Build, test, installation, CI, and governance baseline.
2. **M1 Numeric and Domain Primitives — COMPLETE** — Fixed-point
   types, quantity/price representation, decimal parsing, exact formatting, property tests, and fuzz
   validation.
3. **M2 Order Book Core — COMPLETE** — Deterministic book storage, level management, depth operations.
4. **M3 Sequence and Projection State — COMPLETE** —
   Market-specific sequencing, bootstrap validation, gap detection, reset logic, and synchronized
   projection lifecycle.
5. **M4 Snapshots and Protobuf Boundary — COMPLETE** — Optional
   wire-format adapter outside Core; snapshot production boundary. Merged through PR #8 at
   `ac780d9eb7b49ff20a6b3b4bee6a993b51b70af4`; main CI `31242162782` — 16/16 PASS.
6. **M5 Differential Validation and Performance — APPROVED / MERGED / IN PROGRESS** —
   Replay/differential/fuzz validation; benchmark with representative workloads.
7. **M6 Gateway Integration** — Production host embedding surface; live ingestion.
8. **M7 Platform Hardening and Acceptance** — Harden architectures, toolchains; end-to-end acceptance.

### Cross-cutting concerns

- **Determinism, property tests, and fuzz** are required from M1 onward, not deferred wholesale.
- **Python Binding** is an optional History Track; not a required milestone deliverable.
- **Container choice** is a performance spike within M5; not a standalone milestone. Migration
  requires evidence, an explicit decision record/ADR, full regression, and independent review.
- Detailed API design belongs in per-milestone documents written at the start of each phase.

## Core architecture principles

- C++20.
- Deterministic — all inputs explicit; no system clock, random, or ambient state.
- Single Writer — host preserves ordering per projection instance.
- Core/Wire Separation — protobuf messages do not enter Core.
- Live/Replay Same Core — identical ordered inputs produce identical outputs.
- Strategy Independent — no trading, risk, or strategy logic.

## Phase-10 implementation state

P10-PRE-P1-001 is **CLOSED**. The project owner authorized public
distribution of exactly the two accepted materialized Replay_V1 medium
fixtures, and the immutable versioned Release asset was published:

```text
PHASE9=COMPLETE / DECISION ACCEPTED
M5=IN PROGRESS
PHASE10=COMPLETE
PHASE10_WP_A=COMPLETE
PHASE10_WP_B=COMPLETE
P10_PRE_P1_001=CLOSED
P10_WPB_P1_001=CLOSED
P10_WPB_ACT_P1_001=CLOSED
P10_WPB_ACT_P2_001=CLOSED
MEDIUM_CORPUS_DISTRIBUTION=ESTABLISHED
MEDIUM_CORPUS_RELEASE_TARGET_SHA=041a9e8516409552ea9d6eeada5d58d378c40fc7
RELEASE_TAG=m5-medium-corpus-v1
ASSET_NAME=m5-medium-recorded-v1.tar.gz
RAW_SOURCE_DISTRIBUTION=NO
WEEKLY_M5_PERFORMANCE=ACTIVE
WEEKLY_M5_PERFORMANCE_PURPOSE=EXPLORATORY PERFORMANCE CANARY
WEEKLY_REPETITIONS_PER_FIXTURE=3
WEEKLY_SPOT_REPETITIONS=3
WEEKLY_USDM_REPETITIONS=3
WEEKLY_NUMERIC_GATE=NO
WEEKLY_CONTAINER_DECISION_AUTHORITY=NO
FORMAL_DECISION_EVIDENCE_MIN_REPETITIONS=>=5
M5_PERFORMANCE_WORKFLOW_CREATED=YES AT THIS TREE
WORKFLOW=.github/workflows/m5-performance.yml
SCHEDULE_CRON=17 3 * * 1
SCHEDULE_TIMEZONE=UTC
TIER=recorded medium v1
FIXTURES=M5-REC-SPOT-BTCUSDT-V1 + M5-REC-USDM-BTCUSDT-V1
MODE=Core current-production std::map
RUNNER=ubuntu-24.04 / GitHub-hosted x86_64
COMPILER=Clang
TIMEOUT=45 minutes
ACTIVATION_PROOF=ACCEPTED
PR_BLOCKING=NO
EVIDENCE_CLASS=EXPLORATORY / NONBLOCKING REPORTING
RETENTION=7 days
CONTAINER_DECISION=KEEP_STD_MAP
CONTAINER_REDECISION=NO
PRODUCTION_CONTAINER=std::map
PRODUCTION_MIGRATION=NO
ACTIVATION_RUN_ID=32453032145
ACTIVATION_HEAD_SHA=e6d2cf099ae4d9c5dc7c77789c3b4b55336af672
ACTIVATION_ARTIFACT_ID=9436787118
ACTIVATION_ARTIFACT_NAME=m5-performance-ubuntu-clang-e6d2cf09
ACTIVATION_ARTIFACT_DIGEST=sha256:f2404a068310c3ba39737e7cc6d8fa0107b83ed71e4539840f000210d7cb1d2f
PHASE10_COMPLETE=YES
READY_FOR_PHASE11=YES
PHASE11_STARTED=NO
NEXT_M5_PHASE=PHASE11 / INDEPENDENT IMPLEMENTATION REVIEW
M6_GATEWAY_INTEGRATION=NOT STARTED
```

WP-A provides benchmark-smoke artifact reporting: exact evidence-SHA checkout,
wrapper provenance binding, and the four required smoke JSON outputs in a
three-day SHA-named artifact. It adds no CI job, changes no benchmark
methodology, and adds no numeric performance gate.

The asset contains only `distribution-manifest.json` and the three accepted
payload files for each of `M5-REC-SPOT-BTCUSDT-V1` and
`M5-REC-USDM-BTCUSDT-V1`. It excludes the Raw source and does not change
software licensing. The publication and clean anonymous acquisition evidence,
source authority, exact fixture identities, and hashes are recorded in
`docs/M5_PHASE10_PREIMPLEMENTATION_DECISIONS.md`.

The merged WP-B implementation creates the active weekly operation below. The
weekly exploratory canary no longer repeats the dedicated Spot/USD-M
Core/reference differential/lifecycle validator. The validator implementation
remains, and distribution verification, benchmark preflight, explicit warmup,
three timed repetitions per fixture, timed checksums, serial execution, and the
formal >=5 repetition methodology remain unchanged.

```text
REQUIRED_PHASE10_WEEKLY_OPERATION=.github/workflows/m5-performance.yml
SCHEDULE_SEMANTICS=weekly
TIER=recorded medium v1
FIXTURES=M5-REC-SPOT-BTCUSDT-V1 + M5-REC-USDM-BTCUSDT-V1
MODE=Core current-production std::map
RUNNER=standard GitHub-hosted Ubuntu x86_64
TIMEOUT=45 minutes
PURPOSE=EXPLORATORY PERFORMANCE CANARY
REPETITIONS_PER_ACCEPTED_MEDIUM_FIXTURE=3
FORMAL_DECISION_EVIDENCE_MIN_REPETITIONS=>=5
PR_BLOCKING=NO
EVIDENCE_CLASS=EXPLORATORY / NONBLOCKING REPORTING
RETENTION=7 days
CONTAINER_REDECISION=NO
PRODUCTION_MIGRATION=NO
STATUS=ACTIVE
```

Normal exact-main CI passed, and activation run `32453032145` on the exact
main SHA produced the exact five-file artifact that was independently accepted.
Phase 10 is complete and M5 is ready for Phase 11 independent implementation
review. M5 remains in progress until Phase 11 is completed and accepted.
`READY_FOR_PHASE11=YES` and `PHASE11_STARTED=NO`; full immutable activation
evidence is recorded in `docs/M5_PHASE10_CI_REPORTING_INTEGRATION.md`.
