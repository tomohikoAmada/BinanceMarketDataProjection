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

- `origin/main`: `8bc71f2ae457cf3d15a9dcb4ea659a9c3f85a569` (M3 Spot successor-coverage
  correction PR #14 merged at this SHA).
- M0-M4 are complete on main.
- M5 Phase 1 and Phase 2 are complete/merged on main.
- ADR-0008 is ACCEPTED: Spot bootstrap uses successor coverage (`U <= L + 1 <= u`,
  overflow-guarded); exact-next `[L+1, ...]` is a valid bridge; `U > L + 1` is the true gap.
  ADR-0008 supersedes only the Spot-bootstrap contains-`L` portion of ADR-0005; USD-M semantics
  are unchanged.

## Active Candidate / Pull Request

- PR #13, `Implement M5 deterministic replay validation`, is OPEN and DRAFT.
- Branch: `feat/m5-deterministic-replay-validation`.
- The branch has been rebased onto the accepted M3 successor-coverage main and re-validated:
  both mandatory 100k corpora are established from the pinned authoritative source.
- PR #13 is not merged and must not be described as main or approved.

## Deployed State

N/A. Projection is a library and no Projection runtime deployment is claimed. Recorder's deployed
artifact is a separate identity and remains outside Projection ownership.

## Implemented

- M0-M4: COMPLETE / MERGED.
- M5 Phase 1: COMPLETE / MERGED (PR #11).
- M5 Phase 2: COMPLETE / MERGED (PR #12).
- Active PR #13 Phase 3 candidate: small-tier replay, scaled differential diagnostics, offline
  Recorder Raw-v1 materialization, medium lifecycle validation, direct ADR-0008 Spot conformance
  coverage, ReplayDriver noexcept and uint64 JSON parser hardening.
- Spot mandatory 100k evidence is validated lifecycle-valid under ADR-0008: exact-next bridge
  `U=98288147168 u=98288147175` against `L=98288147167` Applied/Synchronized, 100,001 Applied
  operations, zero gaps, final Synchronized.
- USD-M mandatory 100k evidence is validated lifecycle-valid: bridge Applied/Synchronized,
  100,001 Applied operations, zero gaps, final Synchronized.

## Not Implemented

- M5 Phase 3 is IMPLEMENTED / PENDING INDEPENDENT REVIEW in PR #13; it is not approved or merged.
- Phase 4 semantic observation serialization/digests/manifests and cross-compiler transport are
  NOT STARTED.
- M6 Gateway integration is NOT STARTED.
- Networking, persistence, Gateway runtime, History runtime, derived market state, strategy, and
  trading remain outside the implemented Projection scope.

## Current Blockers

- None on the Phase-3 corpus gates: both mandatory medium corpora are established. PR #13 still
  requires independent review of its exact Head before Ready/merge.
- Exact-head CI for the current candidate must be reported from GitHub and must not be inferred
  from older runs.

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

- Exact-head CI must be queried for the live current candidate head. The last observed prior-head run
  was `31389252990` for `3a678eb…`, `in_progress` at the final pre-update query; later pushes
  supersede it. Prompt-time run `31380230614` for older head `f9061963…` completed `failure` and is
  not evidence for the current head.
- PR #13 body records the current Phase 3 disposition and says independent re-review is required;
  it is not an approval or merge record.

## Next Authorized Step

Keep PR #13 DRAFT and unmerged. Obtain focused independent review and exact-head CI for the current
candidate; do not mark Ready, merge, start Phase 4, or invent alternate Spot bootstrap semantics.

## AI / Reviewer Reading Order

Read `docs/CURRENT_STATE.md`, `AGENTS.md`, `README.md`, `ARCHITECTURE.md`, `docs/MILESTONES.md`,
the M5 phase/design documents, accepted ADR-0007 and ADR-0008, actual code/tests, and then PR #13
with its exact-head CI. Do not trust this file alone for independent review.

## Historical Documents

`docs/M5_PREIMPLEMENTATION_DECISIONS.md`, `docs/M5_PHASE1_CANONICAL_REPLAY.md`,
`docs/M5_PHASE2_DIFFERENTIAL_ORACLE.md`, and `docs/M5_PHASE3_DETERMINISTIC_REPLAY.md` preserve
implementation and review evidence from their respective phases. Their historical evidence must
not be rewritten into a claim that Phase 3 or Phase 4 is approved or merged.
