# ADR-0008: Spot Bootstrap Successor Coverage

- Status: PROPOSED / PENDING INDEPENDENT REVIEW
- Date: 2026-08-10
- Supersedes: the Spot-bootstrap portion of ADR-0005 only
- Motivation: official Binance Spot operational instruction corrected on 2025-11-12

## Status note

This ADR is **not ACCEPTED**. Repository workflow requires independent external review before an
ADR-0005 amendment becomes effective. Until review, the reviewed production code, tests, reference
model, and fuzz in this branch follow the corrected rule recorded below, and this document is the
governance evidence for that correction. ADR-0005's `Status: ACCEPTED` remains historically true
for its contains-`L` decision; this ADR records why that decision no longer matches verified
upstream protocol facts.

## Context

ADR-0005 accepted a Spot bootstrap rule under which the first advancing bridge must **contain** the
snapshot ID `L` (`U <= L < u`), and classified any advancing candidate with `U > L` — including an
exact-next range beginning at `L + 1` — as `SpotBootstrapForwardGap`. That decision was reached in
the repository's own review history:

1. The original M3 proposal (`1ebe809ce071d09dafbabb5815f019ab43b11c7f`) selected **successor
   coverage**: "A Spot bridge requires `u > L` and must cover the successor of `L`. This accepts
   both overlap and exact-next input without unguarded `L + 1` arithmetic."
2. The review correction (`58dfcb82f62e9b70033f8fd7bd4304b3f06388ac`) replaced that with
   **contains-`L`**: "A Spot bridge requires `u > L` and must contain `L`: `U <= L < u`" and
   classified exact-next as a bootstrap forward gap.

The review correction's stated rationale was the then-current official Spot instructions, which
read:

- "If the event `U` (first update ID) is > the update ID of your local order book, something went
  wrong. Discard your local order book and restart the process from the beginning."

That wording was itself corrected by Binance on 2025-11-12. Verified upstream protocol evidence now
contradicts the contains-`L` decision for Spot bootstrap.

## Official Binance evidence

### 2025-11-12 corrective commit

Commit `114929f2bff403e8f62067f6993aa9c2c957e5d3` in `binance/binance-spot-api-docs`
("Corrected how to maintain a local order book", dated 2025-11-12, recorded in the official
CHANGELOG) changed the apply-time rule in `web-socket-streams.md` (and the testnet and CN copies):

- **Old text:** "If the event `U` (first update ID) is > the update ID of your local order book,
  something went wrong. Discard your local order book and restart the process from the beginning."
- **New text:** "If the event first update ID (`U`) is greater than the update ID of your local
  order book **+ 1**, you have missed some events. Discard your local order book and restart the
  process from the beginning."

The corrected page also states the normal-case successor relationship: "Normally, `U` of the next
event is equal to `u + 1` of the previous event."

### Current official operational rule (verbatim paraphrase)

The current official Spot instructions ("How to manage a local order book correctly") are:

1. If the event last update ID (`u`) is less than the update ID of your local order book, ignore
   the event.
2. If the event first update ID (`U`) is greater than the update ID of your local order book + 1,
   you have missed some events; discard the local order book and restart.
3. Normally, `U` of the next event is equal to `u + 1` of the previous event.
4. Apply levels, then set the local order book update ID to the event's `u`.

### Snapshot/buffer orchestration versus apply-time continuity

The official instructions combine two different concerns that must be kept separate:

**A. Binance snapshot/buffer orchestration.** The official materials retain a conservative
bootstrap acquisition procedure: open the stream and buffer events, acquire a depth snapshot, and
while the snapshot `lastUpdateId` is strictly less than the `U` of the first buffered event,
reacquire the snapshot; then discard buffered events with `u <= lastUpdateId`. These steps
orchestrate Host snapshot timing and buffering. They do not, by themselves, define the Core
continuity predicate. In particular, `u > L` alone does not imply `U <= L`: with `L = 100`, the
surviving range `[101,101]` has `u = 101 > L` but does not contain `L`. The `u <= L` discard step
therefore does not prove contains-`L` for every survivor.

**B. Binance apply-time continuity rule.** The corrected operational rule says a gap exists when
`U > local_update_id + 1`, and normally `U` of the next event equals `u + 1` of the previous event.
The official maintained `binance-toolbox-python` example implements
`U <= last_update_id + 1 <= u` as its acceptance predicate.

### Official Binance example implementation

The official `binance-toolbox-python` example `manage_local_order_book.py` implements the predicate

```python
if json_data['U'] <= last_update_id + 1 <= json_data['u']:
    order_book['lastUpdateId'] = json_data['u']
    process_updates(json_data)
else:
    logging.info('Out of sync, re-syncing...')
    order_book = get_snapshot()
```

This predicate has been in the official example since commit `1b465ec` (2023-08-07,
"fixing orderbook issue (#17)"). It accepts overlap and exact-next input and treats only a start
later than `last_update_id + 1` as out of sync.

**C. M3 Core responsibility.** `BookProjection` does not own WebSocket subscription start,
buffered-event retention, REST snapshot timing, the snapshot reacquisition loop, or Host/network
recovery. M3 receives one trusted current ID `C` plus one normalized valid `UpdateRange [U,u]` and
answers whether the incoming advancing range is sequence-continuous. For that Core responsibility,
successor coverage is the reviewed predicate `U <= C + 1 <= u` with guarded arithmetic.

Host-level snapshot acquisition/reacquisition policy and the Core sequence continuity predicate are
therefore separate responsibilities. A future Host may impose additional acquisition or
rebootstrap rules without changing this Core continuity predicate.

## Decision

For Spot, an advancing event (an event whose `u` is greater than the last trusted update ID `C`) is
continuous — including as the bootstrap bridge — iff its range covers the overflow-safe successor
of `C`:

```text
U <= C + 1 <= u
```

equivalently (with `T = C + 1` when `C != UINT64_MAX`):

```text
U <= T <= u
```

The equivalent forward-gap condition is `U > C + 1`. Lifecycle-specific gap reasons remain
distinct: `AwaitingBridge -> SpotBootstrapForwardGap`, `Synchronized -> SpotLiveForwardGap`.

This decision supersedes **only** the Spot-bootstrap portion of ADR-0005. All USD-M decisions in
ADR-0005 (`U <= L <= u` bootstrap range containment, equality bridge, `pu` live continuity,
stale/duplicate ordering, missing/mismatched `pu` gap reasons, and the absence of a Spot
interval-successor rule on USD-M) are unchanged.

## Precise invariants

`C` denotes the last trusted local update ID: `C = L` (snapshot `lastUpdateId`) during bootstrap
and `C =` last accepted event `u` while synchronized.

- **INV-001 — Range validity:** `U <= u` is unchanged; `UpdateRange::try_create` still rejects
  `first > final`.
- **INV-002 — Spot current ID:** `C = L` during bootstrap; `C =` last accepted event `u` while
  synchronized.
- **INV-003 — Spot stale:** `u < C` remains `IgnoredStale`. No book mutation, no sequence
  advancement, no gap.
- **INV-004 — Spot equality:** `u == C` remains `IgnoredDuplicate` (sequence equality only, not
  byte/content identity). No book mutation, no sequence advancement.
- **INV-005 — Spot advancing successor coverage:** for `u > C` with `C != UINT64_MAX`, let
  `T = C + 1`; the event is continuous iff `U <= T <= u`; equivalently the forward-gap condition
  is `U > C + 1`. This applies to Spot bootstrap and Spot live continuity. Lifecycle-specific gap
  reasons remain distinct (`SpotBootstrapForwardGap` vs `SpotLiveForwardGap`).
- **INV-006 — Exact-next is valid:** against `C = 100`, both `U=101,u=101` and `U=101,u=103` are
  valid advancing Spot events, never bootstrap gaps.
- **INV-007 — Overlap remains valid:** against `C = 100`, `U=99,u=101` and `U=100,u=101` remain
  valid.
- **INV-008 — True forward gap:** against `C = 100`, `U=102,u=102` is a gap because update ID 101
  is missing.
- **INV-009 — UINT64_MAX:** `UINT64_MAX + 1` is never evaluated. When `C == UINT64_MAX`, no
  uint64 advancing update can exist; stale/equality classification remains deterministic and
  overflow-free (see below).
- **INV-010 — Spot ignores `pu`:** `previous_final_update_id` remains semantically ignored for
  Spot whether present or absent.
- **INV-011 — USD-M frozen:** USD-M bootstrap/live semantics, gap reasons, equality-bridge
  behavior, and `pu` continuity are unchanged.
- **INV-012 — Transactionality:** BookProjection strong transaction semantics, diagnostic state
  preservation, deterministic behavior, and exception guarantees are unchanged.

## Why exact-next is continuous

The official corrected rule treats a range starting at `local + 1` as the normal successor
("Normally, `U` of the next event is equal to `u + 1` of the previous event"). An exact-next
bootstrap bridge `[L+1, u]` covers the only ID not proven by the snapshot and cannot skip any
update ID; it is therefore continuous under the same predicate the official instructions apply to
every advancing event. The official example predicate `U <= last_update_id + 1 <= u` accepts it
explicitly. Treating `[L+1, ...]` as a gap would force a redundant resync for the exchange's own
normal successor pattern.

## UINT64_MAX handling

`C + 1` is evaluated only after `C != UINT64_MAX` is established. At `C == UINT64_MAX`:

- every representable incoming `u` is `<= C`, so classification completes in the stale/equality
  branches before any successor arithmetic exists;
- no advancing update can exist, hence no bootstrap or live forward-gap branch can evaluate
  `UINT64_MAX + 1`;
- stale/duplicate results are deterministic, and unsigned wrap is never used as successor
  semantics.

## Consequences

### Positive

- Spot bootstrap and live continuity now share the corrected official predicate; exact-next input
  no longer forces an unnecessary resync.
- Production, independent reference model, unit/property tests, and fuzz implement the same
  reviewed semantic rule while remaining implementation-independent.
- M5 replay classification evidence is aligned with the corrected M3 semantic authority.

### Negative

- The historical ADR-0005 contains-`L` decision is superseded for Spot bootstrap; any reviewed
  materializer/replay artifact built on the old rule must be re-validated.
- Bootstrap and live Spot classifiers are now structurally identical apart from gap reasons; the
  distinction is preserved deliberately to keep lifecycle-specific gap evidence.

## Alternatives rejected

- **Keep contains-`L` bootstrap** (`U <= L < u`): contradicted by the corrected official rule and
  the official example predicate; exact-next would remain a false resync trigger. Rejected.
- **Unify gap reasons** between bootstrap and live: rejected, because lifecycle-specific gap
  evidence is a reviewed observability feature.
- **Use live web content as a build dependency**: rejected; this ADR records the verified facts,
  and the repository never reads the web at build time (see governance rule below).

## Evidence

- Official corrective commit:
  `114929f2bff403e8f62067f6993aa9c2c957e5d3` ("Corrected how to maintain a local order book",
  2025-11-12) — diff inspected directly in `binance/binance-spot-api-docs` (web-socket-streams.md,
  testnet/web-socket-streams.md, CHANGELOG entries).
- Current official Spot instructions (accessed 2026-08-10):
  [How to manage a local order book correctly](https://developers.binance.com/docs/binance-spot-api-docs/web-socket-streams#how-to-manage-a-local-order-book-correctly):
  `u < local` ignored; `U > local + 1` restarts; "Normally, `U` of the next event is equal to
  `u + 1` of the previous event".
- Official example predicate in `binance/binance-toolbox-python`
  `manage_local_order_book.py` (`U <= last_update_id + 1 <= u`), present since commit
  `1b465ec` (2023-08-07).
- Repository history: original M3 proposal `1ebe809ce071d09dafbabb5815f019ab43b11c7f`
  (successor-aware bridge) and review correction `58dfcb82f62e9b70033f8fd7bd4304b3f06388ac`
  (contains-`L` bridge).
- Superseded local decision: ADR-0005 "Spot interval policy" bootstrap bullets only.

## Required independent review

Before acceptance, an independent external reviewer must evaluate:

- the corrected official rule against the local decision, including the example predicate;
- INV-002 through INV-010, especially UINT64_MAX determinism and gap-reason separation;
- that USD-M semantics are untouched;
- that production, reference, tests, and fuzz are independently implemented and agree;
- that historical ADR-0005 evidence is preserved, not rewritten.
