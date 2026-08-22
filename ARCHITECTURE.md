# Architecture

## Responsibility

BinanceMarketDataProjection is a strategy-independent, deterministic, replayable, single-writer,
embedded C++20 library. Live ingestion and historical replay invoke the same core logic. M1 added
exact numeric and domain primitives, M2 added the deterministic order book, M3 added
market-specific sequence and projection lifecycle state, and M4 added an optional Protobuf message
adapter outside Core, merged and verified on `main`.

## Explicit non-responsibilities

The module does not own Binance network connections, snapshot downloads, WebSocket sessions, a gRPC
server, consumer queues, storage, history storage, strategy logic, or trading. Hosts own lifecycle,
transport, scheduling, persistence, and external integration.

## Accepted M4 boundary

```text
Gateway / History Host
        ↓ explicit wire input and runtime context
Optional Protobuf Adapter Target
        ↓ checked owning domain values; private call-local views
Projection Core

Projection Core + explicit Host context
        ↓
Optional Protobuf Adapter Target
        ↓
Contracts-generated Protobuf snapshot
```

The optional adapter boundary is implemented, independently reviewed, merged through PR #8, and
verified on `main`. Core remains independently
buildable, installable, and usable without Protobuf, generated Contracts code, or gRPC. The adapter
maps messages and explicit context; it does not own networking, clocks, buffering, recovery, or
Gateway lifecycle. Gateway/gRPC runtime remains M6 scope. The C-M4-001 prerequisite is closed and
the exact Contracts package metadata is validated only when the optional component is requested.

## M5 closure and M6 Gateway Host boundary

M5 is COMPLETE, including Phase 11 independent final implementation review approval. The weekly
M5 performance operation remains active as exploratory, nonblocking reporting; it does not change
the production `std::map` / `KEEP_STD_MAP` decision.

BinanceMarketDataGateway is the future real runtime module and Host. It may embed Projection, but
Projection does not embed or own Gateway. Gateway owns transport and runtime concerns such as
Binance WebSocket and REST access, connection and session lifecycle, snapshot/bootstrap handoff,
reconnect/resync orchestration, queues, backpressure, slow-consumer isolation, threading, gRPC
server runtime, subscriptions, identity/generation/sequence metadata, publish timestamps, and
operational status. Gateway must not duplicate OrderBook semantics, Spot/USD-M sequence policy,
Projection lifecycle, deterministic gap classification, or deterministic book projection.

The frozen logical dependency direction is:

```text
BinanceMarketDataGateway
    |
    +--> BinanceMarketDataContracts
    |
    +--> BinanceMarketDataProjection::ProtoAdapter
              |
              +--> BinanceMarketDataProjection::Core
```

Projection may consume the Contracts Protobuf message package only through its optional adapter,
as already implemented. Projection MUST NOT depend on BinanceMarketDataGateway. The Gateway is the
Host; Projection remains the strategy-independent, deterministic, replayable, single-writer,
embedded C++20 library. M6 verifies that a real Gateway Host can correctly embed and drive that
library; it does not implement Gateway runtime inside Projection.

M5 (Differential Validation and Performance) is COMPLETE and validation-only. Its replay,
differential/reference, cross-compiler manifest, fuzzing, benchmark, allocation/memory,
container-experiment, recorded-replay/canary, and final-audit evidence is recorded in the
[M5 phase documents](docs/M5_PHASE1_CANONICAL_REPLAY.md) and
[`docs/CURRENT_STATE.md`](docs/CURRENT_STATE.md). M5 introduces no new production public API,
production dependency, production container migration, or Gateway runtime. The M5 architecture
was independently reviewed and APPROVED (ADR-0007 ACCEPTED); `KEEP_STD_MAP` is the accepted
container decision.

Adapter owners are bound to the conversion `NumericSpec` and sequence policy and must check both
against the target Core instance before mutation. Schema baseline/fingerprint identity is distinct
from the later Contracts package revision/version. Host runtime quality uses a closed input domain
that cannot represent or override facts derived from the current Core book and lifecycle state.

## Runtime context

```text
Gateway Host → optional adapter → Projection Core → optional adapter → Snapshot
History Host → optional adapter → Projection Core → optional adapter → Snapshot
```

The Host supplies identity, producer metadata, timestamps, depth policy, and runtime quality facts.
The arrows describe the implemented candidate boundary; they do not imply Host runtime ownership.

## Dependency direction

Core does not depend on Gateway, Recorder, History, Health, View, Control, a Protobuf runtime, a
logger, or a network library. Its public headers use only the C++ standard library. Test and
benchmark frameworks remain private development dependencies.

## Thread model

For each Market/Symbol projection instance, the host must preserve single-writer ordering. The Core
does not create threads, schedule work, or synchronize competing writers. Concurrency belongs to the
host. M4 snapshots leave the adapter by value.

## Determinism

All inputs that can affect results, including time, must be supplied explicitly by the host. The Core
must not read the system clock, random state, environment, filesystem, network, or mutable global
configuration. Equal ordered inputs and configuration must produce equal outputs in live and replay
contexts.

## Core/wire boundary

Wire schemas are contracts, not domain storage types. Protobuf messages do not enter the Core API.
The accepted separate adapter target translates explicitly in both directions without changing
deterministic Core rules or making Core consumers link Protobuf.
