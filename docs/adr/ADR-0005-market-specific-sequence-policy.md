# ADR-0005: Market-Specific Sequence Policy

- Status: PROPOSED
- Date: 2026-08-06

## Context

M2 owns deterministic absolute-quantity order-book mutation but has no Binance update IDs,
bootstrap, continuity validation, projection lifecycle, or gap state. M3 must add those capabilities
without introducing network, wire, clock, buffering, or Host-runtime concerns into Core.

Binance does not publish one universal depth-sequence rule. The official Spot instructions use a
contains-`L` bootstrap rule and a distinct live successor/gap rule. The official USD(S)-M and
COIN-M Futures instructions use `U/u` to bridge a snapshot and require each advancing live event's
`pu` to equal the previous event's `u`. The fixed Contracts baseline represents
`previous_final_update_id` as optional so Spot and USD-M share a cross-module event shape;
optionality does not erase the USD-M semantic requirement. That baseline identifies only `SPOT`
and `USD_M_PERPETUAL`, so it cannot select a COIN-M Core policy.

The reviewed M2 `OrderBook` is non-copyable and `noexcept` movable. `replace_all` has a strong
exception guarantee, while `apply_updates` may leave partial changes if allocation fails. M3 must
not advance sequence state against a partially updated book.

## Decision

M3 will use a closed, immutable, market-facing `SequencePolicyKind` value with two explicit public
kinds: `Spot` and `UsdMPerpetual`. Small internal pure functions selected by that enum will classify
bootstrap and live input. There will be no virtual policy hierarchy, callbacks, runtime
registration, or mutable global policy. An internal shared classifier is an implementation detail,
not a substitute for market identity.

The projection will strongly wrap Binance update IDs over `std::uint64_t`, model inclusive update
ranges that are valid by construction, own one M2 `OrderBook`, and expose all mutation through
`BookProjection`. `UpdateRange::try_create` returns `std::optional<UpdateRange>` without allocation
or exceptions and makes `first <= final` a permanent invariant. The projection will not expose a
mutable book reference.

All declarations remain proposed until this ADR passes the required independent external review.

## Policy model

### Spot interval policy

- A range is structurally valid when `U <= u`.
- During bootstrap, `u < L` is stale and `u == L` is a sequence duplicate.
- A Spot bridge requires `u > L` and must contain `L`: `U <= L < u`.
- An advancing bootstrap candidate with `U > L`, including an exact-next range beginning at
  `L + 1`, is `SpotBootstrapForwardGap`.
- During live operation, `u < current` is stale and `u == current` is duplicate.
- An advancing range is accepted when it starts no later than the successor of `current`; a later
  start is a forward gap.
- `previous_final_update_id` is not interpreted by Spot.

Bootstrap and live deliberately use different official predicates. The fixed Contracts handoff
fixture records snapshot `update_id=500` and a later depth `update_id=501`, but it does not record
`first_update_id`; it therefore cannot establish `[501,501]` and is not evidence for an exact-next
bootstrap bridge. Dedicated tests reject exact-next bootstrap and accept exact-next live input.

### USD-M previous-final policy

- During bootstrap, `u < L` is stale and the first relevant range must contain `L`: `U <= L <= u`.
- A relevant Futures bridge must carry `pu`. The first bridge does not compare `pu` to the snapshot
  ID because `pu` refers to the prior WebSocket event.
- A bridge ending exactly at `L` applies every absolute-quantity level transactionally, keeps the
  accepted ID at `L`, and establishes synchronization only after commit. REST snapshot depth is
  limited, so the bridge may contain a changed level not retained in the baseline.
- During live operation, stale and duplicate classification occurs before checking `pu`.
- Every advancing live batch requires a present `pu == current`; absence or mismatch is a gap.
- Spot's interval-successor predicate is not added as a second live Futures rule.
- No separate `pu <= final` structural invariant is imposed because neither the frozen Contracts
  baseline nor the reviewed official rule defines one. For accepted advancing input,
  `pu == current < final` follows from continuity.

### Future compatibility evidence

The reviewed COIN-M documentation currently uses the same previous-final continuity pattern as
USD-M. COIN-M is not part of the M3 public surface because the frozen Contracts baseline cannot
identify that market. Adding it requires an accepted Contracts market identifier, an explicit M4
adapter mapping, a reviewed public-enum extension, and independent conformance tests.

### Sequence equality and content identity

`u < current` is `IgnoredStale`; `u == current` is `IgnoredDuplicate` outside the relevant USD-M
equality-bootstrap case. These are sequence classifications, not proof of byte/content identity.
M3 will not hash, serialize, or retain event history. A same-ID/different-content identity conflict
remains the responsibility of the normalization/Host boundary while Contracts diff-depth identity
is candidate/TBD. M3 will not reapply sequence duplicates.

## State model

`BookProjection` will use four states:

1. `AwaitingBaseline`: empty, with no accepted Binance update ID.
2. `AwaitingBridge`: a baseline and `L` are installed, but normal book visibility is unavailable.
3. `Synchronized`: the baseline is bridged and all subsequently accepted input is continuous under
   the selected policy.
4. `NeedsResync`: a gap was detected; apply is blocked until a new baseline or reset.

Four states are required because an installed baseline is not yet a validated live projection.
`reset()` clears the book, accepted ID, and gap and returns to `AwaitingBaseline` while retaining
numeric and policy configuration.

## Gap behavior

On a policy gap, M3 will:

- preserve the last accepted book and update ID;
- store deterministic `GapInfo` containing policy, reason, last accepted ID, incoming range, and
  optional incoming `pu`;
- enter `NeedsResync` immediately;
- make the ordinary synchronized const view unavailable;
- allow only an explicitly named const diagnostic view;
- reject subsequent diffs; and
- recover only by installing and bridging a new baseline or by reset.

Core does not clear diagnostic evidence, continue applying, fill an interval, or claim
synchronization. `GapInfo` has no clock, timestamp, connection, or Gateway session fields.

## Baseline behavior

The Host owns snapshot download and the bounded buffered-event runtime. Core accepts a non-owning
domain `BookBaseline` containing only `last_update_id`, bid levels, and ask levels, followed by
individual `DepthBatch` calls. Core retains no network event buffer.

Baseline installation uses M2 `replace_all`, including last-write-wins duplicate prices, zero
deletion, deterministic ordering, and crossed/locked-book acceptance. The projection's immutable
`NumericSpec` supplies scale context. Installation changes lifecycle/ID only after `replace_all`
succeeds, providing the strong exception guarantee.

Baseline and bridge are separate strongly exception-safe calls. Normal visibility remains blocked
between them, so no ordinary caller observes an unbridged book as synchronized.

## Exception-safety decision

M3 selects logical copy-on-apply as its correctness baseline for every accepted batch that applies
levels, including a USD-M bridge with `u == L`:

1. copy the current ordered bid and ask levels with M2 `all_levels`;
2. build a candidate `OrderBook` with the same `NumericSpec`;
3. populate it using `replace_all`;
4. apply the batch to the candidate; and
5. commit only by existing `noexcept` move assignment, then update status and advance the ID with
   non-throwing scalar assignment when `u > current`.

An allocation failure before commit destroys the candidate and leaves the live book, accepted ID,
status, gap, and visibility unchanged. In particular, equality-bridge failure leaves the baseline
book, ID `L`, and `AwaitingBridge` state exactly intact. Incremental accepted apply therefore has
the strong exception guarantee without changing the M2 Public API. This may copy the full book per
accepted batch; M3 makes no performance claim. A benchmark-backed later optimization must preserve
the same observable transaction semantics.

Undo logs are rejected because rollback reinsertion may allocate. Merely accepting M2's basic
guarantee and marking the projection invalid is rejected as the correctness baseline because a
partially changed diagnostic book weakens recoverability and evidence.

## M3/M4 boundary

M3 domain types are not Pydantic models, Protobuf messages, or copies of Contracts schemas. M3 owns
only update IDs/ranges, normalized level views, sequence policy, projection lifecycle, deterministic
results, gap evidence, and the M2 book.

M4 will explicitly adapt Contracts/wire values into valid M3 domain values, attach
venue/symbol/producer/time/quality metadata, and construct snapshot or gap wire messages. If
`first_update_id > final_update_id`, `UpdateRange` construction fails before
`BookProjection::apply()` is called. Such malformed wire input cannot be silently skipped while the
projection continues claiming synchronization. The Host must stop reliable publication, reset or
discard the affected projection, and rebootstrap before processing later depth input. A Host also
supplies time and performs network recovery. Gateway `session_sequence`, `connection_generation`,
`ConsumerGapNotice`, and `StreamStatus` do not enter M3.

## Consequences

### Positive

- Spot and Futures rules are explicit, independently testable, and cannot be accidentally unified.
- Live and replay use the same deterministic state machine.
- Gap visibility fails closed while preserving diagnostic evidence.
- No update ID is committed against a partially applied book.
- Current M2 API is sufficient for the initial strong transaction.
- Future market differences can be added as reviewed enum behavior without a plugin framework.
- Invalid update ranges cannot enter projection apply, so normal apply results contain only
  lifecycle and market-sequence outcomes.

### Negative

- The correctness-baseline incremental transaction copies the full book and allocates.
- Four lifecycle states and typed results add API surface beyond raw `OrderBook` mutation.
- M3 cannot detect same-ID/different-content conflicts without a higher-layer identity decision.
- Hosts must own buffering, snapshot acquisition, and resynchronization orchestration.
- Current M3 public policy coverage is limited to markets represented by the frozen Contracts
  baseline: Spot and USD(S)-M perpetual.

## Alternatives rejected

- One continuous-ID rule for every Binance market.
- Applying the Spot live successor predicate during bootstrap; this would incorrectly accept an
  exact-next range that does not contain `L`.
- Sequencing only in Gateway.
- Direct use of Contracts, Pydantic, or Protobuf types in Core.
- Snapshot download or network buffer ownership in Core.
- Continuing after a known gap or treating diffs as self-healing.
- Clearing the book immediately instead of preserving gated evidence.
- Default reliable access to a preserved unsynchronized book.
- Timestamps or Gateway session identity in `GapInfo`.
- A mutable `OrderBook&` escape hatch.
- Virtual inheritance, `std::function`, runtime plugin registration, or mutable globals.
- Event-history storage or silent content hashing for identity.
- Advancing sequence before book commit.
- Basic exception guarantee while continuing to claim synchronization.
- Signed or naked integer update IDs.

## Evidence

- Projection base: `413e3cd9236d0c5de15d4e838149111718260303`.
- Fixed read-only Contracts baseline: `01d76a41929f36d89573159f5f458f9f1e378ada`.
  Its `Market` enum contains only `SPOT` and `USD_M_PERPETUAL`; its depth model validates
  `final_update_id >= first_update_id` but defines no separate `pu <= final` invariant.
- Binance official Spot source: [How to manage a local order book correctly](https://github.com/binance/binance-spot-api-docs/blob/master/web-socket-streams.md#how-to-manage-a-local-order-book-correctly), accessed 2026-08-06. The page displays no Last Modified date; the official changelog records removal of page dates on 2025-11-10 and correction of these steps on 2025-11-12.
- Binance official USD(S)-M source: [How to manage a local order book correctly](https://developers.binance.com/docs/derivatives/usds-margined-futures/websocket-market-streams/How-to-manage-a-local-order-book-correctly), accessed 2026-08-06, page last modified 2026-08-05.
- Binance official COIN-M source: [How to manage a local order book correctly](https://developers.binance.com/docs/derivatives/coin-margined-futures/websocket-market-streams/How-to-manage-a-local-order-book-correctly), accessed 2026-08-06, page last modified 2026-08-05.
- M2 implementation evidence: non-copyable PIMPL `OrderBook`, `noexcept` move, strong
  `replace_all`, non-transactional `apply_updates`, deterministic ordered queries, independent
  vector property model, and model-based fuzz organization.

## Review requirements

ADR status remains `PROPOSED` until an independent external reviewer explicitly evaluates:

- Spot versus Futures policy correctness;
- Spot bootstrap contains-`L` correctness and its separation from live successor coverage;
- stale/equality duplicate ordering;
- USD-M equality-bridge level application and `pu` behavior;
- Contracts-aligned public policy scope;
- valid-by-construction range and malformed-wire fail-closed behavior;
- gap transition and unsynchronized book visibility;
- logical copy-on-apply atomicity under allocation failure;
- Core/Host and M3/M4 boundaries;
- proposed API minimality; and
- independence and completeness of the test model.

Before approval, no M3 implementation branch, public header, source, test, or fuzz harness may be
created. External approval may change this ADR to `ACCEPTED` only in a separately reviewed change.
