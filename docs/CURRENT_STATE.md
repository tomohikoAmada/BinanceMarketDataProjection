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

## Current Main

- M5 Phase-6 merge baseline: `227524e6d17cce77813c6f26cd65bb8d996f5677` (PR #21
  squash merge, on top of the Phase-5 merge `53268d5cd2090f4779ffdc14c070184f470cc899`
  and the post-merge Phase-4 documentation synchronization PR #17 at
  `d287c08ed362c5202f25eb77b411bd24bb82cef0`). Main advances as documentation-state PRs
  merge; see Git history for the current tip.
- M0-M4 are complete on main.
- M5 Phase 1 through Phase 6 are complete/merged on main.
- ADR-0008 is ACCEPTED: Spot bootstrap uses successor coverage (`U <= L + 1 <= u`,
  overflow-guarded); exact-next `[L+1, ...]` is a valid bridge; `U > L + 1` is the true gap.
  ADR-0008 supersedes only the Spot-bootstrap contains-`L` portion of ADR-0005; USD-M semantics
  are unchanged.

## Recent Pull Requests / Active Candidate

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

- Phase 7 (allocation/memory) is NOT STARTED.
- Phase 8 (container spike) is NOT STARTED.
- M6 Gateway integration is NOT STARTED.
- Networking, persistence, Gateway runtime, History runtime, derived market state, strategy, and
  trading remain outside the implemented Projection scope.

## Current Blockers

- No Phase-6 technical blockers remain.
- P6-AUDIT-003 is CLOSED by the post-merge governance/status synchronization (this PR #22):
  Phase-6 repository status/orientation documents are synchronized with the accepted
  implementation Head `9776ba6b93990c44e550f289b69127ca721b0d00`, exact-head CI
  `31803322848` — 18/18 PASS, formal evidence ACCEPTED, independent review APPROVED,
  PR #21 MERGED, squash merge `227524e6d17cce77813c6f26cd65bb8d996f5677`, and post-merge
  main CI `31809917018` — 18/18 PASS.
- Toolchain reproducibility hardening remains a separate future infrastructure task.
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
- PR #16 rejected reviewed Head `bf2239206ff74e11e3ce73de73f28465b033f808` had successful run
  `31559019189`, but independent semantic review found seven P1 defects. That historical green run
  is rejection/correction history and is not acceptance evidence for the merged implementation.

## Next Authorized Step

M5 Phase 6 is complete and merged (PR #21, squash merge
`227524e6d17cce77813c6f26cd65bb8d996f5677`; accepted implementation Head
`9776ba6b93990c44e550f289b69127ca721b0d00`; exact-head CI `31803322848` — 18/18 PASS;
post-merge main CI `31809917018` — 18/18 PASS). P6-AUDIT-003 is CLOSED by this
governance/status synchronization. Immediate sequencing:

1. standalone Toolchain Reproducibility / Quality Toolchain Hardening task;
2. after that task is separately completed/accepted, proceed to separately authorized
   Phase-7 work.

Phase 7 has NOT started.

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
