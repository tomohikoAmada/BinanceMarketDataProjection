# Architecture

## Responsibility

BinanceMarketDataProjection is intended to be a strategy-independent, deterministic, replayable,
single-writer, embedded C++20 library. Live ingestion and historical replay will invoke the same
core logic. M1 adds exact numeric and domain primitives but no order book or projection state.

## Explicit non-responsibilities

The module does not own Binance network connections, snapshot downloads, WebSocket sessions, a gRPC
server, consumer queues, storage, history storage, strategy logic, or trading. Hosts own lifecycle,
transport, scheduling, persistence, and external integration.

## Future layering

```text
Protobuf Adapter
    ↓
Projection Domain Types
    ↓
Projection Core
```

The adapter is planned for M4 and does not exist in M1.

## Runtime context

```text
Gateway → Projection Core → Snapshot
History → Projection Core → Snapshot
```

The arrows describe future data flow, not M1 implementations.

## Dependency direction

Core does not depend on Gateway, Recorder, History, Health, View, Control, a Protobuf runtime, a
logger, or a network library. Its public headers use only the C++ standard library. Test and
benchmark frameworks remain private development dependencies.

## Thread model

For each Market/Symbol projection instance, the host must preserve single-writer ordering. The Core
does not create threads, schedule work, or synchronize competing writers. Concurrency belongs to the
host. Future snapshots are expected to leave the Core by value or through immutable objects.

## Determinism

All inputs that can affect results, including time, must be supplied explicitly by the host. The Core
must not read the system clock, random state, environment, filesystem, network, or mutable global
configuration. Equal ordered inputs and configuration must produce equal outputs in live and replay
contexts.

## Core/wire boundary

Wire schemas are contracts, not domain storage types. Protobuf messages must not enter the Core API.
A future adapter target will translate at the boundary without changing deterministic Core rules.
