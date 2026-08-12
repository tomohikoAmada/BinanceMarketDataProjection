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

- `origin/main`: `a1db0f8374bec84d10b0005552983dd44b4e2026` (Phase-3 merge-completion status
  synchronization on top of PR #13 squash merge `473a907eba2001d18926c57d6c8d16b10c7505be`).
- M0-M4 are complete on main.
- M5 Phase 1, Phase 2, and Phase 3 are complete/merged on main.
- ADR-0008 is ACCEPTED: Spot bootstrap uses successor coverage (`U <= L + 1 <= u`,
  overflow-guarded); exact-next `[L+1, ...]` is a valid bridge; `U > L + 1` is the true gap.
  ADR-0008 supersedes only the Spot-bootstrap contains-`L` portion of ADR-0005; USD-M semantics
  are unchanged.

## Active Candidate / Pull Request

- Phase 4: `feat/m5-cross-compiler-semantic-manifests` (DRAFT), implements canonical
  `OperationObservation` serialization (schema v1), semantic SHA-256 digests, portable
  manifest v1, the manifest producer, cross-compiler artifact fan-in, the `m5-replay`
  and `m5-semantic-compare` blocking jobs, and the fail-closed Python comparator. No
  production-code change (`src/`, `include/` unchanged). Independent review requested changes at
  rejected Head `bf2239206ff74e11e3ce73de73f28465b033f808` (P0: 0, P1: 7, P2: 4). The correction is
  IMPLEMENTED / PENDING FOCUSED INDEPENDENT RE-REVIEW; PR #16 remains DRAFT.
- No active Phase-3 candidate: PR #13, `Implement M5 deterministic replay validation`, is
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
- M5 Phase 4: IMPLEMENTED / PENDING FOCUSED INDEPENDENT RE-REVIEW (branch
  `feat/m5-cross-compiler-semantic-manifests`). Delivered: canonical OperationObservation
  serializer v1, semantic SHA-256 digests, portable manifest v1, manifest producer, fail-closed
  Python comparator, metadata-validated three-cross-compiler artifact fan-in, full-evidence
  `m5-replay` Debug/Release determinism, and exact-evidence-checkout `m5-semantic-compare` blocking
  job. No production-code change.
- Spot mandatory 100k evidence is validated lifecycle-valid under ADR-0008: exact-next bridge
  `U=98288147168 u=98288147175` against `L=98288147167` Applied/Synchronized, 100,001 Applied
  operations, zero gaps, final Synchronized.
- USD-M mandatory 100k evidence is validated lifecycle-valid: bridge Applied/Synchronized,
  100,001 Applied operations, zero gaps, final Synchronized.

## Not Implemented

- Phase 5 (replay differential fuzzing) is NOT STARTED.
- Phase 6 (benchmarks) is NOT STARTED.
- Phase 7 (allocation/memory) is NOT STARTED.
- Phase 8 (container spike) is NOT STARTED.
- M6 Gateway integration is NOT STARTED.
- Networking, persistence, Gateway runtime, History runtime, derived market state, strategy, and
  trading remain outside the implemented Projection scope.

## Current Blockers

- Phase 4 cannot merge and Phase 5 cannot start until PR #16's seven corrected P1 findings receive
  focused independent re-review. The PR remains OPEN / DRAFT.

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
- Post-merge main push run `31500884832` at merge `473a907eba2001d18926c57d6c8d16b10c7505be`
  was queued at last query; its result must be read from GitHub, not inferred.
- PR #16 rejected reviewed Head `bf2239206ff74e11e3ce73de73f28465b033f808` had successful run
  `31559019189`, but independent semantic review found seven P1 defects. That historical green run
  is rejection/correction history and is not acceptance evidence for the corrected implementation.

## Next Authorized Step

The next step is focused independent re-review of the corrected M5 Phase-4 findings plus a minimal
regression scan. Phase 5 remains NOT STARTED and is not authorized by Phase-4 implementation or CI.

## AI / Reviewer Reading Order

Read `docs/CURRENT_STATE.md`, `AGENTS.md`, `README.md`, `ARCHITECTURE.md`, `docs/MILESTONES.md`,
the M5 phase/design documents, accepted ADR-0007 and ADR-0008, actual code/tests, and then draft
PR #16 with its exact-head CI. Do not trust this file alone for independent review.

## Historical Documents

`docs/M5_PREIMPLEMENTATION_DECISIONS.md`, `docs/M5_PHASE1_CANONICAL_REPLAY.md`,
`docs/M5_PHASE2_DIFFERENTIAL_ORACLE.md`, and `docs/M5_PHASE3_DETERMINISTIC_REPLAY.md` preserve
implementation and review evidence from their respective phases. Their historical evidence must
not be rewritten; Phase-3 merge completion is recorded here and in the milestone/status documents,
not by altering phase history.
