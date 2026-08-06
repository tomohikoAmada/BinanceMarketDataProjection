# M4 Snapshots and Protobuf Boundary Architecture

## Status

- Design status: **PROPOSED**
- Implementation status: **NOT STARTED**
- ADR status: **PROPOSED**
- External architecture review: **NOT STARTED**
- Date: 2026-08-06
- Projection base: `27279fe3e61c092d5dcc50cb35b0483b32ed428b`
- Contracts baseline: `01d76a41929f36d89573159f5f458f9f1e378ada`

This document is an architecture proposal, not an implementation claim. Every API and CMake
declaration below is **Proposed, not implemented, and subject to external review**. M4 implementation
must not start until this design is accepted and all implementation blockers are closed.

## Source-of-truth order

M4 applies evidence in this order:

1. The fixed Contracts `.proto` definitions are authoritative for message structure, field numbers,
   enum numbers, optional presence, package names, and wire compatibility.
2. Contracts Pydantic models are authoritative for business meaning and validation rules.
3. Accepted Contracts ADRs define cross-module architecture and wire/domain separation.
4. Accepted Projection ADRs and the implemented M1/M2/M3 API define Core semantics.
5. Projection `ARCHITECTURE.md` defines deterministic and dependency boundaries.
6. Proposed ADR-0006 and this document decide M4 details left open by higher authorities.
7. Implementation convenience has the lowest priority.

A lower source cannot silently weaken a higher one. M4 neither changes Contracts semantics nor
redefines a wire message in Projection.

## Evidence reviewed

### Projection

The following were read at Projection base `27279fe3e61c092d5dcc50cb35b0483b32ed428b`:

- repository guidance, README, architecture, changelog, milestones, open questions, CMake/package
  configuration, Conan configuration, install-consumer workflow, and ADR-0001 through ADR-0005;
- all current public numeric, order-book, and projection-state headers;
- all numeric, order-book, and projection-state implementation sources; and
- the current downstream consumer and CMake helper modules.

Relevant established facts are:

- `BinanceMarketDataProjection::Core` has no Protobuf or Contracts dependency;
- `NumericSpec` binds price and quantity scales from 0 through 18;
- parsing is strict and exact, accepts only representable exact rescaling, and never rounds;
- fixed formatting is locale-independent, non-scientific, and deterministic;
- `OrderBook` stores no zero level, emits bids descending and asks ascending, accepts locked/crossed
  books, and returns ordered vectors by value;
- `BookProjection` owns the only mutable book and has four states;
- `BookBaseline` and `DepthBatch` are synchronous non-owning span views;
- `last_update_id()` and `last_gap()` are optional; and
- only `Synchronized` exposes `synchronized_book()`, while `diagnostic_book()` is explicitly
  available in all states under Host single-writer discipline.

### Contracts

The Contracts repository was clean and at the fixed commit. Its guidance, architecture, living
architecture, required ADRs, dependency/integration documents, contract semantic documents,
Pydantic domain models, Protobuf definitions, fixtures, adapter tests, round-trip tests, descriptor
tests, transcript tests, and dependency-boundary tests were read without modifying the repository.

Relevant wire and domain facts are:

| Contract fact | Fixed-baseline evidence |
|---|---|
| Supported venue | `BINANCE` only |
| Supported markets | `SPOT`, `USD_M_PERPETUAL` |
| Inbound schemas | `exchange-depth-snapshot.v1`, `depth-update.v1` |
| Outbound schema | `local-order-book-snapshot.v1` |
| Decimal representation | Strict positive price string; non-negative quantity string; trailing zeroes valid |
| Snapshot order | Bids strictly descending; asks strictly ascending |
| Update levels | Absolute quantity; zero means delete |
| Update interval | `final_update_id >= first_update_id` |
| Futures continuity field | `previous_final_update_id` is proto optional and Pydantic optional |
| Snapshot required semantics | Non-negative `last_update_id`, required generation time, required synchronization boolean |
| Depth limit | Optional positive Pydantic integer; optional proto `int32` |
| Gap descriptor | Diff stream, detection time, optional previous/next sequence, optional reason and recovery state |
| Wire enums | Zero is `*_UNSPECIFIED`; Python adapters reject unspecified and unknown values |
| Contracts status | Relevant market/snapshot contracts remain PROPOSED; gateway contracts remain DRAFT |

The fixed commit is a pinned semantic baseline, not a claim that those Contracts are formally
ACCEPTED or production-ready.

## Current repository state

M0 through M3 are complete. M4 has no header, source, target, dependency, test, fuzzer, generated
file, or implementation branch. Current Core-only configure/install exposes
`BinanceMarketDataProjection::Core` and does not search for Protobuf.

The Contracts artifact audit found:

| Artifact | Present at fixed baseline? | Evidence / consequence |
|---|---:|---|
| `.proto` sources | Yes | Packaged as Python package data under `binance_market_data_contracts/proto` |
| Generated Python protobuf/grpc stubs | Yes | Generated into `src/binance_market_data`; Python-only codegen tool |
| Generated C++ `.pb.h` / `.pb.cc` | No | No tracked files |
| Generated C++ gRPC files | No | No tracked files |
| CMake project/package | No | No `CMakeLists.txt`, package config, or exported CMake target |
| Conan recipe/package | No | No Contracts Conan recipe or lock for a C++ artifact |
| Installable proto target | No | Python wheel packaging only |
| Standalone descriptor set | No | Descriptor tests load Python generated modules; no installable descriptor-set artifact |
| Stable installed C++ include layout | No | No C++ installation contract exists |
| Exact C++ artifact fingerprint | No | Repository/package version exists, but no C++ target exposes the pinned commit identity |

Therefore M4 implementation has one external blocker: a Contracts-owned versioned C++ Protobuf
package/target is unavailable. Design work can proceed; production implementation cannot.

## Goals

M4 will define a deterministic boundary that:

- converts fixed Contracts `ExchangeDepthSnapshot` and `DepthUpdate` messages into owning,
  lifetime-safe M1/M2/M3 inputs;
- validates wire and domain invariants before Core mutation is possible;
- converts a `BookProjection` plus explicit Host context into a Contracts
  `LocalOrderBookSnapshot` candidate;
- keeps Core entirely independent of Protobuf, generated code, gRPC, clocks, and runtime state;
- makes information loss, optional presence, enum mapping, error results, and compatibility
  explicit; and
- supports live and replay Hosts through the same stateless adapter functions.

## Non-goals

M4 does not implement or own:

- Protobuf schemas, Contracts changes, generated-code copies, or C++ artifact publication;
- REST snapshot download, WebSocket ingestion, buffering, reconnect, recovery orchestration, or
  network-event ordering;
- gRPC servers/clients, stream lifecycle, subscription negotiation, consumer queues, backpressure,
  session sequence, connection generation, or Gateway instance identity;
- system-clock reads, logging, telemetry, persistence, history storage, threads, locks, atomics, or
  coroutine scheduling;
- market-state derivations, mark/index/funding/open-interest state, trading, strategy, risk, matching,
  or prediction; or
- any M5 or M6 implementation.

## Terminology

| Term | Meaning |
|---|---|
| Core | The installed `BinanceMarketDataProjection::Core` target containing M1/M2/M3 only |
| Wire message | A C++ class generated from the pinned Contracts `.proto`; never Core storage |
| Adapter | The proposed optional M4 target and its explicit conversion functions |
| Host | Gateway or History/Replay owner of network/runtime/context concerns |
| Expected identity | Host-owned expected symbol and M3 policy used to validate inbound identity |
| Adapted owner | A value owning normalized vectors and able to construct a fresh synchronous M3 view |
| Snapshot context | Host-owned output identity, producer, source, time, gap, and quality inputs |
| Semantic message equality | Equality of all known output fields and ordered repeated values, independent of allocator/address |
| Contracts fingerprint | Exact build-time identity of the Contracts C++ artifact, including the pinned commit |

Gateway `session_sequence`, Binance update IDs, Protobuf field numbers, and any future Projection
revision are distinct domains and are never interchanged.

## Responsibility matrix

| Concern | Contracts | M4 adapter | M1/M2/M3 Core | Host / M6 |
|---|---|---|---|---|
| Proto fields/numbers/presence | Authority | Consumes explicitly | None | Selects message flow |
| Decimal business grammar | Authority | Validates through M1 exact parser | Owns exact units/formatting | Supplies `NumericSpec` |
| Sequence classification | Semantic reference | Does not classify | M3 authority | Orders calls/recovery |
| Snapshot download/buffering | None | None | None | Owns |
| Span lifetime | None | Owns vectors and call views | Consumes synchronously | Keeps owner alive for call |
| Projection mutation | None | None | Sole owner/API | Single ordered writer |
| Identity and schema check | Authority | Validates | Stores policy, not wire identity | Supplies expected identity |
| Output timestamps | Defines meaning | Copies explicit value | No clocks | Captures/supplies |
| Output producer/source/depth | Defines fields | Validates/maps | No storage | Supplies |
| Core-derived quality facts | Defines enum | Derives only approved facts | Exposes deterministic state/book | Supplies runtime facts |
| Networking/gRPC/subscriptions | Defines wire protocol | None | None | M6 owns |

## Dependency graph

```text
Gateway / History Host
        |
        v
BinanceMarketDataProjection::ProtoAdapter  (optional M4 target)
        |                         |
        v                         v
BinanceMarketDataProjection::Core  BinanceMarketDataContracts::Protobuf
                                             |
                                             v
                                      C++ Protobuf runtime
```

Core has no upward or sideways dependency. No adapter header is installed into Core's header
closure. A Core-only consumer remains unaware that the optional component exists.

## Decision summary

Every detailed decision below is Proposed and subject to external review.

| ID | Decision | Rationale | Rejected alternatives | Compatibility impact | Implementation consequence | Required tests | Blocking status |
|---|---|---|---|---|---|---|---|
| D1 | Separate optional `ProtoAdapter` target | Preserves accepted Core/wire boundary | Core linkage; monolithic target | Core ABI unaffected | Component-aware exports/config | Core-only and adapter consumers | Closed |
| D2 | Contracts-owned versioned C++ package | One schema owner and generated-symbol definition | Source checkout, submodule, FetchContent, copies, arbitrary injection | Exact pin and release coordination required | External Contracts prerequisite | Package, fingerprint, and symbol tests | **IMPLEMENTATION BLOCKER** |
| D3 | Strict inbound snapshot conversion | Wire objects can bypass Pydantic validation | Trust generated setters | Rejects malformed or future unsupported values | Validate before owner return | Field/order/decimal tests | Closed |
| D4 | Strict inbound depth conversion without sequencing | Separation between adaptation and M3 policy | Adapter repairs or classifies IDs | Preserves M3 semantics | Produce owner; never call projection | Range/presence/order tests | Closed |
| D5 | Movable owning wrappers with fresh `view()` | Prevents dangling spans | Naked views, stored spans, repeated-field views | Adds adapter value types | Own vectors and delete rvalue view | Move/lifetime tests | Closed |
| D6 | `std::variant<T, AdapterError>` | Matches C++20/repository style | Exceptions, optional plus out error, callbacks | Stable typed error API | Validation returns values | Taxonomy/precedence tests | Closed |
| D7 | Explicit closed enum maps | Enum numbers are wire authority, not Core identity | Casts/numeric alignment | Unknown enum fails closed | One mapping switch per enum | Every value, zero, unknown tests | Closed |
| D8 | Owning Host snapshot context | Core lacks runtime fields and adapter cannot read ambient state | Clocks, globals, string views | Context/API versioned separately | Caller constructs complete context | Lifetime/identity tests | Closed |
| D9 | State-specific Local snapshot eligibility | Prevents fabricated IDs/reliability | Synchronized-only or always-allowed | Diagnostic output is explicit | Branch on all four M3 states | Four-state matrix tests | Closed |
| D10 | Generic Contracts gap reason plus preserved sequence endpoints | Current wire lacks M3-specific reason | Schema copy/change in this PR | Detailed reason is intentionally lost | Explicit five-to-one map | Five-reason mapping tests | Closed; non-blocking loss |
| D11 | Positive `DepthLimit`; absence means unlimited | Mirrors Pydantic and safe `int32` | Zero sentinel/raw signed input | Presence remains exact | Valid-by-construction wrapper | Boundaries/top-N tests | Closed |
| D12 | Fixed `NumericSpec` decimal output | Deterministic new snapshot, no input-spelling promise | Float/source precision heuristics | Canonical fixed scale | Use M1 fixed formatters | Formatting/locale tests | Closed |
| D13 | Merge explicit Host flags with three Core facts | Avoids implicit runtime inference | Infer from logs/history | Known enums only, stable order | Deduplicate by explicit rank | Ownership/order/dedup tests | Closed |
| D14 | Defer `MarketStateSnapshot` | Core lacks approved derived/full state | Partial adapter computation | No output promise in M4 | No MarketState builder | Absence/boundary review | Closed |
| D15 | M6 owns `ConsumerGapNotice` and `StreamStatus` | They are subscription runtime messages | M4 Gateway state machine | M4 only maps snapshot gap descriptor | No subscription API in adapter | Dependency tests | Closed |
| D16 | M6 owns gRPC | M4 is a message adapter | Server/client in adapter | No gRPC dependency in M4 | Depend on message target only | Link/package tests | Closed |
| D17 | Stateless deterministic candidate construction | Live/replay equivalence and strong isolation | Ambient config/partial output | Semantic output stable | Build local candidate then return | Replay/allocation tests | Closed |
| D18 | No adapter synchronization or Arena in baseline | Host already owns single-writer lifetime | Locks/atomics/Arena API | Simple value ownership | Stateless free functions | Thread-boundary/lifetime tests | Closed |
| D19 | Exact build pin plus exact schema strings | Messages carry schema version, not repository identity | Schema string alone/runtime guessing | Package fingerprint required | Configure and defensive runtime checks | Version-positive/negative tests | Part of D2 blocker |

## Target layout

The proposed build target names are:

```text
bmd_projection_core
  alias: BinanceMarketDataProjection::Core

bmd_projection_proto_adapter
  alias: BinanceMarketDataProjection::ProtoAdapter
```

`BMD_PROJECTION_BUILD_PROTO_ADAPTER` defaults to `OFF`. With it off, configure, build, install, and
the Core package config do not call `find_package(Protobuf)` or inspect Contracts.

Adapter public headers may include generated `.pb.h` because the input and output message types are
part of that component's API. Core public headers may not include an adapter header or any generated
header. The adapter links publicly to Core and the single Contracts Protobuf target; that Contracts
target must propagate its Protobuf runtime linkage and compile requirements. This applies to static
and shared builds so consumers do not guess transitive link order.

Installed exports are component-aware:

- `find_package(BinanceMarketDataProjection CONFIG REQUIRED COMPONENTS Core)` loads only Core;
- requesting `ProtoAdapter` resolves Protobuf and Contracts and loads a separate adapter export;
- a missing optional adapter does not break Core package discovery; and
- `check_required_components` rejects a requested component that was not built/installed.

The proto package `binance_market_data.projection.v1` generates C++ classes in
`::binance_market_data::projection::v1`, which is also the current Core namespace. This is not a
current name collision, but public adapter headers must fully qualify generated/Core class names,
must never use blanket `using namespace`, and must add a compatibility test that compiles both
header families together. A future same-name Core class would be a source conflict requiring review.

## Contracts artifact acquisition analysis

| Option | Reproducibility / pinning | Offline/install behavior | Ownership/drift/symbol risk | Decision |
|---|---|---|---|---|
| Projection accesses Contracts source tree | Depends on developer checkout; path-sensitive | Not independently installable | Projection implicitly owns codegen; drift likely | Rejected |
| Git submodule | Commit can pin | Extra checkout/tooling; package consumers still lack target | Cross-repo source coupled into Projection | Rejected |
| `FetchContent` | Can pin network revision | Poor offline behavior; configure downloads | Dependency acquisition and codegen hidden in Projection | Rejected |
| Copy `.proto` files | Reproducible only by manual sync | Self-contained but forked authority | Silent schema/field drift | Forbidden/rejected |
| Commit generated C++ here | Buildable but generator/runtime lock hidden | Bloats and duplicates install | Duplicate symbols and generated drift | Forbidden/rejected |
| Contracts versioned C++ CMake/Conan package | Exact version/commit can pin | Supports cache/offline/install consumers | Contracts owns codegen and one symbol set | **Selected** |
| Host/monorepo injects arbitrary targets | Can work in one integration tree | Projection install not self-contained | Target/version contract can vary by Host | Rejected as acceptance baseline; may prototype only after separate review |

The selected Contracts prerequisite must provide:

- generated C++ messages for the fixed proto packages, with no gRPC target required by M4;
- a stable installed include layout following proto imports;
- `BinanceMarketDataContractsConfig.cmake` and an exported target, proposed as
  `BinanceMarketDataContracts::Protobuf`;
- one documented compatible C++ Protobuf runtime version/range;
- static/shared and PIC behavior;
- exact Contracts commit/package fingerprint available to CMake and C++;
- package tests for downstream compilation, serialization, optional presence, and duplicate-symbol
  avoidance; and
- an offline-consumable versioned package, preferably a Conan package aligned with this repository's
  existing dependency workflow.

This is a build/distribution addition in Contracts, not a schema change.

## Inbound `ExchangeDepthSnapshot` conversion

Proposed flow:

```text
::binance_market_data::market::v1::ExchangeDepthSnapshot
    -> adapt_exchange_depth_snapshot(...)
    -> AdaptedBookBaseline (owns vectors)
    -> AdaptedBookBaseline::view()
    -> BookProjection::install_baseline(...)
```

Validation is ordered and deterministic:

1. Verify the compiled Contracts fingerprint is the pinned baseline.
2. Reject `VENUE_UNSPECIFIED`, unknown venue values, and every venue except Binance.
3. Map only Spot and USD-M perpetual; reject unspecified/unknown/unsupported markets.
4. Require a valid non-empty Contracts symbol and exact equality with `ExpectedIdentity.symbol`.
5. Require market-to-policy mapping to equal `ExpectedIdentity.policy`.
6. Require schema version exactly `exchange-depth-snapshot.v1`.
7. Validate required producer, producer-version, and request-identity strings even though Core does
   not retain them.
8. Accept every `uint64` last update ID, including zero and maximum. The non-optional proto scalar
   cannot distinguish omitted from explicitly encoded zero, and zero is valid by Contracts; M4 must
   not invent a presence rule.
9. Validate every quality enum even though quality metadata does not enter Core.
10. Parse price and quantity strings through the M1 exact parser using the supplied `NumericSpec`.
11. Revalidate normalized bids as strictly descending and asks as strictly ascending. Equal unit
    prices, including differently spelled exact decimals that normalize equal, are invalid.
12. Build local vectors completely, then return the owner. No Core call occurs inside conversion.

Price must be greater than zero. Quantity may be zero. Exact rescaling is allowed when discarded
fractional digits are all zero; scale need not textually match `NumericSpec`. Inexact rescaling is
`ScaleMismatch`, overflow is `NumericOverflow`, and rounding/truncation is forbidden.

Baseline zero quantities retain their Contracts/M2 meaning: the adapted vector may contain them,
and M3 `replace_all` removes or omits those levels. Duplicate prices are rejected by strict snapshot
ordering rather than converted to M2's last-write-wins fallback. Locked/crossed books are accepted;
ordering is per side, and M2 intentionally preserves them.

Exchange/receive times, producer metadata, request identity, and inbound quality flags are validated
but do not enter Core. The Host retains any runtime metadata it needs independently. Conversion
failure returns a typed error and leaves every `BookProjection` untouched.

## Inbound `DepthUpdate` conversion

Proposed flow:

```text
::binance_market_data::market::v1::DepthUpdate
    -> adapt_depth_update(...)
    -> AdaptedDepthBatch (owns one ordered vector)
    -> AdaptedDepthBatch::view()
    -> BookProjection::apply(...)
```

The adapter:

1. requires the `metadata` message and validates Binance venue, supported market, valid symbol,
   expected identity, producer/version/connection identity, and exact `depth-update.v1` schema;
2. rejects unspecified/unknown stream values and requires `DIFF_DEPTH`;
3. maps market to the same immutable M3 policy as the expected projection;
4. creates `UpdateId` values and calls `UpdateRange::try_create`; `first > final` returns
   `InvalidUpdateRange` before M3 is callable;
5. preserves `previous_final_update_id` presence exactly with `has_previous_final_update_id()`;
6. validates all metadata quality enums but does not store them in Core;
7. parses every level exactly using `NumericSpec`, rejecting non-positive price, negative quantity,
   malformed decimal, inexact scale, and overflow;
8. appends all bid updates in their wire order followed by all ask updates in their wire order; and
9. returns an owner only after all validation and allocation succeeds.

Contracts defines no cross-side ordering between the separate repeated arrays. Bid-then-ask is the
canonical merge order. It preserves same-side input order, so duplicate prices on one side keep M2's
last-write-wins semantics. Cross-side order cannot affect independent side maps. Update arrays are
not required to be price-sorted. Quantity zero maps to a deletion; negative quantity text is rejected.

The adapter does not classify stale, duplicate, bridge, live, or gap input; does not repair IDs,
fill ranges, synthesize `pu`, or reorder same-side values; and does not call `BookProjection`.
Malformed payloads cannot enter M3. The Host decides the fail-closed recovery action after a typed
adaptation failure.

## Owning adapter objects and span lifetime

`AdaptedBookBaseline` owns:

- one `UpdateId`;
- `std::vector<BookLevel> bids`; and
- `std::vector<BookLevel> asks`.

`AdaptedDepthBatch` owns:

- one valid `UpdateRange`;
- optional `UpdateId previous_final`; and
- `std::vector<LevelUpdate> levels`.

Both are non-copyable to avoid accidental full-depth copies and are move-constructible/move-
assignable with the default allocator's `noexcept` vector move. A moved-from owner is destruction-
or assignment-only. `view() const & noexcept` constructs a new `BookBaseline` or `DepthBatch` from
current vector storage on each call; `view() const &&` is deleted. Neither class stores a span.

Host call discipline is:

```cpp
auto result = adapt_depth_update(wire, spec, expected);
if (auto* adapted = std::get_if<AdaptedDepthBatch>(&result)) {
    const auto apply_result = projection.apply(adapted->view());
}
```

The owner remains alive and unmoved through the synchronous call. The view is not stored or returned
by the Host. Destroying the Protobuf message before the Core call is safe.

| Alternative | Decision |
|---|---|
| Return `BookBaseline`/`DepthBatch` directly | Rejected: would reference temporary adapter vectors |
| Store spans as owner members | Rejected: self-references break under move and vector relocation |
| Reference Protobuf repeated fields | Rejected: wrong element type, lifetime leak, and no exact numeric conversion storage |
| Copy into M3 persistent event storage | Rejected: M3 intentionally owns no network buffer/history |
| Owning vectors plus fresh call view | Selected |

Views obtained before an owner move are invalid, as are views retained beyond owner lifetime. The
ref-qualified API and tests make the supported synchronous usage explicit.

## Adapter error model

M4 selects `std::variant<T, AdapterError>` through a named `AdapterResult<T>` alias. It matches the
repository's M1 result style without inventing exception-based business control flow.

| Candidate | Evaluation |
|---|---|
| `std::variant<Value, AdapterError>` | Selected: standard C++20, owning, explicit, testable |
| Custom `Result<T>` class | Rejected initially: duplicates variant visitation/access semantics and adds ABI surface |
| `optional` plus external error/output parameter | Rejected: permits forgotten/partially initialized errors |
| Exceptions for malformed wire | Rejected: validation failures are expected boundary outcomes |
| `std::error_code` | Rejected: loses field/decimal detail or requires global category machinery |

The proposed `AdapterErrorCode` contains at least:

```text
UnsupportedVenue
UnsupportedMarket
UnexpectedStream
IdentityMismatch
UnsupportedSchemaVersion
UnspecifiedEnum
UnknownEnumValue
InvalidUpdateRange
MissingRequiredField
InvalidIdentifier
InvalidDecimal
NegativeQuantity
NonPositivePrice
ScaleMismatch
NumericOverflow
InvalidDepthLimit
InvalidOrdering
UnsupportedProjectionState
MissingLastUpdateId
InvalidGapContext
ContractsVersionMismatch
```

`AdapterError` owns a code, an `AdapterField` enum, optional `DecimalErrorCode`/byte offset, and an
optional raw enum value. It contains no borrowed string, log message, Protobuf reference, timestamp,
or dynamic context required for machine classification. `to_string` may provide stable diagnostics,
but text is never the business result.

Error precedence is:

1. Contracts fingerprint and required message presence;
2. venue/market/stream enum and expected identity;
3. exact schema version and required identifiers;
4. structural range/depth/ordering constraints;
5. level decimal grammar/domain, exact scale, then numeric overflow in M1's established precedence;
6. projection-state/output-context eligibility.

Negative quantity and non-positive price are detected as domain-specific codes rather than exposing
only a generic sign/zero decimal code. M1 decimal code and offset are retained as detail.
`std::bad_alloc` is not converted and may propagate. Other ordinary validation never throws.

## Enum mapping

M4 defines adapter-owned enums where Host input is required and explicit mapping functions for all
wire values. No `static_cast` assumes numeric alignment.

| Contracts enum | M4 behavior / destination |
|---|---|
| `Venue` | `VENUE_BINANCE` accepted and output fixed; unspecified/unknown/other rejected |
| `Market` | Spot -> `SequencePolicyKind::Spot`; USD-M -> `UsdMPerpetual`; unspecified/unknown rejected |
| `Stream` | Inbound update and gap output require `DIFF_DEPTH`; unspecified/unknown rejected |
| `SnapshotSource` | Explicit map from adapter `SnapshotOrigin::{GatewayLive,RecorderReplay,HistoryReplay}` |
| `QualityFlag` | Explicit two-way map to closed adapter `QualityFact`; zero/unknown rejected |
| `ReasonCode` | Every current M3 gap reason maps explicitly to `SEQUENCE_GAP_DETECTED` |
| `ResyncState` | Explicit map from Host `GapRecoveryState`; zero is never emitted for current gap |

Unknown fields are compatible and ignored by inbound semantic conversion. Unknown enum values are
not safe defaults: M4 returns `UnknownEnumValue` until a reviewed mapping exists. Optional enum
presence is handled with `has_*`; absent optional reason/recovery remains absent unless M4 output
rules require an explicit value.

Core stores only `SequencePolicyKind`, `GapReason`, and other Core values. It never stores a wire
enum or enum number.

## Identity and snapshot context model

`ExpectedIdentity` owns:

- `std::string symbol`; and
- `SequencePolicyKind policy`.

Venue is implicitly Binance. Inbound symbol must satisfy Contracts syntax and equal the expected
owned string. Market must map to the expected policy.

`SnapshotContext` is a Host-created owning value containing:

- `ExpectedIdentity identity`;
- owned `producer` and `producer_version` strings;
- `SnapshotOrigin source`;
- explicit `generated_time_utc_ns`;
- optional `generated_monotonic_ns`;
- optional `CurrentGapContext` containing detection time and recovery state; and
- no caller-provided schema version or venue.

`SnapshotOptions` owns:

- optional valid `DepthLimit`; and
- a vector of explicit Host `QualityFact` values.

All strings are owned because the context may be passed through value-returning build paths and
must not borrow request buffers. The context remains alive for the synchronous builder call, but the
returned Protobuf message owns its strings. The adapter fixes
`local-order-book-snapshot.v1`, fixes venue to Binance, derives market from policy, and rejects a
context policy different from `projection.policy()`.

The adapter does not accept arbitrary schema/venue fields and therefore cannot produce a snapshot
that claims another schema or exchange.

## Time ownership

The Host captures and supplies every time value. M4 never calls a system or monotonic clock.

- `generated_time_utc_ns` is required and copied exactly, including zero because Contracts permits
  non-negative time.
- `generated_monotonic_ns` preserves optional presence exactly.
- `detected_at_utc_ns` is required from `CurrentGapContext` when emitting a current gap.
- Exchange and receive times on inbound messages are validated structurally by their unsigned wire
  representation/presence but do not enter Core.

Equal Core state plus equal explicit context therefore cannot vary with execution time.

## `LocalOrderBookSnapshot` eligibility matrix

| Projection state | Eligible? | Book source | `synchronized` | ID | `last_gap` / required flags | Failure |
|---|---:|---|---:|---|---|---|
| `AwaitingBaseline` | No | None | N/A | Absent | None | `MissingLastUpdateId` (no fabricated zero) |
| `AwaitingBridge` | Yes, diagnostic only | `diagnostic_book()` | `false` | Installed baseline ID | No gap; add `SNAPSHOT_BRIDGE_PENDING` | Context mismatch still errors |
| `Synchronized` | Yes, reliable | `synchronized_book()` | `true` | Accepted ID | No historical gap emitted | Missing synchronized view is invariant error |
| `NeedsResync` | Yes, diagnostic only | `diagnostic_book()` | `false` | Last accepted ID | Emit current mapped gap; add `SEQUENCE_GAP`; require `CurrentGapContext` | Missing Core/context gap -> typed error |

`synchronized=true` is derived only from M3 `Synchronized` and cannot be overridden by a boolean
option. Awaiting states and NeedsResync cannot use the reliable view. A snapshot builder never uses
`UpdateId{0}` as a sentinel.

`UnsupportedProjectionState` covers an inconsistent or future unrecognized state. `MissingLastUpdateId`
is the precise result for `AwaitingBaseline`; `MissingRequiredField` identifies absent required Host
gap context in `NeedsResync`.

## Gap mapping

M3 current gap fields map as follows:

| `GapDescriptor` field | Source |
|---|---|
| `stream` | Fixed `DIFF_DEPTH` |
| `detected_at_utc_ns` | Host `CurrentGapContext` |
| `previous_sequence` | `GapInfo.last_accepted_final` |
| `next_sequence` | `GapInfo.incoming_range.first()` |
| `reason_code` | Explicit map to `SEQUENCE_GAP_DETECTED` |
| `recovery_state` | Explicit Host `GapRecoveryState` |

For a snapshot emitted from `NeedsResync`, the Host recovery value may map only to
`RESYNC_REQUIRED`, `RESYNC_IN_PROGRESS`, or `RESYNC_FAILED`. `SYNCHRONIZED`, `RECOVERED`, an
unspecified value, or an unknown value would contradict the current M3 state and returns
`InvalidGapContext`.

Every current `GapReason` maps to the generic reason:

| M3 reason | Contracts reason |
|---|---|
| `SpotBootstrapForwardGap` | `SEQUENCE_GAP_DETECTED` |
| `SpotLiveForwardGap` | `SEQUENCE_GAP_DETECTED` |
| `FuturesBootstrapRangeMiss` | `SEQUENCE_GAP_DETECTED` |
| `FuturesMissingPreviousFinal` | `SEQUENCE_GAP_DETECTED` |
| `FuturesPreviousFinalMismatch` | `SEQUENCE_GAP_DETECTED` |

`next_sequence` is the incoming range's first ID because it is the first wire sequence represented
by the discontinuous candidate. Mapping the final ID would conceal the candidate's starting edge.
The current Contracts message cannot retain incoming final ID, incoming `pu`, policy, or the exact
M3 reason. That information remains available through Core diagnostics but is lost on this wire
snapshot.

This loss is non-blocking: `GapDescriptor` is documented as a general gap description and the
generic reason plus previous/next evidence truthfully represents every M3 gap. If cross-module
consumers later require the precise M3 classification, Contracts may add an optional, versioned
field through a separate additive schema review; M4 does not require or propose that change now.

## Historical `last_gap` policy

M3 retains `last_gap` when a new baseline is installed after a gap, so it is historical evidence in
`AwaitingBridge` and can remain historical after synchronization. M4 interprets the wire
`LocalOrderBookSnapshot.last_gap` as the **current reason the emitted diagnostic book is unreliable**:

- emit it only in `NeedsResync`;
- omit it in `AwaitingBaseline`, `AwaitingBridge`, and `Synchronized`;
- never let the Host opt a historical gap into an otherwise current/reliable snapshot; and
- leave historical incident reporting to Host telemetry/history or a future explicit contract.

This prevents a recovered synchronized snapshot from appearing currently gapped and prevents an old
gap from being misidentified as the reason an unrelated new baseline is merely awaiting a bridge.

## Depth-limit semantics

`DepthLimit` is a valid-by-construction adapter type holding a positive `std::int32_t`.
`DepthLimit::create(std::int64_t)` returns `AdapterResult<DepthLimit>`.

| Input | Result |
|---|---|
| Absent | Unlimited; use `all_levels` on both sides; omit proto field |
| `1..INT32_MAX` | Valid; use `top_levels` independently for bids and asks; set presence |
| `0` | `InvalidDepthLimit` |
| Negative | `InvalidDepthLimit` |
| Greater than `INT32_MAX` | `InvalidDepthLimit`; no narrowing conversion |

Each side is limited separately to at most N. Empty sides remain empty. Order comes directly from
M2, so bids are descending, asks ascending, and duplicates/zero quantities cannot appear. Locked or
crossed books are output unchanged and add the deterministic `CROSSED_BOOK` fact.

Absence is the only unlimited representation. Zero is never an unlimited sentinel.

## Decimal formatting

Snapshot output calls `format_price_fixed(value, spec.price_scale)` and
`format_quantity_fixed(value, spec.quantity_scale)`.

- Every value uses exactly the configured storage-scale fractional digits.
- Trailing zeroes for that scale are emitted deterministically.
- The adapter does not promise to reconstruct an inbound message's original fractional-digit count;
  a local snapshot is a new derived contract.
- Price is structurally positive; book quantity is structurally positive because M2 stores no zero
  levels. Negative quantity is impossible in Core.
- No float, locale, scientific notation, implicit rescale, or rounding is used.
- Live and replay with equal `NumericSpec` and state produce equal strings.
- Allocation failure may propagate; an unexpected formatter error becomes a typed numeric/scale
  adapter error before a message is returned.

## Quality-flag ownership

M4 output flags are the deduplicated union of explicit Host facts and facts directly observable in
the current Core snapshot. A fixed adapter rank table defines output order; it does not sort by an
assumption that adapter and Protobuf enum numbers align.

| Quality flag | Owner / M4 rule |
|---|---|
| `CROSSED_BOOK` | M4 derives when both best levels exist and `best_bid >= best_ask` (locked included by Contracts semantics) |
| `SEQUENCE_GAP` | M4 derives only in `NeedsResync` |
| `SNAPSHOT_BRIDGE_PENDING` | M4 derives only in `AwaitingBridge` |
| `ORDERBOOK_RESYNC` | Host supplies when recovery activity is observed; `NeedsResync` alone is not proof that recovery started |
| `DUPLICATE` | Host/normalization supplies; M3 does not retain prior apply dispositions |
| `OVERLAP` | Host supplies from an observed input/result; not reconstructible from current state |
| `OUT_OF_ORDER` | Host/normalization supplies |
| `SNAPSHOT_TOO_OLD` | Host supplies |
| `BOOTSTRAP_BUFFER_OVERFLOW` | Host supplies; M4 owns no buffer |
| `EXCHANGE_TIME_MISSING` | Host supplies; Core has no exchange time |
| `SLOW_CONSUMER_GAP` | M6 Gateway supplies; subscription-runtime fact |
| `PRODUCER_RESTART` | Host supplies |
| `RECOVERED_TAIL` | Recorder/History Host supplies |
| `MALFORMED_PAYLOAD` | Host supplies on a later diagnostic artifact; the failing inbound conversion itself produces no snapshot |
| `RECEIVE_CLOCK_DISCONTINUITY` | Host supplies |
| `IDENTITY_CONFLICT` | Normalization/Host supplies; M3 does not hash or retain event history |

M4 never infers flags from logs, time comparisons, network state, or event history. Unspecified or
unknown Host flag values cannot enter `SnapshotOptions` because `QualityFact` is closed; inbound wire
quality flags are explicitly mapped and rejected if unsupported.

The canonical output rank is explicitly:

```text
DUPLICATE
OUT_OF_ORDER
SEQUENCE_GAP
ORDERBOOK_RESYNC
SNAPSHOT_BRIDGE_PENDING
SNAPSHOT_TOO_OLD
BOOTSTRAP_BUFFER_OVERFLOW
RECOVERED_TAIL
MALFORMED_PAYLOAD
EXCHANGE_TIME_MISSING
RECEIVE_CLOCK_DISCONTINUITY
SLOW_CONSUMER_GAP
PRODUCER_RESTART
OVERLAP
IDENTITY_CONFLICT
CROSSED_BOOK
```

This list is a semantic table, not a cast or sort by Protobuf numeric value. Future flags require an
explicit reviewed insertion point.

## `MarketStateSnapshot` scope decision

M4 generates `LocalOrderBookSnapshot` only. `MarketStateSnapshot` is deferred.

M3 provides book and sequence state, and M2 exposes best levels, but M4's rule is “wire adapter
maps; Core computes; Host supplies runtime context.” Computing mid, spread, or microprice in the
adapter would add unapproved numeric/rounding semantics. Mark price, index price, funding, open
interest, trade state, freshness, and trade IDs are not present in M3. Emitting a sparse partial
message would create an unstable semantic promise even though fields are optional.

Contracts ADR-0006 remains the long-term direction for a strategy-independent market-state
projection. A separately reviewed future Projection milestone must add the necessary deterministic
Core facts and define partial-state semantics before a wire adapter is added. M4 makes no M5/M6
assignment and no derived calculation.

## `ConsumerGapNotice` and `StreamStatus` ownership

Both messages describe Gateway consumer-subscription runtime. They require subscription identity,
delivery sequence/recovery behavior, observed time, and lifecycle state that M3 does not own.

- M6 Gateway Host owns their creation and sequencing.
- M4 does not accept subscription ID, session sequence, connection generation, or Gateway instance
  identity.
- M4 provides only the reusable Core-gap-to-`GapDescriptor` mapping used inside a local snapshot.
- M4 does not create a Gateway runtime state machine or a consumer gap notice.

## gRPC boundary

```text
M4 = Protobuf message adapter
M6 = Gateway/gRPC runtime
```

M4 has no gRPC generated service dependency, server/client, stream lifecycle, queues, backpressure,
subscription negotiation, retry, or connection management. The selected Contracts C++ artifact may
publish a separate gRPC target, but `ProtoAdapter` must depend only on its message target.

## Determinism

Equal Core state, `NumericSpec`, expected identity, context, options, and pinned Contracts artifact
produce semantically byte-equivalent known message content:

- all scalar and optional fields match;
- bid/ask repeated fields use M2's stable order;
- quality flags use the explicit canonical rank and are deduplicated;
- no Protobuf map is used;
- no unknown inbound field is copied into a newly built output message; and
- standard deterministic serialization of the resulting map-free message is byte-stable when the
  same supported Protobuf version/serialization mode is used.

The adapter reads no clock, random state, address, process ID, environment variable, log, network,
filesystem, or mutable global configuration. It has no plugin callbacks.

## Exception safety

| Operation | Guarantee |
|---|---|
| Inbound validation failure | Returns `AdapterError`; no owner and no Core mutation |
| Inbound allocation failure | `std::bad_alloc` propagates; local vectors die; no Core mutation |
| Owner `view()` | Non-allocating, `noexcept`, valid for synchronous call lifetime |
| M3 install/apply after adaptation | Existing M3 strong guarantees; adapter no longer participates |
| Output validation failure | Returns `AdapterError`; projection unchanged |
| Output allocation/Protobuf construction failure | Exception propagates; local candidate destroyed; projection unchanged |
| Successful output | Fully constructed message returned by value/move; no partial caller-visible message |

Builders never mutate a caller-supplied output parameter. They create a local candidate message and
return it only after all fields, levels, flags, and optional gap content succeed.

## Thread model and ownership

The adapter is stateless free-function code with no mutable globals. Independent calls using
independent messages/owners/projections may run concurrently. The Host still guarantees one writer
and no concurrent read/write for each `BookProjection`. During snapshot construction the Host must
not mutate or move that projection.

M4 adds no mutex, atomic, thread, scheduler, or coroutine. The baseline does not use Protobuf Arena:
return-by-value ownership is clearer, avoids exporting an Arena lifetime, and is sufficient without
benchmark evidence. A future internal Arena optimization must preserve returned-message ownership
and cannot change Core ownership or public lifetime rules.

## Packaging and install model

The installed Projection package is componentized:

```text
Core component
  - Core headers/library/export
  - no Protobuf/Contracts discovery

ProtoAdapter component (only when built)
  - adapter headers/library/export
  - requires exact Contracts C++ package
  - requires the Contracts target's supported Protobuf runtime
```

Core and adapter have distinct exported target files. The top-level config includes the adapter
export and calls its dependencies only when the consumer requests `ProtoAdapter`. Static consumers
receive transitive generated-message/runtime libraries through target link interfaces; shared
consumers receive required headers and any public runtime dependency. No consumer compiles a second
copy of generated Contracts sources.

## Compatibility and versioning

- Contracts commit pin: `01d76a41929f36d89573159f5f458f9f1e378ada`.
- Required schema strings: `exchange-depth-snapshot.v1`, `depth-update.v1`, and
  `local-order-book-snapshot.v1`.
- Proto package, field numbers, enum numbers, and optional presence come only from the Contracts
  target.
- Configure fails if the exported Contracts fingerprint differs from the pin. A defensive runtime
  fingerprint check returns `ContractsVersionMismatch` if an inconsistent artifact bypasses CMake.
- Unknown fields are accepted/ignored for forward-compatible parsing and are not propagated to the
  new output message.
- Unspecified and unknown enum values that M4 interprets are rejected.
- Optional `pu`, monotonic time, depth limit, and gap fields use explicit presence APIs.
- Adding an unused optional field can remain compatible; using a new field requires explicit mapping
  and tests. A new enum value fails closed until reviewed.
- Field removal/renaming, package/type rename, number reuse/renumber, required/presence change,
  decimal/unit/ordering/gap semantic change, or incompatible enum behavior is breaking and requires
  a new Contracts major version or coordinated adapter change.
- Core binary/source compatibility is independent. ProtoAdapter ABI follows the Projection library
  version and the generated Contracts/Protobuf ABI; cross-version binary mixing is unsupported
  unless the packages explicitly guarantee it.

A schema string alone cannot identify the repository commit. Build-time pin plus artifact
fingerprint is required and sufficient for M4 because all messages in one binary use the same
linked generated target. No new runtime wire field is required.

## Proposed public API sketch

**Proposed, not implemented, and subject to external review:**

```cpp
namespace binance_market_data::projection_adapter::v1 {

enum class AdapterErrorCode : std::uint8_t {
    UnsupportedVenue,
    UnsupportedMarket,
    UnexpectedStream,
    IdentityMismatch,
    UnsupportedSchemaVersion,
    UnspecifiedEnum,
    UnknownEnumValue,
    InvalidUpdateRange,
    MissingRequiredField,
    InvalidIdentifier,
    InvalidDecimal,
    NegativeQuantity,
    NonPositivePrice,
    ScaleMismatch,
    NumericOverflow,
    InvalidDepthLimit,
    InvalidOrdering,
    UnsupportedProjectionState,
    MissingLastUpdateId,
    InvalidGapContext,
    ContractsVersionMismatch,
};

enum class AdapterField : std::uint8_t;

namespace detail {
struct AdapterFactory;
}

struct AdapterError final {
    AdapterErrorCode code;
    AdapterField field;
    std::optional<projection::v1::DecimalError> decimal_error;
    std::optional<std::int32_t> raw_enum_value;
    friend constexpr bool operator==(const AdapterError&, const AdapterError&) noexcept = default;
};

template <typename T>
using AdapterResult = std::variant<T, AdapterError>;

struct ExpectedIdentity final {
    std::string symbol;
    projection::v1::SequencePolicyKind policy;
};

class AdaptedBookBaseline final {
  public:
    AdaptedBookBaseline(AdaptedBookBaseline&&) noexcept;
    AdaptedBookBaseline& operator=(AdaptedBookBaseline&&) noexcept;
    AdaptedBookBaseline(const AdaptedBookBaseline&) = delete;
    AdaptedBookBaseline& operator=(const AdaptedBookBaseline&) = delete;
    [[nodiscard]] projection::v1::BookBaseline view() const & noexcept;
    projection::v1::BookBaseline view() const && = delete;
  private:
    friend struct detail::AdapterFactory;
    AdaptedBookBaseline(
        projection::v1::UpdateId,
        std::vector<projection::v1::BookLevel>,
        std::vector<projection::v1::BookLevel>) noexcept;
    projection::v1::UpdateId last_update_id_;
    std::vector<projection::v1::BookLevel> bids_;
    std::vector<projection::v1::BookLevel> asks_;
};

class AdaptedDepthBatch final {
  public:
    AdaptedDepthBatch(AdaptedDepthBatch&&) noexcept;
    AdaptedDepthBatch& operator=(AdaptedDepthBatch&&) noexcept;
    AdaptedDepthBatch(const AdaptedDepthBatch&) = delete;
    AdaptedDepthBatch& operator=(const AdaptedDepthBatch&) = delete;
    [[nodiscard]] projection::v1::DepthBatch view() const & noexcept;
    projection::v1::DepthBatch view() const && = delete;
  private:
    friend struct detail::AdapterFactory;
    AdaptedDepthBatch(
        projection::v1::UpdateRange,
        std::optional<projection::v1::UpdateId>,
        std::vector<projection::v1::LevelUpdate>) noexcept;
    projection::v1::UpdateRange range_;
    std::optional<projection::v1::UpdateId> previous_final_;
    std::vector<projection::v1::LevelUpdate> levels_;
};

enum class SnapshotOrigin : std::uint8_t {
    GatewayLive,
    RecorderReplay,
    HistoryReplay,
};

enum class QualityFact : std::uint8_t;
enum class GapRecoveryState : std::uint8_t;

class DepthLimit final {
  public:
    [[nodiscard]] static AdapterResult<DepthLimit> create(std::int64_t value) noexcept;
    [[nodiscard]] std::int32_t value() const noexcept;
};

struct CurrentGapContext final {
    std::uint64_t detected_at_utc_ns;
    GapRecoveryState recovery_state;
};

struct SnapshotContext final {
    ExpectedIdentity identity;
    std::string producer;
    std::string producer_version;
    SnapshotOrigin source;
    std::uint64_t generated_time_utc_ns;
    std::optional<std::uint64_t> generated_monotonic_ns;
    std::optional<CurrentGapContext> current_gap;
};

struct SnapshotOptions final {
    std::optional<DepthLimit> depth_limit;
    std::vector<QualityFact> additional_quality_flags;
};

[[nodiscard]] AdapterResult<AdaptedBookBaseline>
adapt_exchange_depth_snapshot(
    const ::binance_market_data::market::v1::ExchangeDepthSnapshot& wire,
    projection::v1::NumericSpec numeric_spec,
    const ExpectedIdentity& expected);

[[nodiscard]] AdapterResult<AdaptedDepthBatch>
adapt_depth_update(
    const ::binance_market_data::market::v1::DepthUpdate& wire,
    projection::v1::NumericSpec numeric_spec,
    const ExpectedIdentity& expected);

[[nodiscard]] AdapterResult<::binance_market_data::projection::v1::LocalOrderBookSnapshot>
make_local_order_book_snapshot(
    const projection::v1::BookProjection& projection,
    const SnapshotContext& context,
    const SnapshotOptions& options);

} // namespace binance_market_data::projection_adapter::v1
```

The generated snapshot and Core classes currently occupy the same proto-derived
`::binance_market_data::projection::v1` namespace but have distinct names. The adapter API lives in
`projection_adapter::v1` to avoid implying that generated wire storage is part of Core. The real
header must include every standard/generated declaration it uses and receive self-containment tests.

`AdapterField` and `QualityFact` will list closed values rather than accepting arbitrary integers or
strings. Value types use owned strings. There are no output parameters, boolean behavior modes,
callbacks, virtual plugins, or mutable global policy.

## Proposed CMake design sketch

**Proposed, not implemented, and subject to external review:**

```cmake
option(BMD_PROJECTION_BUILD_PROTO_ADAPTER
       "Build the optional Contracts Protobuf adapter" OFF)

if(BMD_PROJECTION_BUILD_PROTO_ADAPTER)
  find_package(Protobuf CONFIG REQUIRED)
  find_package(BinanceMarketDataContracts CONFIG REQUIRED COMPONENTS Protobuf)

  # Fail configure unless the package exports the exact approved Contracts fingerprint.
  # add_library(bmd_projection_proto_adapter ...)
  # target_link_libraries(bmd_projection_proto_adapter
  #   PUBLIC
  #     BinanceMarketDataProjection::Core
  #     BinanceMarketDataContracts::Protobuf)
  # install/export as the separate ProtoAdapter component.
endif()
```

The exact Contracts target, package-version check, and whether direct `protobuf::libprotobuf`
linkage is necessary depend on prerequisite C-M4-001 and must be verified rather than guessed.

## Test strategy

### Header and dependency-boundary tests

- Every Core header compiles in isolation without Protobuf include paths.
- A Core-only installed consumer configures and links with no Protobuf or Contracts package.
- Adapter headers compile with only installed Projection/Contracts dependencies.
- Adapter installed consumer links `BinanceMarketDataProjection::ProtoAdapter` in static and shared
  configurations.
- Including Core plus every generated header detects namespace/type collisions.
- Core exported targets contain no adapter/Protobuf link interface.

### Inbound conversion matrix

| Area | Cases |
|---|---|
| Valid input | Spot/USD-M DepthUpdate; Spot/USD-M ExchangeDepthSnapshot; empty sides/updates |
| Enum | enum zero; unknown value; unsupported market/venue; exact explicit mapping |
| Identity | empty/invalid symbol; mismatch; market/policy mismatch; missing metadata/identifiers |
| Schema/stream | exact versions; wrong/empty version; non-DIFF stream |
| Sequence | zero/max IDs; first=final; first>final; optional `pu` absent/present/zero/max |
| Decimal | malformed syntax, negative quantity, zero quantity, non-positive price, exact rescale, inexact scale, overflow |
| Levels | update input order, repeated same-side price, bid-then-ask merge, strict snapshot order, normalized duplicate price |
| Book shape | empty side, locked/crossed baseline, zero baseline quantity |
| Failure | result code/field/precedence; no projection mutation |

### Lifetime tests

- Owner move construction and move assignment preserve the destination view.
- Existing pre-move views are never used; moved-from owner is destruction/assignment-only.
- `view()` is unavailable on an rvalue at compile time.
- Protobuf input destruction before the synchronous Core call is safe.
- Owner vectors outlive the call and no class contains a cached span.
- Sanitizers exercise use-after-free-sensitive scenarios.

### Snapshot output matrix

- Every `ProjectionStatus` row, including no fabricated ID in `AwaitingBaseline`.
- AwaitingBridge diagnostic snapshot, false synchronization, and bridge-pending flag.
- Synchronized reliable view and no historical gap.
- NeedsResync diagnostic view, current gap, required Host gap context, and sequence-gap flag.
- Rebaseline/resynchronize proves historical `last_gap` is omitted.
- Depth limit absent, 1, `INT32_MAX`, zero, negative, and above max.
- Empty side/book, locked/crossed book, deterministic orders, no output zero/duplicates.
- Explicit timestamp/monotonic presence and exact identity/producer/source mapping.
- All five M3 gap reasons, next=`incoming.first`, generic reason, recovery state, and known
  information loss.
- Host/Core quality merge, canonical ordering, and deduplication.
- Fixed-scale decimal output, trailing zeroes, no float/locale/scientific notation.

### Semantic round trip

Round trip means:

```text
fixed Contracts wire fixture
  -> M4 owning domain value
  -> M3 projection call(s)
  -> LocalOrderBookSnapshot wire output
  -> semantic comparison with expected fixed-baseline fields
```

It does not promise byte reproduction of the inbound message, source decimal spelling, unknown
fields, request metadata, or quality flag history.

### Allocation-failure plan

A dedicated executable uses the repository's deterministic global allocation-failure sweep pattern
without production failpoint APIs. It exercises decimal/string/vector construction, owner return,
Core ordered-vector queries, output strings, repeated-field insertion, gap construction, and quality
flags. At every fired failure point:

- no partially adapted owner/message is returned;
- the Protobuf input remains caller-owned;
- projection status, ID, gap, and complete diagnostic book equal a pre-call checkpoint; and
- a final non-failing attempt proves the scenario actually succeeds.

### State and determinism tests

Replay identical fixtures twice with identical explicit context and compare every adapter result,
M3 result, output field/presence, ordered repeated element, and deterministic serialization bytes
under the pinned runtime. Change one context input at a time to show no hidden source exists.

## Property and fuzz strategy

An independent reference converter uses primitive integers, its own decimal grammar/reference exact
rescale, vectors, and explicit enum tables. It must not call production adapter helpers or M3
sequence classifiers.

Property generators cover:

- valid and invalid decimal strings across scale 0..18;
- zero/max update IDs and arbitrary range pairs;
- enum zero, every known value, and unknown `int32` values;
- optional `pu`, monotonic/depth/gap presence;
- arbitrary bid/ask arrays, duplicate prices, ordering, empty updates, and crossed books;
- all projection states, depth limits, Host quality permutations, and gap reasons; and
- repeated replay for semantic equality.

A dedicated adapter libFuzzer directly constructs/mutates Protobuf message objects; network parsing
or serialized bytes are not required. Operations include adapt snapshot, adapt update, install/apply,
build snapshot, move owner, query view, and reset projection. The harness compares the production
result with the independent reference after each operation and asserts Core is unchanged on every
adapt/output failure.

Seed corpus categories include both valid fixtures, enum zero/unknown, empty required strings,
optional presence toggles, max IDs, invalid range, invalid/exact-rescale decimals, duplicate and
misordered levels, all four states, current/historical gaps, depth boundaries, crossed/empty books,
and quality duplicates.

## Downstream consumer strategy

Two isolated staged-install consumers are required:

1. Core-only consumer: no Protobuf package in its discovery path; includes M1/M2/M3 and links only
   `BinanceMarketDataProjection::Core`.
2. Adapter consumer: finds exact Contracts/Protobuf packages, includes generated input/output types
   plus the adapter header, adapts one fixture-equivalent message, applies it, and builds a snapshot.

Both test build-tree-independent install paths. Static and shared package configurations must prove
that generated symbols occur once and all public dependencies propagate correctly.

## Open decisions

All M4 semantic decisions in this proposal have recommended answers. One external artifact decision
remains an implementation blocker:

| ID | Question | Recommended answer | Alternative | Impact | Blocks implementation? | Evidence needed to close |
|---|---|---|---|---|---:|---|
| OD-M4-001 | What exact installable C++ target provides Contracts messages? | Contracts-owned versioned CMake/Conan package exporting one Protobuf message target and exact commit fingerprint | Arbitrary Host injection | Determines dependency, package config, include layout, runtime version, and symbol ownership | **YES — IMPLEMENTATION BLOCKER** | Merged Contracts prerequisite plus successful clean install-consumer audit |

## Implementation blockers

Closed potential blockers:

| Candidate | Decision | Blocking? |
|---|---|---:|
| B1 Contracts C++ package | Absent; C-M4-001 required | **Yes** |
| B2 Gap information loss | Generic `SEQUENCE_GAP_DETECTED` plus endpoints is truthful; exact reason stays Core-only | No |
| B3 Historical `last_gap` | Emit only for current `NeedsResync` | No |
| B4 MarketState owner/data | Deferred to separate future Core design | No |
| B5 Exact Contracts identity | Build pin + package fingerprint is sufficient; delivered by C-M4-001 | No separate blocker |

Implementation blocker count: **1**.

## Contracts prerequisites

This design changes no Contracts file. The required separate work is:

| Prerequisite ID | Repository | Required change | Why required | Compatibility classification | Blocks M4 implementation? | Suggested branch/PR |
|---|---|---|---|---|---:|---|
| C-M4-001 | `tomohikoAmada/BinanceMarketDataContracts` | Publish versioned installable C++ Protobuf message target/package, stable include layout, Protobuf dependency metadata, and exact commit fingerprint for `01d76a4...` | Reproducible/offline Projection build, one schema owner, no copied/generated drift, safe static/shared linking | Additive build/distribution change; no `.proto` semantic change | **YES** | Separate Contracts design/implementation branch and PR |

No Contracts schema enhancement is required for initial M4 gap output. A possible future optional
field for exact Core gap reason is explicitly non-prerequisite and would require its own evidence and
compatibility review.

## Rejected alternatives

- Put generated types, Protobuf runtime, or Contracts dependencies into Core.
- Build all markets with one implicit wire policy or cast Market directly to Core policy.
- Trust raw Protobuf objects as already Pydantic-validated.
- Parse prices/quantities with floating point, locale, scientific notation, rounding, or truncation.
- Require source fractional digits to equal `NumericSpec` when exact rescale is possible.
- Repair malformed ranges or synthesize missing previous-final IDs.
- Let M4 sequence-classify or apply events while converting.
- Return naked span views, cache spans in owners, or borrow repeated-field memory.
- Throw for ordinary malformed-wire input or report only strings.
- Generate timestamps or infer Host quality from clocks/logs/network state.
- Allow a caller boolean to set `synchronized=true`.
- Emit a fabricated last ID for `AwaitingBaseline`.
- Always emit historical `last_gap` after recovery.
- Clear diagnostic state or mutate projection while building output.
- Compute MarketState-derived values in the wire adapter.
- Build `ConsumerGapNotice`, `StreamStatus`, gRPC, queues, or subscription state in M4.
- Use a virtual/plugin/callback mapping framework or global mutable configuration.
- Store all wire messages/history in the adapter.
- Introduce Protobuf Arena ownership into the initial public API without evidence.

## Implementation sequence

Implementation remains NOT STARTED. After design acceptance and C-M4-001 completion, a separate
`feat/m4-snapshots-protobuf-boundary` branch should proceed in these reviewable steps:

1. Verify and pin the installed Contracts C++ target/fingerprint in an isolated consumer.
2. Add optional target/package-component scaffolding while keeping Core-only discovery clean.
3. Add adapter error/field enums and `AdapterResult` boundary tests.
4. Add `ExpectedIdentity` and explicit enum mapping functions.
5. Add owning wrappers and compile-time/runtime lifetime tests.
6. Add inbound snapshot conversion and matrix tests.
7. Add inbound depth conversion and matrix tests.
8. Add depth limit, context, gap, quality, and output builder value types.
9. Add state-specific Local snapshot generation and deterministic formatting.
10. Add semantic round trip, replay determinism, and allocation-failure sweeps.
11. Add independent property model, fuzzer, seeds, and sanitizer coverage.
12. Add Core-only and Adapter staged-install consumers for static/shared builds.
13. Update implementation documentation, run full acceptance gates, and obtain external code review.

No implementation branch is created by this design task.

## Acceptance gates

Future M4 implementation must pass:

- accepted M4 design/ADR and closed C-M4-001 artifact blocker;
- public-header self-containment for Core and adapter;
- Core-only configure/build/install/consumer with Protobuf unavailable;
- adapter configure/build/install/consumer with exact Contracts fingerprint;
- Debug and Release on Ubuntu GCC, Ubuntu Clang, and macOS AppleClang;
- ASan and UBSan; supported TSan evidence;
- clang-format and clang-tidy with warnings as errors;
- complete inbound/output/state/error/enum/presence/lifetime test matrices;
- independent property and deterministic replay tests;
- model-based adapter fuzz and blocking smoke run;
- executable allocation-failure sweeps;
- static/shared and duplicate-generated-symbol packaging tests;
- Contracts fixed-fixture semantic compatibility;
- no Core dependency regression; and
- independent external implementation code review.

None of these gates is claimed to have passed for M4 in this design PR.

## External review checklist

External M4 Architecture Review must challenge:

- whether the Contracts C++ artifact prerequisite and selected ownership are sufficient;
- Core/adapter package-component isolation for build tree and install tree;
- the generated/Core namespace overlap;
- strict validation coverage versus Pydantic semantics;
- exact-rescale and output fixed-scale decisions;
- same-side duplicate update and strict baseline ordering semantics;
- owning wrapper move/view lifetime safety;
- error taxonomy and precedence;
- enum zero/unknown forward-compatibility policy;
- identity, schema, context, and time ownership;
- four-state snapshot eligibility and diagnostic visibility;
- mapping `next_sequence` to incoming first ID;
- acceptability of detailed Core gap information loss;
- current-only historical-gap policy;
- quality ownership and canonical order;
- MarketState deferral and M6 ownership of Gateway/gRPC messages;
- exception safety, determinism, and Arena rejection;
- static/shared/export dependency responsibility;
- version fingerprint and compatibility gates; and
- independence of the proposed reference model and executability of failpoint tests.

The required next step after this docs-only PR is **External M4 Architecture Review**.
