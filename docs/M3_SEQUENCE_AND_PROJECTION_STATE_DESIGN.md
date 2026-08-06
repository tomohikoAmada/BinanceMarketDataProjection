# M3 Sequence and Projection State Design

## Status

- Design status: **IN REVIEW**
- Implementation status: **NOT STARTED**
- ADR status: **PROPOSED**
- Design date: 2026-08-06
- Projection base: `413e3cd9236d0c5de15d4e838149111718260303`
- Contracts baseline: `01d76a41929f36d89573159f5f458f9f1e378ada`

Every C++ declaration in this document is **Proposed**, **Not implemented**, and **Subject to
external review**. This document does not authorize an implementation branch. An independent
external Architecture and Sequence Policy Review must approve this design before work may start on
`feat/m3-sequence-projection-state`.

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
4. ADR-0005 defines the proposed internal M3 decision where the higher sources leave choices open.

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
| Spot bootstrap text says the first surviving interval contains `L`, while the live gap rule permits an event beginning exactly at `L + 1`. | Current official Spot instructions; the fixed Contracts handoff also models snapshot 500 followed by update 501. | Spot accepts a bridge when `u > L` and the inclusive range covers the successor of `L`: `U <= L + 1 <= u`. This includes both overlap (`U <= L`) and exact-next (`U == L + 1`). | Exact-next is continuous under the official live rule and must not be manufactured into a gap. Tests separately lock overlapping and exact-next bridges. Arithmetic is guarded against overflow. |
| Futures bootstrap uses `u < L` and `U <= L <= u`, not Spot's effective `u > L` successor rule. | Both official Futures pages. | Futures requires the first non-stale event to contain `L`. An event ending exactly at `L` may establish the bridge without mutating the baseline; an exact-next interval beginning at `L + 1` is not accepted as the Futures bootstrap bridge. | This follows the product-specific official bootstrap text instead of importing the Spot interpretation. Tests lock equality and the exact-next rejection. |
| Spot live continuity uses an interval; Futures live continuity uses `pu`. | Official product pages. | Spot uses successor coverage. USD-M and COIN-M require a present `previous_final_update_id` equal to the last accepted `u`; no additional Spot interval-continuity rule is imposed on Futures live events. | `pu` is the product-specific continuity anchor for batched Futures intervals. Cross-policy tests ensure the rules cannot be accidentally unified. |

If Binance changes a rule, a new explicit `SequencePolicyKind` behavior and focused conformance
tests will be reviewed. Existing policy semantics will not be silently changed, and Contracts will
not be modified to hide a source difference.

## Vocabulary

| Term | Definition |
|---|---|
| Baseline | A Core domain value containing the complete book levels supplied to M3 and the Binance update ID through which those levels are valid. It is not a wire snapshot. |
| Snapshot Last Update ID | The Binance `lastUpdateId` from the exchange snapshot, represented in Core as the baseline `last_update_id`; abbreviated `L` only in algorithms. |
| Update Range | The inclusive Binance interval `[first_update_id, final_update_id]`, also written `[U,u]`. It may cover multiple exchange update IDs. |
| First Update ID | `U`, the first Binance update ID represented by a depth batch. |
| Final Update ID | `u`, the last Binance update ID represented by a depth batch. |
| Previous Final Update ID | `pu`, the previous WebSocket event's `u` for Futures; optional at the Contracts boundary and required by the M3 Futures policy for a relevant candidate. |
| Bootstrap | Installing a baseline and proving that buffered WebSocket input connects to it before the projection is exposed as synchronized. |
| Bridge Update | The first non-stale batch accepted against an installed baseline. |
| Live Update | A batch classified after the projection has entered `Synchronized`. |
| Stale Update | A structurally valid batch with `u < current`; it cannot advance the projection. |
| Duplicate Update | A structurally valid batch with `u == current`; this is sequence equality, not proof of content identity. |
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
| `UpdateRange` | Inclusive candidate interval; fields: `first`, `final`; semantic validity requires `first <= final` before policy evaluation | Owns values | Trivially copyable/movable/equality; validation `noexcept` | Yes |
| `SequencePolicyKind` | Selects `Spot`, `UsdMPerpetual`, or `CoinMPerpetual` semantics | Scalar enum | Trivial/equality; all inspection `noexcept` | Yes |
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

`UpdateRange` is an input aggregate so malformed `first > final` can be represented long enough to
return `RejectedInvalidRange` deterministically. The projection validates it before inspecting
state-specific continuity or levels. Valid ranges are inclusive and may cover many IDs; M3 does not
assume that each event represents exactly one integer.

## Sequence policy abstraction

M3 selects a closed, immutable value policy at `BookProjection` construction:

| Policy kind | Product | Bootstrap anchor | Live continuity anchor | `previous_final_update_id` |
|---|---|---|---|---|
| `Spot` | Binance Spot | Range covers the successor of `L`, with `u > L` | Range covers the successor of current | Ignored whether absent or present |
| `UsdMPerpetual` | Binance USD(S)-M perpetual | Range contains `L` | Present `pu == current` for every advancing batch | Required for relevant bridge/live candidates |
| `CoinMPerpetual` | Binance COIN-M perpetual | Range contains `L` | Present `pu == current` for every advancing batch | Required for relevant bridge/live candidates |

The implementation will use an enum and small internal pure classification functions. USD-M and
COIN-M share an algorithm initially but retain distinct enum values so diagnostics, tests, and a
future official divergence remain explicit.

| Mechanism considered | Decision |
|---|---|
| Explicit enum plus pure functions | Selected: closed, testable, allocation-free policy selection with no mutable configuration |
| Public value-type policy object | Not needed in M3 because callers have no legitimate per-rule knobs; the enum is the value |
| Compile-time policy template | Rejected for the public surface because hosts select market at runtime and template instantiations would expand the API |
| Virtual inheritance | Rejected: no open runtime hierarchy is required; it adds allocation/ownership and ABI complexity |
| `std::function` callbacks | Rejected: permits unreviewed behavior and captures ambient state, and may allocate |
| Runtime plugin registration | Rejected: the supported policy set is small and governed by source evidence |
| Global mutable configuration | Rejected: violates determinism and instance isolation |

Supporting another Binance market requires official evidence, a new explicit enum value, a reviewed
classification branch, and conformance tests. M3 is not a plugin framework.

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
book exactly unchanged. Result names refer to `ApplyDisposition` unless an install result is named.

| Old state | Input | Validation | Book change | Sequence change | New state / result | Exception guarantee |
|---|---|---|---|---|---|---|
| none | Construct | Valid `NumericSpec` value and explicit policy | Empty book created | None | `AwaitingBaseline` | Constructor either succeeds or creates no object |
| `AwaitingBaseline` | Install baseline | No state conflict; levels use the projection's numeric context | Atomic `replace_all` | Set `L` | `AwaitingBridge` / `Installed` | Strong |
| `AwaitingBridge` | Install newer baseline | Same rules | Atomically replaces pending baseline | Replace `L` | `AwaitingBridge` / `Installed` | Strong |
| `NeedsResync` | Install new baseline after gap | Same rules | Atomically replaces preserved stale book | Replace accepted ID with new `L`; retain last-gap evidence | `AwaitingBridge` / `Installed` | Strong |
| `Synchronized` | Install baseline without reset | Wrong lifecycle | No | No | `Synchronized` / `RejectedWrongState` install result | No-throw result |
| `AwaitingBridge` | Accept Spot bridge | Valid range; `u > L`; range covers successor of `L` | Apply all levels atomically | Set accepted ID to `u` | `Synchronized` / `Applied` | Strong |
| `AwaitingBridge` | Accept Futures bridge with `u > L` | Valid range contains `L`; relevant `pu` present | Apply all levels atomically | Set accepted ID to `u` | `Synchronized` / `Applied` | Strong |
| `AwaitingBridge` | Accept Futures bridge with `u == L` | Valid range contains `L`; relevant `pu` present | No; snapshot already includes updates through `L` | ID remains `L`; bridge becomes established | `Synchronized` / `Applied` | No allocation; no-throw result |
| `Synchronized` | Accept live update | Policy continuity succeeds | Apply all levels atomically, including an empty level set | Advance accepted ID to `u` | `Synchronized` / `Applied` | Strong |
| `AwaitingBridge` or `Synchronized` | Stale update (`u < current`) | Range structurally valid; stale classification precedes optional-field requirements | No | No | Same state / `IgnoredStale` | No-throw result |
| `AwaitingBridge` or `Synchronized` | Exact sequence duplicate (`u == current`) | Range structurally valid; in Futures `AwaitingBridge`, equality is the special bridge case above | No | No | Same state / `IgnoredDuplicate` | No-throw result |
| `Synchronized` | Overlapping Spot update (`U <= current < u`) | Range covers successor | Atomic level apply | Advance to `u` | `Synchronized` / `Applied` | Strong |
| `Synchronized` | Spot forward gap | `u > current` and `U` is later than successor | Preserve | Preserve last accepted ID; store `GapInfo` | `NeedsResync` / `GapDetected` | No allocation; no-throw result |
| `AwaitingBridge` | Non-bridgeable candidate | Spot misses successor or Futures does not contain `L` | Preserve baseline | Preserve `L`; store `GapInfo` | `NeedsResync` / `GapDetected` | No allocation; no-throw result |
| `AwaitingBridge` or `Synchronized` | Futures missing relevant `pu` | Candidate would otherwise need bridge/live validation | Preserve | Preserve; store gap | `NeedsResync` / `GapDetected` | No allocation; no-throw result |
| `Synchronized` | Futures `pu` mismatch | Advancing `u`, present `pu != current` | Preserve | Preserve; store gap | `NeedsResync` / `GapDetected` | No allocation; no-throw result |
| Any state | Invalid range (`first > final`), or Futures `pu > final` | Structural validation occurs before lifecycle/policy validation | No | No | Same state / `RejectedInvalidRange` | No-throw result |
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

1. For each structurally valid candidate, ignore `u < L` as stale and `u == L` as a sequence
   duplicate.
2. For the first candidate with `u > L`, determine whether it covers the successor of `L` without
   unguarded addition.
3. If it does, atomically apply its absolute-quantity levels, set the accepted ID to `u`, and enter
   `Synchronized`.
4. If it starts after the successor, record `SpotBootstrapForwardGap`, enter `NeedsResync`, and do
   not change the book.

This accepts both an overlapping bridge and an exact-next bridge and records why that interpretation
was selected in the source-difference table.

### Futures bootstrap (USD-M and COIN-M)

1. Ignore `u < L` as stale.
2. The first relevant candidate must contain `L`: `U <= L <= u`.
3. The candidate must carry `previous_final_update_id`; it is not compared to `L` for the first
   bridge because it refers to the prior WebSocket event, not to the REST snapshot.
4. If `u == L`, establish synchronization without reapplying levels already represented by the
   baseline. Future live input must then have `pu == L`.
5. If `u > L`, atomically apply levels, advance to `u`, and enter `Synchronized`.
6. A missing relevant `pu` or a first relevant range that does not contain `L` records a Futures
   bootstrap gap and enters `NeedsResync`.

## Live update algorithm

### Common preclassification

The following order is stable across policies:

1. Reject `first > final`. For Futures, also reject a present `previous_final > final` as invalid
   metadata. Spot does not interpret `previous_final`.
2. Reject calls in `AwaitingBaseline` or `NeedsResync` as wrong-state input.
3. If `final < current`, return `IgnoredStale`.
4. If `final == current`, return `IgnoredDuplicate`.
5. Only an advancing batch (`final > current`) reaches market-specific continuity validation.

This ordering prevents a replay/network duplicate from becoming a false Futures `pu` gap merely
because its `pu` points to the event that preceded the duplicate.

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

This table applies independently to `UsdMPerpetual` and `CoinMPerpetual`.

| Input relative to current | `previous_final` | Classification | Book / ID / status |
|---|---|---|---|
| `final < current` | Any / missing | `IgnoredStale` | No change |
| `final == current` | Any / missing | `IgnoredDuplicate` | No change |
| `final > current` | Missing | `FuturesMissingPreviousFinal` gap | Preserve; enter `NeedsResync` |
| `final > current` | Not equal to current | `FuturesPreviousFinalMismatch` gap | Preserve; enter `NeedsResync` |
| `final > current` | Equal to current | Accepted live batch, regardless of whether `U` equals `current + 1` | Atomic apply; ID becomes `final`; remains synchronized |

Futures still requires `first <= final`. It does not add Spot's interval-continuity predicate after
`pu` succeeds. Update IDs are batch intervals, not proof that every event increments by one.

## Duplicate and identity conflict

M3 distinguishes these concepts:

| Concept | Meaning | M3 behavior |
|---|---|---|
| Sequence duplicate | Incoming `u == current` | `IgnoredDuplicate`; do not reapply |
| Stale/replay duplicate | Incoming `u < current`, whether seen before or merely older | `IgnoredStale`; no event history lookup |
| Network duplicate | Transport cause for receiving the same sequence again | Same sequence classification; transport cause is not stored |
| Content duplicate | Byte/domain content equals an earlier event | Not computed |
| Same `u`, different content | Candidate identity conflict under a still-TBD Contracts identity rule | Not detected by M3; it is ignored by sequence to avoid corrupting accepted state |

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

Malformed ranges return `RejectedInvalidRange` and are not fabricated into a market gap.

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

An advancing sequence ID must never be committed against a partially applied live book.

### Alternatives matrix

| Alternative | Correctness | Complexity / cost | Decision |
|---|---|---|---|
| A: temporary complete `OrderBook` | Strong if the candidate can be populated and committed by `noexcept` move | Requires reconstructing the current complete state because public copy is deleted | This is the mechanism used by selected option B |
| B: logical copy-on-apply | Read current sides with `all_levels`, construct a candidate with the same spec, `replace_all`, apply the batch to the candidate, then move-assign on success | O(full book) copying and allocations per advancing batch; simple and testable with current M2 API | **Selected M3 correctness baseline** |
| C: undo log | Could be strong only if rollback can never allocate and every change is captured | Reinsertion may allocate; node-handle/allocator design would substantially expand M2 and failure tests | Rejected for M3 |
| D: accept M2 basic guarantee and mark `NeedsResync` after failure | Fail-closed visibility, but leaves a partially changed diagnostic book and makes allocation failure a resync event | Smallest code, weaker recoverability and evidence semantics | Rejected as the baseline; retained only as a future explicitly reviewed fallback if strong commit proves impossible |

### Selected incremental transaction

For every advancing accepted batch:

1. Copy current bids and asks into ordered vectors using `all_levels`.
2. Construct a candidate `OrderBook` with the same `NumericSpec`.
3. Populate it with `replace_all`.
4. Call `candidate.apply_updates(batch.levels)`.
5. If any allocation throws, destroy the candidate and propagate the exception. The live projection
   is unchanged.
6. Move-assign the fully updated candidate into the live `OrderBook`; current M2 move assignment is
   `noexcept`.
7. Advance the sequence ID and status using non-throwing scalar assignments.

This needs no M2 Public API extension. It prioritizes a verifiable strong guarantee over unmeasured
performance. M5 benchmarks may justify an internal transactional clone/swap or allocator-aware
optimization without changing M3 semantics.

### Exception guarantee matrix

| Operation | Proposed guarantee | Failure state |
|---|---|---|
| Projection construction | Strong construction | No object on failure |
| Baseline installation | Strong | Prior book, status, ID, and gap unchanged |
| Stale/duplicate/rejected/gap classification | Non-allocating, expected `noexcept` internally | Deterministic result; gap transition is fully committed |
| Incremental accepted apply | Strong | Prior synchronized book, status, ID, and gap unchanged; `std::bad_alloc` propagates |
| Result construction | Non-allocating value construction | No failure expected |
| Reset | `noexcept` | Empty book, no ID/gap, `AwaitingBaseline` |
| Queries returning M2 vectors | Existing allocation behavior | Projection unchanged if allocation throws |

No path advances `last_update_id` before the book commit.

## Proposed public API

The layout below is illustrative header-level design only. It is **Proposed**, **Not implemented**,
and **Subject to external review**.

```cpp
namespace binance_market_data::projection::v1 {

class UpdateId final {
  public:
    explicit constexpr UpdateId(std::uint64_t value) noexcept;
    [[nodiscard]] constexpr std::uint64_t value() const noexcept;
    friend constexpr auto operator<=>(UpdateId, UpdateId) noexcept = default;
};

struct UpdateRange final {
    UpdateId first;
    UpdateId final;
    [[nodiscard]] constexpr bool valid() const noexcept;
    friend constexpr bool operator==(const UpdateRange&, const UpdateRange&) noexcept = default;
};

enum class SequencePolicyKind : std::uint8_t {
    Spot,
    UsdMPerpetual,
    CoinMPerpetual,
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
    RejectedInvalidRange,
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
| `Applied` | Yes for advancing batch; no for Futures equality bridge or empty/no-op levels | To `u` when greater; equality bridge remains `L` | `AwaitingBridge` may become `Synchronized` | No | Normal market input |
| `IgnoredStale` | No | No | No | No | Normal replay/network condition |
| `IgnoredDuplicate` | No | No | No | No | Normal sequence condition; not content identity proof |
| `GapDetected` | No | No | To `NeedsResync` | Yes | Normal adverse market/input continuity condition |
| `RejectedInvalidRange` | No | No | No | No | Boundary/programmer validation error |
| `RejectedWrongState` | No | No | No | No | Lifecycle misuse or input received while recovery is required |

Every result reports the resulting status and accepted ID, making transcripts directly comparable.
Only `GapDetected` carries `GapInfo`, and its `gap` must be present. For every other disposition it
must be absent. These are constructor/implementation invariants tested exhaustively.

## Overflow and boundary behavior

| Input | Behavior |
|---|---|
| `UpdateId{0}` | Valid baseline or event ID |
| `UpdateId{UINT64_MAX}` | Valid. If current is max, all representable incoming finals are stale or duplicate; no successor arithmetic occurs |
| `first > final` | `RejectedInvalidRange`, no mutation |
| `current + 1` would overflow | The algorithm first establishes whether `final > current`; this is impossible at max. A guarded successor predicate is used elsewhere |
| Futures `previous_final > final` | `RejectedInvalidRange`, no mutation; Spot ignores the field |
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
| ID/range | zero, max, equality, first greater than final, huge inclusive interval, guarded successor at max, Futures previous greater than final |
| Spot bootstrap | documented overlap, exact-next bridge, stale, equality duplicate, forward gap, empty-level bridge |
| Spot live | exact-next, overlap, stale, equality duplicate, forward gap, max final, previous-final present but ignored |
| USD-M bootstrap/live | range contains `L`, equality bridge without mutation, `pu` continuity, stale and duplicate before `pu` validation, missing `pu`, mismatch, exact-next bootstrap range miss |
| COIN-M bootstrap/live | Same policy matrix tested independently so enum routing cannot regress |
| Lifecycle | construction, wrong-state apply, install, replace pending baseline, bridge, gap, reject after gap, install after gap, reset from every state, reinstallation |
| Book semantics through projection | mixed bid/ask, zero deletion, missing delete, duplicate-price last-write-wins, empty batch advances, crossed and locked book retained |
| Context and ownership | `NumericSpec` retained through baseline/reset/move, policy retained and immutable, move destination exact, no copy, no mutable book exposure |
| Visibility | synchronized view absent before bridge and after gap, present only when synchronized, diagnostic view preserved after gap |
| Results | every disposition's status/ID/gap invariants and equality |

Every row in the state transition matrix receives at least one direct test. Cross-policy table tests
feed the same range/`pu` values to Spot, USD-M, and COIN-M and assert their intentionally different
outcomes.

### Independent property model

The test reference model will use primitive `uint64_t`, a vector-based reference book, and
independently written decision tables. It must not call production policy helpers, successor
predicates, `BookProjection`, or M2 map logic.

Deterministic generators will produce:

- legal contiguous Spot and Futures sequences;
- batch intervals and overlapping ranges;
- stale and equality-duplicate batches;
- Spot forward gaps;
- Futures missing/mismatched `pu`;
- reset and new-baseline operations;
- empty batches; and
- mixed bid/ask insert, replacement, and deletion updates.

After every operation, compare production and reference status, result, accepted ID, gap, visible
state, and complete diagnostic book. Assert that no gap/rejected operation changes the book or
advances the ID, and that only synchronized state exposes the normal view. Seeds and operation
indexes must be printed on failure.

### Model-based fuzz plan

A future M3 harness will decode these operations:

- `InstallBaseline`;
- `ApplySpotUpdate`;
- `ApplyUsdMUpdate`;
- `ApplyCoinMUpdate`;
- `Reset`;
- `Query`;
- `MalformedRange`;
- `Gap`; and
- `Duplicate`.

One projection instance has a fixed policy; operations for a different policy are applied to a
separate instance or decoded as reference-only comparisons, never by mutating policy. The harness
will compare the production machine with an independent vector/reference model after each step and
abort on any mismatch.

Seed corpus categories will include: empty baseline, zero/max IDs, Spot overlap bridge, Spot
exact-next bridge, both Futures equality bridges, multi-ID live ranges, empty advancing update,
duplicate, stale, forward gap, `pu` mismatch, missing `pu`, reset/rebaseline, crossed book, zero
delete, missing delete, duplicate price, and query in every state.

### Replay determinism

Each curated Spot/USD-M/COIN-M transcript is replayed into two fresh projections. The complete
result sequence and every checkpoint listed in the determinism invariants must compare equal. A
second test resets and replays into the same projection and compares with a fresh instance.

### Executable allocation-failure tests

Exception tests will live in a dedicated test executable that overrides allocation with a
deterministic fail-after counter; production code receives no failpoint API. For a known baseline
and advancing batch, the test will first count successful transaction allocations, then sweep every
failure position through `all_levels`, candidate construction, `replace_all`, and candidate
`apply_updates`. After each caught `std::bad_alloc`, it will compare status, ID, gap, bids, and asks
with an exact pre-call checkpoint. A final no-failure run must apply successfully.

The same countdown sweep covers baseline `replace_all`. The test only claims a failure point when
the counter actually fires, so it cannot pass without exercising allocation failure. Sanitizer
runs execute these tests as normal.

### Test deliverable matrix

| Deliverable | Oracle | Primary failure found |
|---|---|---|
| Unit tests | Explicit expected table | Boundary and policy regressions |
| State-transition tests | Complete transition matrix | Illegal lifecycle mutation |
| Property tests | Independent primitive/vector model | Composed sequence/book divergence |
| Model fuzz | Independent decoded model | Unexpected operation sequences and malformed input |
| Replay determinism | Full transcript/checkpoint equality | Ambient or unstable behavior |
| Allocation-failure sweep | Exact pre-call checkpoint | Book/sequence partial commit |

## Acceptance gates

The future M3 implementation is not complete until all of these gates pass:

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

This design-stage PR does not claim that any future M3 gate has passed.

## Implementation plan

Implementation, if externally approved, will use a separate
`feat/m3-sequence-projection-state` branch and proceed in small reviewed steps:

1. Add `UpdateId`, `UpdateRange`, enum values, and header self-containment tests.
2. Implement independent pure Spot and Futures classification functions and table tests.
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

This design PR does not create that branch or any production/test/fuzz files.

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
| Virtual/plugin/callback policy framework | Adds open behavior, allocation, and ambient-state risks for three closed policy kinds |
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

Implementation blocker count from unresolved technical questions: **0**. Independent external
Architecture and Sequence Policy Review is still a mandatory process gate; it is not a deferred
technical decision. If review rejects a decision, this document and PROPOSED ADR must be revised
before any implementation branch is created.
