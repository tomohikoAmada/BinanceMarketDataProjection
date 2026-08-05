# ADR-0001: C++20 Projection Core

- Status: ACCEPTED
- Date: 2026-08-05

## Context

The same deterministic projection logic must embed in a future C++20 Gateway and support replay use.
Primary deployment targets include macOS ARM64 and Ubuntu ARM64/RK3588, with hosted Ubuntu x86_64 CI.
The library needs predictable ownership, memory behavior, and latency.

## Decision

Implement the Core as a C++20 library with standard-library-only public foundations. Use the same
library in live and replay hosts rather than introducing an independent service boundary.

## Consequences

The Gateway and Core share a language and can integrate without serialization or IPC. Memory layout,
allocation, and latency remain controllable. The cost is a more complex native build/toolchain matrix
and greater responsibility for memory safety, undefined behavior, ABI discipline, and sanitizer use.
