# M5 Differential Validation and Performance Design

## Status

- Design status: **APPROVED / MERGED**
- Implementation status: **IN PROGRESS**
- Phase 1 status: **COMPLETE / MERGED** (PR #11, merge
  `5e8629a7ff825f8ea941304d9b09be1670643e8a`, post-merge main CI `31264500905` — PASS 16/16)
- Phase 2 status: **COMPLETE / MERGED** (PR #12, merge
  `75c619dd683ff2a3893f9535e206231e7bfecc41`, post-merge main CI `31315421548` — PASS 16/16)
- Phase 3 status: **COMPLETE / MERGED** (PR #13, final approved Head
  `a8e4ccfc31efd4e67bc10cf0ac9ad2a99faa8354`, exact-head CI `31491615547` — PASS 16/16, merge
  `473a907eba2001d18926c57d6c8d16b10c7505be`) — see
  `docs/M5_PHASE3_DETERMINISTIC_REPLAY.md`
- Phase 4 status: **COMPLETE / MERGED** (PR #16, final approved Head
  `b612f85d281315346c0ccf6f599d51af538e3cf4`, exact-head CI `31571166506` — PASS 18/18, merge
  `7b6d9ef3b222675138fdd34f3fed381216fe9d02`) — see
  `docs/M5_PHASE4_CROSS_COMPILER_SEMANTIC_MANIFESTS.md`
- Phase 5 status: **COMPLETE / MERGED** (PR #18, final approved Head
  `e56f5dbd12b9e66946343467221e8e3ba9984531`, exact-head CI `31668465623` — PASS 18/18, squash
  merge `53268d5cd2090f4779ffdc14c070184f470cc899`, post-merge main CI `31671708958` — PASS
  18/18) — see `docs/M5_PHASE5_DIFFERENTIAL_FUZZING.md`
- Phase 6 status: **COMPLETE / MERGED** (PR #21, accepted implementation Head
  `9776ba6b93990c44e550f289b69127ca721b0d00`, exact-head CI `31803322848` — PASS 18/18, final
  independent review APPROVED, formal exact-head evidence ACCEPTED, squash merge
  `227524e6d17cce77813c6f26cd65bb8d996f5677`, post-merge main CI `31809917018` — PASS 18/18) —
  see `docs/M5_PHASE6_REPRESENTATIVE_BENCHMARKS.md`
- Phase 7 status: **AUTHORIZED / NOT STARTED** — pre-implementation decision record
  proposed on the immutable base `eed3de99efaba8eaa96083a5348d538ed44f6bfe`; fresh
  independent methodology review: CHANGES REQUESTED (P0: 1, P1: 8, P2: 1; findings
  M5-P7-MR-001 .. M5-P7-MR-010); corrections IMPLEMENTED FOR FOCUSED INDEPENDENT
  RE-REVIEW, independent closure PENDING; see
  `docs/M5_PHASE7_PREIMPLEMENTATION_DECISIONS.md` (decisions OD-M5-P7-001 through
  OD-M5-P7-023). Implementation authorization is NO until the corrected record is
  independently re-reviewed and merged without material unreviewed decision changes.
- ADR status: **ACCEPTED** (ADR-0007)
- Design date: 2026-08-08
- Initial independent architecture review: **CHANGES REQUESTED** (P0: 0, P1 design: 1, P1 implementation: 2, P2: 7)
- Correction date: 2026-08-08
- Corrected findings: M5-AR-001, M5-AR-002, M5-AR-003 (mandatory); M5-AR-004 through M5-AR-010 (P2, documentation-only)
- Projection base: `ac780d9eb7b49ff20a6b3b4bee6a993b51b70af4` (M4 merge commit)
- M4 post-merge CI: `31242162782` — completed / success, 16/16
- Contracts baseline: `01d76a41929f36d89573159f5f458f9f1e378ada`
- Pre-implementation decisions: **CLOSED** (OD-M5-001, OD-M5-002). OD-M5-003: **SPIKE-RESOLVABLE**. See `docs/M5_PREIMPLEMENTATION_DECISIONS.md`.

## Architecture approval record

Independent M5 architecture review: **APPROVED** (focused re-review).

```text
Reviewed Head:
9fff05ca8333d89d28d89c794d65255b56578715

Reviewed CI:
31245814229 — PASS (16/16)

M5-AR-001 through M5-AR-010:
CLOSED

P0:
0

P1 Design:
0

ADR-0007:
ACCEPTED
```

Implementation authorization was blocked by open decisions OD-M5-001 and OD-M5-002.
Both are now CLOSED. M5 implementation is authorized to begin after the decision/CI-policy PR
is reviewed and merged. See `docs/M5_PREIMPLEMENTATION_DECISIONS.md`.

Non-blocking review observations (implementation-time only, not blockers):

```text
- string-token grammar may receive additional implementation-time hardening
- adapter-mode manifest scope may receive additional implementation-time clarification
```

Phase 1 records the exact canonical grammar, byte validation, normalized operation model, fixture
identity loader, and offline recorded-corpus materializer boundary in
`docs/M5_PHASE1_CANONICAL_REPLAY.md`. M5-PIR-001 and M5-PIR-004 are incorporated there; M5-PIR-002
and M5-PIR-003 remain deferred/non-blocking. This does not implement the differential oracle,
ReplayDriver, semantic observations, fuzzing, benchmarks, or CI reporting.

This document designs M5 **Differential Validation and Performance**. Phase 1 implementation details
are recorded separately in `docs/M5_PHASE1_CANONICAL_REPLAY.md`; phases 2+ are not implemented here.
No production code or order-book container is changed, no production container migration is
authorized, and M6 is not started.

## Goal

M5 must answer two separate questions with separate evidence:

```text
A. CORRECTNESS CONFIDENCE

   Can M1-M4 produce identical deterministic results under large replay,
   differential, property, and fuzz workloads?

B. PERFORMANCE CHARACTERIZATION

   Where are the actual CPU, allocation, latency, and memory costs, and would a
   different internal order-book container materially improve them?
```

Correctness and performance evidence must not be mixed. A faster result that changes semantics is
invalid, and a semantic match without performance evidence does not answer B.

The milestone charter from `docs/MILESTONES.md` is:

```text
M5 Differential Validation and Performance
Replay / differential / fuzz validation
+ benchmark with representative workloads
```

Open question O-P003 constrains the container question:

```text
std::map remains the correctness baseline. M5 compares candidate order-book
containers such as flat_map, Abseil B-tree, sorted vectors, and other justified
candidates. No third-party production container is introduced before benchmark
evidence exists. PIMPL isolates container choice from the public API.
```

These constraints are preserved. `std::map` remains the M2 correctness baseline throughout M5 until
an explicit, independently reviewed decision records otherwise.

## Current baseline

Facts established by the merged M1-M4 implementation on `main`:

- M1: exact decimal parsing/rescaling/formatting on `std::int64_t` units, scales 0..18, no floating
  point, no allocation in the parser, retained `source_fraction_digits`.
- M2: deterministic market-by-price `OrderBook` behind a PIMPL; bids in `std::map` with
  `std::greater<PriceUnits>` (descending) and asks with `std::less<PriceUnits>` (ascending);
  absolute-quantity replacement, zero deletion, missing-delete no-op, same-price last-write-wins,
  batch `apply_updates`, atomic `replace_all` (strong exception guarantee), best/top-N/full-level
  queries returning vectors by value, locked/crossed acceptance, `noexcept` move, copy deleted.
- M3: `BookProjection` lifecycle (`AwaitingBaseline`, `AwaitingBridge`, `Synchronized`,
  `NeedsResync`), `UpdateId`/`UpdateRange`/`GapInfo`/`ApplyResult`/`InstallResult`, closed
  Spot/USD-M policies, and a **logical copy-on-apply** incremental transaction: every accepted
  batch copies both complete sides with `all_levels`, builds a candidate `OrderBook`, applies to the
  candidate, and commits by `noexcept` move assignment. This is O(book) copying per accepted batch
  and is the dominant documented correctness-baseline cost. ADR-0005 explicitly says: "A
  benchmark-backed later optimization must preserve the same observable transaction semantics."
- M4: optional `ProtoAdapter` target outside Core with owning `AdaptedBookBaseline`/`AdaptedDepthBatch`
  checked invocation, disjoint Host/Core/inbound quality domains, state-specific
  `LocalOrderBookSnapshot` eligibility, `DepthLimit`, fixed-scale decimal output, and canonical
  quality rank. Core has no Protobuf/Contracts/gRPC dependency.

Validation infrastructure that M5 builds on:

- Independent reference models already exist: `ReferenceOrderBook` (vector-based, M2),
  `ReferenceProjection` (primitive `uint64_t` + own decision tables, M3), and a primitive
  `reference_parse`/`reference_fixed` decimal model (M4 property tests). All are documented as
  deliberately independent of production classification helpers.
- Deterministic generators use fixed-seed LCG/xorshift64\* PRNGs with fixed iteration counts and
  `SCOPED_TRACE` context printing (no `<random>` ambient state).
- `ProjectionCheckpoint` in `tests/projection_state/test_helpers.hpp` is the canonical state-equality
  utility (status, last ID, gap, synchronized visibility, full bid/ask vectors).
- The allocation-failure sweep pattern (global `operator new` fail-after-counter in dedicated
  single-threaded executables, no production failpoint API) is fully established and reusable.
- Four libFuzzer harnesses exist (`decimal_parser`, `order_book`, `book_projection`,
  `proto_adapter`) with checked-in seed corpora and a blocking Ubuntu Clang smoke job (10,000
  inputs per harness).
- Google Benchmark 1.9.5 is already a repository-local Conan test dependency; the current
  benchmark executable is an infrastructure smoke test only (`BM_LibraryVersionAccess`). Its JSON
  output is produced but never consumed, uploaded, or compared.
- CI is a 16-job matrix (quality, release x3, sanitizers x2, benchmark smoke, fuzz smoke, M4 static
  x6, M4 shared x2). No artifact upload exists.
- The Conan lock graph already contains `abseil/20260107.1` as a transitive dependency of
  `protobuf/6.33.5`; Abseil C++ components are therefore already available through the repository
  dependency workflow for benchmark-only use.
- The three deferred M4 P2 findings (M4-IIR-1, M4-IIR-2, M4-IIR-3) remain open and non-blocking.

## Inputs

- The implemented M1-M4 public API and production sources on `main` (read-only inputs; M5 consumes
  them, it does not modify them).
- The fixed Contracts baseline and pinned C++ package identities recorded in M4 documentation.
- Existing reference models, deterministic generators, corpus seeds, and the allocation-failure
  sweep pattern.
- O-P003 (container decision) and the M4 deferred P2 findings.
- Externally recorded Binance-style transcripts (provenance-verified, converted offline into the
  canonical M5 fixture format; no live access in CI).

## Non-goals

M5 explicitly excludes:

```text
live Binance connections
WebSocket ownership
REST snapshot downloading
gRPC
Gateway lifecycle
queues
backpressure
reconnect
network scheduling

strategy
risk
orders
matching engine

persistent database
history service runtime
Python binding

production container migration without evidence/ADR

M6
M7
```

M5 also does not: change production Core/ProtoAdapter code, change the `std::map` container in
production, add production dependencies, change the Contracts or Recorder repositories, or weaken
the PIMPL encapsulation.

## Differential architecture

### Layered reference oracle (selected)

M5 uses **layered reference models**, not one monolithic oracle.

| Layer | Reference model | Validates | Independence requirement |
|---|---|---|---|
| R1 | `ReferenceDecimal` — primitive digit grammar, checked powers of ten, exact rescale, canonical fixed formatting | M1 parsing/rescaling/formatting | Own grammar and arithmetic; must not call M1 parser/formatter internals |
| R2 | `ReferenceOrderBook` — existing vector model (linear search + per-side sort) | M2 book state | Vector storage only; must not call production maps or M2 helpers |
| R3 | `ReferenceProjection` — existing primitive model with own decision tables | M3 sequence/lifecycle/gap state | Primitive `uint64_t` IDs; own policy tables; must not call production classifiers or the production range factory |
| R4 | `ReferenceAdapter` — semantic wire mapping: enum tables, eligibility matrix, gap mapping, quality rank, fixed decimal output fields | M4 adaptation and `LocalOrderBookSnapshot` semantics | Own mapping tables and eligibility rules; may consume R1-R3 values as value types only |
| D | `ReplayDriver` — canonical transcript consumer that drives the production pipeline and the layered reference pipeline | Cross-layer composition, determinism, ordering | Drives both pipelines through their public APIs; contains no duplicated business logic |

Existing `ReferenceOrderBook` and `ReferenceProjection` are already structured this way and are
retained as the R2/R3 basis rather than rewritten. R1 already exists in embryonic form inside the
M4 property tests (`reference_parse`) and is promoted to a shared test-only support library. R4 is
new.

### Monolithic alternative (rejected)

A single monolithic reference model that mirrors the whole pipeline would be simpler to invoke but:

- re-implements composition logic that production also has, making accidental logic sharing more
  likely and divergence less diagnosable;
- cannot attribute the first divergent event to a milestone layer;
- would duplicate R2/R3 models that are already reviewed, tested, and fuzz-linked.

### Layered advantage

- A failure reports the first divergent event and the layer that diverged (R1-R4 or D).
- Each layer keeps the established "must not call production helpers" discipline documented in the
  M3/M4 design documents.
- Replay, property, and fuzz tests can target one layer or the whole pipeline.
- The oracle is composed only through public API calls, so it exercises the same boundaries a Host
  would exercise (live/replay equivalence).

### Oracle independence rules

- The reference pipeline consumes production value types (`PriceUnits`, `BookLevel`,
  `NumericSpec`, ...) as opaque values and never calls production functions that compute business
  state.
- The reference pipeline stores only primitive integers and `std::vector`; no `std::map`, no
  production containers, no unordered containers.
- R3's decision tables are written from the official Binance evidence in the M3 design document,
  not derived from production source.
- Any production change discovered to be "required" by the oracle is evidence of an oracle leak and
  must be fixed in the oracle.

## Replay representation

### Canonical replay event log (v1)

Replay input is a **canonical, versioned, line-oriented text event log** plus a **manifest**. It is
not a wire format, not Protobuf, and not a network capture. It exists solely as deterministic
offline input for Core and adapter pipelines.

Header (one line): schema version, market, symbol, `NumericSpec` (price/quantity scales), policy,
fixture ID, optional provenance fields.

Event lines (token-based, one event per line, no comments inside the event section):

```text
INSTALL_BASELINE <last_update_id> <bids> <asks>
DEPTH_UPDATE <first_update_id> <final_update_id> [pu=<id>] <levels>
REBASELINE <last_update_id> <bids> <asks>
RESET
SNAPSHOT_REQUEST <depth_limit|-> <host_quality_facts...> <snapshot_context...>
ADAPTER_METADATA <facts...>         # adapter-mapped inbound wire quality for the following DEPTH_UPDATE
MALFORMED_RANGE <first> <final>     # negative event: must fail domain construction, never reach apply
```

Levels are spelled as decimal strings exactly as a Host would produce them (`price,quantity`
pairs), so the replay exercises M1 exact parsing rather than bypassing it. Prices/quantities may be
given at any textual precision; exact rescaling rules apply. Bid-then-ask ordering follows the M4
canonical merge order.

Quality facts in the replay grammar represent three distinct M4 quality domains:

| Domain | Replay event | Storage target | Semantics |
|---|---|---|---|
| Inbound wire quality | `ADAPTER_METADATA` before `DEPTH_UPDATE` | `AdaptedMetadata::observed_quality` | Mapped from original Contracts `QualityFlag` values by the adapter conversion; captures what the adapter observed on the inbound wire message |
| Host-observed quality | `<host_quality_facts...>` inside `SNAPSHOT_REQUEST` | `SnapshotOptions::host_quality_facts` | Asserted by the Host at snapshot-production time; separate from inbound-wire quality |
| Core-derived quality | Not represented in the replay (derived by Core from book/projection state) | Emitted in snapshot output only | `CROSSED_BOOK`, `SEQUENCE_GAP`, `SNAPSHOT_BRIDGE_PENDING` — never provided as input |

`ADAPTER_METADATA` is state/context that precedes a `DEPTH_UPDATE` in adapter-mode replay.
It provides the quality facts that the adapter would have mapped from inbound wire `QualityFlag`
values during `adapt_depth_update`. In Core-only replay mode, `ADAPTER_METADATA` events are
ignored. This separation preserves M4's three-domain quality architecture without conflating
inbound, host, and Core quality.

Coverage requirements (minimum set the grammar must represent):

```text
baseline snapshots
depth updates
Spot sequencing
USD-M sequencing
pu presence
reset/rebaseline
gap events
bridge behavior
duplicate/stale updates
out-of-order input
adapter-metadata quality facts (inbound wire quality)
host-quality facts (snapshot context)
snapshot requests (with explicit identity, producer, timestamps, depth limit, gap context)
malformed-range negative events
```

### Adapter dimension scoping

The replay grammar exercises the full M4 adapter boundary through `ADAPTER_METADATA` and
`SNAPSHOT_REQUEST` events. The table below maps each M4 adapter function to its replay
representation:

| M4 API function | Replay event(s) | Fields in differential scope | Fields unit/fuzz only |
|---|---|---|---|
| `adapt_exchange_depth_snapshot` | `INSTALL_BASELINE` + `ADAPTER_METADATA` | Price/quantity decimal parsing, venue, market, symbol, last_update_id, `observed_quality` mapping | Wire schema version, producer version, request_id, connection_id (non-semantic wire identity fields) |
| `adapt_depth_update` | `DEPTH_UPDATE` + optional `ADAPTER_METADATA` | Price/quantity decimal parsing, first/final update IDs, `pu`, venue, market, symbol, `observed_quality` mapping | Wire schema version, producer version, request_id (non-semantic wire identity fields) |
| `install_into` / `apply_to` | `INSTALL_BASELINE`, `DEPTH_UPDATE` | `NumericSpec` + policy binding check, `InstallResult`/`ApplyResult` | Internal adapter span-view validity (validated by construction in the replay driver) |
| `make_local_order_book_snapshot` | `SNAPSHOT_REQUEST` | `depth_limit`, `host_quality_facts`, `SnapshotContext` (identity, producer, source, timestamps, gap recovery state), output snapshot semantics, eligibility rules | Protobuf wire serialization byte equality (non-deterministic across compilers) |

Fields not representable in the replay grammar (wire schema version, request_id, connection_id,
non-semantic producer metadata) are validated through M4 unit and property tests, not through
differential replay.

### Manifest

Every fixture directory ships with `manifest.json` (or a canonical text manifest) containing:

```text
fixture ID
schema version (replay-log v1)
market, symbol, NumericSpec, policy
event count
SHA-256 of the canonical log bytes
provenance (for recorded transcripts, see Recorded workloads)
conversion/normalization version
```

The fixture hash is the identity used by all comparison and reporting; no replay result is
recorded without its fixture hash.

### Normalized in-memory log

The replay driver parses the canonical log once into a normalized in-memory operation sequence.
The timed portion of replay benchmarks consumes the in-memory sequence; fixture parsing is never
inside a timed region. The normalized sequence is what a future M6 Host can produce from live
normalization: one normalized op per canonical event, so live ingestion and replay invoke the same
Core semantics (Section: Live/replay equivalence).

## Canonical text format

The replay log, manifest, and observation stream are all canonical text formats. The rules below
apply to every canonical text artifact produced or consumed by M5.

### Byte encoding and line discipline

```text
Encoding:            UTF-8 (no BOM)
Line ending:         LF (0x0A) only — carriage return (0x0D) is forbidden
Final newline:       required — every canonical text file ends with exactly one LF
Blank lines:         forbidden (every line is a header, event, or record)
Leading whitespace:  forbidden on any line
Trailing whitespace: forbidden on any line
Tabs:                forbidden — spaces only for intra-line separation
Comment lines:       forbidden in the canonical event/observation section;
                     if present in a header, they must be explicitly prefixed
                     with a defined comment marker and their canonical treatment
                     documented
```

### Token separation

```text
Token separator:  single ASCII space (0x20)
Multiple spaces:  non-canonical — the parser rejects them in checked-in fixtures
```

### Integer canonical spelling

```text
Format:      ASCII decimal digits only
Sign:        no leading '+' sign; negative sign '-' only for fields intentionally
             representing negative values (not applicable to IDs)
Zero:        canonical zero is "0" — "-0" forbidden, leading zeros forbidden
             ("001" is invalid)
Range:       0 .. UINT64_MAX for unsigned IDs; exact decimal representation
Hex/octal/scientific notation:  forbidden
```

### Decimal spelling

Replay fixtures intentionally test unusual but valid inbound decimal spellings
(leading zeros, trailing zeros, different fractional precisions). Therefore the
**replay input log preserves the exact intended decimal token spelling** as
supplied by the fixture author or generator. The replay parser does not
canonicalize decimal tokens before hashing.

```text
Allowed forms:  positive decimal strings matching the M1 parser grammar
                (no sign, optional fractional part with '.')
Explicit tests: trailing zeros, leading zeros, exact-precision edge cases
                are preserved verbatim in the fixture
```

For the **canonical observation stream** (output), semantic decimal values use
the M1 canonical fixed format:
```text
integer part in decimal with no leading zeros (except zero itself)
decimal point
fractional part with trailing zeros preserved according to NumericSpec scale
no sign for positive values
```

### Enum and event symbolic spelling

```text
Event names:     uppercase ASCII tokens: INSTALL_BASELINE, DEPTH_UPDATE,
                 REBASELINE, RESET, SNAPSHOT_REQUEST, ADAPTER_METADATA,
                 MALFORMED_RANGE
Market:          Spot | UsdMPerpetual
Policy:          Spot | UsdMPerpetual
Quality facts:   stable enum member names (e.g., Duplicate, OutOfOrder,
                 OrderBookResync, ...) — see the HostQualityFact enum
Result dispositions:  Applied, IgnoredStale, IgnoredDuplicate, GapDetected,
                      RejectedWrongState, Installed, RejectedWrongState
Adapter errors:  stable AdapterErrorCode enum member names
All:             case-sensitive, no locale-dependent parsing
```

Unknown wire enum numeric values for adapter coverage are representable through an
explicit numeric field in the fixture if needed.

### Optional-value syntax

```text
Absence:  "-" (single hyphen)
Presence: the value
```
The hyphen token "-" cannot collide with legitimate value tokens because no numeric
or enum token is a single hyphen.

### Timestamp and context fields

```text
generated_time_utc_ns:       unsigned decimal nanoseconds
generated_monotonic_ns:      unsigned decimal nanoseconds, or "-" when absent
detected_at_utc_ns:          unsigned decimal nanoseconds
Timestamps use the units defined by the M4 SnapshotContext API;
no new time semantics are invented
```

### Lists and ordering

```text
Level list:        comma-separated price,quantity pairs; bid-then-ask canonical
                   merge order; price order within each side is the fixture's
                   intended input order
Quality-fact list: space-separated enum names in HostQualityFact integer-value
                   ascending order for observation stream; fixture input order
                   is preserved in the replay log
Empty list:        "-" (consistent with optional-value absence)
Duplicates:        forbidden in observation stream; allowed in replay log if
                   semantically meaningful for the fixture
```

### Canonical manifest serialization

Fixture identity uses the canonical replay-log bytes. The manifest is stored as
JSON AND its scalar identity-bearing fields (fixture hash, schema version, market,
symbol, NumericSpec, policy) are checked during identity validation. Two manifest
files that are logically equal but byte-different produce the same identity as long
as the hash of the canonical replay log matches.

### Hash domain separation

```text
ReplayLogSHA256 = SHA-256(exact canonical replay-log bytes)
FixtureIdentity = versioned tuple: (replay-log schema version, ReplayLogSHA256)

The manifest includes metadata (market, symbol, NumericSpec, policy) that affects
semantic interpretation. The fixture hash is the ReplayLogSHA256, not the manifest
hash. Two replay logs with identical bytes but different manifest metadata (and
therefore different expected semantics) would share a FixtureIdentity only if the
metadata difference is intentional (e.g., same log replayed under a different
policy) — in that case the workload identity is (FixtureIdentity, metadata), not
FixtureIdentity alone.
```

### Observation stream canonicalization

The canonical OperationObservation stream uses the same foundational rules:
UTF-8, LF, single-space token separation, canonical integer spelling, canonical
M1 decimal format for output values, stable enum names, explicit optional presence
("-"), and quality-fact ordering by integer value.

Unlike the replay input log, observation output decimal values are canonicalized
to the M1 fixed format. This ensures the semantic digest is reproducible across
compilers and platforms.

### Parser behavior for checked-in fixtures

The canonical replay-log parser rejects non-canonical input for checked-in fixtures.
A separate offline converter accepts non-canonical source input and emits canonical
replay-log v1. The replay driver does not silently normalize whitespace or line endings
before hashing — it rejects.

## Reference model

The composed oracle executes:

```text
canonical event
  -> R1 exact decimal parse/rescale (text -> units)
  -> R3 reference projection (or R2 directly for book-only fixtures)
  -> R4 reference snapshot semantics (for SNAPSHOT_REQUEST events)
```

For M4 adapter-enabled replay, the production path additionally runs the real adapter
(`adapt_depth_update`, `install_into`/`apply_to`, `make_local_order_book_snapshot`) while R4
computes the expected semantic outcome independently. The M4 adapter is exercised because M5 must
validate the full milestone boundary; Core-only replay remains a separate mode that never links the
adapter.

### Monolithic versus layered tradeoff summary

| Criterion | Layered (selected) | Monolithic |
|---|---|---|
| Divergence attribution | First divergent layer reported | Whole-pipeline mismatch only |
| Reuse of reviewed models | R2/R3 already exist | Requires discarding them |
| Oracle/production accidental sharing risk | Low (per-layer rules) | Higher (composition replicated) |
| Implementation cost | Medium (D + R1/R4 new) | Lower |
| Diagnostic quality | High | Low |

## Datasets and workloads

### A. Synthetic deterministic workloads

Generated from fixed seeds with the repository's existing deterministic generator pattern
(LCG/xorshift64\*), compiled to canonical replay logs at build/test time or checked in as fixtures.
Coverage dimensions:

```text
small books (<= 8 levels/side)
medium books (50-200 levels/side)
deep books (1,000-10,000 levels/side)

high update locality (prices cluster near best)
low update locality (uniform over full depth)

mostly replacements
many deletions
many insertions

top-of-book churn
deep-book churn

duplicate prices within a batch
stale updates
gaps/rebaseline
locked/crossed books

Spot
USD-M
```

Every synthetic workload is reproducible from `(generator, seed, parameters)`; the fixture hash is
computed from the generated canonical log so results always identify the exact input.

### B. Recorded representative transcripts

M5 defines an **offline fixture format** for real recorded Binance-style workloads: the canonical
replay log plus manifest described above. A converter (design-only in this PR, implemented in M5)
transforms externally recorded data into the canonical format. Provenance requirements recorded in
the manifest for every real transcript:

```text
source (recording tool / repository)
market
symbol
capture time range
event count
normalization/conversion version
SHA-256
schema identity
```

Rules:

- No CI test may depend on live Binance access. Recorded fixtures are checked in (small tier) or
  fetched from a pinned, hashed location (medium/large tiers) with the manifest verified before use.
- If BinanceMarketDataRecorder datasets exist externally, the converter consumes them read-only;
  this design does not modify the Recorder repository and does not specify its internals.
- The exact representative corpus is a blocking open decision (Section: Open decisions) because it
  requires a data-acquisition step outside this repository.

## Dataset size tiers

| Tier | Events | Book depth (levels/side) | Fixture bytes (approx) | Runtime budget | Purpose |
|---|---|---|---|---|---|
| tiny | 10-100 | <= 8 | <= 1 KB | seconds | Unit/debug validation; checked in |
| small | 10^3-10^4 | <= 50 | 0.1-1 MB | 1-3 min per job | Blocking CI differential validation; checked in (or generated deterministically) |
| medium | 10^5-10^6 | <= 1,000 | 10-100 MB | 10-30 min | Scheduled/normal benchmark evidence; generated or pinned |
| large | 10^6-10^8 | <= 10,000 | 0.1-10 GB | hours | Manual/performance characterization; generated or pinned |

Mandatory PR CI stays within the small tier for differential validation and within a short
benchmark correctness smoke; medium/large runs are scheduled or `workflow_dispatch`.

## Determinism validation

M5 proves replay determinism by replaying the same canonical transcript:

```text
multiple times within one process
into different temporary paths
in separate process invocations (re-exec)
under Debug and Release builds
across supported compilers (Ubuntu GCC, Ubuntu Clang, macOS AppleClang)
```

Result requirement: the recorded semantic observation stream must be identical in every case.

### Differential observation model

The differential oracle observes every canonical replay event as an **OperationObservation**.
This is the fundamental unit of correctness comparison.

An `OperationObservation` for event N contains:

```text
event index
event kind (INSTALL_BASELINE, DEPTH_UPDATE, REBASELINE, RESET, SNAPSHOT_REQUEST)
observable operation result (see operation-result domain below)
post-operation semantic checkpoint
snapshot semantic observation (only for SNAPSHOT_REQUEST where produced)
```

The operation result is compared BEFORE the post-operation checkpoint. The first mismatch
among {result kind, result value/error fields, checkpoint, snapshot observation} is the
first observable divergence. A state-only comparison is insufficient because two
implementations can return different observable results while coincidentally ending in
identical state (e.g., `IgnoredStale` vs `IgnoredDuplicate` both leave the book unchanged).

### Canonical operation-result domain

For every canonical event class, the operation result is canonicalized as text and recorded
before the post-operation checkpoint:

**M1 parsing (for all events that carry decimal tokens):**
```text
success:  canonical value (units + scale)
failure:  canonical parse/rescale error category (DecimalError enum name)
```
A parsing failure that leaves the projection unchanged is still observable as a distinct
operation result. The reference does not silently succeed where production rejects.

**Baseline installation (INSTALL_BASELINE, REBASELINE):**
```text
result:   canonical InstallResult:
            disposition:  Installed | RejectedWrongState
            status_after: AwaitingBaseline | AwaitingBridge | Synchronized | NeedsResync
            last_update_id_after:  <decimal> | -
```
Captures the full `InstallResult` returned by `BookProjection::install_baseline`.

**Incremental application (DEPTH_UPDATE):**
```text
result:   canonical ApplyResult:
            disposition:  Applied | IgnoredStale | IgnoredDuplicate | GapDetected | RejectedWrongState
            status_after: <ProjectionStatus enum>
            last_update_id_after:  <decimal> | -
            gap:  <GapInfo fields> | -
```
Captures the full `ApplyResult` returned by `BookProjection::apply`.
`IgnoredStale` and `IgnoredDuplicate` differ in disposition but produce identical
checkpoint state; the operation result captures the distinction.

**M4 inbound adaptation (DEPTH_UPDATE in adapter mode, INSTALL_BASELINE in adapter mode):**
```text
result:   success { underlying M3 InstallResult or ApplyResult }
          | AdapterError {
              code: <AdapterErrorCode enum name>
              field: <AdapterField enum name> | -
              decimal_error: <DecimalError> | -
            }
```
On success, the underlying M3 result is preserved. On error, the observable
`AdapterErrorCode` and `AdapterField` are canonicalized. If an adapter error occurs
before any Core mutation, the post-operation checkpoint is unchanged from the previous
event.

**Snapshot request (SNAPSHOT_REQUEST):**
```text
result:   success {
            synchronized: true | false
            bids: <ordered levels>
            asks: <ordered levels>
            quality_flags: <canonical rank ordered>
            gap_descriptor: <fields> | -
          }
          | AdapterError { code, field, decimal_error }
```
Captures the full observable outcome of `make_local_order_book_snapshot`.
An eligibility rejection is canonicalized as an `AdapterError`.

### SemanticCheckpoint

The post-operation `SemanticCheckpoint` captures persistent projection state:
```text
status:            AwaitingBaseline | AwaitingBridge | Synchronized | NeedsResync
last_update_id:    <decimal> | -
last_gap:          { last_accepted_final, incoming_range, previous_final, reason, policy } | -
synchronized_visible:  true | false
bids:              price,quantity pairs in deterministic descending order
asks:              price,quantity pairs in deterministic ascending order
```
This extends the existing `ProjectionCheckpoint` (`tests/projection_state/test_helpers.hpp`)
and additionally captures `NumericSpec` identity for the fixture.

### Canonical observation stream and semantic result digest

- For every event, the **canonical OperationObservation** (operation result +
  SemanticCheckpoint + snapshot observation where applicable) is serialized as canonical
  text according to the rules in Section "Canonical text format".
- The stream of OperationObservations is hashed with SHA-256 into the **semantic result
  digest** for the workload.
- Determinism within a toolchain is proven by identical digests across runs/processes/build
  types.
- Cross-compiler equivalence is proven by identical digests across toolchains (Section:
  Cross-compiler semantic manifest).
- Only semantic content enters the digest. Never compared: addresses, ABI layouts,
  unordered output, wall-clock values, sanitizer artifacts, exception text (unless part of
  a stable API contract), compiler-specific strings.

## Failure diagnostics

### First-divergence artifacts

The first divergence is the first event N for which any of the following differ between
production and reference, compared in this exact order:

```text
1. operation-result kind (success vs error vs disposition category)
2. operation-result value/error fields (canonical InstallResult, ApplyResult, AdapterError, etc.)
3. post-operation SemanticCheckpoint
4. snapshot semantic observation (if produced)
```

On any differential mismatch, the replay driver emits:

```text
seed / transcript identity
fixture ID and manifest hash
event index
divergence category (operation result / checkpoint / snapshot observation)
offending event text / normalized event
production operation result (canonical)
reference operation result (canonical)
production SemanticCheckpoint
reference SemanticCheckpoint
relevant NumericSpec
sequence policy
market identity
diverging layer (R1-R4 or D)
```

All artifacts are deterministic text, printed by the test and written to the failure artifact file.

### Replay minimizer / reducer

Design recommendation: a deterministic **event-index reducer** for synthetic fixtures (the fixture is
regenerated from the seed and truncated by index ranges) and an **interval reducer** for recorded
transcripts (binary search over contiguous index ranges to find a minimal failing subsequence).
The reducer runs only on failure, outside CI time budgets. It is designed but not implemented in
this PR.

## Cross-compiler semantic manifest

A **portable semantic-result manifest** records, per workload:

```text
fixture ID + manifest hash
workload ID
toolchain (compiler, version, OS, architecture)
build type
fixture set identity
fixture hashes
semantic result digest (SHA-256 of canonical OperationObservation stream)
comparison status
manifest schema version
```

Rules:

- Only semantic content enters the digest: enum names as stable strings, integers in decimal,
  decimals through the canonical fixed format, levels in deterministic M2 order.
- Never compared: compiler-dependent binary layout, addresses, unordered iteration (M5 introduces
  no unordered container on an observable path), wall-clock values, sanitizer artifacts.
- The manifest is produced by a CTest-style test on each of the three release-matrix jobs
  (Ubuntu GCC, Ubuntu Clang, macOS AppleClang) and uploaded as a GitHub Actions artifact.
  A dedicated `m5-semantic-compare` blocking job downloads all three artifacts and asserts
  cross-toolchain digest equality. See "Cross-job semantic manifest transport" for the
  artifact fan-in architecture.

### Cross-job semantic manifest transport

The cross-compiler manifest comparison requires reliable artifact transport across isolated
GitHub Actions jobs. The architecture defines a concrete artifact fan-in model.

**Producer jobs.** Each of the three release-matrix jobs (Ubuntu GCC, Ubuntu Clang,
macOS AppleClang — all Release builds) produces a machine-readable semantic manifest
tied to:

```text
exact HEAD SHA
toolchain identity (compiler name + version)
build type (Release)
fixture set identity
fixture hashes
observation-stream digest(s) (SHA-256 of canonical OperationObservation stream)
manifest schema version (v1)
```

The manifest is uploaded as a GitHub Actions artifact with a deterministic naming scheme:

```text
m5-semantic-manifest-ubuntu-gcc-<short-head-sha>
m5-semantic-manifest-ubuntu-clang-<short-head-sha>
m5-semantic-manifest-macos-appleclang-<short-head-sha>
```

**Comparison job.** A dedicated blocking job `m5-semantic-compare` (`runs-on: ubuntu-latest`) with:

```text
needs: all three release-matrix producer jobs
```

The comparison job:

```text
1. Downloads all three expected artifacts via actions/download-artifact.
2. Fails immediately if any artifact is missing (no silent skip).
3. Validates manifest schema version on each artifact.
4. Validates identical HEAD SHA across all three manifests.
5. Validates identical fixture-set identity and fixture hashes.
6. Compares observation-stream digests per workload.
7. Reports the first mismatching workload ID, toolchains involved,
   fixture hash, expected and actual digests.
8. Does not compare manifests generated from different commits.
```

**Fail-closed behavior.** The comparison job fails with a clear diagnostic if:

```text
any producer job was skipped, failed, or re-run with a different SHA
any expected artifact is missing
an artifact contains a HEAD SHA mismatch
fixture hashes differ across manifests
a required workload is present in some but not all manifests
manifest schema version is unrecognized
manifest JSON is unparseable
any observation-stream digest differs
```

A digest mismatch alone must not produce an opaque failure. The comparison artifact
includes enough stable diagnostic information for a developer to identify the first
divergent workload and re-run locally.

**Debug/Release comparison scope.** Cross-toolchain blocking comparison is restricted
to Release builds across the three release-matrix platforms. Debug-vs-Release determinism
within a single toolchain is validated by the `m5-replay` job (replaying under both Debug
and Release on Ubuntu Clang and asserting identical observation-stream digests). This
avoids a 3×2 = 6-way matrix explosion while still proving that build type does not affect
semantics.

**Job count reconciliation.** The original "only +1 new PR-blocking job" claim is
superseded. The corrected CI strategy adds two PR-blocking jobs:

```text
m5-replay             (small-tier replay, determinism re-runs, diagnostics)
m5-semantic-compare   (cross-toolchain manifest download and comparison)
```

## Fuzz strategy

M5 does not replace existing M1-M4 fuzzers. It adds one replay/differential fuzzer:

```text
bmd_projection_replay_fuzz
```

Decoded operations span milestone boundaries:

```text
snapshot install
incremental updates
sequence transitions
reset
rebaseline
snapshot generation
Host context
wire adaptation where enabled
```

The harness drives production and the layered reference pipeline after every decoded operation and
aborts on any divergence, reusing the existing per-step OperationObservation comparison discipline.
The fuzzer uses **direct structured operation decoding from fuzz bytes**, not text fixture parsing.
This maximizes semantic reach: fuzz bytes are decoded into a sequence of structured operations
(install baseline, apply depth update, reset, snapshot request, etc.) without going through the
canonical text parser. The canonical text log format is used only for deterministic replay tests.

Where a full oracle is not practical (e.g., Protobuf byte-serialization equality is only defined
within one binary), explicit invariants are asserted instead:

```text
failure of adapt/binding/output leaves the projection checkpoint unchanged
binding errors precede any Core call
M3 dispositions propagate unchanged through the adapter
snapshot eligibility matrix holds for every state
```

The harness is added to `scripts/fuzz-smoke.sh` and the existing Ubuntu Clang blocking smoke job.

## Fuzz corpus management

Corpus categories are structural, not arbitrary labels:

```text
Spot synchronized stream
USD-M synchronized stream
bridge transition
gap
recovery
duplicate/stale
locked/crossed
decimal boundaries
depth-limit snapshot
quality combinations
```

The `quality combinations` category explicitly seeds HostQualityFact combinations and their
state-eligibility rules — the gap identified by deferred finding M4-IIR-3 (Section: M4 deferred P2
disposition). Seeds are generated deterministically or checked in as text; every corpus file maps to
a named category.

## M4 deferred P2 disposition

| Finding | Decision | Rationale |
|---|---|---|
| M4-IIR-1: full dependency-set lock-drift assertion | **DEFER TO MAINTENANCE** | Packaging/dependency test hardening, orthogonal to differential validation and performance; it is a small standalone fix outside the M5 charter |
| M4-IIR-2: shared ProtoAdapter symbol audit | **DEFER TO MAINTENANCE** | Packaging symbol-ownership audit concern; M5 performs no packaging work |
| M4-IIR-3: richer M4 fuzz inputs/corpus | **INCORPORATE INTO M5** | M5's replay/differential fuzzer and the `quality combinations` corpus category subsume quality-flag and HostQualityFact coverage; closing is recommended after M5 corpus work lands, decided by implementation-review evidence — it is not closed by this design |

These are design dispositions only; no code changes occur in this PR, and the three findings remain
DEFERRED / NON-BLOCKING until evidence closes them.

## Benchmark methodology

### Principles

```text
Release/optimized builds only for timing
no sanitizer builds are timed
warmup runs before measurement
multiple repetitions (>= 5; more where variance is high)
statistical reporting (median, mean, stddev, percentiles)
stable CPU environment when possible (CI cannot guarantee; document noise)
setup cost excluded from the measured region unless deliberately measured
results consumed (benchmark::DoNotOptimize / KeepBytes) so nothing is optimized away
toolchain/hardware recorded in the metadata wrapper
```

Google Benchmark 1.9.5 already exists as a repository dependency and is the framework. M5 adds no
other benchmark framework.

## Benchmark dimensions

Separate benchmarks per milestone boundary:

```text
M1: decimal parse, exact rescale, fixed format
M2: single insert (apply_level), replace, delete, batch update (apply_updates),
    replace_all, best bid/ask, quantity_at, top-N, full level copy (all_levels)
M3: baseline install, incremental apply (copy-on-apply cost: book depth x batch size),
    decomposed into full-side vector copy (all_levels both sides), candidate OrderBook
    construction from vectors, move-assignment commit; stale/duplicate/gap classification,
    state transitions, reset
M4: adapt_exchange_depth_snapshot, adapt_depth_update, checked install/apply,
    make_local_order_book_snapshot (with/without depth limit), output serialization
End-to-end: canonical replay events per second (production pipeline vs replay driver)
```

Nothing is optimized in M5 before evidence exists; M5 measures the current implementation.

## Latency metrics

- Microbenchmarks: Google Benchmark statistics (median, stddev, percentiles from repetitions) are
  sufficient; the repository documents that single-operation microbenchmark statistics do not
  represent production tail latency.
- Replay workloads: events/sec, ns/event, CPU time and wall time, and p50/p90/p99 (p99.9 only where
  the sample size supports it — documented alongside the metric).
- Methodology statement: replay latency is measured per event on the preloaded in-memory log under
  single-writer serial semantics; tail claims are scoped to the sample size and runner environment.

## Allocation metrics

- A **counting/live-bytes allocator** (global `operator new` override in benchmark executables only,
  following the established allocation-failure pattern but counting instead of failing) measures:
  allocation count and total allocated bytes. Peak live bytes is a documented best-effort metric:
  it requires tracking allocation sizes on deallocation (via C++14 sized `operator delete` where
  supported) and is reported with explicit platform-dependence caveats. Allocation count and
  total allocated bytes are the mandatory metrics.
- Allocation metrics are captured for: accepted M3 apply (the copy-on-apply path), baseline install,
  M4 adaptation, snapshot output, and replay of a small workload.
- Production APIs receive no benchmark-only hooks. Instrumentation lives in test/benchmark-only
  executables and the replay driver, never in production code.

## Memory footprint

- Measure order-book storage versus depth for 100 / 1,000 / 5,000 / 10,000 levels per side.
- Metrics: estimated storage (bytes per level, container overhead, per-side and total), measured via
  the live-bytes instrumentation after population, plus a documented container-size model for
  `std::map` nodes.
- Object/container storage is separated from benchmark infrastructure memory by measuring in an
  isolated benchmark executable whose own baseline footprint is subtracted.

## Container performance spike

O-P003 requires evaluating alternatives to `std::map` with evidence.

### Candidate set (justified)

| Candidate | Justification | Dependency |
|---|---|---|---|
| `std::map` | M2 correctness baseline | none (production) |
| Sorted contiguous vector (naive) | Cache-friendly, no dependency, `std::lower_bound` + `std::vector::insert`/`erase` per individual level; classic candidate for read-heavy books | none (benchmark-only header) |
| Abseil `btree_map` | Cache-efficient B-tree, already available in the dependency graph transitively via `protobuf`; proven C++20-ecosystem option | `abseil/20260107.1` benchmark-only |
| Sorted vector with batch-aware last-write-wins | Sorted contiguous storage with batch-aware update: deduplicates all levels at a given price within a batch before a single erase+insert per distinct price, reducing shift cost relative to the naive vector | none (benchmark-only header) |

The "sorted contiguous vector (naive)" does one insert/erase per individual `LevelUpdate`.
The "sorted vector with batch-aware last-write-wins" groups all updates to the same price
in a batch, applies last-write-wins in-place, and does at most one erase/insert per distinct
price. They are algorithmically distinct and measurable as separate candidates.
Any additional candidate must be justified before inclusion; the set is not open-ended. If a
candidate would add a new dependency, it remains benchmark/spike-only until an explicit later
decision.

## Container semantic requirements

Every candidate must preserve exact M2 behavior; a candidate that cannot is invalid regardless of
speed:

```text
bid descending, ask ascending
absolute quantity replacement
zero deletion
missing deletion no-op
same-price last-write-wins
replace_all atomic semantics (strong exception guarantee)
locked/crossed acceptance
deterministic output order
noexcept move requirement for OrderBook-style ownership
```

A semantic conformance suite (the M2 unit/property/replay checks run against a
container-model harness) is a mandatory gate before any candidate is benchmarked.

## Container experiment isolation

- Container candidates are evaluated through **benchmark-only adapters**: a small internal
  interface (`OrderBookContainerModel`, benchmark-only) with one implementation per candidate,
  a compile-time selection in the benchmark harness, and a semantic conformance harness that runs
  the M2 checks against each model.
- Production `OrderBook` and its `std::map` storage are not modified during the spike.
- No candidate is benchmarked by changing production code first.

## Container decision criteria

"One microbenchmark is slightly faster" is not evidence. A migration decision uses a decision
matrix:

```text
representative replay throughput
update latency
top-N / read latency
memory (bytes per level, footprint at deep books)
allocation count
deep-book behavior (1k/5k/10k levels/side)
implementation complexity
dependency cost
portability (Ubuntu GCC, Ubuntu Clang, macOS AppleClang)
exception safety
determinism
maintenance burden
```

Decision methodology:

1. Semantic conformance suite must pass for the candidate.
2. Results must come from the representative workload suite (not a single microbenchmark).
3. Material improvement threshold: statistically significant improvement (non-overlapping
   confidence intervals after >= 5 repetitions, >= 3 sigma under the measured noise floor) of
   **at least 20%** in at least two of {replay throughput, update latency, top-N latency, memory}
   AND no regression beyond 10% in any other measured dimension.
4. The noise floor is established empirically by repeated identical measurements on unchanged code
   in the same environment.
5. All evidence, candidate metadata, and the matrix are recorded in the machine-readable report.

## No premature container migration

M5 distinguishes:

```text
PERFORMANCE SPIKE          benchmark-only candidate comparison (part of M5)
PRODUCTION CONTAINER MIGRATION   changing production OrderBook storage (NOT part of M5)
```

Initial M5 implementation benchmarks candidates only. If evidence supports migration, the actual
production replacement requires:

```text
an explicit decision record / ADR (proposed as a future ADR-0008 at migration time)
full M2-M5 regression
independent review
```

This design does not pre-authorize migration.

## Performance reporting

### Baseline artifact format

A **machine-readable benchmark-result format**: Google Benchmark JSON (already produced) plus a
stable JSON metadata wrapper:

```text
git SHA
compiler
compiler version
OS
architecture
CPU
build type
benchmark name
workload ID
fixture hash
iterations
timing metrics (median/mean/stddev/percentiles)
allocation metrics (count/bytes/peak live)
memory metrics where applicable
```

The wrapper schema is versioned; reports without a wrapper or fixture hash are not comparable.

### Benchmark baseline comparison

- A scripted comparison tool compares a candidate branch's recorded results against a known
  baseline SHA's recorded results; both result sets come from executions on the same environment
  (A/B or sequential runs on the same runner), never across environments.
- The tool distinguishes statistically meaningful regression from noise using the noise floor and
  confidence-interval overlap; it reports, it does not block.

## Performance regression policy

```text
Blocking CI:
  benchmark target builds and executes (correctness smoke)
  differential/correctness gates (replay, property, fuzz, semantic manifest)
  gross sanity thresholds only if robust (e.g., an absolute events/sec floor that
  catches pathological regressions, not percentage deltas)

Non-blocking / reporting:
  fine-grained performance comparison
  historical trend
  percentage regression gates
```

Percentage regression gates are explicitly **not** PR-blocking on shared GitHub runners: runner
CPU noise is uncontrolled, so a percentage delta cannot be attributed to the change. This is the
justification for the separation; any future percentage gate requires a controlled environment
(dedicated runner) and a documented noise-floor study.

## CI strategy

Existing 16-job matrix remains intact. Proposed additions:

| Job | Platform | PR blocking | Content |
|---|---|---|---|
| `m5-replay` | Ubuntu Clang | Yes | Small-tier differential replay (Spot + USD-M), determinism re-runs (Debug + Release), first-divergence diagnostics, snapshot-semantics replay, M5 property tests |
| `m5-semantic-compare` | ubuntu-latest | Yes | Downloads semantic-manifest artifacts from the three release-matrix producer jobs; validates HEAD SHA, fixture identity, manifest schema, and cross-toolchain observation-stream digest equality |
| Semantic manifest computation | folded into existing `release` matrix jobs (Ubuntu GCC, Ubuntu Clang, macOS AppleClang) | Yes | Each release job computes the semantic observation-stream digest and uploads it as a named artifact |
| `fuzz smoke (Ubuntu Clang)` | existing job, extended | Yes | Adds `bmd_projection_replay_fuzz` to the 10,000-input smoke |
| `benchmark smoke (Ubuntu Clang)` | existing job, extended | Yes | Builds and runs the representative benchmark suite (correctness smoke + gross sanity floor); JSON + wrapper uploaded as artifacts |
| `m5-performance` | Ubuntu Clang | No (workflow_dispatch / scheduled) | Medium/large tiers, container spike, full statistical comparison, allocation/memory characterization |

Two new PR-blocking jobs are added (`m5-replay`, `m5-semantic-compare`); everything else is
an extension of existing jobs or a scheduled/manual workflow. Existing M0-M4 CI behavior is
unchanged.

## Scheduled/manual performance workflow

Serious performance runs use:

```text
workflow_dispatch for manual characterization runs
a scheduled workflow (e.g., weekly) for medium-tier trends and the container spike
a future dedicated self-hosted runner only after a documented noise-floor study
```

Design only; no workflow is created in this PR.

## Historical performance policy

Artifact retention options evaluated:

| Option | Tradeoff |
|---|---|
| GitHub Actions artifacts | Easy, zero repo cost, but expire (30-90 days) and are not searchable |
| Checked-in small canonical baselines | Permanent, reviewable, but must be updated deliberately; limited to tiny/small tiers |
| Release artifacts | Permanent for released SHA, but M5 produces no releases |
| External future dashboard | Best long-term, but requires infrastructure outside this repository |

Selected policy: checked-in canonical semantic digests for tiny/small tiers (they are part of the
correctness gates), GitHub Actions artifact retention for medium/large benchmark reports, and a
documented conversion path to a future dashboard if one is ever deployed. Retention duration is a
non-blocking open decision.

## Dependencies

| Dependency | Why | Version strategy | Conan | License | Platforms | Scope | Can it leak into Core? |
|---|---|---|---|---|---|---|---|
| `abseil` (`btree_map`) | Container spike candidate | Pin to the version already in the lock graph (`20260107.1`); lock updated only through Conan | Already present transitively via `protobuf`; added as an explicit benchmark-only test requirement if needed | Apache-2.0 | Same as existing Protobuf usage (Ubuntu GCC/Clang, macOS AppleClang) | Benchmark/spike only | No: spike target is isolated; production Core stays standard-library-only; migration requires a separate ADR + review |

No other new dependencies are proposed. Google Benchmark and GoogleTest remain existing
test/benchmark-only dependencies. There are **no new production dependencies**.

## API surface

- **No new production public API** is proposed for M5.
- Replay support, reference layers, fixtures, and the container spike live in test/benchmark-only
  targets and `fuzz/`.
- If measuring internals ever requires a new public API, that is an architecture concern requiring
  strong justification and a separate review; the PIMPL boundary is not weakened for benchmarks.
- Test-only introspection strategy: internal benchmark-only headers under `tests/`/`benchmarks/`,
  allocator overrides in dedicated executables, and the existing public const queries. No
  benchmark convenience API may become a production contract accidentally.

## Live/replay equivalence

The canonical event set is the **normalized operation vocabulary**: each canonical event maps to
exactly one normalized adapter/Core operation (`install_baseline`, `apply`, `reset`, snapshot
build with explicit context). M5 documents this mapping so that a future M6 Host can produce the
same operations from live normalization. Live ingestion and historical replay therefore invoke the
same Core semantics through the same adapter boundary. M5 requires no live Gateway.

## Replay performance boundary

The benchmark harness excludes:

```text
network I/O
WebSocket decoding outside the approved wire fixture boundary
REST calls
disk reads inside the timed loop (except intentional replay-I/O measurement)
gRPC
Gateway runtime
```

Fixtures are parsed into the normalized in-memory log before timed execution.

## Reproducibility

- Fixed random seeds, fixed transcripts, fixed `NumericSpec`, fixed markets/policies.
- Stable fixture hashes (SHA-256) recorded in every report.
- Preallocated inputs where appropriate; explicit benchmark parameters.
- No benchmark workload depends semantically on system clock, `random_device`, network, or mutable
  environment.

## Risks and mitigations

| Risk | Mitigation |
|---|---|
| Reference model accidentally shares production logic | Layered models with explicit "must not call production helpers" rules; reviewed per layer; divergence diagnostics name the layer |
| Unrepresentative synthetic workloads | Workload dimensions derived from M1-M4 semantics; recorded transcripts provide real-world validation; provenance recorded |
| Real transcript provenance drift | Manifest requires source/market/symbol/time range/event count/conversion version/SHA-256/schema identity; conversion is offline and versioned |
| Benchmark noise on GitHub runners | No percentage gates in PR CI; noise floor measured; confidence-interval methodology; gross sanity floors only |
| Compiler optimization invalidating benchmarks | Google Benchmark `DoNotOptimize`/`KeepBytes`; results consumed; Release-only timing |
| Container comparison with different semantics | Semantic conformance suite is a mandatory gate before any benchmark; invalid candidates excluded |
| Memory/allocation measurement distortion | Isolated executables; baseline subtracted; documented measurement scope |
| Fixture repository size growth | Tiers: small fixtures checked in, medium/large generated or pinned with hashed fetch |
| Cross-platform timing comparability | Timing never compared across environments; semantic digests are the only cross-platform comparison |
| Third-party benchmark dependency leakage | Spike-only targets; production Core dependency graph audited by existing Core-only gates |
| Overfitting to one symbol or market | Representative suite covers Spot and USD-M, multiple depth/localities/churn profiles |

## Open decisions

### Blocking before implementation — CLOSED

| ID | Decision | Current position |
|---|---|---|
| OD-M5-001 | Exact representative transcript corpus | **CLOSED:** Recorder M21.4 validated 24h BTCUSDT Spot + USD-M perpetual live Raw capture as M5 corpus source v1. Primary medium fixtures: M5-REC-SPOT-BTCUSDT-V1, M5-REC-USDM-BTCUSDT-V1. See `docs/M5_PREIMPLEMENTATION_DECISIONS.md`. |
| OD-M5-002 | CI job/runner budget for medium-tier scheduled runs | **CLOSED:** Free standard GitHub-hosted runners only. Docs-only CI skip, superseded-run cancellation, <= weekly scheduled medium, 45 min timeout, manual-only container spike, 200 MiB/7-day artifact ceilings. See `docs/M5_PREIMPLEMENTATION_DECISIONS.md`. |

### Can be resolved during the performance spike

| ID | Decision |
|---|---|
| OD-M5-003 | Semantic manifest granularity (per-event vs per-checkpoint observation digest) — default: per-event for tiny fixtures, per-checkpoint for small. The cross-job transport architecture is fixed; granularity choice does not affect manifest schema identity |
| OD-M5-004 | Candidate flat-map implementation details (batching strategy, erase discipline) |
| OD-M5-005 | Regression statistical threshold tuning (noise-floor study refines the 20%/10% figures) |
| OD-M5-006 | Replay minimizer heuristic details (interval vs seed reduction) |
| OD-M5-007 | Artifact retention duration for medium/large reports |

## Implementation sequence

```text
Phase 1:  canonical replay fixture/model infrastructure (grammar, manifest, parser,
          normalized log, tiny fixtures)
Phase 2:  independent differential oracle (R1 promoted, R4 new, ReplayDriver,
          layer attribution)
Phase 3:  large deterministic replay validation (small/medium tiers, checkpoint
          comparison, first-divergence diagnostics)
Phase 4:  cross-compiler semantic manifests (canonical observation stream, SHA-256
          digests, comparison test)
Phase 5:  M5 differential fuzzing (bmd_projection_replay_fuzz, corpus categories,
          fuzz-smoke integration)
Phase 6:  representative Google Benchmark workloads (M1-M4 microbenchmarks,
          replay events/sec, metadata wrapper)
Phase 7:  allocation/memory instrumentation (counting/live-bytes allocator,
          footprint measurement)
Phase 8:  container comparison spike (semantic conformance harness, candidate
          models, benchmark comparison, decision matrix evidence)
Phase 9:  container decision / ADR if needed (KEEP std::map is a valid outcome)
Phase 10: CI/reporting integration (m5-replay job, manifest fold-in, performance
          workflow, artifact retention)
Phase 11: independent implementation review
```

## Acceptance gates

### Correctness acceptance gates

```text
all existing M0-M4 tests remain green
deterministic canonical replay (tiny/small tiers, repeated runs and processes)
production/reference differential match across all replay fixtures
first-divergence diagnostics present and actionable on seeded faults
Spot and USD-M coverage
gap/reset/rebaseline coverage
M4 adapter-enabled replay coverage (Core-only mode also covered)
cross-compiler semantic consistency (manifest digests equal)
new M5 property tests
new M5 differential fuzz smoke
ASan, UBSan, TSan where supported / accurately documented
no Core/Contracts/ProtoAdapter architecture regression
```

### Performance acceptance gates

```text
representative workload suite exists and is deterministic
baseline std::map results recorded
container candidates measured consistently (where the spike runs)
allocation/memory characterization recorded
no candidate semantic divergence
results reproducible enough to support conclusions
container recommendation recorded (KEEP std::map is valid)
```

### Completion semantics

M5 is COMPLETE when each of the four components has passed its own gate:

```text
validation completion (correctness gates above)
benchmark coverage completion (dimensions above measured and recorded)
performance characterization completion (baseline + allocation/memory reports)
container-decision completion (recommendation recorded with evidence and review)
```

"Fast enough" is not a gate; the container decision may legitimately be `KEEP std::map`.

## Independent review checklist

Independent M5 Architecture Review must challenge:

- oracle independence per layer and the D-driver's neutrality;
- replay grammar coverage versus the required event classes;
- checkpoint comparison state completeness (book, sequence, gap, snapshot semantics,
  byte-vs-semantic rules);
- determinism claims across processes/compilers and the semantic digest design;
- dataset tier sizes and CI cost;
- fuzzer design and corpus categories (including the M4-IIR-3 disposition);
- benchmark methodology, latency/allocation/memory measurement validity;
- container spike isolation and semantic conformance gate;
- decision criteria and material-improvement threshold;
- regression policy (no fragile gates on shared runners);
- ADR-0007 decision and the migration-gate boundary;
- dependency scope (Abseil benchmark-only, no production leakage);
- API-surface restraint and PIMPL integrity;
- risks and open-decision completeness.

## Alternatives rejected

| Alternative | Reason rejected |
|---|---|
| Monolithic single oracle | Cannot attribute divergence to a layer; discards reviewed R2/R3 models |
| Running production twice and comparing it to itself | No independent reference; cannot detect shared-logic bugs |
| Live-network replay in M5 | Violates determinism and CI constraints; live ingestion belongs to M6 |
| Binary replay log format | Harder to review/diff/hash; text canonical log is deterministic and versioned |
| Production container replacement during M5 | Requires evidence, ADR, full regression, and independent review first |
| Percentage performance gates in PR CI | Shared-runner noise makes them unreliable |
| New production API for measurement | Unneeded; existing const queries and test-only instrumentation suffice |
| Weakening PIMPL for benchmarks | Encapsulation is a reviewed architecture property |
| Adding Boost/BTree dependencies beyond the justified spike set | Unjustified cost for the decision at hand |

## References

- `docs/MILESTONES.md` (M5 charter, M4 completion)
- `docs/OPEN_QUESTIONS.md` (O-P003 and M5 open decisions)
- `docs/adr/ADR-0007-differential-validation-oracle-architecture.md` (ACCEPTED)
- `docs/adr/ADR-0003-single-writer-order-book.md`, `ADR-0005-market-specific-sequence-policy.md`,
  `ADR-0006-protobuf-adapter-boundary.md`
- `docs/M2_ORDER_BOOK_SEMANTICS.md`, `docs/M3_SEQUENCE_AND_PROJECTION_STATE_DESIGN.md`,
  `docs/M4_SNAPSHOTS_AND_PROTOBUF_BOUNDARY_DESIGN.md`
- Merged M4 implementation at `ac780d9eb7b49ff20a6b3b4bee6a993b51b70af4`
