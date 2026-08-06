# M3 Sequence and Projection State Design

## Status

- Design status: **APPROVED**
- Implementation status: **APPROVED; PENDING MERGE**
- ADR status: **ACCEPTED**
- Design date: 2026-08-06
- Projection base: `413e3cd9236d0c5de15d4e838149111718260303`
- Contracts baseline: `01d76a41929f36d89573159f5f458f9f1e378ada`

The accepted API and semantics in this document are implemented on the separate
`feat/m3-sequence-projection-state` branch. External implementation code review is approved, but
Draft PR #6 is not merged and M3 is not complete.

## Design acceptance

- External architecture review round 1: CHANGES REQUESTED
- Revision commit: `58dfcb82f62e9b70033f8fd7bd4304b3f06388ac`
- External architecture review round 2: APPROVED
- Blocking findings after round 2: 0
- Approved scope: M3 architecture and sequence policy design only
- Implementation status: APPROVED; PENDING MERGE

## Implementation review record

- Implementation base: `3d3a7ee3131e1f8b489f76921a4c69c0fce1ab05`
- Implementation branch: `feat/m3-sequence-projection-state`
- Public header:
  `include/binance_market_data/projection/v1/projection_state/book_projection.hpp`
- Production source: `src/projection_state/book_projection.cpp`
- Tests: direct policy/lifecycle and transition tests, public-header self-containment, an independent
  primitive/vector property model, curated Spot and USD-M replay, and the dedicated
  `bmd_projection_m3_allocation_failure_tests` executable.
- Fuzz: `bmd_projection_book_projection_fuzz` with the independent vector model and checked-in
  `fuzz/corpus/book_projection/` seeds.
- Packaging: the staged downstream consumer includes and links the installed M3 API only through
  `BinanceMarketDataProjection::Core`.
- Local acceptance gates: `scripts/verify.sh` passed on 2026-08-06; local clang-tidy and libFuzzer
  were explicitly skipped by the existing AppleClang rules, while Debug, Release, ASan, UBSan,
  TSan, coverage, benchmark smoke, staged install, and 139 registered tests passed.
- External implementation code review: APPROVED.
- Reviewed implementation head: `7606a60bbb2d2a192f6c0259942174fbd49847ba`.
- Blocking findings: 0.
- Production code changes requested: 0.
- Test changes requested: 0.
- Reviewed CI run: `31083008166`.
- Reviewed CI head: `7606a60bbb2d2a192f6c0259942174fbd49847ba`.
- Reviewed CI result: 8/8 PASS.

This approval covers the reviewed implementation head. Draft PR #6 remains unmerged, M3 remains
incomplete, and this approval-record commit and its resulting PR head require final CI verification
before merge authorization. M4 and later milestones must not start.

## Goals

M3 will deterministically apply validated, ordered Binance depth updates to the M2 `OrderBook`
without depending on wire types, networking, clocks, or ambient runtime state. The Core will:

- model Binance update identifiers and inclusive update ranges without integer-domain ambiguity;
- use an explicit market-specific sequence policy rather than treating all Binance markets alike;
- install a domain baseline and validate the bootstrap bridge;
- validate live continuity, classify stale and duplicate input, and detect gaps;
- maintain the projection lifecycle and keep sequence state consistent with book validity;
- expose deterministic apply results and synchronization-aware read access; and
- use the same state machine for live hosts and replay hosts.

## Non-goals

M3 does not implement or design ownership for the following concerns:

| Owner / milestone | Out of M3 scope |
|---|---|
| Host runtime / M6 | REST snapshot download, WebSocket connection, reconnect, event-buffer runtime, queues, threads, locks, atomics, coroutines, Gateway identity, `connection_generation`, and `session_sequence` |
| M4 adapter and snapshot boundary | Protobuf, generated code, Pydantic, JSON parsing, gRPC, `ConsumerGapNotice`, `StreamStatus`, `LocalOrderBookSnapshot`, `MarketStateSnapshot`, wire metadata, and timestamp production |
| History / infrastructure | Persistence, history storage, logging, telemetry, metrics, and system-time reads |
| Trading systems | Matching, orders, strategy, risk control, and derived market prices |

The Host may buffer network events, obtain a snapshot, normalize wire input into M3 domain values,
feed calls in order, add timestamps to outbound wire contracts, and perform network recovery after
`NeedsResync`. The Core owns none of those runtime activities.

## Source of truth

Sources are applied in this order:

1. Binance official documentation defines exchange update-ID behavior.
2. The fixed Contracts baseline defines cross-module fields and established contract semantics.
3. Projection architecture defines deterministic Core boundaries.
4. ADR-0005 defines the accepted internal M3 decision where the higher sources leave choices open.

A lower-ranked source never silently overrides a higher-ranked source. Differences are recorded in
the evidence and interpretation sections below. Contracts types are semantic references and wire
contracts; they are not Core storage types.

## Evidence reviewed

### Projection and Contracts

The repository API, implementation, tests, property model, fuzz harness, CMake configuration, and
verification scripts were reviewed at Projection commit
`413e3cd9236d0c5de15d4e838149111718260303`. The specified Contracts files and fixtures were read
from the fixed commit `01d76a41929f36d89573159f5f458f9f1e378ada` without changing the Contracts
repository.

The Contracts baseline establishes these relevant facts:

- `Market` identifies only `SPOT` and `USD_M_PERPETUAL`; it cannot identify COIN-M input.
- `DepthUpdate` carries non-negative `first_update_id`, `final_update_id`, optional
  `previous_final_update_id`, and absolute-quantity bid and ask updates.
- `final_update_id >= first_update_id` is required.
- `ExchangeDepthSnapshot` carries `last_update_id`, bids, and asks.
- `LocalOrderBookSnapshot` carries `last_update_id`, `synchronized`, and optional `last_gap`.
- Gateway `session_sequence` is delivery order and is not a Binance update ID.
- Diff-depth identity remains a candidate/TBD rule in Contracts.

### Binance official sequence pages

Access date for every source in this table: **2026-08-06**.

| Page title | Product | Page-displayed Last Modified | Update fields | Bootstrap rule | Live rule / reinitialize condition |
|---|---|---|---|---|---|
| [WebSocket Streams for Binance: How to manage a local order book correctly](https://github.com/binance/binance-spot-api-docs/blob/master/web-socket-streams.md#how-to-manage-a-local-order-book-correctly) | Spot | Not displayed. The [official Spot changelog](https://github.com/binance/binance-spot-api-docs/blob/master/CHANGELOG.md) says Last Updated dates were removed on 2025-11-10 and records a correction to these steps on 2025-11-12. | `U`, `u` | Discard buffered events with `u <= L`; the page says the first remaining event contains `L` in `[U,u]`. | Ignore when `u < current`; a gap exists when `U > current + 1`; discard/restart after a gap. |
| [How to manage a local order book correctly](https://developers.binance.com/docs/derivatives/usds-margined-futures/websocket-market-streams/How-to-manage-a-local-order-book-correctly) | USD(S)-M Futures | August 5, 2026 | `U`, `u`, `pu` | Discard when `u < L`; the first processed event satisfies `U <= L <= u`. | Each new event requires `pu == previous event u`; otherwise reinitialize from the snapshot step. |
| [How to manage a local order book correctly](https://developers.binance.com/docs/derivatives/coin-margined-futures/websocket-market-streams/How-to-manage-a-local-order-book-correctly) | COIN-M Futures | August 5, 2026 | `U`, `u`, `pu` | Discard when `u < L`; the first processed event satisfies `U <= L <= u`. | Each new event requires `pu == previous event u`; otherwise reinitialize from the snapshot step. |

`L` in this document means the snapshot/baseline `last_update_id`. The table paraphrases the
official pages; it does not copy their text as a substitute for the sources.

### Explicit source differences and adopted interpretation

| Difference | Evidence | M3 interpretation | Reason and tests |
|---|---|---|---|
| Spot bootstrap uses `u <= L` as stale, while Spot live handling says ignore only `u < current`. | Current official Spot instructions. | During `AwaitingBridge`, `u < L` is `IgnoredStale` and `u == L` is `IgnoredDuplicate`; both remain unsynchronized. During live operation the same two outcomes remain distinct and equality is not reapplied. | The split preserves the official bootstrap discard set, avoids allowing same-ID/different-content input to rewrite accepted state while identity is TBD, and gives stable observability for equality. Tests lock both branches. |
| Spot bootstrap requires the first surviving interval to contain `L`, while Spot live continuity permits an event beginning exactly at `current + 1`. | Current official Spot instructions. The fixed Contracts handoff fixture records only one `update_id` field after snapshot 500 and cannot establish the original `U/u` interval; it is not evidence for an exact-next bootstrap bridge. | Bootstrap and live are separate rules. A Spot bridge requires `u > L` and `U <= L <= u`; an advancing bootstrap candidate with `U > L` is a gap. Once synchronized, an advancing range is accepted when `U` is no later than the overflow-safe successor of current. | This follows the official contains-`L` bootstrap rule without importing the distinct live predicate. Tests reject `[501,501]` and `[501,502]` against `L=500` during bootstrap, while accepting exact-next during live operation. |
| A USD-M bridge may end exactly at `L`, and Binance snapshots have a finite depth limit. | Official USD-M bootstrap and absolute-quantity rules; the official pages warn that snapshot depth is limited. | A relevant USD-M event satisfying `U <= L == u` applies every absolute-quantity level transactionally, keeps the accepted ID at `L`, and enters `Synchronized` only after commit. | A bridge can carry a changed price outside the retained snapshot depth. Skipping its levels could omit valid state. Focused book-content and allocation-failure tests lock the equality case. |
| Spot live continuity uses an interval; USD-M live continuity uses `pu`. | Official product pages. | Spot uses successor coverage. USD-M requires a present `previous_final_update_id` equal to the last accepted `u`; no additional Spot interval-continuity rule is imposed on USD-M live events. | `pu` is the product-specific continuity anchor for batched Futures intervals. Cross-policy tests ensure the rules cannot be accidentally unified. |

The reviewed COIN-M documentation currently uses the same previous-final continuity pattern as
USD-M. COIN-M is nevertheless not part of the M3 public surface because the frozen Contracts
baseline cannot identify a COIN-M market. Adding it requires an accepted Contracts market
identifier, an explicit adapter mapping, a reviewed public-enum extension, and independent
conformance tests.

If Binance changes a rule, an explicit policy behavior and focused conformance tests will be
reviewed. Existing policy semantics will not be silently changed, and Contracts will not be
modified to hide a source difference.

## Vocabulary

| Term | Definition |
|---|---|
| Baseline | A Core domain value containing the complete book levels supplied to M3 and the Binance update ID through which those levels are valid. It is not a wire snapshot. |
| Snapshot Last Update ID | The Binance `lastUpdateId` from the exchange snapshot, represented in Core as the baseline `last_update_id`; abbreviated `L` only in algorithms. |
| Update Range | A valid-by-construction inclusive Binance interval `[first_update_id, final_update_id]`, also written `[U,u]`, whose invariant is always `first <= final`. It may cover multiple exchange update IDs. |
| First Update ID | `U`, the first Binance update ID represented by a depth batch. |
| Final Update ID | `u`, the last Binance update ID represented by a depth batch. |
| Previous Final Update ID | `pu`, the previous WebSocket event's `u` for Futures; optional at the Contracts boundary and required by the M3 Futures policy for a relevant candidate. |
| Bootstrap | Installing a baseline and proving that buffered WebSocket input connects to it before the projection is exposed as synchronized. |
| Bridge Update | The first non-stale batch accepted against an installed baseline. |
| Live Update | A batch classified after the projection has entered `Synchronized`. |
| Stale Update | A structurally valid batch with `u < current`; it cannot advance the projection. |
| Duplicate Update | A sequence-equality batch classified as `IgnoredDuplicate`; this is not proof of content identity. A relevant USD-M `u == L` candidate in `AwaitingBridge` is instead an equality bridge and is applied. |
| Overlapping Update | A batch whose `U <= current` and whose `u > current`; Spot may accept it because its interval covers the next required ID. |
| Gap | Evidence that a relevant incoming batch cannot continue the selected market policy. A gap invalidates synchronized visibility. |
| Needs Resync | A lifecycle state in which the preserved book is diagnostic-only and normal apply calls are rejected until a new baseline is installed or the projection is reset. |
| Synchronized Projection | A projection with an installed, bridged baseline and no subsequently detected gap. |
| Reset | A deterministic, `noexcept` operation that clears book, sequence, and gap state while retaining `NumericSpec` and policy. |

Binance update IDs, Gateway `session_sequence`, and any future Projection revision are separate
domains. M3 models only Binance update IDs. It neither consumes nor synthesizes the other two.

## Proposed domain types

### Type summary

| Type | Responsibility and fields | Ownership / containers | Copy, move, equality, `noexcept` | Public API |
|---|---|---|---|---|
| `UpdateId` | Strong wrapper around one non-negative Binance ID; field: `uint64_t value` | Owns one scalar; no container | Trivially copyable/movable; total ordering and equality; constructor/accessor `noexcept` | Yes |
| `UpdateRange` | Valid inclusive interval; private fields: `first`, `final`; `try_create` is the only public construction path and enforces `first <= final` | Owns values; no container | Copyable/movable/equality; allocation-free `constexpr` factory and accessors are `noexcept` | Yes |
| `SequencePolicyKind` | Selects `Spot` or `UsdMPerpetual` market semantics represented by the frozen Contracts baseline | Scalar enum | Trivial/equality; all inspection `noexcept` | Yes |
| `ProjectionStatus` | `AwaitingBaseline`, `AwaitingBridge`, `Synchronized`, `NeedsResync` | Scalar enum | Trivial/equality; `noexcept` | Yes |
| `ApplyDisposition` | Stable classification of one apply call | Scalar enum | Trivial/equality; `noexcept` | Yes |
| `GapReason` | Stable reason for loss of policy continuity | Scalar enum | Trivial/equality; `noexcept` | Yes |
| `GapInfo` | Last accepted `u`, incoming range, optional incoming `pu`, reason, and policy | Owns only scalars/`optional`; no time field | Copy/move/equality; expected `noexcept` | Yes |
| `DepthBatch` | Synchronous non-owning view of one normalized depth event: range, optional `pu`, and `span<const LevelUpdate>` | Caller retains level storage for the call; exposes `span`, not a storage container | Copyable view; no value equality promised for the span; construction `noexcept` | Yes |
| `BookBaseline` | Synchronous non-owning view: `last_update_id`, bid span, ask span | Caller retains storage for the call; exposes spans only | Copyable view; no value equality promised; construction `noexcept` | Yes |
| `ApplyResult` | Deterministic outcome, resulting status, resulting accepted ID, and optional gap | Owns scalars/optionals; no strings or containers | Copy/move/equality; expected `noexcept` | Yes |
| `BookProjection` | Owns policy, lifecycle, sequence state, gap evidence, and one M2 `OrderBook` behind a PIMPL | Sole mutable owner; no mutable book view | Non-copyable; movable; destination move preserves state; query functions documented below | Yes |

`DepthBatch` and `BookBaseline` expose only standard non-owning views because M3 does not retain
network buffers or snapshot inputs. `ApplyResult` and `GapInfo` expose no standard storage
container. Book queries retain the existing M2 return-by-value behavior through a const book view.

### Update ID representation decision

| Candidate | Benefits | Problems | Decision |
|---|---|---|---|
| `std::uint64_t` directly | Matches Protobuf `uint64` capacity and has defined unsigned arithmetic | Permits accidental mixing with prices, quantities, Gateway sequence, and arbitrary counters | Rejected as a naked Public API type |
| `std::int64_t` | Matches current price/quantity backing convention | Wastes the non-negative half-range, is narrower than wire `uint64`, and introduces signed-overflow hazards | Rejected |
| Strong wrapper over `std::uint64_t` | Full non-negative contract range, domain separation, defined representation, explicit arithmetic | Adds a small API type | Selected |

Every `uint64_t` value, including zero and `UINT64_MAX`, is a valid `UpdateId`. No implicit
conversion to or from integers is proposed. Algorithms compare wrapped values. They never rely on
signed arithmetic and never evaluate `current + 1` until `current != UINT64_MAX` is proven.

`UpdateRange` is not an aggregate. `try_create(first, final)` returns `nullopt` when `first > final`
and otherwise returns a value whose invariant cannot subsequently be broken. The factory compares
only wrapped scalars, allocates nothing, throws nothing, and is usable in constant evaluation under
C++20. It constructs the private value inside the class and then converts that value to
`std::optional<UpdateRange>`; it does not ask `std::optional` to invoke the private constructor.
Valid ranges are inclusive and may cover many IDs; M3 does not assume that each event represents
exactly one integer.

## Sequence policy abstraction

M3 selects a closed, immutable value policy at `BookProjection` construction:

| Policy kind | Product | Bootstrap anchor | Live continuity anchor | `previous_final_update_id` |
|---|---|---|---|---|
| `Spot` | Binance Spot | Advancing range contains `L`: `U <= L < u` | Advancing range starts no later than the overflow-safe successor of current | Ignored whether absent or present |
| `UsdMPerpetual` | Binance USD(S)-M perpetual | Range contains `L` | Present `pu == current` for every advancing batch | Required for relevant bridge/live candidates |

The implementation will use the market-facing enum and small internal pure classification
functions. An internal Futures classifier may be reusable, but an algorithm category such as
`FuturesPreviousFinal` is not a market identity and is not exposed as the public policy value.

| Mechanism considered | Decision |
|---|---|
| Explicit enum plus pure functions | Selected: closed, testable, allocation-free policy selection with no mutable configuration |
| Public value-type policy object | Not needed in M3 because callers have no legitimate per-rule knobs; the enum is the value |
| Compile-time policy template | Rejected for the public surface because hosts select market at runtime and template instantiations would expand the API |
| Virtual inheritance | Rejected: no open runtime hierarchy is required; it adds allocation/ownership and ABI complexity |
| `std::function` callbacks | Rejected: permits unreviewed behavior and captures ambient state, and may allocate |
| Runtime plugin registration | Rejected: the supported policy set is small and governed by source evidence |
| Global mutable configuration | Rejected: violates determinism and instance isolation |

Supporting another Binance market requires official evidence, a Contracts market identifier, an
explicit adapter mapping, a new reviewed enum value and classification branch, and conformance
tests. M3 is not a plugin framework. COIN-M remains reviewed future-compatibility evidence, not a
current supported policy.

## Lifecycle state machine

Four states are the minimum that distinguish no data, an installed but unbridged baseline, reliable
state, and invalidated state:

| State | Baseline / accepted ID | Normal book visibility | Apply behavior |
|---|---|---|---|
| `AwaitingBaseline` | None | Unavailable | Rejected |
| `AwaitingBridge` | Present | Unavailable; diagnostic-only | Classify stale/duplicate or validate a bridge |
| `Synchronized` | Present | Available | Validate live input |
| `NeedsResync` | Last accepted ID retained | Unavailable; preserved diagnostic book only | Rejected until baseline/reset |

### State transition matrix

`Strong` below means that a thrown allocation exception leaves status, accepted ID, gap, and live
book exactly unchanged. Every `DepthBatch` in this matrix already contains a valid-by-construction
`UpdateRange`; malformed wire values cannot reach these transitions. Result names refer to
`ApplyDisposition` unless an install result is named.

| Old state | Input | Validation | Book change | Sequence change | New state / result | Exception guarantee |
|---|---|---|---|---|---|---|
| none | Construct | Valid `NumericSpec` value and explicit policy | Empty book created | None | `AwaitingBaseline` | Constructor either succeeds or creates no object |
| `AwaitingBaseline` | Install baseline | No state conflict; levels use the projection's numeric context | Atomic `replace_all` | Set `L` | `AwaitingBridge` / `Installed` | Strong |
| `AwaitingBridge` | Install newer baseline | Same rules | Atomically replaces pending baseline | Replace `L` | `AwaitingBridge` / `Installed` | Strong |
| `NeedsResync` | Install new baseline after gap | Same rules | Atomically replaces preserved stale book | Replace accepted ID with new `L`; retain last-gap evidence | `AwaitingBridge` / `Installed` | Strong |
| `Synchronized` | Install baseline without reset | Wrong lifecycle | No | No | `Synchronized` / `RejectedWrongState` install result | No-throw result |
| `AwaitingBridge` | Accept Spot bridge | `u > L` and range contains `L`: `U <= L <= u` | Apply all levels atomically | Set accepted ID to `u` | `Synchronized` / `Applied` | Strong |
| `AwaitingBridge` | Accept Futures bridge with `u > L` | Valid range contains `L`; relevant `pu` present | Apply all levels atomically | Set accepted ID to `u` | `Synchronized` / `Applied` | Strong |
| `AwaitingBridge` | Accept USD-M equality bridge with `u == L` | Range contains `L`; relevant `pu` present | Apply all absolute-quantity levels atomically | ID remains `L`; bridge becomes established only after commit | `Synchronized` / `Applied` | Strong; failure leaves the exact baseline and `AwaitingBridge` state |
| `Synchronized` | Accept live update | Policy continuity succeeds | Apply all levels atomically, including an empty level set | Advance accepted ID to `u` | `Synchronized` / `Applied` | Strong |
| `AwaitingBridge` or `Synchronized` | Stale update (`u < current`) | Range structurally valid; stale classification precedes optional-field requirements | No | No | Same state / `IgnoredStale` | No-throw result |
| `AwaitingBridge` or `Synchronized` | Exact sequence duplicate (`u == current`) | Range structurally valid; in Futures `AwaitingBridge`, equality is the special bridge case above | No | No | Same state / `IgnoredDuplicate` | No-throw result |
| `Synchronized` | Overlapping Spot update (`U <= current < u`) | Range covers successor | Atomic level apply | Advance to `u` | `Synchronized` / `Applied` | Strong |
| `Synchronized` | Spot forward gap | `u > current` and `U` is later than successor | Preserve | Preserve last accepted ID; store `GapInfo` | `NeedsResync` / `GapDetected` | No allocation; no-throw result |
| `AwaitingBridge` | Non-bridgeable candidate | Advancing Spot range has `U > L`, or USD-M range does not contain `L` | Preserve baseline | Preserve `L`; store `GapInfo` | `NeedsResync` / `GapDetected` | No allocation; no-throw result |
| `AwaitingBridge` or `Synchronized` | Futures missing relevant `pu` | Candidate would otherwise need bridge/live validation | Preserve | Preserve; store gap | `NeedsResync` / `GapDetected` | No allocation; no-throw result |
| `Synchronized` | Futures `pu` mismatch | Advancing `u`, present `pu != current` | Preserve | Preserve; store gap | `NeedsResync` / `GapDetected` | No allocation; no-throw result |
| `AwaitingBaseline` or `NeedsResync` | Apply update | Wrong lifecycle | No | No | Same state / `RejectedWrongState` | No-throw result |
| Any live object | Reset | None | Internal `OrderBook::clear()` | Clear accepted ID and last gap | `AwaitingBaseline` | `noexcept` |
| `NeedsResync` | Receive more updates | Wrong lifecycle; no attempt to heal from diffs | No | No | `NeedsResync` / `RejectedWrongState` | No-throw result |
| Any state | Move construction | Source is a valid, non-moved-from projection | Transfer exact book, policy, status, ID, and gap | Transfer exact state | Destination has source's prior state | `noexcept` is expected and tested; source is destruction/assignment-only |
| Any state | Clear | There is no independent public `BookProjection::clear()` | Not expressible | Not expressible | Call `reset()` so book and sequence cannot diverge | N/A |

`clear_side`, `apply_level`, `apply_updates`, and `replace_all` are likewise not exposed through a
mutable book reference. M3 operations are the only mutation surface.

## Bootstrap algorithm

### Ownership boundary

The Host owns the bounded network-event buffer and the snapshot request. Core receives one baseline
and then one candidate `DepthBatch` per call. M3 stores no network events and no unbounded or runtime
queue. This keeps memory policy, overflow recovery, reconnection, and scheduling in the Host while
allowing the exact same calls during replay.

If the Host exhausts its candidate buffer while Core remains `AwaitingBridge`, no synthetic Core
result is generated: absence of input is not a domain event. The Host observes `AwaitingBridge` and
obtains a newer baseline or calls `reset`. If a supplied non-stale candidate proves discontinuity,
Core returns `GapDetected` and enters `NeedsResync`.

Baseline installation and bridge application are separate strongly exception-safe calls. They do
not need one mutating API call because `synchronized_book()` remains unavailable between them; the
two-step bootstrap is observationally atomic to ordinary readers. `diagnostic_book()` intentionally
reveals pending state only to callers that opt into diagnostic semantics.

### Common baseline installation

1. Require `AwaitingBaseline`, `AwaitingBridge`, or `NeedsResync`.
2. Call M2 `replace_all(bids, asks)` using the projection's existing `NumericSpec`.
3. Only after replacement succeeds, store `last_update_id`, set `AwaitingBridge`, and leave the
   selected policy unchanged.
4. Feed buffered candidates in their received order. Core, not the Host, performs policy
   classification.

### Spot bootstrap

1. Ignore `u < L` as stale and `u == L` as a sequence duplicate; both leave the projection in
   `AwaitingBridge`.
2. For the first candidate with `u > L`, require the inclusive interval to contain the snapshot
   ID: `U <= L <= u`. Because `u > L` is already established, the bridge predicate can also be
   written `U <= L < u` and requires no successor arithmetic.
3. If the interval contains `L`, atomically apply all absolute-quantity levels, set the accepted ID
   to `u`, and enter `Synchronized`.
4. If `U > L`, record `SpotBootstrapForwardGap`, enter `NeedsResync`, and preserve the baseline.

An exact-next range beginning at `L + 1` is therefore a Spot bootstrap gap even though an exact-next
range is valid after the projection is synchronized. Bootstrap contains-`L` and live successor
coverage are deliberately different official rules.

### USD-M Futures bootstrap

1. Ignore `u < L` as stale.
2. The first relevant candidate must contain `L`: `U <= L <= u`.
3. The candidate must carry `previous_final_update_id`; it is not compared to `L` for the first
   bridge because it refers to the prior WebSocket event, not to the REST snapshot.
4. For both `u == L` and `u > L`, run the same logical copy-on-apply transaction and apply every
   absolute-quantity level. A REST snapshot has a finite depth limit, so an equality bridge can add
   or update a price that the baseline did not retain.
5. If `u == L`, commit the candidate book, keep the accepted ID at `L`, and then enter
   `Synchronized`. Future live input must have `pu == L`.
6. If `u > L`, commit the candidate book, advance to `u`, and enter `Synchronized`.
7. A missing relevant `pu` or a first relevant range that does not contain `L` records a Futures
   bootstrap gap and enters `NeedsResync`.

If candidate construction or level application throws for an equality bridge, the live baseline
book, accepted ID `L`, status `AwaitingBridge`, gap, and visibility are all unchanged. COIN-M uses a
similar rule in the reviewed official documentation but has no M3 runtime policy because the frozen
Contracts baseline cannot identify it.

## Live update algorithm

### Common preclassification

The following order is stable across policies:

1. Receive a `DepthBatch` whose `UpdateRange` invariant has already been established by its factory.
2. Reject calls in `AwaitingBaseline` or `NeedsResync` as wrong-state input.
3. If `final < current`, return `IgnoredStale`.
4. If `final == current`, return `IgnoredDuplicate`, except that a relevant USD-M candidate in
   `AwaitingBridge` is the equality-bridge transaction defined above.
5. Only an advancing batch (`final > current`) reaches market-specific live-continuity validation.

This ordering prevents a replay/network duplicate from becoming a false Futures `pu` gap merely
because its `pu` points to the event that preceded the duplicate.

M4 converts Contracts/wire fields into M3 values. If `first_update_id > final_update_id`, range
construction fails before `BookProjection::apply()` is called. A malformed wire event cannot be
silently skipped while the projection continues claiming synchronization. The Host must stop
reliable publication for that feed, `reset()` or discard the affected projection, and rebootstrap
before processing later depth input; M4 adapter tests will verify the failed construction and the
Host integration will verify this fail-closed response.

### Spot input classification matrix

| Input relative to current | Classification | Book / ID / status |
|---|---|---|
| `final < current` | `IgnoredStale` | No change |
| `final == current` | `IgnoredDuplicate` | No change; content is not compared |
| `final > current`, `first <= current` | Accepted overlap; it necessarily covers the successor | Atomic apply; ID becomes `final`; remains synchronized |
| `final > current`, `first == successor(current)` | Accepted exact-next | Atomic apply; ID becomes `final`; remains synchronized |
| `final > current`, `first > successor(current)` | `SpotLiveForwardGap` | Preserve book/ID; enter `NeedsResync` |
| Any `previous_final` value or absence | Ignored by Spot | Does not affect classification |

The implementation uses a guarded predicate equivalent to
`first <= current || (current != max && first == current + 1)`. Since an advancing `final` cannot
exist when `current == max`, stale/duplicate classification completes before any successor is
needed.

### Futures input classification matrix

This table applies to `UsdMPerpetual`.

| Input relative to current | `previous_final` | Classification | Book / ID / status |
|---|---|---|---|
| `final < current` | Any / missing | `IgnoredStale` | No change |
| `final == current` | Any / missing | `IgnoredDuplicate` | No change |
| `final > current` | Missing | `FuturesMissingPreviousFinal` gap | Preserve; enter `NeedsResync` |
| `final > current` | Not equal to current | `FuturesPreviousFinalMismatch` gap | Preserve; enter `NeedsResync` |
| `final > current` | Equal to current | Accepted live batch, regardless of whether `U` equals `current + 1` | Atomic apply; ID becomes `final`; remains synchronized |

The range factory already guarantees `first <= final`. USD-M does not add Spot's
interval-continuity predicate after `pu` succeeds. Update IDs are batch intervals, not proof that
every event increments by one. No additional `previous_final <= final` invariant is imposed: the
frozen Contracts baseline does not define it, and for an advancing accepted USD-M input
`previous_final == current < final` follows naturally from the selected live rule.

## Duplicate and identity conflict

M3 distinguishes these concepts:

| Concept | Meaning | M3 behavior |
|---|---|---|
| Sequence duplicate | Incoming `u == current` outside the relevant USD-M equality-bootstrap case | `IgnoredDuplicate`; do not reapply |
| Stale/replay duplicate | Incoming `u < current`, whether seen before or merely older | `IgnoredStale`; no event history lookup |
| Network duplicate | Transport cause for receiving the same sequence again | Same sequence classification; transport cause is not stored |
| Content duplicate | Byte/domain content equals an earlier event | Not computed |
| Same `u`, different content | Candidate identity conflict under a still-TBD Contracts identity rule, outside the baseline-versus-USD-M-equality-bridge case | Not detected by M3; a sequence duplicate is ignored to avoid corrupting accepted state |

M3 will not serialize, hash, or retain event history. Contracts assigns normalized identity conflict
detection to the normalization layer and still marks diff-depth identity as TBD. The Host or future
normalization component may flag an identity conflict before Core input. The consequence is explicit:
M3 can report sequence equality but cannot prove content identity or expose a conflict flag. This is
safer than reapplying different content at an already accepted ID.

## Gap semantics

### Proposed gap value

`GapInfo` contains:

- last accepted final ID;
- incoming first and final IDs;
- optional incoming previous-final ID;
- `GapReason`; and
- `SequencePolicyKind`.

It contains no detection time, receive time, system timestamp, symbol, connection identity, or
Gateway session metadata. A Host/M4 adapter may combine this deterministic value with explicit
metadata when constructing a wire contract.

Proposed reasons are:

- `SpotBootstrapForwardGap`;
- `SpotLiveForwardGap`;
- `FuturesBootstrapRangeMiss`;
- `FuturesMissingPreviousFinal`; and
- `FuturesPreviousFinalMismatch`.

Malformed wire ranges fail M4 adaptation/domain construction before apply and are not fabricated
into a market gap or silently treated as an ignorable market event.

### Behavior after a gap

M3 selects **preserve but quarantine**:

- preserve the last accepted book and accepted ID;
- store `GapInfo` and enter `NeedsResync`;
- make `synchronized_book()` unavailable immediately;
- permit only the explicitly named `diagnostic_book()` const view;
- reject subsequent apply calls without modifying anything; and
- recover only by installing a new baseline (then bridging it) or by `reset`.

A gap is never silently skipped, filled, forward-filled, or followed by a claim of synchronization.
Diffs cannot self-heal a known gap.

## Book visibility while unsynchronized

| Option | Evaluation |
|---|---|
| A: clear immediately after a gap | Prevents stale reads but destroys useful deterministic evidence and is unnecessary for safety if visibility is gated |
| B: preserve but make every query fail | Safe for production use but makes incident diagnosis and state-machine testing unnecessarily opaque |
| C: preserve, gate normal access, and allow an explicit diagnostic query | Selected: reliable access is impossible while evidence remains inspectable |

`synchronized_book()` returns a const reference wrapper only in `Synchronized`. In every other
state it returns `nullopt`. `diagnostic_book()` is deliberately explicit and returns a const view in
all states. No mutable `OrderBook&` is exposed. This avoids wrapper duplication for every M2 query
while making synchronization a required choice at the call site.

The diagnostic view is transient and must not outlive or be used concurrently with a mutating call.
The single-writer Host owns that sequencing. M4 must set `synchronized=false` when it intentionally
adapts diagnostic state; it cannot turn diagnostic access into reliable state.

## Baseline model

`BookBaseline` is a domain view containing only `last_update_id`, bids, and asks. It is not an
`ExchangeDepthSnapshot`, `LocalOrderBookSnapshot`, Pydantic model, or Protobuf message. Venue,
market, symbol, request identity, timestamps, quality flags, and producer metadata remain outside
Core.

- `NumericSpec` is fixed when the projection is constructed and is not repeated in the baseline.
- The adapter/Host must convert text exactly and supply units using that spec. M3 cannot infer scale
  from `PriceUnits` or `QuantityUnits` because M1 deliberately stores scale in context.
- Installation calls M2 `replace_all`.
- Duplicate prices therefore use input-order last-write-wins independently per side.
- Zero quantity removes the price from the replacement; it is never stored.
- A zero for a missing price is a normal no-op.
- Input ordering need not be trusted for storage correctness; M2 emits deterministic bid/ask order.
- Crossed and locked baselines remain accepted under M2 semantics.
- On allocation failure, `replace_all` leaves the old book unchanged; M3 changes `L` and status only
  after it succeeds. Baseline installation therefore has the strong exception guarantee.

## Atomicity and exception safety

M2 facts at the reviewed base are:

- `OrderBook` is non-copyable, owns a PIMPL, and is `noexcept` move-constructible and move-assignable.
- `replace_all` builds temporary maps and has the strong exception guarantee.
- `apply_updates` applies levels in input order and is not transactional if allocation fails.
- `all_levels` returns ordered vectors by value and may allocate.

An advancing sequence ID must never be committed against a partially applied live book, and an
equality bridge must never expose `Synchronized` before all of its levels commit.

### Alternatives matrix

| Alternative | Correctness | Complexity / cost | Decision |
|---|---|---|---|
| A: temporary complete `OrderBook` | Strong if the candidate can be populated and committed by `noexcept` move | Requires reconstructing the current complete state because public copy is deleted | This is the mechanism used by selected option B |
| B: logical copy-on-apply | Read current sides with `all_levels`, construct a candidate with the same spec, `replace_all`, apply the batch to the candidate, then move-assign on success | O(full book) copying and allocations per accepted batch; simple and testable with current M2 API | **Selected M3 correctness baseline** |
| C: undo log | Could be strong only if rollback can never allocate and every change is captured | Reinsertion may allocate; node-handle/allocator design would substantially expand M2 and failure tests | Rejected for M3 |
| D: accept M2 basic guarantee and mark `NeedsResync` after failure | Fail-closed visibility, but leaves a partially changed diagnostic book and makes allocation failure a resync event | Smallest code, weaker recoverability and evidence semantics | Rejected as the baseline; retained only as a future explicitly reviewed fallback if strong commit proves impossible |

### Selected incremental transaction

For every accepted batch that applies levels, including a USD-M equality bridge:

1. Copy current bids and asks into ordered vectors using `all_levels`.
2. Construct a candidate `OrderBook` with the same `NumericSpec`.
3. Populate it with `replace_all`.
4. Call `candidate.apply_updates(batch.levels)`.
5. If any allocation throws, destroy the candidate and propagate the exception. The live projection
   is unchanged.
6. Move-assign the fully updated candidate into the live `OrderBook`; current M2 move assignment is
   `noexcept`.
7. Commit status using non-throwing scalar assignment and set the accepted ID to `u` only when it
   advances. A USD-M equality bridge keeps `L` but still commits the candidate book before entering
   `Synchronized`.

This needs no M2 Public API extension. It prioritizes a verifiable strong guarantee over unmeasured
performance. M5 benchmarks may justify an internal transactional clone/swap or allocator-aware
optimization without changing M3 semantics.

### Exception guarantee matrix

| Operation | Proposed guarantee | Failure state |
|---|---|---|
| Projection construction | Strong construction | No object on failure |
| Baseline installation | Strong | Prior book, status, ID, and gap unchanged |
| Stale/duplicate/wrong-state/gap classification | Non-allocating, expected `noexcept` internally | Deterministic result; gap transition is fully committed |
| Accepted bridge or live apply, including USD-M `u == L` | Strong | Prior book, status, ID, gap, and visibility unchanged; `std::bad_alloc` propagates. Equality-bridge failure remains exactly `AwaitingBridge` with ID `L` |
| Result construction | Non-allocating value construction | No failure expected |
| Reset | `noexcept` | Empty book, no ID/gap, `AwaitingBaseline` |
| Queries returning M2 vectors | Existing allocation behavior | Projection unchanged if allocation throws |

No path advances `last_update_id` before the book commit.

## Approved public API

The declarations below are implemented in the public header named in the implementation review
record. Their implementation remains **In external code review**.

```cpp
namespace binance_market_data::projection::v1 {

class UpdateId final {
  public:
    explicit constexpr UpdateId(std::uint64_t value) noexcept;
    [[nodiscard]] constexpr std::uint64_t value() const noexcept;
    friend constexpr auto operator<=>(UpdateId, UpdateId) noexcept = default;
};

class UpdateRange final {
  public:
    [[nodiscard]] static constexpr std::optional<UpdateRange>
    try_create(UpdateId first, UpdateId final) noexcept;

    [[nodiscard]] constexpr UpdateId first() const noexcept;
    [[nodiscard]] constexpr UpdateId final() const noexcept;
    friend constexpr bool operator==(const UpdateRange&, const UpdateRange&) noexcept = default;

  private:
    constexpr UpdateRange(UpdateId first, UpdateId final) noexcept;

    UpdateId first_;
    UpdateId final_;
};

enum class SequencePolicyKind : std::uint8_t {
    Spot,
    UsdMPerpetual,
};

enum class ProjectionStatus : std::uint8_t {
    AwaitingBaseline,
    AwaitingBridge,
    Synchronized,
    NeedsResync,
};

enum class ApplyDisposition : std::uint8_t {
    Applied,
    IgnoredStale,
    IgnoredDuplicate,
    GapDetected,
    RejectedWrongState,
};

enum class GapReason : std::uint8_t {
    SpotBootstrapForwardGap,
    SpotLiveForwardGap,
    FuturesBootstrapRangeMiss,
    FuturesMissingPreviousFinal,
    FuturesPreviousFinalMismatch,
};

struct GapInfo final {
    UpdateId last_accepted_final;
    UpdateRange incoming_range;
    std::optional<UpdateId> incoming_previous_final;
    GapReason reason;
    SequencePolicyKind policy;
    friend constexpr bool operator==(const GapInfo&, const GapInfo&) noexcept = default;
};

struct DepthBatch final {
    UpdateRange range;
    std::optional<UpdateId> previous_final;
    std::span<const LevelUpdate> levels;
};

struct BookBaseline final {
    UpdateId last_update_id;
    std::span<const BookLevel> bids;
    std::span<const BookLevel> asks;
};

enum class InstallDisposition : std::uint8_t {
    Installed,
    RejectedWrongState,
};

struct InstallResult final {
    InstallDisposition disposition;
    ProjectionStatus status_after;
    std::optional<UpdateId> last_update_id_after;
    friend constexpr bool operator==(const InstallResult&, const InstallResult&) noexcept = default;
};

struct ApplyResult final {
    ApplyDisposition disposition;
    ProjectionStatus status_after;
    std::optional<UpdateId> last_update_id_after;
    std::optional<GapInfo> gap;
    friend constexpr bool operator==(const ApplyResult&, const ApplyResult&) noexcept = default;
};

class BookProjection final {
  public:
    explicit BookProjection(NumericSpec numeric_spec, SequencePolicyKind policy);
    ~BookProjection();

    BookProjection(BookProjection&&) noexcept;
    BookProjection& operator=(BookProjection&&) noexcept;
    BookProjection(const BookProjection&) = delete;
    BookProjection& operator=(const BookProjection&) = delete;

    [[nodiscard]] NumericSpec numeric_spec() const noexcept;
    [[nodiscard]] SequencePolicyKind policy() const noexcept;
    [[nodiscard]] ProjectionStatus status() const noexcept;
    [[nodiscard]] std::optional<UpdateId> last_update_id() const noexcept;
    [[nodiscard]] std::optional<GapInfo> last_gap() const noexcept;

    [[nodiscard]] InstallResult install_baseline(BookBaseline baseline);
    [[nodiscard]] ApplyResult apply(DepthBatch batch);
    void reset() noexcept;

    [[nodiscard]] std::optional<std::reference_wrapper<const OrderBook>>
    synchronized_book() const noexcept;
    [[nodiscard]] const OrderBook& diagnostic_book() const noexcept;

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace binance_market_data::projection::v1
```

Design choices embodied by the draft:

- The public header directly includes the standard declarations it uses, including `<compare>`,
  `<cstdint>`, `<functional>`, `<memory>`, `<optional>`, and `<span>`, and remains self-contained.
- `UpdateRange::try_create` is a small allocation-free C++20 `constexpr` factory. It returns
  `nullopt` for `first > final`; it does not throw for normal range validation and does not require
  a custom expected/result framework.
- `first()` and `final()` follow the repository's noun-style `value()` and `numeric_spec()` getter
  convention; no `get_` prefix or public data members are introduced.
- Default construction is forbidden because numeric and policy context are mandatory.
- Policy and numeric context have no mutator and are logically immutable for object lifetime.
- Copy is forbidden because the projection has single ownership and copying a large book should not
  be implicit.
- Move follows current `OrderBook` style. Tests will verify destination state; a moved-from object is
  only destroyed or move-assigned.
- A PIMPL follows the existing `OrderBook` compiler-firewall style and keeps lifecycle storage out
  of the Public Header; it does not conceal wire or Host dependencies.
- The projection does not duplicate every M2 query. It provides gated and diagnostic const views.
- No mutable book reference, output parameter, policy callback, or boolean mode flag is exposed.
- Expected market classifications are returned as values. Allocation failures remain exceptions;
  business results are not encoded in exception text.

## Apply result semantics

| Disposition | Book changes | ID advances | Status changes | Carries `GapInfo` | Classification |
|---|---:|---:|---:|---:|---|
| `Applied` | May change for every accepted transaction. A USD-M equality bridge applies all levels; an empty/no-op batch may leave content equal | To `u` only when greater; equality bridge remains `L` | `AwaitingBridge` may become `Synchronized` only after book commit | No | Normal market input |
| `IgnoredStale` | No | No | No | No | Normal replay/network condition |
| `IgnoredDuplicate` | No | No | No | No | Normal sequence condition; not content identity proof |
| `GapDetected` | No | No | To `NeedsResync` | Yes | Normal adverse market/input continuity condition |
| `RejectedWrongState` | No | No | No | No | Lifecycle misuse or input received while recovery is required |

Every result reports the resulting status and accepted ID, making transcripts directly comparable.
Only `GapDetected` carries `GapInfo`, and its `gap` must be present. For every other disposition it
must be absent. These are constructor/implementation invariants tested exhaustively.

## Overflow and boundary behavior

| Input | Behavior |
|---|---|
| `UpdateId{0}` | Valid baseline or event ID |
| `UpdateId{UINT64_MAX}` | Valid. In `Synchronized`, all representable incoming finals are stale or duplicate and no successor arithmetic occurs; USD-M equality-bootstrap behavior is listed separately below |
| `UpdateRange::try_create(first, final)` with `first > final` | Returns `nullopt`; no `UpdateRange` exists and no projection call is possible |
| `current + 1` would overflow | The algorithm first establishes whether `final > current`; this is impossible at max. A guarded successor predicate is used elsewhere |
| Baseline `L == UINT64_MAX` | Spot has no representable advancing bridge; `u == L` is duplicate and lower `u` is stale. USD-M may still apply an equality bridge without successor arithmetic |
| `previous_final` greater than `final` | No standalone structural rule. Spot ignores the field; USD-M stale/duplicate classification precedes `pu`, and an advancing accepted event requires only `pu == current` |
| Extremely wide valid range | Classified as an inclusive range; M3 neither expands nor iterates IDs |
| Empty bids/asks | Valid; an accepted batch may advance the ID without level changes |
| Duplicate prices in one update | M2 ordered application gives same-price last-write-wins |
| Zero quantity | Deletes the level; missing delete is a no-op |

Unsigned wraparound is defined by C++, but M3 does not use it as successor semantics.

## Determinism and replay invariants

Given identical `NumericSpec`, `SequencePolicyKind`, baseline, and ordered domain calls, live and
replay executions must produce identical:

- `ProjectionStatus` after every call;
- accepted `last_update_id`;
- bid and ask levels in deterministic M2 order;
- `InstallResult` and `ApplyResult` sequence; and
- `GapInfo` and retained diagnostic book.

Results do not depend on system time, thread scheduling, logging, network state, process identity,
randomness, addresses, environment, or unstable hash-map iteration. The selected sequence state is
changed only by explicit calls. M3 introduces no unordered container to an externally observable
path.

## Testing strategy

### Unit test matrix

| Area | Required cases |
|---|---|
| Update ID/range | IDs zero and max; ranges `[0,0]`, `[0,max]`, `[max,max]`, and `first < final` construct successfully; `try_create(first > final)` returns `nullopt`; range accessors/equality; guarded live successor at max |
| Spot bootstrap | `L=500`: `[499,501]` and `[500,501]` apply; `[501,501]` and `[501,502]` detect `SpotBootstrapForwardGap`; `u=500` is duplicate; `u=499` is stale; empty-level contains-`L` bridge; `L=max` cannot advance |
| Spot live | exact-next, overlap, stale, equality duplicate, forward gap, max final, previous-final present but ignored |
| USD-M bootstrap | contains-`L` bridge with `u > L`; `U <= L == u` applies every level; equality bridge adds a deeper price absent from the limited baseline; ID remains `L`; missing `pu`; range miss; exact-next range miss; allocation failure preserves the exact pending baseline |
| USD-M live | present `pu == current`; missing `pu`; mismatched `pu`; stale and duplicate classified before `pu`; empty advancing update |
| API scope | `SequencePolicyKind` has only `Spot` and `UsdMPerpetual`; no COIN-M M3 public value; public-header self-containment includes `std::optional` support |
| Adapter boundary documentation | Malformed Contracts/wire `first_update_id > final_update_id` fails range construction before domain apply; no fail-open projection result exists. M4 adapter implementation is not part of M3 |
| Lifecycle | construction, wrong-state apply, install, replace pending baseline, bridge, gap, reject after gap, install after gap, reset from every state, reinstallation |
| Book semantics through projection | mixed bid/ask, zero deletion, missing delete, duplicate-price last-write-wins, empty batch advances, crossed and locked book retained |
| Context and ownership | `NumericSpec` retained through baseline/reset/move, policy retained and immutable, move destination exact, no copy, no mutable book exposure |
| Visibility | synchronized view absent before bridge and after gap, present only when synchronized, diagnostic view preserved after gap |
| Results | every disposition's status/ID/gap invariants and equality |

Every row in the state transition matrix receives at least one direct test. Cross-policy table tests
feed the same range/`pu` values to Spot and USD-M and assert their intentionally different outcomes.

### Independent property model

The test reference model will use primitive `uint64_t`, its own valid-range construction, a
vector-based reference book, and independently written decision tables. It must not call production
policy helpers, successor predicates, `BookProjection`, the production range factory, or M2 map
logic. Range-factory properties are tested separately; the model never passes an invalid range to
production apply.

Deterministic generators will produce:

- legal continuous Spot and USD-M sequences;
- valid batch intervals and overlapping ranges;
- Spot bootstrap intervals that contain `L` and exact-next bootstrap intervals that must gap;
- stale and equality-duplicate batches;
- Spot forward gaps;
- USD-M missing/mismatched `pu` and equality bridges whose levels change the reference book without
  advancing the ID;
- reset and new-baseline operations;
- empty batches; and
- mixed bid/ask insert, replacement, and deletion updates.

After every operation, compare production and reference status, result, accepted ID, gap, visible
state, and complete diagnostic book. Assert that no gap/wrong-state operation changes the book or
advances the ID, that an equality bridge can change the book without advancing the ID, and that only
synchronized state exposes the normal view. COIN-M official evidence is not instantiated as an M3
runtime model. Seeds and operation indexes must be printed on failure.

### Model-based fuzz harness

The M3 harness decodes these operations:

- `InstallBaseline`;
- `ApplySpotUpdate`;
- `ApplyUsdMUpdate`;
- `Reset`;
- `Query`;
- `Gap`;
- `Duplicate`; and
- `Stale`.

One projection instance has a fixed policy; operations for a different policy are applied to a
separate instance or decoded as reference-only comparisons, never by mutating policy. The harness
will compare the production machine with an independent vector/reference model after each step and
abort on any mismatch.

An independent range-factory fuzz input may exercise arbitrary `(first, final)` pairs, but a failed
factory result is never forwarded to projection apply. Seed corpus categories will include: empty
baseline, zero/max IDs, Spot contains-`L` overlap bridge, Spot exact-next bootstrap gap, Spot
exact-next live acceptance, USD-M `u > L` bridge, USD-M equality bridge that adds a
snapshot-depth-excluded price, multi-ID live ranges, empty advancing update, duplicate, stale,
forward gap, `pu` mismatch, missing `pu`, reset/rebaseline, crossed book, zero delete, missing delete,
duplicate price, and query in every state.

### Replay determinism

Each curated Spot and USD-M transcript is replayed into two fresh projections. The corpus includes
a Spot exact-next bootstrap gap, a Spot exact-next live update, and a USD-M equality bridge whose
levels change the book while its ID remains `L`. The complete result sequence and every checkpoint
listed in the determinism invariants must compare equal. A second test resets and replays into the
same projection and compares with a fresh instance.

### Executable allocation-failure tests

Exception tests will live in a dedicated test executable that overrides allocation with a
deterministic fail-after counter; production code receives no failpoint API. Separate scenarios
cover baseline installation, a Spot accepted bridge, a Spot accepted live batch, a USD-M `u > L`
bridge, a USD-M `u == L` equality bridge, and a USD-M accepted live batch. Each scenario first
counts successful transaction allocations, then sweeps every failure position through
`all_levels`, candidate construction, `replace_all`, and candidate `apply_updates`.

After each caught `std::bad_alloc`, the test compares status, ID, gap, bids, asks, synchronized-view
availability, and diagnostic-view contents with an exact pre-call checkpoint. Equality-bridge
failures must remain `AwaitingBridge` with ID `L` and the unchanged baseline book. A final
no-failure run must apply successfully and prove that the equality bridge can change book content
without advancing the ID. The test only claims a failure point when the counter actually fires, so
it cannot pass without exercising allocation failure. Sanitizer runs execute these tests as normal.

### Test deliverable matrix

| Deliverable | Oracle | Primary failure found |
|---|---|---|
| Unit tests | Explicit expected table | Boundary and policy regressions |
| State-transition tests | Complete transition matrix | Illegal lifecycle mutation |
| Property tests | Independent primitive/vector model | Composed sequence/book divergence |
| Model fuzz | Independent decoded model plus separate range-factory input | Unexpected operation sequences and domain-construction boundaries |
| Replay determinism | Full transcript/checkpoint equality | Ambient or unstable behavior |
| Allocation-failure sweep | Exact pre-call checkpoint | Book/sequence partial commit |

## Acceptance gates

The M3 implementation is not complete until all of these gates pass:

- public-header self-containment;
- Debug and Release builds/tests;
- Ubuntu GCC and Ubuntu Clang;
- macOS AppleClang;
- ASan and UBSan;
- clang-format;
- clang-tidy with warnings as errors;
- staged install consumer;
- complete state-transition and unit tests;
- independent property tests;
- model-based fuzz and seed corpus;
- deterministic replay tests;
- executable allocation-failure tests; and
- independent external code review.

Local gates have passed subject to the explicit AppleClang skips recorded above. Linux
clang-tidy/libFuzzer, the full pull-request matrix, and independent external code review remain
pending and are required before M3 can be marked complete.

## Implementation sequence

Implementation uses the separate `feat/m3-sequence-projection-state` branch and follows these
reviewable steps:

1. Add `UpdateId`, `UpdateRange`, enum values, and header self-containment tests.
2. Implement independent pure Spot and USD-M classification functions and table tests.
3. Add projection lifecycle/value results without book mutation.
4. Add baseline installation and strong-guarantee tests.
5. Add copy-on-apply incremental transaction and sequence commit.
6. Add gap storage, visibility gating, reset, and recovery transitions.
7. Complete const query views and move behavior.
8. Complete unit and full transition-matrix tests.
9. Implement the independent property/reference model.
10. Add model-based fuzz harness and seed corpus.
11. Add deterministic transcript replay and allocation-failure sweep.
12. Update implementation documentation, run all acceptance gates, and obtain external review.

Steps 1 through 11 and the documentation/local-validation portions of step 12 are represented in
the implementation review branch. Passing CI and external code review remain outstanding
acceptance gates.

## Alternatives rejected

| Alternative | Reason rejected |
|---|---|
| One continuous-ID rule for all markets | Erases Spot interval versus Futures `pu` semantics |
| Sequencing only in Gateway | Replay would need different logic and Core could accept unverified mutation |
| Contracts/Pydantic/Protobuf types directly in Core | Violates the Core/wire boundary and creates an external runtime dependency |
| Core downloads snapshots | Introduces networking, clock/runtime policy, and non-deterministic recovery into Core |
| Core owns the bootstrap event buffer | Couples memory/runtime overflow policy to the deterministic state machine |
| Continue applying after a known gap | Can never truthfully claim a continuous local book |
| Default-expose the old book after a gap | Lets ordinary callers mistake stale state for reliable state |
| Clear immediately after every gap | Destroys diagnostic evidence without improving safety over gated visibility |
| Put system time in `GapInfo` | Core must not read a clock; the Host can add explicit time at the adapter boundary |
| Expose mutable `OrderBook&` | Allows bypassing sequence policy and creates permanent sequence/book divergence |
| Virtual/plugin/callback policy framework | Adds open behavior, allocation, and ambient-state risks for two closed policy kinds |
| Store all event history | Unbounded memory and identity concerns are outside M3 |
| Silently content-deduplicate every same-ID event | Content identity is unproven/TBD and would require history/hash/serialization |
| Advance sequence before applying levels | Allocation failure would permanently associate the wrong book with the new ID |
| Accept basic guarantee without invalidating reliability | A partially updated book could remain falsely synchronized |
| Use signed update IDs | Narrows the Contracts range and creates signed-overflow hazards |
| Return log text as business outcome | Unstable, untyped, and violates the no-logging Core boundary |

## Open questions

No unresolved M3 architecture question remains in this proposal. In particular, the cross-market
bootstrap difference, equality classification, gap visibility, buffering boundary, and atomicity
baseline all have recommended decisions and executable tests above. Therefore this design does not
add an `O-P004` entry to `docs/OPEN_QUESTIONS.md`.

Implementation blocker count from unresolved technical questions: **0**. External Architecture and
Sequence Policy Review Round 2 is recorded as approved with no blocking findings. The separate
implementation branch now exists and remains subject to its validation cycle and external code
review.
