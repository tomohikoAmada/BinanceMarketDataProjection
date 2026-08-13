# M5 Phase 5 — Differential Replay Fuzzing

## Status

**IMPLEMENTED / PENDING INDEPENDENT REVIEW**

NOT MERGED. Do not mark COMPLETE before independent implementation review.

## Authoritative base

- Starting main: `d287c08ed362c5202f25eb77b411bd24bb82cef0`
- Phase 4: COMPLETE / MERGED (PR #16, squash merge `7b6d9ef3b222675138fdd34f3fed381216fe9d02`)
- Phase 5: NOT STARTED at time of implementation

## Scope

Phase 5 adds exactly one new cross-layer differential fuzzer: `bmd_projection_replay_fuzz`.

It does NOT replace, merge, or delete the existing four fuzz targets:
- `bmd_projection_decimal_parser_fuzz`
- `bmd_projection_order_book_fuzz`
- `bmd_projection_book_projection_fuzz`
- `bmd_projection_proto_adapter_fuzz`

## Architecture

### Fundamental property

For every decoded fuzz operation:

```
production OperationObservation == reference OperationObservation
```

using the existing ReplayDriver comparison discipline (Phase 2).

On the first divergence: the fuzz input produces a libFuzzer abort, so libFuzzer can
save/minimize/replay the reproducer.

### Production/reference sides

**Core mode:**
- Production: `CoreProductionSide` (M1 parser + M3 BookProjection)
- Reference: `ReferenceSide` with `ReplayMode::CoreOnly` (R1 decimal + R3 projection)

**Adapter mode (ProtoAdapter=ON):**
- Production: `AdapterProductionSide` (M4 ProtoAdapter + M3 BookProjection)
- Reference: `ReferenceSide` with `ReplayMode::AdapterEnabled` (R1 + R3 + R4 adapter)

### ReplayFixture identity

A minimal truthful `ReplayFixture` is constructed from the decoded FuzzCase. The identity
fields are truthful about the structured fuzz origin ("structured-fuzz-input"). No canonical
text log is fabricated. The ReplayDriver uses fixture identity only for diagnostic messages.

## Structured byte decoder

### Direct byte decoding (NOT text parsing)

```
FUZZ BYTES → STRUCTURED OPERATIONS → PRODUCTION + REFERENCE
```

The canonical text replay parser (`replay_parser`) is NOT called as the semantic input path.
The fuzz target decodes arbitrary bytes directly into replay operations using a bounded
deterministic `ByteCursor`.

### Decoder bounds

| Resource | Cap |
|----------|-----|
| Operations per input | 64 |
| Levels per event | 8 |
| Decimal token bytes | 31 |
| Snapshot strings | 15 |
| Symbol length | 10 |

Install/rebaseline bid and ask vectors share the single eight-level event budget. Quality-fact
decoding likewise honors each call site's bound (six for snapshot host facts, eight for adapter
metadata). An inverted DepthUpdate consumes its complete encoded payload before becoming a
`MalformedRangeOp`, so following operations retain stable byte framing.

### Determinism

The same byte string always produces the same structured test case. No `std::random_device`,
system clock, filesystem entropy, ambient global state, or networking.

### Operation variant reachability

All seven operation variants are reachable from byte input:
- `InstallBaselineOp` (opcode 0)
- `DepthUpdateOp` (opcode 1)
- `RebaselineOp` (opcode 2)
- `ResetOp` (opcode 3)
- `SnapshotRequestOp` (opcode 4)
- `AdapterMetadataOp` (opcode 5)
- `MalformedRangeOp` (opcode 6)

### Special uint64 values

`read_var_u64` makes these special values reachable:
- 0
- 1
- 2
- `UINT64_MAX`
- `UINT64_MAX - 1`
- `UINT64_MAX - 2`

### NumericSpec

Scale values are clamped to [0, 18] via modular reduction. Invalid out-of-domain scales
never reach `DecimalScale::create`.

### Decimal token coverage

Level tokens are generated with varied forms:
- Empty strings
- Zero ("0")
- Integers
- Decimal-form with point and fractional digits
- Leading zeros
- Sign-prefixed (parser should reject)
- Embedded invalid characters
- Large magnitude

## Core and Adapter modes

Both modes are reachable. The header byte bit 0 selects the mode.

### Adapter scenario dimensions

In AdapterEnabled mode, the decoder exercises M4 binding/adaptation boundaries with
structured fuzz bytes for:

- `ScenarioVenue`: Binance, Unspecified, UnknownNumeric
- `ScenarioMarket`: Spot, UsdMPerpetual, Unspecified, UnknownNumeric
- Wire symbol and expected symbol
- Conversion and projection `NumericSpec`
- Expected and projection `SequencePolicy`

Invalid C++ enum object representations are never created. Fuzz bytes map to explicitly
defined enum values.

## Market coverage

Both Spot and USD-M perpetual markets are reachable (header byte bit 1).

Spot authority remains successor coverage per ADR-0008.
USD-M semantics are unchanged per ADR-0005.

## Divergence behavior

After `ReplayDriver::run()`:
1. If `first_divergence.has_value()`: print stable diagnostic text, then `std::abort()`
2. The libFuzzer crash input is the reproducer
3. No custom Phase-5 reducer is implemented

## Observation retention

`ObservationRetention::RetainNone` — per-event comparison only. Successful observations
are not retained. Divergence diagnostics include enough stable information to reproduce.

## Cross-input state

Every `LLVMFuzzerTestOneInput` call starts from a fresh state. No static book state,
pending adapter metadata, or cross-input mutable state.

## CMake integration

A shared `cmake/M5Support.cmake` defines the M5 replay, oracle, and reference support
targets once. Both `tests/CMakeLists.txt` and `fuzz/CMakeLists.txt` include it.

The fuzz preset (`BUILD_TESTS=OFF, BUILD_FUZZERS=ON`) builds the M5 support targets
from the shared definitions without dragging in GTest or the full test tree.

In fuzz configuration only, those existing support targets compile with
`-fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer`. The final replay fuzzer
continues to compile/link with `-fsanitize=fuzzer,address,undefined`; static support libraries do
not receive a libFuzzer main. `scripts/check-m5-fuzz-instrumentation.py` inspects the generated
compile database and fails the fuzz smoke unless every replay/oracle/reference source (including
`reference_side.cpp`, `reference_decimal.cpp`, `replay_driver.cpp`, and `divergence.cpp`) carries
the intended fuzz-only compile instrumentation.

## Corpus

### Path

```
fuzz/corpus/replay/
```

### Mandatory categories

| Category | Seed file | Verification |
|----------|-----------|-------------|
| Spot synchronized stream | `spot_synchronized_stream.bin` | Spot mode plus exact baseline/bridge/continuation ID geometry |
| USD-M synchronized stream | `usdm_synchronized_stream.bin` | USD-M mode plus bootstrap range, `pu`, and live continuation geometry |
| Bridge transition | `bridge_transition.bin` | Successor-covering baseline bridge |
| Gap | `gap.bin` | Forward-separated update IDs |
| Recovery | `recovery.bin` | Gap, reset, rebaseline, then a post-rebaseline bridge; focused replay ends Synchronized |
| Duplicate/stale | `duplicate_stale.bin` | Established current ID followed by both duplicate and stale ranges |
| Locked/crossed | `locked_crossed.bin` | Actual equal best-side price strings (locked geometry) |
| Decimal boundaries | `decimal_boundaries.bin` | Exact decoded set: empty, zero, fractional, leading-zero, signed, embedded-invalid, large, integer |
| Depth-limit snapshot | `depth_limit_snapshot.bin` | AdapterEnabled; three levels per side, synchronized, depth limit one; focused replay produces and truncates both sides |
| Quality combinations | `quality_combinations.bin` | AdapterEnabled; inbound metadata, distinct host facts, and locked geometry yielding derived CrossedBook |

All categories verified by `bmd_projection_m5_replay_corpus_structural_validate` which
checks that each seed decodes into the intended structural shape.

The three CI regression inputs remain additional corpus seeds and continue protecting the closed
range-domain and baseline/rebaseline-side findings.

## fuzz-smoke integration

`scripts/fuzz-smoke.sh` now builds and runs five targets:

1. `bmd_projection_decimal_parser_fuzz`
2. `bmd_projection_order_book_fuzz`
3. `bmd_projection_book_projection_fuzz`
4. `bmd_projection_proto_adapter_fuzz`
5. `bmd_projection_replay_fuzz` (new)

The replay fuzzer runs `BMD_PROJECTION_REPLAY_FUZZ_RUNS` (default 10,000) inputs against
the checked-in replay corpus directory with `-timeout=5`.

With `BMD_PROJECTION_REQUIRE_FUZZERS=1`, all five must be available and pass.

## Test coverage

### Decoder tests (23 tests)

- Empty input, minimum input, single-byte input
- Core/Adapter mode decoding
- Spot/USD-M market decoding
- All 7 operation types reachable
- uint64 special values (0, 1, 2, UINT64_MAX, etc.)
- NumericSpec bounds clamping
- Per-input operation cap
- Deterministic identical-input decode
- MalformedRange reachability
- Shared InstallBaseline/Rebaseline level cap and stable discarded-level framing
- Snapshot/metadata quality-fact call-site bounds
- Inverted DepthUpdate full-payload framing with a following operation
- Snapshot depth-limit variants

### Harness tests (16 tests)

- Spot Core no-divergence
- USD-M Core no-divergence
- Reset/rebaseline no-divergence
- Snapshot request no-divergence
- Malformed range no-divergence
- Decimal error no-divergence (production/reference agree)
- Gap no-divergence
- Large case no-divergence
- Corrected recovery seed reaches Synchronized
- Spot AdapterEnabled seed produces a real per-side depth-limited snapshot
- USD-M AdapterEnabled no-divergence
- Adapter error leaves projection state unmutated
- Quality seed keeps inbound adapter quality separate from host and Core-derived snapshot quality
- Three preserved CI reproducer regressions

### Corpus structural validator

`bmd_projection_m5_replay_corpus_structural_validate` validates all 10 mandatory seed
categories for structural correctness.

## Existing fuzzers preserved

All four existing fuzz targets are unchanged:
- `bmd_projection_decimal_parser_fuzz`
- `bmd_projection_order_book_fuzz`
- `bmd_projection_book_projection_fuzz`
- `bmd_projection_proto_adapter_fuzz`

## M4-IIR-3

**IMPLEMENTED COVERAGE / CLOSURE PENDING INDEPENDENT REVIEW**

The AdapterEnabled quality seed exercises inbound adapter-observed quality, distinct eligible host
snapshot facts, and Core-derived `CrossedBook` output in one deterministic replay. Focused tests
prove all three domains are present and do not leak into one another. ReplayDriver differential
comparison provides the oracle. Final closure decision belongs to the independent implementation
reviewer.

## OD-M5-003

**NOT CLOSED** — remains SPIKE-RESOLVABLE. Phase 5 differential fuzzing does not close it.

## Production code

`src/**` and `include/**` are **UNCHANGED**. No BookProjection, OrderBook, numeric,
ProtoAdapter, or public API changes.

## No new production dependencies

No external packages, Conan dependencies, hashing libraries, or RNG libraries added.
A local byte cursor/decoder is sufficient.

## Sanitizer compatibility

The final target and all reused M5 support translation units run under libFuzzer coverage + ASan +
UBSan, with compile-command evidence enforced before fuzz execution. No deliberate UB,
out-of-bounds reads,
invalid enum casts, unaligned reinterpret_casts, or signed overflow.

## Phase 6

**NOT STARTED**. No benchmark changes or implementation.

## CI

Expected job count: ~18 (reuse existing fuzz-smoke job, no new Phase-5-specific job).

## Non-goals

- No canonical text parser in fuzz semantic path
- No silent Phase-4 schema mutation: historical observation/manifest V1 meaning is preserved;
  current evidence explicitly uses `M5_SEMANTIC_OBSERVATION_V2` and
  `M5_SEMANTIC_MANIFEST_V2`
- No production code modifications for differential failures
- No custom replay-minimizer subsystem
- No live/external corpus dependency
- No Phase 6 benchmarks

## Files added/modified

### New files
- `cmake/M5Support.cmake` — shared M5 target definitions
- `fuzz/m5/replay_fuzz_decoder.hpp` — structured byte decoder
- `fuzz/m5/replay_fuzz_decoder.cpp` — decoder implementation
- `fuzz/m5/replay_fuzz_fixture.hpp` — structured fuzz fixture builder
- `fuzz/m5/replay_fuzz_fixture.cpp` — fixture builder implementation
- `fuzz/replay_fuzz.cpp` — fuzz target (LLVMFuzzerTestOneInput)
- `fuzz/corpus/replay/*.bin` — 10 mandatory seed files
- `tests/m5/phase5/replay_fuzz_decoder_test.cpp` — decoder tests
- `tests/m5/phase5/differential_fuzz_test.cpp` — harness tests
- `tests/m5/phase5/corpus_structural_validate.cpp` — seed validator
- `scripts/generate_replay_corpus.py` — seed generator
- `scripts/check-m5-fuzz-instrumentation.py` — compile-database instrumentation assertion
- `docs/M5_PHASE5_DIFFERENTIAL_FUZZING.md` — this document

### Modified files
- `tests/CMakeLists.txt` — include M5Support.cmake, add phase5 test targets
- `fuzz/CMakeLists.txt` — add replay fuzz target
- `scripts/fuzz-smoke.sh` — add fifth fuzz target
- `docs/CURRENT_STATE.md` — status sync
- `docs/MILESTONES.md` — status sync
- `README.md` — status sync
- `CHANGELOG.md` — entry
- `docs/M5_DIFFERENTIAL_VALIDATION_AND_PERFORMANCE_DESIGN.md` — status sync
