CURRENT-STATE ORIENTATION ONLY.
This file summarizes repository state for humans and AI reviewers.
It does not override accepted ADRs or semantic design documents.
When this file and Git/GitHub/code disagree, verify Git/GitHub/code and update this file.

# Current State

## Purpose of This File

This is the current orientation index for a fresh human or AI reviewer. It distinguishes merged
main behavior, the active PR candidate, historical evidence, and the next authorized milestone.

## Repository Role

Projection owns deterministic market-state projection semantics, fixed-point interpretation, the
single-writer order book, Spot/USD-M sequence policy, projection lifecycle, the optional Protobuf
adapter boundary, and M5 validation infrastructure. It does not own Binance network acquisition,
Recorder persistence/archive, or Gateway runtime.

## Source-of-Truth Order

For implemented facts, use checked-out code, tests, build configuration, Git history, and current
GitHub state. Accepted ADRs and accepted semantic designs govern deliberate semantics. This file is
orientation only.

## Current Main / Recent Main Baseline

- INFRA-TC-001 / PR #23 squash merge: `24fb72232e928290add45ed8634cd0bf9a8d3442` (reproducible
  Quality toolchain). This is the accepted infrastructure merge baseline immediately preceding
  this docs-only governance synchronization; Git history is authoritative for any later tip.
- PR #22 governance/status synchronization: `22f4a92b1417fe1c844f9da588e5f3014833ca77`
  (M5 Phase-6 record).
- M5 Phase-6 implementation squash merge: `227524e6d17cce77813c6f26cd65bb8d996f5677` (PR #21).
  This is the Phase-6 implementation baseline, not the main tip; main advanced through PR #22
  and PR #23 after it.
- M5 Phase-6 accepted implementation Head: `9776ba6b93990c44e550f289b69127ca721b0d00`
  (accepted exact-head CI `31803322848` — 18/18 PASS; the accepted Head and the squash merge
  share the same repository tree `79aa0151d65d42a1c63e6649bf24bf9053105667`).
- Main advances as documentation-state PRs merge; Git history is authoritative for the tip.
- M0-M4 are complete on main.
- M5 Phase 1 through Phase 6 are complete/merged on main.
- ADR-0008 is ACCEPTED: Spot bootstrap uses successor coverage (`U <= L + 1 <= u`,
  overflow-guarded); exact-next `[L+1, ...]` is a valid bridge; `U > L + 1` is the true gap.
  ADR-0008 supersedes only the Spot-bootstrap contains-`L` portion of ADR-0005; USD-M semantics
  are unchanged.

## Recent Pull Requests / Acceptance Records

- INFRA-TC-001 record: PR #23, `Make the Quality toolchain reproducible`, is MERGED (final
  independently accepted implementation Head `6071a9f18bb4f0ebc9f1bed2938477419f7ab2ac`;
  accepted exact-head CI `31928210161` — 19/19 PASS; final independent review APPROVED,
  P0: 0, P1: 0, P2: 0; INFRA-TC-001 technical acceptance ACCEPTED; squash merge
  `24fb72232e928290add45ed8634cd0bf9a8d3442`; post-merge main CI `31931002850` — 19/19 PASS).
  The accepted Head and the squash merge commit share the same repository tree
  (`278853c1942dc72a5a1fedda31f1bba08e01aa50`). All known INFRA-TC findings are CLOSED;
  the earlier review rounds that requested changes (INFRA-TC-FINAL-, INFRA-TC-REREVIEW-, and
  INFRA-TC-FINALREREVIEW-series findings, including the trust-boundary and in-build base-binding
  findings) are historical review record — their corrections are part of the merged
  implementation. No active INFRA-TC-001 candidate remains.
- Phase-6 record: PR #21, `Implement M5 Phase 6 representative benchmarks`, is
  MERGED (final independently accepted implementation Head
  `9776ba6b93990c44e550f289b69127ca721b0d00`; accepted exact-head CI `31803322848` — 18/18
  PASS; final independent review APPROVED, P0: 0, P1: 0, P2: 1; formal exact-head Phase-6
  evidence ACCEPTED; squash merge `227524e6d17cce77813c6f26cd65bb8d996f5677`; post-merge
  main CI `31809917018` — 18/18 PASS). The accepted Head and the squash merge commit share
  the same repository tree (`79aa0151d65d42a1c63e6649bf24bf9053105667`). No active
  Phase-6 candidate remains.
- PR #18, `Implement M5 differential replay fuzzing`, is MERGED (squash merge
  `53268d5cd2090f4779ffdc14c070184f470cc899`; final approved Head
  `e56f5dbd12b9e66946343467221e8e3ba9984531`; exact-head CI `31668465623` — 18/18 PASS;
  post-merge main CI `31671708958` — 18/18 PASS). The final blocking-correction work started
  from Head `95c9bc8918eaa4d3447648a6ad3698d445ec8dcb`; its pre-correction exact-head CI run
  `31620426837` passed 18/18. Its remote branch `feat/m5-differential-replay-fuzzing` has been
  deleted. No active Phase-5 candidate remains.
- Phase-4 record: PR #16, `Implement M5 cross-compiler semantic manifests`, is
  MERGED (squash merge `7b6d9ef3b222675138fdd34f3fed381216fe9d02`; approved Head
  `b612f85d281315346c0ccf6f599d51af538e3cf4`; exact-head CI `31571166506` — 18/18 PASS;
  post-merge main CI `31576511096` — 18/18 PASS). Its remote branch
  `feat/m5-cross-compiler-semantic-manifests` has been deleted.
- Phase-3 record: PR #13, `Implement M5 deterministic replay validation`, is
  MERGED (squash merge `473a907eba2001d18926c57d6c8d16b10c7505be`). Its remote branch
  `feat/m5-deterministic-replay-validation` has been deleted.

## Deployed State

N/A. Projection is a library and no Projection runtime deployment is claimed. Recorder's deployed
artifact is a separate identity and remains outside Projection ownership.

## Implemented

- M0-M4: COMPLETE / MERGED.
- M5 Phase 1: COMPLETE / MERGED (PR #11).
- M5 Phase 2: COMPLETE / MERGED (PR #12).
- M5 Phase 3: COMPLETE / MERGED (PR #13, merge
  `473a907eba2001d18926c57d6c8d16b10c7505be`; final approved Head
  `a8e4ccfc31efd4e67bc10cf0ac9ad2a99faa8354`; exact-head CI `31491615547` — PASS 16/16).
  Delivered: deterministic 2,048-event small-tier replay, scaled differential diagnostics,
  the offline Recorder Raw-v1 materializer, medium lifecycle validation, direct ADR-0008 Spot
  conformance coverage, ReplayDriver exception hardening, and uint64 JSON parser hardening.
- M5 Phase 4: COMPLETE / MERGED (PR #16; final approved Head
  `b612f85d281315346c0ccf6f599d51af538e3cf4`; exact-head CI `31571166506` — PASS 18/18; squash
  merge `7b6d9ef3b222675138fdd34f3fed381216fe9d02`; post-merge main CI `31576511096` — PASS
  18/18). Delivered: canonical OperationObservation serializer v1, semantic SHA-256 digests,
  portable manifest v1, manifest producer, fail-closed Python comparator, metadata-validated
  three-cross-compiler artifact fan-in, full-evidence `m5-replay` Debug/Release determinism, and
  exact-evidence-checkout `m5-semantic-compare` blocking job. Post-merge GNU/Clang/AppleClang
  Release manifests agree on fixture-set `c688c046c1dcfc08a87a1d3737a6760598d5af0fa1338ba87087059127310e27`,
  the four authoritative workloads, fixture hashes, and semantic digests. No production-code change.
- M5 Phase 5: COMPLETE / MERGED (PR #18; final approved Head
  `e56f5dbd12b9e66946343467221e8e3ba9984531`; exact-head CI `31668465623` — PASS 18/18; final
  focused independent review APPROVED, P0: 0, P1: 0, P2: 0; squash merge
  `53268d5cd2090f4779ffdc14c070184f470cc899`; post-merge main CI `31671708958` — PASS 18/18).
  Delivered: direct structured byte decoder, bounded deterministic
  `LLVMFuzzerTestOneInput`, production/reference ReplayDriver comparison, Core and Adapter mode
  coverage, Spot and USD-M market coverage, libFuzzer abort on first divergence, 10 mandatory
  checked-in replay seed categories with structural validation, fifth fuzz-smoke target
  (10,000-run CI smoke), and shared `cmake/M5Support.cmake` reuse. Post-merge
  GNU/Clang/AppleClang Release manifests record `head_sha`
  `53268d5cd2090f4779ffdc14c070184f470cc899` (schema `M5_SEMANTIC_MANIFEST_V2`, observation
  schema `M5_SEMANTIC_OBSERVATION_V2`), agree on the fixture-set
  `c688c046c1dcfc08a87a1d3737a6760598d5af0fa1338ba87087059127310e27` and the four authoritative
  semantic digests (Core Spot `988e96d69f20748af758fd6a9273d2bc3b3d08680a98047c249f1bc0ab07d9e7`,
  Core USD-M `1b6c0b11a4a601f3efc091c737ba9ddb60b2984854ebdfdf6a2cf3504f74b1fb`,
  Adapter Spot `797001c2516994ea45b180047dfa2531dba9423d2653027412de9fa7207b55e6`,
  Adapter USD-M `b9cdd80f6678e771e01d6f238713d3412ce84b99009a0942cf2fe90fba564e74`),
  cross-compiler semantic equality PASS. No production-code change.
- M5 Phase 6: COMPLETE / MERGED (PR #21; final independently accepted implementation Head
  `9776ba6b93990c44e550f289b69127ca721b0d00`; accepted exact-head CI `31803322848` — PASS
  18/18; final independent review APPROVED, P0: 0, P1: 0, P2: 1; formal exact-head Phase-6
  evidence ACCEPTED; squash merge `227524e6d17cce77813c6f26cd65bb8d996f5677`; post-merge main
  CI `31809917018` — PASS 18/18). Delivered: normative M1 representative benchmarks; M2
  stateful/query workloads; the complete 48-cell M3 accepted live-apply matrix plus
  classification and component/proxy measurements; M4 adaptation/checked/snapshot/
  serialization benchmarks; CoreNormalizedReplay and AdapterWireReplay (Spot + USD-M);
  the production event-latency sampler; `M5_BENCHMARK_WORKLOAD_SPEC_V1`,
  `M5_BENCHMARK_WRAPPER_V1`, `M5_REPLAY_LATENCY_V1`; fail-closed
  inventory/smoke/wrapper/latency validation; the formal/manual benchmark-full evidence
  workflow; the benchmark-smoke CI structural gate; and truthful
  source/binary/dependency/workload/result provenance. Final provenance corrections closed
  two P1s: P6-FINAL-001 (CheckedApply canonical workload identity explicitly records the
  locked Spot successor `initial_update_id=1000001`, `first/final_update_id=1000002`,
  `previous_final_update_id=not_applicable`, bound to the actual timed workload with
  fail-closed regression/validation) and P6-FINAL-002 (`contracts_package_id` records the
  real Conan binary package ID derived from the exact consumed package's conaninfo.txt, not
  the cache-directory locator). Production `src/**` and public `include/**` were unchanged by
  Phase 6.
- Spot mandatory 100k evidence is validated lifecycle-valid under ADR-0008: exact-next bridge
  `U=98288147168 u=98288147175` against `L=98288147167` Applied/Synchronized, 100,001 Applied
  operations, zero gaps, final Synchronized.
- USD-M mandatory 100k evidence is validated lifecycle-valid: bridge Applied/Synchronized,
  100,001 Applied operations, zero gaps, final Synchronized.

## Not Implemented

- Phase 7 (allocation/memory) methodology is APPROVED / MERGED (PR #25). Phase-7
  implementation is **IN PROGRESS**:
  - PR-A (work package WP1) implements the executable-local allocation
    instrumentation substrate (constant-initialized process-lifetime state,
    fixed-capacity non-allocating provenance table, complete 20-form
    replaceable global new/delete surface with checked aligned backing
    arithmetic, MeasurementScope A/P/B semantics, fail-closed
    overflow/failure behavior, test-only seams) and its adversarial
    validation executable `bmd_projection_allocation_instrumentation_tests`.
  - PR-B (work packages WP2–WP4) implements the measurement/evidence
    machinery: the Phase-7 evidence schemas (`M5_ALLOCATION_WRAPPER_V1`,
    `M5_PHASE7_MEASUREMENT_CONTRACT_V1`, `M5_PHASE7_ALLOCATION_RECORD_V1`,
    `M5_PHASE7_FOOTPRINT_RECORD_V1`) with a canonical result-payload
    binding; the independent fail-closed Python validator
    (`scripts/benchmark_phase7.py`); shared Phase-6 workload identities
    (one identity source for Phase-6 timing and Phase-7 allocation
    measurement, bit-for-bit golden regression); the M2/M3 allocation
    characterization (54 M2 cells, the complete 48-cell M3 accepted
    matrix with B=0 preserved, classification cells, Component/Proxy
    diagnostics), the persistent footprint experiment (depths
    100/1000/5000/10000 with non-additive node model, estimated
    allocator model, RSS not measured), the M4 allocation
    characterization (24 accepted cells, owning-output B/D lifecycles),
    the replay allocation characterization (4 identities, exact rational
    per-event values, oracle outside the bracket), and the EXPLORATORY
    local driver (`scripts/benchmark-allocation.sh`).
  - NOT implemented yet (later Phase-7 PRs): the formal Release runner
    (`scripts/benchmark-allocation-formal.sh`), formal evidence, and
    Phase-7 final acceptance.
- Phase 8 (container spike) is NOT STARTED.
- M6 Gateway integration is NOT STARTED.
- Networking, persistence, Gateway runtime, History runtime, derived market state, strategy, and
  trading remain outside the implemented Projection scope.

## Current Blockers

- No Phase-6 technical blockers remain.
- P6-AUDIT-003 is CLOSED by the post-merge governance/status synchronization (PR #22):
  Phase-6 repository status/orientation documents are synchronized with the accepted
  implementation Head `9776ba6b93990c44e550f289b69127ca721b0d00`, exact-head CI
  `31803322848` — 18/18 PASS, formal evidence ACCEPTED, independent review APPROVED,
  PR #21 MERGED, squash merge `227524e6d17cce77813c6f26cd65bb8d996f5677`, and post-merge
  main CI `31809917018` — 18/18 PASS.
- INFRA-TC-001 (reproducible Quality toolchain): COMPLETE / ACCEPTED / MERGED. Final
  independent review APPROVED (P0: 0, P1: 0, P2: 0); all known INFRA-TC findings are CLOSED;
  no blocker remains. PR #23 is MERGED: accepted implementation Head
  `6071a9f18bb4f0ebc9f1bed2938477419f7ab2ac`, exact-head acceptance CI `31928210161` —
  19/19 PASS, squash merge `24fb72232e928290add45ed8634cd0bf9a8d3442`, post-merge main CI
  `31931002850` — 19/19 PASS (attempt 2; attempt 1's only failure was a transient external
  `snapshot.ubuntu.com` HTTP 503 while apt downloaded the pinned historical snapshot package
  `binutils_2.42-4ubuntu2.10_amd64.deb`; the failed job was rerun and passed with no
  repository change). Implementation facts: exact clang/clang++/clang-tidy/clang-format
  identity in `.toolchain/quality.env` (single source of truth), one repo-owned canonical
  Quality entrypoint `scripts/quality.sh` used by CI and local developers, base-image digest
  bound through a build argument (no independent Dockerfile literal) AND verified in-build:
  the Dockerfile re-derives the base reference from the exact baked contract with the
  production helper and fails the build if it differs from the FROM argument (before TLS
  bootstrap/apt), Ubuntu archive state pinned
  to the HISTORICAL snapshot `UBUNTU_SNAPSHOT_ID=20260814T120000Z` (format + real UTC
  calendar + temporal validation; future IDs fail closed), duplicate contract key assignments
  invalid and fail closed in every consumer (no first-wins/last-wins), test-only
  `BMD_QUALITY_*` hooks and stale internal `BMD_CANONICAL_QUALITY_*` orchestration variables
  rejected by the canonical entrypoint before image construction (container mode is not
  ambient-selectable; canonical source/work roots are fixed `/src` and `/work`; the internal
  container mode proves the canonical image boundary — baked contract equals mounted source
  contract — before any source copy or recursive execution), canonical runtime requires a
  real Docker Engine backend (Docker Engine on Linux, Docker Desktop on macOS; Podman and
  podman-docker/libpod backends are not validated and are rejected), immutable SHA-256-pinned
  TLS bootstrap artifact (no mutable live archive), cache namespaces keyed by the canonical
  contract, ephemeral /work with never-persisted build trees, dpkg provenance plus installed
  payload md5 verification, and 100 deterministic/adversarial contract tests (100/100 PASS;
  canonical Quality at the accepted Head PASS; Projection tests under canonical Quality
  413/413 PASS; staged-install consumer PASS). See `docs/QUALITY_TOOLCHAIN.md` and the PR #23
  record for the implementation/evidence report.
- No Phase-7 methodology blocker remains: the pre-implementation methodology is
  APPROVED / MERGED and all findings (M5-P7-MR-001..010, M5-P7-RR-001) are
  CLOSED. Phase-7 implementation has not yet started — a sequencing fact, not a
  technical blocker.
- Phase 8 remains blocked by sequencing until Phase-7 evidence/handoff is
  complete as defined by the accepted design (OD-M5-P7-021).
- Later M5 phases remain separately authorized work.

## Accepted Semantic Authorities

ADR-0005 is the accepted market-specific sequence authority, with its Spot-bootstrap contains-`L`
portion superseded by ACCEPTED ADR-0008. For Projection Spot bootstrap:

> `u < L` stale; `u == L` duplicate/non-advancing; `U <= L + 1 <= u` valid advancing bridge
> (overflow-guarded); `U > L + 1` is `SpotBootstrapForwardGap`.

USD-M bootstrap (`U <= L <= u`) and live (`pu == current`) semantics are unchanged. ADR-0007 is
the accepted M5 differential validation architecture. This file does not override accepted ADRs
or design documents.

## Known Semantic Conflicts

Recorder R-034 remains OPEN: Recorder's local reconstruction behavior uses `lastUpdateId + 1`.
ADR-0008 records the independently reviewed official 2025-11-12 Spot correction; the Projection
Spot bootstrap rule is successor coverage. Contracts owns neither rule.

## Cross-Repository Relationships

- Recorder supplies immutable Raw/source evidence for offline M5 use; it does not define Projection
  semantics.
- Contracts owns public schemas and the C++ package consumed by Projection's optional adapter;
  C-M4-001 is implemented/accepted/merged, package revision remains unassigned, and publication is
  not claimed.
- Projection consumes Contracts only at the optional adapter boundary; Core remains independent of
  Contracts/Protobuf and Gateway.

## Current Validation / CI Evidence

- PR #13 exact-head CI run `31491615547` for the final approved head
  `a8e4ccfc31efd4e67bc10cf0ac9ad2a99faa8354` completed `success` — 16/16 jobs PASS.
- Post-merge main push run `31500884832` at Phase-3 merge
  `473a907eba2001d18926c57d6c8d16b10c7505be` completed `success`.
- PR #16 approved exact-head CI run `31571166506` for the final approved head
  `b612f85d281315346c0ccf6f599d51af538e3cf4` completed `success` — 18/18 jobs PASS. This is the
  acceptance evidence for the merged Phase-4 implementation.
- Post-merge main push run `31576511096` at Phase-4 merge
  `7b6d9ef3b222675138fdd34f3fed381216fe9d02` completed `success` — 18/18 jobs PASS (quality, ASan,
  UBSan, benchmark smoke, fuzz smoke, Release GCC/Clang/AppleClang, all M4 static/shared gates,
  `m5-replay`, `m5-semantic-compare`). Its semantic evidence manifests record `head_sha`
  `7b6d9ef3b222675138fdd34f3fed381216fe9d02` and pass cross-compiler semantic equality.
- PR #18 approved exact-head CI run `31668465623` for the final approved head
  `e56f5dbd12b9e66946343467221e8e3ba9984531` completed `success` — 18/18 jobs PASS. This is the
  acceptance evidence for the merged Phase-5 implementation.
- Post-merge main push run `31671708958` at Phase-5 merge
  `53268d5cd2090f4779ffdc14c070184f470cc899` completed `success` — 18/18 jobs PASS (quality, ASan,
  UBSan, benchmark smoke, fuzz smoke, Release GCC/Clang/AppleClang, all M4 static/shared gates,
  `m5-replay`, `m5-semantic-compare`). Its semantic evidence manifests record `head_sha`
  `53268d5cd2090f4779ffdc14c070184f470cc899` (schema `M5_SEMANTIC_MANIFEST_V2`, observation
  schema `M5_SEMANTIC_OBSERVATION_V2`) and pass cross-compiler semantic equality.
- PR #21 accepted exact-head CI run `31803322848` for the final independently accepted
  implementation head `9776ba6b93990c44e550f289b69127ca721b0d00` completed `success` — 18/18
  jobs PASS. This is the acceptance evidence for the merged Phase-6 implementation.
- Post-merge main push run `31809917018` at Phase-6 merge
  `227524e6d17cce77813c6f26cd65bb8d996f5677` completed `success` — 18/18 jobs PASS (quality, ASan,
  UBSan, benchmark smoke, fuzz smoke, Release GCC/Clang/AppleClang, all M4 static/shared gates,
  `m5-replay`, `m5-semantic-compare`).
- INFRA-TC-001 accepted exact-head CI run `31928210161` for the final independently accepted
  implementation head `6071a9f18bb4f0ebc9f1bed2938477419f7ab2ac` completed `success` — 19/19
  jobs PASS. This is the acceptance evidence for the merged INFRA-TC-001 implementation
  (deterministic Quality toolchain suite 100/100 PASS; canonical Quality at the accepted
  Head PASS; Projection tests under canonical Quality 413/413 PASS; staged-install consumer
  PASS).
- Post-merge main push run `31931002850` at the INFRA-TC-001 squash merge
  `24fb72232e928290add45ed8634cd0bf9a8d3442` completed `success` — 19/19 jobs PASS
  (attempt 2; attempt 1's only failure was the `quality (canonical pinned toolchain)` job
  hitting a transient external `snapshot.ubuntu.com` HTTP 503 while apt downloaded the
  pinned historical snapshot package `binutils_2.42-4ubuntu2.10_amd64.deb`; the failed job
  was rerun with `gh run rerun 31931002850 --failed` and passed without any repository
  change).
- PR #16 rejected reviewed Head `bf2239206ff74e11e3ce73de73f28465b033f808` had successful run
  `31559019189`, but independent semantic review found seven P1 defects. That historical green run
  is rejection/correction history and is not acceptance evidence for the merged implementation.

## Next Authorized Step

M5 Phase 6 is complete and merged (PR #21, squash merge
`227524e6d17cce77813c6f26cd65bb8d996f5677`; accepted implementation Head
`9776ba6b93990c44e550f289b69127ca721b0d00`; exact-head CI `31803322848` — 18/18 PASS;
post-merge main CI `31809917018` — 18/18 PASS). P6-AUDIT-003 is CLOSED by the PR #22
governance/status synchronization. INFRA-TC-001 (reproducible Quality toolchain) is
COMPLETE / ACCEPTED / MERGED (PR #23; accepted implementation Head
`6071a9f18bb4f0ebc9f1bed2938477419f7ab2ac`; exact-head acceptance CI `31928210161` —
19/19 PASS; final independent review APPROVED, P0: 0, P1: 0, P2: 0; squash merge
`24fb72232e928290add45ed8634cd0bf9a8d3442`; post-merge main CI `31931002850` — 19/19 PASS;
all known INFRA-TC findings CLOSED). The INFRA-TC-001 prerequisite for later M5 work is
satisfied. Immediate sequencing:

1. INFRA-TC-001: COMPLETE / ACCEPTED / MERGED — no blocker remains.
2. Phase-7 preimplementation methodology: **APPROVED / MERGED** — PR #25
   (`Lock M5 Phase 7 allocation/memory measurement contract`); final
   independent methodology review APPROVED (P0: 0, P1: 0, P2: 0); all findings
   (M5-P7-MR-001..010, M5-P7-RR-001) CLOSED; approved Head
   `92288927165f2d7486491371a11a7a586c645565`; squash merge
   `c2ad198677130d05ad054ea48ade3a1d8021c153` (merged tree == approved Head
   tree).
3. Phase-7 implementation: **IN PROGRESS** — PR-A (work package WP1) delivers
   the executable-local allocation instrumentation substrate (fixed-capacity
   non-allocating provenance, complete 20-form replaceable global new/delete
   surface with checked aligned backing arithmetic, MeasurementScope A/P/B
   semantics, fail-closed overflow/failure behavior, test-only seams) and the
   adversarial instrumentation validation executable. PR-B (work packages
   WP2–WP4) delivers the measurement/evidence machinery (schemas, validator,
   shared workload identities, M2/M3/M4/replay/footprint measurement
   executables, exploratory driver); it is pending independent exact-head
   review. The formal Release runner, formal evidence, and Phase-7 final
   acceptance are NOT implemented yet.

Phase 7 implementation is the active engineering step. Phase 8 (container
spike) is NOT STARTED and must not begin before Phase-7 evidence/handoff
satisfies the accepted sequencing contract (OD-M5-P7-021).

## AI / Reviewer Reading Order

Read `docs/CURRENT_STATE.md`, `AGENTS.md`, `README.md`, `ARCHITECTURE.md`, `docs/MILESTONES.md`,
the M5 phase/design documents, accepted ADR-0007 and ADR-0008, actual code/tests, and the merged
PR #21 record (Phase 6) plus its exact-head CI. Do not trust this file alone for independent
review.

## Historical Documents

`docs/M5_PREIMPLEMENTATION_DECISIONS.md`, `docs/M5_PHASE1_CANONICAL_REPLAY.md`,
`docs/M5_PHASE2_DIFFERENTIAL_ORACLE.md`, and `docs/M5_PHASE3_DETERMINISTIC_REPLAY.md` preserve
implementation and review evidence from their respective phases. Their historical evidence must
not be rewritten; Phase-3 merge completion is recorded here and in the milestone/status documents,
not by altering phase history.
