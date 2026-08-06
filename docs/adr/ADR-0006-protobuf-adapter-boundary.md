# ADR-0006: Protobuf Adapter Boundary

- Status: ACCEPTED
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

External architecture review Round 1 requested changes at design head
`62283fcefcf17aef50b189714c8ffcfd2a04db39`: separate schema/package identity, bind adapted inputs
to their target projection, and prevent Host injection of Core-derived quality. Round 2 approved the
revised architecture with all three findings closed.

## Acceptance record

- Round 1 reviewed head: `62283fcefcf17aef50b189714c8ffcfd2a04db39`
- Round 1 result: **CHANGES REQUESTED**
- Round 1 blocking findings: **3**
- Round 2 reviewed head: `44e8f0fe8a8449cf895767c24e79921b5dc14456`
- Round 2 result: **APPROVED**
- Blocking architecture findings after Round 2: **0**
- Accepted design: Separate optional Protobuf adapter boundary with checked owning inputs,
  separate schema/package identity, and disjoint quality ownership.

ADR acceptance does not close C-M4-001. M4 implementation remains **NOT STARTED**.

## Decision

M4 will be a separate optional target outside Core, proposed as:

```text
bmd_projection_proto_adapter
BinanceMarketDataProjection::ProtoAdapter
```

The target will provide explicit bidirectional conversions:

```text
Contracts-generated Protobuf messages
    -> owning adapter values bound to NumericSpec and sequence policy
    -> checked synchronous M1/M2/M3 invocation with private call-local views

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
`std::vector<LevelUpdate>`. Each also owns the conversion `NumericSpec`, mapped
`SequencePolicyKind`, and validated Host-relevant inbound quality metadata.

Owners expose only checked `install_into(BookProjection&)` or `apply_to(BookProjection&)`. They
compare target numeric spec before policy, return typed mismatch errors without invoking M3, then
construct a private temporary `BookBaseline` or `DepthBatch` for the synchronous call. M3 results,
including gaps, duplicates, stale input, and wrong state, are returned unchanged as successful
checked-call results. Raw span views are not public.

The Protobuf message may be destroyed after conversion. The private view cannot escape and is valid
only while its owner is alive and unmoved during the call. Returning a naked M3 view, storing a
self-referential span, or referencing Protobuf repeated fields is forbidden. Core cannot validate
symbol because it stores none; the Host Projection Registry retains that responsibility.

## Output context ownership

Core does not own venue, symbol, producer identity, generated time, snapshot source, depth policy,
or runtime quality facts. A value-type `SnapshotContext` supplied by the Host will own required
strings and timestamps. `SnapshotOptions` will carry a valid positive depth limit and explicit Host
quality flags. Current-gap detection time and recovery state are also Host input.

The adapter fixes venue to Binance and fixes the output schema version. Market is derived from and
checked against the immutable M3 sequence policy. The adapter reads no clock and no ambient
configuration.

## Quality ownership

Core-derived and Host-observed facts are disjoint types. The adapter alone derives
`CROSSED_BOOK`, `SEQUENCE_GAP`, and `SNAPSHOT_BRIDGE_PENDING` from the exact Core state; none can be
constructed through the Host input API. `SnapshotOptions` accepts a closed `HostQualityFact` domain
for runtime/normalization assertions, validates status-dependent combinations, then merges,
deduplicates, and orders both domains by an explicit semantic rank.

Inbound wire quality is validated and handled by an owning `AdaptedMetadata` sidecar. Supported
Host-relevant values map to `HostQualityFact`; the three source-message Core-derived values are
recognized but omitted because they cannot describe the target projection. The sidecar never enters
Core or output automatically. Unknown and unspecified wire values fail closed.

## Error model direction

Normal wire-validation failures will return a C++20 `std::variant<T, AdapterError>`-based result.
`AdapterError` will contain a stable enum code and deterministic field identifier, not diagnostic
text as machine state and not references into a Protobuf message. `std::bad_alloc` may propagate.
No validation failure mutates `BookProjection`.

## Contracts identity

Schema identity and package identity are separate. The approved schema baseline remains
`01d76a41929f36d89573159f5f458f9f1e378ada`. The future C++ package is necessarily built at a later,
currently unknown Package Revision and has a separately defined Package Version.

C-M4-001 must export the schema baseline plus a deterministic canonical M4
`FileDescriptorSet` SHA-256, the package revision/version, `protoc` and generated-ABI options, and
the compatible Protobuf runtime range. The dependency manager pins package identity; Projection
separately verifies schema identity and runtime compatibility. No message-reported value is trusted,
and no per-message descriptor hash is computed. An explicit one-time debug integrity probe may
compare compiled exported metadata but cannot invent repository identity.

## Contracts artifact requirement

M4 implementation is blocked until Contracts publishes a versioned, installable C++ Protobuf
package/target generated from the approved schema set. That prerequisite must own C++ code
generation, stable installed include paths, one generated-symbol definition, Protobuf runtime
linkage, package export, and all separate identity metadata above. Projection will consume the
dependency-locked package; it will not generate from another repository's source tree.

The prerequisite is a separate Contracts change and is not part of this ADR's implementation.

## Consequences

### Positive

- Core remains independently buildable, testable, installable, and usable without Protobuf.
- Wire validation and information loss are explicit and testable.
- Owning wrappers make M3 span lifetimes independent of Protobuf message storage.
- Checked owners prevent numeric-spec and policy cross-application before Core mutation.
- Schema compatibility remains stable across separately pinned build/distribution revisions.
- Host input cannot contradict the three facts derived from the current Core snapshot.
- Host/runtime facts remain explicit, so live and replay can produce the same semantic output.
- One Contracts-owned generated target prevents copied-schema drift and duplicate generated symbols.

### Negative

- Adapter consumers acquire Protobuf and Contracts C++ package dependencies.
- A second installed component and its dependency-aware package configuration add build complexity.
- Owner values are slightly heavier because they retain numeric spec, policy, and quality sidecar.
- Checked calls replace a more flexible but unsafe public raw-view API.
- Contracts must publish more package, schema, generator, and runtime identity metadata.
- `HostQualityFact` requires explicit maintenance as Contracts quality values evolve.
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
- Expose raw owner views or rely only on Host discipline to match numeric spec and policy.
- Let Host input name Core-derived quality facts.
- Treat schema baseline commit and package revision as one identity.
- Recompute descriptor hashes per wire message or trust a message-reported version.
- Use exceptions for ordinary malformed-wire input.
- Let the adapter read the system clock, own snapshot download, or implement gRPC/runtime state.

## Compatibility impact

Contracts `.proto` files remain authoritative for packages, field numbers, enum numbers, and
optional presence. M4 maps every enum explicitly and rejects unspecified or unknown values it must
interpret. It accepts compatible unknown fields but does not copy them into newly built messages.
Schema-version strings must match exactly.

The adapter package will pin Package Revision/Version and independently require the approved schema
baseline/fingerprint and compatible generator/runtime metadata. A changed field meaning,
required/presence rule, enum behavior, package/type name, or decimal semantic requires reviewed
adapter work; a breaking wire change requires a new Contracts major package. A later package
revision with the same fingerprint is not selected until the dependency lock is reviewed and
updated. Core ABI and Core-only source compatibility are unaffected by the optional adapter.

## Implementation prerequisites

1. Independent external review and acceptance of this ADR and the detailed M4 design.
2. A separate Contracts PR providing the versioned installable C++ Protobuf target, reproducible
   schema fingerprint, separate Package Revision/Version, and generator/runtime metadata.
3. Review of the resulting Contracts target names, package configuration, Protobuf version range,
   stable include layout, and static/shared ownership.
4. A separate `feat/m4-snapshots-protobuf-boundary` implementation branch after prerequisites are
   merged; this design task does not create that branch.

## Acceptance criteria

- Core-only configure/build/install and downstream consumption require no Protobuf.
- ProtoAdapter consumption resolves exactly one Contracts-generated C++ target and Protobuf runtime.
- Inbound messages are fully validated before an owning adapter value is returned.
- Adapted owners reject numeric-spec/policy mismatch before M3 and expose no public raw span view.
- M3 results propagate unchanged after a successful checked invocation.
- Inbound quality sidecars are owning; Host and Core quality domains cannot represent each other's
  members, and output combinations remain state-consistent.
- Snapshot output uses only Core state and explicit Host context, with deterministic ordering.
- Normal failures are typed values; allocation failure exposes no partial result or Core mutation.
- Boundary, lifetime, conversion, compatibility, downstream, property, fuzz, and allocation-failure
  tests pass before implementation acceptance.
- Independent external code review approves the implementation.

## Superseded by

None.
