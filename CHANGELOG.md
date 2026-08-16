# Changelog

All notable changes will be recorded here.

## [Unreleased]

### Added

- M5 Phase-7 pre-implementation decision record (docs-only, NOT implementation):
  `docs/M5_PHASE7_PREIMPLEMENTATION_DECISIONS.md` records decisions OD-M5-P7-001 through
  OD-M5-P7-022 on the immutable base `eed3de99efaba8eaa96083a5348d538ed44f6bfe`:
  attribution model (persistent storage vs per-operation vs M3 transaction vs M4 output vs
  replay vs harness overhead), dedicated single-threaded instrumentation executables reusing
  the allocation-failure isolation pattern, the complete allocation operator surface
  (plain/array/nothrow/sized/aligned new and delete), exact `allocation_count` /
  `total_allocated_bytes` / deallocation / live-bytes / peak-live semantics with fail-closed
  eligibility (never estimated), footprint experiment at 100/1,000/5,000/10,000 levels per
  side with MEASURED vs MODELED vs RSS separation, exact baseline-subtraction formula, M2/M3
  (full 48-cell accepted-apply matrix)/M4/replay allocation inventories reusing Phase-6
  workload identities, Component/Proxy diagnostics preserved as approximate (never an exact
  BookProjection decomposition), warmup/determinism rules, machine-readable schemas
  `M5_ALLOCATION_WRAPPER_V1` / `M5_PHASE7_ALLOCATION_RECORD_V1` /
  `M5_PHASE7_FOOTPRINT_RECORD_V1` (contract `M5_PHASE7_MEASUREMENT_CONTRACT_V1`, existing
  Phase-6 schemas unchanged), formal evidence eligibility anchored to the INFRA-TC-001
  canonical environment, overflow/failure fail-closed behavior, adversarial
  instrumentation-validation requirements, explicit Phase-8 handoff, and the Phase-10 CI
  boundary. Status: PROPOSED / PENDING INDEPENDENT METHODOLOGY REVIEW; Phase-7 implementation
  authorization is NO until the record is independently reviewed and merged without material
  unreviewed decision changes. No production/test/benchmark/script/CMake/workflow changes.

- M5 Phase-6 representative benchmark infrastructure (**COMPLETE / MERGED** via PR #21):
  M1 normative parse/format cases, M2 apply_level/apply_updates/replace_all/query families,
  the full 48-cell M3 accepted live-apply matrix plus classification and component/proxy cells,
  M4 adaptation/install/apply/snapshot/serialization boundaries (fail-closed inventory),
  production CoreNormalizedReplay and AdapterWireReplay wall-time throughput,
  a dedicated production-only event-latency executable with nearest-rank-v1 statistics and
  calibration, `M5_BENCHMARK_WORKLOAD_SPEC_V1` workload identity, the
  `M5_BENCHMARK_WRAPPER_V1` metadata/provenance wrapper with configure-time dirty capture and
  exact binary/payload SHA binding, `M5_REPLAY_LATENCY_V1` latency evidence, deterministic
  fail-closed inventory/smoke/wrapper/latency validators with Python and C++ tests,
  `scripts/benchmark-smoke.sh` / `scripts/benchmark-full.sh`, and the extended benchmark-smoke
  CI job (ProtoAdapter ON, current 45-minute timeout, structural evidence only, no numeric
  threshold). OD-M5-P6-024 retains its 15-minute recommendation; the workflow uses 45 minutes to
  allow cold-run margin for the required pinned Contracts bootstrap before benchmark execution.
  Production `src/`/`include/` unchanged. Final provenance corrections closed two P1s:
  P6-FINAL-001 (CheckedApply canonical workload identity explicitly records the locked Spot
  successor `initial_update_id=1000001`, `first/final_update_id=1000002`,
  `previous_final_update_id=not_applicable`, bound to the actual timed workload with fail-closed
  regression/validation) and P6-FINAL-002 (`contracts_package_id` records the real Conan binary
  package ID derived from the exact consumed package's conaninfo.txt rather than the cache-
  directory locator). Accepted implementation Head
  `9776ba6b93990c44e550f289b69127ca721b0d00`; accepted exact-head CI `31803322848` — 18/18 PASS;
  independent final review APPROVED (P0 = 0, P1 = 0, P2 = 1); formal exact-head Phase-6 evidence
  ACCEPTED; squash merge `227524e6d17cce77813c6f26cd65bb8d996f5677`; post-merge main CI
  `31809917018` — 18/18 PASS.
  (`docs/M5_PHASE6_REPRESENTATIVE_BENCHMARKS.md`).
- M5 Phase-5 differential replay fuzzer (`bmd_projection_replay_fuzz`): direct structured byte
  decoder (no canonical text parser in fuzz semantic path), bounded deterministic
  `LLVMFuzzerTestOneInput`, production/reference ReplayDriver comparison with libFuzzer abort
  on first divergence, Core and Adapter mode coverage, Spot and USD-M market coverage, all
  seven operation variants reachable, 10 mandatory checked-in seed categories with structural
  validator, shared `cmake/M5Support.cmake` for single-source replay/oracle/reference reuse,
  fifth fuzz-smoke target (10,000-run CI smoke). Production `src/`/`include/` unchanged.
  Existing four fuzzers preserved. Phase 5 is COMPLETE / MERGED via PR #18 (final approved
  Head `e56f5dbd12b9e66946343467221e8e3ba9984531`; exact-head CI `31668465623` — 18/18 PASS;
  final focused independent review APPROVED, P0=0, P1=0, P2=0; squash merge
  `53268d5cd2090f4779ffdc14c070184f470cc899`; post-merge main CI `31671708958` — 18/18 PASS).
  M4-IIR-3: CLOSED.
  `docs/M5_PHASE5_DIFFERENTIAL_FUZZING.md`. Phase 6: PRE-IMPLEMENTATION DECISIONS RECORDED /
  IMPLEMENTATION NOT STARTED (`docs/M5_PHASE6_PREIMPLEMENTATION_DECISIONS.md`).

### Changed

- Repaired the Phase-6 measurement contract after focused review: total logical-item reporting,
  explicit discarded warmup passes, prepared CandidateApplyUpdates and M4 checked-operation
  storage, adapter-path rebaseline, pre-parsed typed event latency, complete generated-workload
  identity, fail-closed unknown Git provenance, and the formal five-repetition minimum are now
  enforced with deterministic regression coverage. Production `src/`/`include/` remain unchanged.

- Corrected the M5 Phase-5 candidate's final validation blockers: snapshot observations now cover
  actual venue, market, and schema version via explicit observation/manifest V2 schemas while V1
  bytes remain frozen; all ten mandatory replay seeds carry substantive decoded structure with
  AdapterEnabled depth/quality outcome tests; and reused M5 differential-fuzz support sources are
  fuzz-instrumented with compile-database enforcement. Also tightened shared event-level and
  quality-fact bounds and preserved inverted-update byte framing. Production `src/`/`include/`
  remain unchanged. A final independent review closed M4-IIR-3 and requested two focused P1
  corrections; the corrections were applied and Phase 5 subsequently passed final focused
  independent re-review and was merged (see the Added entry above).

- Corrected the two remaining Phase-5 P1 findings without production changes: actual snapshot
  source/current-gap wire enums now fail closed at the test observation boundary, including
  unspecified and unknown numeric values; semantic manifest rendering accepts only V1/V1 and
  V2/V2, preserves the exact historical V1 JSON shape without `observation_schema_version`, and
  rejects mixed/unknown pairings. The current producer and comparator remain V2/V2.

- Corrected the M3 Spot bootstrap predicate to successor coverage (`U <= L + 1`, overflow-guarded)
  per the official Binance 2025-11-12 correction of the Spot local-order-book instructions.
  Exact-next bootstrap input is now a valid bridge; a true forward gap is `U > L + 1`.
  ADR-0008 (`ACCEPTED`) supersedes the Spot-bootstrap portion of
  ADR-0005; USD-M semantics are unchanged. Production classifier, independent reference model,
  unit/property/fuzz coverage, M5 replay classification, and semantic documentation were aligned.

- M3 Spot successor-coverage correction acceptance recorded: ADR-0008 is **ACCEPTED**.
  Independent focused re-review **APPROVED** (reviewed implementation head
  `5195a5cf639989ef073d908dfbf5ec5be1e3cc40`; exact-head CI `31446514958` — PASS 16/16;
  P0: 0; P1: 0; blocking findings: 0; M3-SC-RR-001 through M3-SC-RR-005: **CLOSED**).
  Acceptance supersedes only the Spot-bootstrap contains-`L` portion of ADR-0005; USD-M semantics
  unchanged; Binance Host snapshot/buffer orchestration remains outside M3 Core. PR #14 merged at
  main `8bc71f2ae457cf3d15a9dcb4ea659a9c3f85a569`.

- M5 Phase-3 rebased onto the accepted M3 successor-coverage main and re-validated: the Spot
  materializer now bridges the pinned authoritative source's exact-next candidate
  (`L=98288147167`, bridge `U=98288147168 u=98288147175`) under ADR-0008, and both mandatory
  100k corpora are validated PASS (Spot `M5-REC-SPOT-BTCUSDT-V1` SHA-256
  `9e983123…2cc9d`; USD-M `M5-REC-USDM-BTCUSDT-V1` SHA-256 `d28ffe19…29afc`; 100,001 Applied,
  zero gaps, final Synchronized; Core and Adapter validators agree; deterministic repeat PASS).
  Direct Spot successor-conformance coverage (exact-next acceptance, true-gap rejection,
  `C=500` table) locks ADR-0008 semantics independently of production/reference equality;
  the ReplayDriver summary accumulators no longer mark allocation-capable paths `noexcept`; the
  minimal JSON uint64 parser uses a checked-before-multiply overflow guard. Phase 3 is
  COMPLETE / MERGED via PR #13 (merge `473a907eba2001d18926c57d6c8d16b10c7505be`, final approved
  Head `a8e4ccfc31efd4e67bc10cf0ac9ad2a99faa8354`, exact-head CI `31491615547` — PASS 16/16);
  Phase 4 had not started at the time of Phase-3 completion.

- M5 Phase 4 is COMPLETE / MERGED via PR #16 (final approved Head
  `b612f85d281315346c0ccf6f599d51af538e3cf4`; exact-head CI `31571166506` — PASS 18/18; P0: 0;
  P1: 0; squash merge `7b6d9ef3b222675138fdd34f3fed381216fe9d02`; post-merge main CI
  `31576511096` — PASS 18/18): canonical OperationObservation serialization
  (schema `M5_SEMANTIC_OBSERVATION_V1`), semantic SHA-256 digests (reuses existing test-only
  `replay::sha256_hex`), portable manifest v1 (JSON, schema `M5_SEMANTIC_MANIFEST_V1`),
  deterministic manifest producer (`bmd_projection_m5_semantic_manifest`), fail-closed Python
  comparator (`scripts/compare-m5-semantic-manifests.py`), three-cross-compiler Release
  artifact upload/fan-in, `m5-replay` Debug/Release determinism job, and `m5-semantic-compare`
  blocking comparison job. Four mandatory small workloads: Core Spot, Core USD-M, Adapter Spot,
  Adapter USD-M (reusing Phase-3 deterministic 2,048-event generator). Post-merge
  GNU/Clang/AppleClang Release manifests agree on fixture-set identity, the authoritative
  workload set, fixture hashes, and semantic digests. No production-code change (`src/`,
  `include/` unchanged). See
  `docs/M5_PHASE4_CROSS_COMPILER_SEMANTIC_MANIFESTS.md`. Phase 5 is NOT STARTED.
- Independent Phase-4 review findings from rejected Head `bf2239206ff74e11e3ce73de73f28465b033f808`
  (P0: 0, P1: 7, P2: 4) were corrected and cleared: canonical physical-line/final-LF ownership
  and byte-safe string encoding; runtime enum and compile-time variant fail-closed behavior;
  strict evidence SHA validation; exact-evidence comparator checkout; manifest-metadata toolchain
  roles; schema/structure and authoritative-workload validation on every manifest; full
  fixture/build identity in `m5-replay`; complete JSON control escaping; collision-free comparison
  reporting; producer-result hardening; and status-document synchronization. Focused independent
  re-review APPROVED (P0: 0, P1: 0, P2: 0); PR #16 merged at
  `7b6d9ef3b222675138fdd34f3fed381216fe9d02`.
  (squash merge). Final approved PR head: `a8e4ccfc31efd4e67bc10cf0ac9ad2a99faa8354`; exact-head
  CI `31491615547` — PASS 16/16; final independent review APPROVED (P0: 0, P1: 0; M5-P3-RR2-001
  CLOSED; M5-P3-RR2-002 PARTIALLY CLOSED / ACCEPTED NON-BLOCKING P2). Delivered at summary level:
  deterministic large replay validation (2,048-event small tier), Spot/USD-M authoritative medium
  validation, direct Spot successor-conformance guard (ADR-0008), medium lifecycle validation,
  ReplayDriver exception hardening, and uint64 validation hardening.

### Added

- M5 architecture approval recorded: independent M5 architecture review **APPROVED**
  (focused re-review of corrected head `9fff05ca8333d89d28d89c794d65255b56578715`,
  reviewed CI `31245814229` — PASS, 16/16). Findings M5-AR-001 through M5-AR-010:
  **CLOSED**. P0: **0**. P1 Design: **0**. ADR-0007: **ACCEPTED**. M5 design:
  **APPROVED / PENDING MERGE**. M5 implementation remains **NOT STARTED / NOT
  AUTHORIZED**, blocked by OD-M5-001 and OD-M5-002 (separate pre-implementation
  decision task required). M6 remains NOT STARTED.

- M5 differential validation and performance design: layered differential oracle
  (ADR-0007 ACCEPTED), canonical versioned replay
  event-log grammar with ADAPTER_METADATA/HOST_QUALITY quality-domain separation and
  adapter dimension scoping, canonical text format rules (UTF-8/LF/whitespace/token
  semantics), OperationObservation differential model (observable operation results
  plus semantic checkpoints), cross-job semantic manifest transport architecture
  with artifact fan-in and fail-closed behavior, dataset size tiers, determinism and
  cross-compiler semantic manifests, replay/differential fuzz with structured byte
  decoding, Google Benchmark methodology, allocation/memory instrumentation, and a
  benchmark-only container spike with batch-aware sorted-vector candidate distinction.
  Initial independent architecture review returned CHANGES REQUESTED (P0: 0,
  P1 design: 1, P1 implementation: 2, P2: 7). Corrected findings: M5-AR-001,
  M5-AR-002, M5-AR-003 (mandatory), M5-AR-004 through M5-AR-010 (P2).
  Design only; no implementation.

- M5 pre-implementation decisions closed: OD-M5-001 (representative transcript corpus
  acquisition plan — Recorder M21.4 validated 24h BTCUSDT Spot + USD-M perpetual live Raw
  capture as M5 corpus source v1) and OD-M5-002 (free standard GitHub-hosted runners only;
  docs-only CI skip via `paths-ignore`; superseded-run cancellation with
  `concurrency.cancel-in-progress`; scheduled medium: <= weekly, 45 min timeout, non-blocking;
  container spike: manual only, 60 min timeout; artifact ceiling: 200 MiB/run, 7-day retention;
  no paid larger runners; no self-hosted public-repo runner). OD-M5-003 remains
  SPIKE-RESOLVABLE. M5 implementation authorized on merge of the decision/CI-policy PR. M6
  remains NOT STARTED. See `docs/M5_PREIMPLEMENTATION_DECISIONS.md`.

### Changed

- M4 recorded as COMPLETE on `main`: merged through PR #8 at
  `ac780d9eb7b49ff20a6b3b4bee6a993b51b70af4`, post-merge main CI `31242162782` — 16/16 PASS.
  The three deferred M4 P2 findings remain open and non-blocking.

- M4 optional `BinanceMarketDataProjection::ProtoAdapter` component with strict owning conversion
  for Contracts `ExchangeDepthSnapshot`/`DepthUpdate`, checked `NumericSpec` and sequence-policy
  binding, deterministic four-state `LocalOrderBookSnapshot` output, explicit gap/recovery/context
  mapping, and separated Host/Core/inbound quality domains.
- Pinned C-M4-001 Conan bootstrap and lock identity, configure-time schema/package/runtime metadata
  checks and negative gates, component-aware installs, isolated Core/adapter consumers, static/shared
  generated-symbol ownership checks, property/fixture/lifetime/allocation matrices, and an M4
  libFuzzer corpus. M4 is approved pending merge.
- M3 `UpdateId`, valid-by-construction `UpdateRange`, and `BookProjection` public API with explicit
  Spot and USD-M sequence policies, a four-state lifecycle, deterministic gap evidence,
  synchronization-aware const visibility, and strongly transactional baseline/incremental apply.
- M3 unit/state-transition coverage, independent primitive/vector property model, deterministic
  Spot and USD-M replay tests, exhaustive allocation-failure sweeps, installed-consumer coverage,
  and a model-based libFuzzer harness with seed corpus.
- M2 deterministic order book core: `BookSide`, `BookLevel`, `LevelUpdate`, `LevelChange`, `OrderBook`
  with PIMPL storage, absolute-quantity semantics, batch updates, atomic replace-all, best bid/ask,
  top-N queries, and model-based fuzzing.

## [0.1.0-alpha.0]

### Added

- M1 exact numeric primitives: `DecimalScale`, `PriceUnits`, `QuantityUnits`, and `NumericSpec`.
- Strict Contracts-compatible price, quantity, and positive-quantity parsing with stable errors,
  exact rescaling, source fractional digit metadata, and checked signed 64-bit arithmetic.
- Deterministic exact formatting, fixed-format conveniences, boundary/roundtrip/property tests,
  public-header self-containment checks, installed-consumer coverage, and a libFuzzer harness.
- M0 C++20 repository foundation, version API, build/test/benchmark infrastructure, package export,
  CI, architecture documentation, and ADRs.
