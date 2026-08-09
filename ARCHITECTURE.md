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

M5 (Differential Validation and Performance) adds replay/differential/fuzz validation and
benchmark infrastructure as test/benchmark-only artifacts outside Core. It introduces no new
production public API, no production dependency, and no container change; validation and
performance evidence are recorded separately. The M5 architecture was independently reviewed
and APPROVED (ADR-0007 ACCEPTED); pre-implementation decisions OD-M5-001 and OD-M5-002 are
CLOSED. Phase 1 (canonical replay infrastructure) is COMPLETE / MERGED and Phase 2 (independent
differential oracle: R1/R4 reference layers, neutral ReplayDriver, OperationObservation, layer
attribution) is IMPLEMENTED / PENDING INDEPENDENT REVIEW — see
`docs/M5_PHASE2_DIFFERENTIAL_ORACLE.md`.
OD-M5-003 remains SPIKE-RESOLVABLE.

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
