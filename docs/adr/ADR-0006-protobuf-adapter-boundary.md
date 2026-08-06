# ADR-0006: Protobuf Adapter Boundary

- Status: PROPOSED
- Date: 2026-08-06

## Context

M1 through M3 form a deterministic C++20 Core with no wire-runtime dependency. The accepted
Core/wire separation decision forbids Contracts-generated Protobuf messages and the Protobuf runtime
from entering `BinanceMarketDataProjection::Core`. M4 must nevertheless consume the fixed Contracts
`ExchangeDepthSnapshot` and `DepthUpdate` messages and produce the fixed Contracts
`LocalOrderBookSnapshot` message.

The conversion is not a field-copying convenience. It must enforce Contracts enum, presence,
identity, schema-version, decimal, ordering, and depth semantics; detach M3's span-based views from
Protobuf message lifetime; and combine projection state with explicit Host-owned runtime context.
Core must remain usable by consumers that do not install or link Protobuf.

The fixed Contracts baseline `01d76a41929f36d89573159f5f458f9f1e378ada` contains `.proto`
sources and generated Python stubs. It does not contain generated C++ sources/headers, a CMake
package, exported CMake targets, a Conan package, an installable Protobuf target, or a standalone
descriptor set suitable for a Projection C++ build.

## Decision

M4 will be a separate optional target outside Core, proposed as:

```text
bmd_projection_proto_adapter
BinanceMarketDataProjection::ProtoAdapter
```

The target will provide explicit bidirectional conversions:

```text
Contracts-generated Protobuf messages
    -> owning adapter values
    -> synchronous M1/M2/M3 domain views

BookProjection + explicit Host context
    -> candidate Contracts-generated Protobuf snapshot
```

Core will not depend on the adapter. The adapter will depend on Core, the C++ Protobuf runtime, and
one versioned Contracts-owned C++ Protobuf target. M4 will not copy `.proto` files or generated C++
files into Projection.

## Dependency direction

```text
Host
  -> BinanceMarketDataProjection::ProtoAdapter
       -> BinanceMarketDataProjection::Core
       -> BinanceMarketDataContracts::Protobuf
            -> protobuf runtime
```

No reverse dependency is permitted. Building or installing Core alone must not discover, include,
or link Protobuf or Contracts artifacts.

## Target boundary

The adapter will be disabled by default through an explicit CMake option. Its public headers may
include generated `.pb.h` files because generated message types are part of the adapter API. Core
public headers must remain standard-library/project-domain only.

The installed package will expose Core and ProtoAdapter as distinct CMake components and export
sets. Requesting only Core will not call `find_dependency(Protobuf)` or
`find_dependency(BinanceMarketDataContracts)`. Requesting ProtoAdapter will resolve and expose the
Contracts and Protobuf transitive link requirements needed by static and shared consumers.

## Input ownership

Inbound conversion will copy validated price levels into owning adapter values. An
`AdaptedBookBaseline` owns bid/ask `std::vector<BookLevel>` values; an `AdaptedDepthBatch` owns one
`std::vector<LevelUpdate>`. Each constructs a fresh `BookBaseline` or `DepthBatch` in `view()` and
never stores a span member.

The Protobuf message may be destroyed after conversion. The view is valid only while its owner is
alive and unmoved and only for the synchronous `BookProjection` call. Returning a naked M3 view,
storing a self-referential span, or referencing Protobuf repeated fields is forbidden.

## Output context ownership

Core does not own venue, symbol, producer identity, generated time, snapshot source, depth policy,
or runtime quality facts. A value-type `SnapshotContext` supplied by the Host will own required
strings and timestamps. `SnapshotOptions` will carry a valid positive depth limit and explicit Host
quality flags. Current-gap detection time and recovery state are also Host input.

The adapter fixes venue to Binance and fixes the output schema version. Market is derived from and
checked against the immutable M3 sequence policy. The adapter reads no clock and no ambient
configuration.

## Error model direction

Normal wire-validation failures will return a C++20 `std::variant<T, AdapterError>`-based result.
`AdapterError` will contain a stable enum code and deterministic field identifier, not diagnostic
text as machine state and not references into a Protobuf message. `std::bad_alloc` may propagate.
No validation failure mutates `BookProjection`.

## Contracts artifact requirement

M4 implementation is blocked until Contracts publishes a versioned, installable C++ Protobuf
package/target for the exact baseline. That prerequisite must own C++ code generation, stable
installed include paths, one generated-symbol definition, Protobuf runtime linkage, package export,
and an exact commit/version fingerprint. Projection will consume the target; it will not generate
from another repository's source tree.

The prerequisite is a separate Contracts change and is not part of this ADR's implementation.

## Consequences

### Positive

- Core remains independently buildable, testable, installable, and usable without Protobuf.
- Wire validation and information loss are explicit and testable.
- Owning wrappers make M3 span lifetimes independent of Protobuf message storage.
- Host/runtime facts remain explicit, so live and replay can produce the same semantic output.
- One Contracts-owned generated target prevents copied-schema drift and duplicate generated symbols.

### Negative

- Adapter consumers acquire Protobuf and Contracts C++ package dependencies.
- A second installed component and its dependency-aware package configuration add build complexity.
- Generated snapshot types share the proto-derived C++ namespace
  `binance_market_data::projection::v1` with current Core types; adapter headers must use explicit
  qualification and avoid future name collisions.
- M4 implementation cannot start until the external Contracts artifact prerequisite is complete.

## Alternatives rejected

- Put Protobuf types or runtime linkage in Core.
- Build directly from a developer's Contracts source checkout.
- Add a Git submodule or use `FetchContent` for Contracts.
- Copy Contracts `.proto` files into Projection.
- Commit separately generated C++ files to Projection.
- Accept arbitrary Host-injected generated targets without an installable, versioned artifact
  contract.
- Return M3 span views that reference temporary vectors or Protobuf repeated fields.
- Use exceptions for ordinary malformed-wire input.
- Let the adapter read the system clock, own snapshot download, or implement gRPC/runtime state.

## Compatibility impact

Contracts `.proto` files remain authoritative for packages, field numbers, enum numbers, and
optional presence. M4 maps every enum explicitly and rejects unspecified or unknown values it must
interpret. It accepts compatible unknown fields but does not copy them into newly built messages.
Schema-version strings must match exactly.

The adapter package will pin the Contracts commit and fingerprint. A changed field meaning,
required/presence rule, enum behavior, package/type name, or decimal semantic requires reviewed
adapter work; a breaking wire change requires a new Contracts major package. Core ABI and Core-only
source compatibility are unaffected by adding or evolving the optional adapter.

## Implementation prerequisites

1. Independent external review and acceptance of this ADR and the detailed M4 design.
2. A separate Contracts PR providing the versioned installable C++ Protobuf target and exact
   baseline fingerprint.
3. Review of the resulting Contracts target names, package configuration, Protobuf version range,
   stable include layout, and static/shared ownership.
4. A separate `feat/m4-snapshots-protobuf-boundary` implementation branch after prerequisites are
   merged; this design task does not create that branch.

## Acceptance criteria

- Core-only configure/build/install and downstream consumption require no Protobuf.
- ProtoAdapter consumption resolves exactly one Contracts-generated C++ target and Protobuf runtime.
- Inbound messages are fully validated before an owning adapter value is returned.
- M3 span views cannot outlive temporary storage by construction and documented call discipline.
- Snapshot output uses only Core state and explicit Host context, with deterministic ordering.
- Normal failures are typed values; allocation failure exposes no partial result or Core mutation.
- Boundary, lifetime, conversion, compatibility, downstream, property, fuzz, and allocation-failure
  tests pass before implementation acceptance.
- Independent external code review approves the implementation.

## Superseded by

None.
