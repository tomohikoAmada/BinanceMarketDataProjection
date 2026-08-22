# M6 Cross-Repository Integration Contract

- Status: **ACCEPTED DESIGN / DOCUMENTATION FREEZE**
- Date: 2026-08-22
- Scope: Projection-side cross-repository contract only
- Implementation status: **NOT STARTED**

This document freezes the accepted M6 integration contract between
`BinanceMarketDataGateway`, `BinanceMarketDataContracts`, and
`BinanceMarketDataProjection`. It does not implement Gateway runtime, change a
Protobuf schema, or change Projection production code.

## Reviewed baselines and authority

The freeze was reviewed against these exact `main` baselines:

| Repository | Reviewed head |
|---|---|
| `BinanceMarketDataProjection` | `59a7bca6a22e39a31323b221d18cfd4d5ac24b90` |
| `BinanceMarketDataContracts` | `4068bdc865a30b0d513ea40298660e9883896ce5` |
| `BinanceMarketDataRecorder` | `23e0a2164431d78bbc2425872559e794053c5350` |

Semantic authority is applied in this order:

1. verified external Binance protocol facts recorded in accepted authorities;
2. accepted Projection ADRs and milestone designs;
3. exact checked-out code, tests, Git history, and build evidence; and
4. orientation summaries and review/status records.

The relevant Projection authorities are [ARCHITECTURE.md](../ARCHITECTURE.md),
[the M4 boundary design](M4_SNAPSHOTS_AND_PROTOBUF_BOUNDARY_DESIGN.md),
[ADR-0001](adr/ADR-0001-cpp20-projection-core.md),
[ADR-0003](adr/ADR-0003-single-writer-order-book.md),
[ADR-0004](adr/ADR-0004-core-wire-separation.md),
[ADR-0005](adr/ADR-0005-market-specific-sequence-policy.md),
[ADR-0006](adr/ADR-0006-protobuf-adapter-boundary.md), and
[ADR-0008](adr/ADR-0008-spot-bootstrap-successor-coverage.md). This document
references those decisions and does not replace or rewrite them.

## Goals and non-goals

The goal is a durable contract for a real C++ Gateway Host to drive the current
Projection APIs through the Contracts wire boundary while preserving deterministic
Core semantics and single-writer mutation.

This freeze does not authorize:

- Gateway or Contracts implementation;
- a new Projection runtime or host abstraction;
- a Protobuf adapter or schema change;
- networking, storage, threads, queues, or system-time access in Core;
- a second order-book or sequence-classification implementation; or
- full market-state composition.

## Dependency direction and ownership

The only permitted integration direction is:

```text
Binance Public APIs
        |
        v
BinanceMarketDataGateway
        |
        +--> BinanceMarketDataContracts
        |
        +--> BinanceMarketDataProjection::ProtoAdapter
                  |
                  +--> BinanceMarketDataProjection::Core
```

Projection does not depend on Gateway or Recorder. Gateway does not depend on
Recorder. Gateway must consume Contracts-owned schemas and must not copy or fork
Contracts `.proto` definitions.

| Responsibility | Owner |
|---|---|
| Public wire schemas, Gateway RPC definitions, compatibility, language bindings and package surfaces | Contracts |
| Binance WebSocket/REST, connection lifecycle, bootstrap orchestration, runtime scheduling, serialization, gRPC server, subscriptions, bounded consumer queues, timestamps and telemetry | Gateway |
| Strict Contracts-to-Core adaptation, boundary validation, `NumericSpec`/policy binding, and `LocalOrderBookSnapshot` construction | Projection `ProtoAdapter` |
| Numeric semantics, Spot/USD-M sequence policies, lifecycle, gap classification, deterministic mutation and local-book state | Projection Core |

Gateway must not implement an independent Binance depth sequence classifier.

## Projection Host boundary

There is no new Gateway-to-Projection abstraction:

```text
NEW_GATEWAY_PROJECTION_HOST_ABSTRACTION=NO
```

The Gateway reuses the existing M3/M4 surfaces:

- construct and own one `BookProjection` for one projection identity;
- adapt an `ExchangeDepthSnapshot` with
  `adapt_exchange_depth_snapshot(...)`, then call
  `AdaptedBookBaseline::install_into(BookProjection&)`;
- adapt each `DepthUpdate` with `adapt_depth_update(...)`, then call
  `AdaptedDepthBatch::apply_to(BookProjection&)`;
- use `BookProjection::status()`, `last_update_id()`, `synchronized_book()`,
  and `reset()` as already defined; and
- create consumer-facing snapshots with
  `make_local_order_book_snapshot(const BookProjection&, const SnapshotContext&,
  const SnapshotOptions&)`.

The adapter remains stateless with respect to runtime orchestration. It owns
validated conversion values and checked binding, but not clocks, networking,
buffering, persistence, recovery, or gRPC.

## Projection identity and single writer

The external Projection identity is:

```text
venue + market + symbol
```

The current venue is `BINANCE`. The practical current mutable lookup key is
`market + symbol`, because Core does not retain the symbol and the venue is
fixed by the current Contracts/Projection boundary. The Host registry is
responsible for selecting the matching `BookProjection`.

The single-writer unit is one `BookProjection` instance. Only one serialized
execution path may mutate a particular instance at a time. Concurrent or
reentrant mutation is forbidden. This contract deliberately does not choose
the mechanism: dedicated thread, Asio strand, mutex, executor, pool size,
coroutine scheduler, CPU affinity, and exact executor technology remain Gateway
implementation decisions.

## Bootstrap contract

Gateway owns the acquisition and buffer lifecycle:

```text
open WebSocket
    -> start bounded pre-snapshot buffering
    -> obtain REST depth snapshot
    -> adapt and install the baseline
    -> feed buffered depth events in source receive order
    -> let Projection classify each event
    -> reach Synchronized
    -> continue ordered live application
```

Gateway owns buffer lifetime and boundedness, snapshot acquisition or
reacquisition, and feeding order. Projection owns stale, duplicate, valid
bridge/successor, gap, and continuity classification. Gateway must not add a
second `is_valid_successor()`, `is_gap()`, or
`classify_depth_sequence()` semantic authority. Acquisition actions required by
Binance's bootstrap procedure do not authorize duplicate sequence semantics.

The accepted Projection policies remain authoritative:

- Spot advancing continuity, including the bootstrap bridge, uses guarded
  successor coverage `U <= C + 1 <= u`; `u < C` is stale, `u == C` is duplicate,
  and `U > C + 1` is a gap. Spot `pu` is ignored.
- USD-M uses the accepted policy in ADR-0005, including its bootstrap and live
  `pu` continuity rules.

The Gateway supplies normalized inputs and follows the returned Projection
result. It does not repair IDs, synthesize `pu`, reorder events, or classify a
gap independently.

## Reconnect and resynchronization

Transport lifecycle and Projection sequence semantics are distinct.

Gateway detects and orchestrates network disconnect, transport close, planned
connection replacement, REST failure, runtime lifecycle, and recovery. Projection
determines deterministic sequence consequences only from updates actually fed to
it.

For the minimal M6 design, a rebuilt or replaced upstream connection starts a
new connection generation. The book-dependent path may conservatively
rebootstrap. Event stitching across old and new transport generations and
make-before-break merging are not required at this contract level.

## Identity and ordering domains

These values are distinct and must never be interchanged:

| Domain | Meaning and owner |
|---|---|
| Binance `U` / `u` / `pu` | Exchange depth sequence values; interpreted by Projection policy |
| Projection `last_update_id` | Last accepted local Projection sequence value |
| `connection_id` | Opaque identity of one concrete Binance upstream connection; Gateway-owned; a new concrete connection gets a new ID |
| `connection_generation` | Gateway-owned upstream source transport epoch; a rebuild/replacement of the applicable source advances it |
| `gateway_instance_id` | One Gateway runtime/process incarnation; a new runtime start gets a new ID |
| `subscription_id` | One accepted consumer subscription |
| `session_sequence` | Per-subscription Gateway delivery order, starting at 1 and increasing monotonically |
| Protobuf field number | Wire-schema identity; not a runtime or exchange sequence |

`connection_generation` has an important presence rule. If one upstream source
uniquely applies to the published item, expose that source's generation. If no
unique upstream source applies, the wire contract must express **N/A**. Never
fabricate a generation merely to populate a mandatory field. The exact Contracts
Proto3 presence correction is a subsequent Contracts task and is not prescribed
by this Projection document.

`CONTIGUOUS_EVENTS` means no silent loss inside the Gateway's ordered consumer
delivery session. It does not mean a global Binance sequence across multiple
streams or selectors. A multi-selector subscription can merge sources with
separate exchange sequence domains; `session_sequence` defines Gateway delivery
order while source-specific sequence semantics remain in each payload and the
Projection domain.

## Snapshot types and publication cut

These types and responsibilities remain separate:

| Type | Meaning and owner |
|---|---|
| `ExchangeDepthSnapshot` | Binance REST source acquired and owned by Gateway; adapted by `ProtoAdapter`; not a consumer local-book snapshot |
| `BookBaseline` | Core input/view produced through adaptation and used to install a baseline; not a wire product |
| `LocalOrderBookSnapshot` | Deterministically produced from `BookProjection` through `ProtoAdapter`; consumer-facing |
| `MarketStateSnapshot` | Broader state product; full composition remains deferred |

The initial order-book snapshot must not race subsequent updates. Gateway must
establish this logical serialized cut in the same Projection/publication ordering
domain:

```text
Projection has deterministically accepted through update C
        -> capture LocalOrderBookSnapshot(last_update_id = C)
        -> establish subscription publication cut
        -> enqueue/publish the initial snapshot
        -> order all subsequent applicable accepted updates after it
```

This is a Gateway orchestration guarantee. It does not require CPU atomics, a
new Projection lock, or a new Projection API. Snapshot continuity is defined
primarily by `snapshot.last_update_id` and the subsequent Binance depth sequence,
not merely by `connection_generation`.

## Consumer delivery and backpressure

All consumer queues are bounded. One slow consumer must not indefinitely stall
Binance ingress, Projection mutation, or other consumers. Exact capacities and
queue technologies are not frozen.

### `SubscribeEvents`

Delivery uses a bounded FIFO, or a semantically equivalent bounded ordered queue.
Silent drop is forbidden. If overflow destroys continuity, the subscription or
session can no longer claim contiguous delivery and must produce explicit
failure/gap/termination behavior so the consumer can resubscribe.

### `SubscribeOrderBook`

Delivery is an initial consistent `LocalOrderBookSnapshot` followed by ordered
applicable depth updates. Silent or arbitrary update dropping is forbidden. Loss
of one required update destroys the snapshot/update chain, so the subscription
must terminate or recover with a new subscription and snapshot.

### `SubscribeMarketState`

This is `LATEST_STATE` semantics. Pending intermediate states may be
coalesced or overwritten when explicit, and the result must never be presented
as contiguous event history.

## Market-state deferral

The `SubscribeMarketState` schema/RPC may remain **DRAFT**. Full implementation
is deferred from the minimal runnable Gateway because the current
`MarketStateSnapshot` includes data such as mark price, index price, funding,
and open interest beyond deterministic local order-book state. Projection Core
does not acquire external market feeds merely to populate that DRAFT message.
This deferral does not delete or reject the future RPC.

## Gateway language

```text
GATEWAY_IMPLEMENTATION_LANGUAGE=C++
```

The Gateway is the direct Host of an embedded C++20 Projection library and its
C++ `ProtoAdapter`. A different implementation language would currently add an
unjustified FFI, process, or RPC boundary. Contracts remains language-neutral
externally. No comparative C++/Rust/Go/Python benchmark is required to reopen
this decision.

## Contracts prerequisite

```text
CONTRACTS_CPP_GRPC_PREREQUISITE=YES
```

At the reviewed Contracts baseline, `BinanceMarketDataContracts::Protobuf` is a
message-only C++ package. Its CMake generation covers seven message protos with
`--cpp_out`; it does not generate or export C++ bindings for
`gateway/v1/gateway_service.proto`. The subsequent Contracts prerequisite must
narrowly provide the Contracts-owned C++ gRPC service/stub bindings needed by the
Gateway. It must not be implemented in this task, and Projection must not copy
the service definitions.

## Recorder boundary

```text
Gateway -> Recorder dependency = NO
```

Recorder is evidence only. Its accepted/current-main material provides reusable
semantic lessons about bounded ingress, explicit failure rather than silent
loss, receive timestamp discipline, connection identity/generation, planned
rotation, reconnect boundaries, and failure isolation.

M6 does not reuse Recorder's Raw spool, catalog, manifest, archive transaction,
persistence, or runtime framework by default, and does not create a shared
Recorder/Gateway framework.

## Explicit anti-overengineering decisions

The M6 contract does not include a `GatewayProjectionHost`, `IProjectionRuntime`,
`ProjectionEngineFacade`, generic event bus, Kafka, custom internal RPC,
cross-process Projection service, generic transport framework, dynamic plugins,
multi-exchange framework, persistent Gateway event log, second order-book
implementation, second sequence classifier, shared Recorder/Gateway runtime
framework, Projection thread pool, Projection networking, pre-measurement
lock-free queues, arbitrary thread counts or capacities, or Kubernetes.

## Risks and open prerequisites

Only these blocking prerequisites remain from the freeze:

| ID | Risk | Resolution owner |
|---|---|---|
| `M6-P1-001` | The Contracts Gateway wire must support absence of a unique applicable upstream `connection_generation`; a fabricated generation is invalid. | Contracts prerequisite stage |
| `M6-P1-003` | The reviewed Contracts C++ package lacks Gateway gRPC service bindings. | Contracts prerequisite stage |

The order-book publication cut is resolved by this design and is not an open P1.
Market-state composition is deferred and nonblocking for the minimal runnable
Gateway.

## Frozen implementation sequence

This sequence is recorded for later authorization; this documentation task does
not authorize steps 2 through 13:

1. M6 cross-repository integration contract freeze.
2. Contracts Gateway DRAFT semantic correction.
3. Contracts C++ gRPC service-binding prerequisite.
4. Create the `BinanceMarketDataGateway` repository.
5. Build the minimal C++ Gateway foundation.
6. Establish the first market+symbol depth pipeline.
7. Implement reconnect, resync, and planned rotation.
8. Implement bounded consumer egress.
9. Implement `SubscribeOrderBook`.
10. Implement `SubscribeEvents`.
11. Implement minimal `GetGatewayStatus`.
12. Defer full `SubscribeMarketState` until state-composition authority is frozen.
13. Perform Projection M6 real-Gateway integration acceptance.

## Acceptance and freeze summary

The M6 cross-repository contract design is complete when this document is
accepted with the following frozen conclusions:

```text
M6_CROSS_REPOSITORY_CONTRACT_DESIGN=COMPLETE
M6_IMPLEMENTATION=NOT STARTED
GATEWAY_IMPLEMENTATION=NOT STARTED
PROJECTION_CODE_CHANGE_REQUIRED_BEFORE_GATEWAY=NO
CONTRACTS_CHANGE_REQUIRED_BEFORE_GATEWAY=YES
CONTRACTS_CPP_GRPC_PREREQUISITE=YES
GATEWAY_IMPLEMENTATION_LANGUAGE=C++
NEW_GATEWAY_PROJECTION_HOST_ABSTRACTION=NO
GATEWAY_RECORDER_DEPENDENCY=NO
```

Acceptance means the dependency direction, ownership, existing Projection Host
boundary, single-writer invariant, bootstrap/reconnect boundary, identity and
ordering domains, snapshot publication cut, bounded delivery semantics,
MarketState deferral, language choice, Contracts prerequisite, Recorder boundary,
anti-overengineering limits, risks, and implementation order above are the M6
contract. It does not claim any M6 runtime or Gateway implementation exists.
