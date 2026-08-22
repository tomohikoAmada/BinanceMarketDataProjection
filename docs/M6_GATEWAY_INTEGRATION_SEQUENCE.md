# M6 Gateway Integration Sequence

```text
STATUS=PROJECT-LEAD APPROVED DEVELOPMENT SEQUENCE
M6_DESIGN=NOT STARTED
M6_IMPLEMENTATION=NOT STARTED
GATEWAY_RUNTIME=NOT IMPLEMENTED IN THIS REPOSITORY
GATEWAY_IMPLEMENTATION_STARTED=NO
OWNER_EXPLICIT_START_REQUIRED=YES
```

This document freezes development order and responsibility boundaries only.

It is NOT:

- the M6 integration contract;
- the Gateway implementation design;
- a thread model design;
- a queue sizing design;
- a networking implementation plan;
- a gRPC performance design.

Those require the next independent M6 preimplementation review.

## Approved development order

```text
M5 closure
    ↓
M6 cross-repo integration contract design
    ↓
Contracts Gateway C++/gRPC prerequisite, if required
    ↓
BinanceMarketDataGateway implementation
    ↓
minimal runnable Gateway Host
    ↓
Projection M6 Gateway Integration
    ↓
M6 acceptance
```

The five development steps are:

1. M5 formally closes. This record establishes that M5 and Phase 11 are
   complete.
2. Perform the read-only, architecture-first M6 cross-repository integration
   contract design. The next review defines only the necessary boundaries:
   Gateway-to-Contracts and Gateway-to-Projection dependencies, Projection Host
   interface expectations, snapshot ownership, WebSocket buffering/bootstrap
   handoff, reconnect/resync orchestration, single-writer preservation,
   thread/queue ownership, gRPC boundary, slow-consumer/backpressure semantics,
   session/connection-generation ownership, and status/telemetry ownership.
3. Resolve the narrowly scoped Contracts prerequisite needed for the real
   Gateway C++ implementation, if required. Gateway consumes Contracts-owned
   public wire/service definitions and must not copy or fork Contracts `.proto`
   definitions. If the current Contracts C++ package does not expose required
   gRPC bindings, that prerequisite belongs in
   `BinanceMarketDataContracts` before full Gateway implementation.
4. Establish and develop `BinanceMarketDataGateway` as the real Gateway
   Host/runtime module.
5. After a minimal runnable Gateway Host can drive Projection through the
   accepted boundary, perform Projection M6 Gateway Integration. Projection M6
   verifies the real Gateway Host ↔ Projection integration; it does not
   implement Gateway runtime inside Projection.

## Frozen dependency direction

```text
BinanceMarketDataGateway
    |
    +--> BinanceMarketDataContracts
    |
    +--> BinanceMarketDataProjection::ProtoAdapter
              |
              +--> BinanceMarketDataProjection::Core
```

Projection may optionally consume the Contracts Protobuf message package
through its adapter, as already implemented. Projection MUST NOT depend on
`BinanceMarketDataGateway`.

## Ownership distinction

`BinanceMarketDataGateway` is a real software/runtime module. Its Host/runtime
responsibilities include Binance independent WebSocket connections, Binance REST
snapshot acquisition, connection/reconnect lifecycle, required 24-hour
connection rotation, bootstrap buffering, snapshot-plus-stream handoff,
reconnect/resync orchestration, queues, backpressure, slow-consumer isolation,
threading, gRPC server runtime, subscription lifecycle,
`gateway_instance_id`, `connection_generation`, `session_sequence`, publish
timestamps, and operational telemetry/status.

Gateway does not own a second implementation of Projection semantics. It must
not independently reimplement OrderBook semantics, Spot sequence policy, USD-M
sequence policy, Projection lifecycle, deterministic gap classification, or
deterministic book projection. Those remain owned by
`BinanceMarketDataProjection`.

Projection M6 Gateway Integration is a Projection milestone that proves the
real Gateway Host can correctly embed and drive Projection. It is not a second
Gateway artifact and it is not Gateway runtime inside Projection.

```text
DEVELOP_PROJECTION_M6_RUNTIME_FIRST=NO
DEVELOP_GATEWAY_FIRST=YES
M6_INTEGRATION_CONTRACT_DESIGN_FIRST=YES
PROJECTION_M6_FINAL_INTEGRATION_AFTER_RUNNABLE_GATEWAY=YES
M6_GATEWAY_INTEGRATION=NOT STARTED
M6_PLANNING_AUTHORIZED=YES
```

`DEVELOP_GATEWAY_FIRST=YES` describes sequencing after the M6 integration-contract and any
required prerequisite stages, followed by explicit project-owner authorization. It does not
authorize Gateway implementation now. M6 design, M6 implementation, and Gateway implementation
remain not started.

Projection remains strategy-independent, deterministic, replayable,
single-writer, and embedded. It does not own WebSocket clients, REST clients,
gRPC servers, runtime queues, thread schedulers, or reconnect managers. The
Gateway is the Host; M6 verifies the integration while preserving this
architecture boundary.
