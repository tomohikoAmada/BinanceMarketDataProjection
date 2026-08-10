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

- `origin/main`: `75c619dd683ff2a3893f9535e206231e7bfecc41`.
- M0-M4 are complete on main.
- M5 Phase 1 and Phase 2 are complete/merged on main; Phase 2 merged at this SHA.
- Main CI: run `31315421548`, completed `success` (16/16).

## Active Candidate / Pull Request

- PR #13, `Implement M5 deterministic replay validation`, is OPEN and DRAFT.
- Branch: `feat/m5-deterministic-replay-validation`.
- Current candidate head: `cd67b8988f6f7698c0f8ca0b7004c266ee7071db`.
- PR #13 is not merged and must not be described as main or approved.

## Deployed State

N/A. Projection is a library and no Projection runtime deployment is claimed. Recorder's deployed
artifact is a separate identity and remains outside Projection ownership.

## Implemented

- M0-M4: COMPLETE / MERGED.
- M5 Phase 1: COMPLETE / MERGED (PR #11).
- M5 Phase 2: COMPLETE / MERGED (PR #12 at main SHA above).
- Active PR #13 Phase 3 candidate: small-tier replay, scaled differential diagnostics, offline
  Recorder Raw-v1 materialization, and medium lifecycle validation are implemented in the candidate.
- USD-M mandatory 100k evidence is validated lifecycle-valid: bridge Applied/Synchronized,
  100,001 Applied operations, zero gaps, final Synchronized.

## Not Implemented

- M5 Phase 3 is not complete because the mandatory Spot 100k corpus is not established.
- Phase 4 semantic observation serialization/digests/manifests and cross-compiler transport are
  NOT STARTED.
- M6 Gateway integration is NOT STARTED.
- Networking, persistence, Gateway runtime, History runtime, derived market state, strategy, and
  trading remain outside the implemented Projection scope.

## Current Blockers

- The authoritative Recorder archive was found and validated, but its pinned in-window Spot source
  has no Projection-valid bootstrap bridge under the accepted contains-`L` predicate. Its only
  advancing candidate begins at `L + 1`, so Spot 100k is NOT ESTABLISHED / source INELIGIBLE.
- USD-M medium evidence is valid; this does not remove the mandatory Spot corpus gate.
- Exact-head CI for the current candidate must be reported from GitHub and must not be inferred from
  older runs.

## Accepted Semantic Authorities

ADR-0005 is the accepted market-specific sequence authority. For Projection Spot bootstrap:

> `u < L` stale; `u == L` duplicate/non-advancing; `U <= L < u` valid advancing bridge; `U > L`
> is `SpotBootstrapForwardGap`.

Bootstrap and live successor semantics are distinct. ADR-0007 is the accepted M5 differential
validation architecture. This file does not override accepted ADRs or design documents.

## Known Semantic Conflicts

Recorder R-034 remains OPEN: Recorder's local reconstruction behavior uses `lastUpdateId + 1`.
That Recorder-local rule is not portable as Projection authority and does not replace Projection's
accepted Spot bootstrap rule `U <= L < u`. Contracts owns neither rule.

## Cross-Repository Relationships

- Recorder supplies immutable Raw/source evidence for offline M5 use; it does not define Projection
  semantics.
- Contracts owns public schemas and the C++ package consumed by Projection's optional adapter;
  C-M4-001 is implemented/accepted/merged, package revision remains unassigned, and publication is
  not claimed.
- Projection consumes Contracts only at the optional adapter boundary; Core remains independent of
  Contracts/Protobuf and Gateway.

## Current Validation / CI Evidence

- Exact-head CI for the current candidate: run `31389180996` for head
  `cd67b8988f6f7698c0f8ca0b7004c266ee7071db`, status `queued` at query time.
- The prior exact-head run `31386405720` for head `719d8bf…` was cancelled when this documentation
  commit was pushed; prompt-time run `31380230614` for older head `f9061963…` completed `failure`.
  Neither older run is evidence for the current head.
- PR #13 body records the current Phase 3 disposition and says independent re-review is required;
  it is not an approval or merge record.

## Next Authorized Step

Keep PR #13 DRAFT and unmerged. Obtain focused independent review and exact-head CI for the current
candidate; do not mark Ready, merge, start Phase 4, or invent alternate Spot bootstrap semantics.
If the candidate is accepted, Phase 3 still requires an authorized resolution of the Spot corpus
eligibility gate before Phase 4 work.

## AI / Reviewer Reading Order

Read `docs/CURRENT_STATE.md`, `AGENTS.md`, `README.md`, `ARCHITECTURE.md`, `docs/MILESTONES.md`,
the M5 phase/design documents, accepted ADR-0005 and ADR-0007, actual code/tests, and then PR #13
with its exact-head CI. Do not trust this file alone for independent review.

## Historical Documents

`docs/M5_PREIMPLEMENTATION_DECISIONS.md`, `docs/M5_PHASE1_CANONICAL_REPLAY.md`,
`docs/M5_PHASE2_DIFFERENTIAL_ORACLE.md`, and `docs/M5_PHASE3_DETERMINISTIC_REPLAY.md` preserve
implementation and review evidence from their respective phases. Their historical evidence must
not be rewritten into a claim that Phase 3 or Phase 4 is complete.
