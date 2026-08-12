# M5 Phase 4 Cross-Compiler Semantic Manifests

## Status

- M5 implementation: **IN PROGRESS**
- Phase 1: **COMPLETE / MERGED** (PR #11, merge `5e8629a7ff825f8ea941304d9b09be1670643e8a`)
- Phase 2: **COMPLETE / MERGED** (PR #12, merge `75c619dd683ff2a3893f9535e206231e7bfecc41`)
- Phase 3: **COMPLETE / MERGED** (PR #13, merge `473a907eba2001d18926c57d6c8d16b10c7505be`)
- Phase 4: **IMPLEMENTED / PENDING INDEPENDENT REVIEW**
- Phase 5: **NOT STARTED**
- M6: **NOT STARTED**

Phase 4 is test/tool/CI/documentation infrastructure. `include/**`, `src/**`, production Core,
and production ProtoAdapter are unchanged.

## Scope

Phase 4 implements:

- canonical `OperationObservation` serialization (schema v1)
- semantic observation-stream SHA-256 digest
- portable semantic-result manifest v1 (JSON)
- deterministic Phase-4 manifest producer executable
- same-toolchain process/build-type determinism checks
- Ubuntu GCC, Ubuntu Clang, macOS AppleClang Release manifest production
- GitHub Actions artifact upload
- `m5-semantic-compare` blocking artifact fan-in job
- `m5-replay` blocking Debug/Release determinism job
- fail-closed manifest comparator (Python)
- Phase-4 tests
- Phase-4 documentation/status

## Authority

- M5 architecture: `docs/M5_DIFFERENTIAL_VALIDATION_AND_PERFORMANCE_DESIGN.md`
- ADR-0007: **ACCEPTED**
- ADR-0008: **ACCEPTED**
- OD-M5-003: **SPIKE-RESOLVABLE** (not closed)

## Canonical Observation Schema v1

Schema identifier: `M5_SEMANTIC_OBSERVATION_V1`

Each `OperationObservation` is serialized as a deterministic text record:

```text
OBS <event_index> <EventKind>
  RESULT <OperationResultValue>
  CHECKPOINT <SemanticCheckpoint>
  SNAPSHOT <SnapshotOutcome> | NONE
  DECIMALS <count> <decimal_observations>  # only when non-empty
```

### Integer rules

All integer values serialized as canonical base-10 ASCII:
- No locale formatting, no leading `+`, no leading zeros except zero itself
- Signed negatives use exactly one leading `-`
- Boolean: `true`/`false`

### Enum rules

Every enum value has an explicit stable name mapping. Examples:
- `Applied`, `IgnoredStale`, `Synchronized`, `Spot`, `UsdMPerpetual`
- Enum values are case-sensitive, no locale-dependent parsing
- Unknown/unhandled values produce `UNKNOWN_<type>_<int>` to fail closed

### Optional-value rules

Optional values are `none` when absent or `some <value>` when present. Presence is always
explicit.

### Variant/result rules

Every variant alternative has an explicit type tag:
- `DecimalErrorOutcome`, `InstallOutcome`, `ApplyOutcome`, `AdapterErrorOutcome`,
  `AdapterSuccessOutcome`, `SnapshotOutcome`, `SnapshotNotProducedOutcome`,
  `ResetOutcome`, `RangeOutcome`, `MetadataOutcome`

### Vector rules

Every vector encodes:
- element count
- elements in semantic order

Vectors include: decimal observations, checkpoint bids/asks, quality flags, snapshot levels.

### String rules

Strings use byte-length prefix: `<length>:<bytes>`. This is unambiguous for all UTF-8 content
including delimiter and special characters.

### Record framing

Each observation is a self-contained text record ending with `\n`. The `OBS` marker prevents
concatenation ambiguity. The SHA-256 digest concatenates all canonical observation records
(including their trailing newlines).

### Complete field coverage

Every field currently present in `OperationObservation` contributes to its canonical representation:

| Category | Fields |
|---|---|
| Event identity | `event_index`, `event_kind` |
| Decimal observations | All `CanonicalDecimalObservation` entries (side, level_position, role, result) |
| Decimal result | `CanonicalDecimalValue` (units, storage_scale, source_fraction_digits) or `CanonicalDecimalFailure` (category, offset) |
| Install result | `disposition`, `status_after`, `last_update_id_after` |
| Apply result | `disposition`, `status_after`, `last_update_id_after`, `gap` (all CanonicalGapEvidence fields) |
| Adapter error | `code`, `field`, `decimal_error` |
| Adapter success | underlying core result, `observed_quality` (canonical rank order) |
| Snapshot | `policy`, `symbol`, `producer`, `producer_version`, `source`, `generated_time_utc_ns`, `generated_monotonic_ns`, `last_update_id`, `synchronized`, `bids`, `asks`, `quality_flags`, `depth_limit`, `gap_descriptor` |
| Checkpoint | `status`, `last_update_id`, `last_gap`, `synchronized_visible`, `bids`, `asks`, `price_scale`, `quantity_scale` |
| Reset, Range, Metadata | Full outcomes with all fields |

## Semantic Digest

SHA-256 of the concatenated canonical observation records:

```text
SHA-256(canonical_obs_0 || "\n" || canonical_obs_1 || "\n" || ... || canonical_obs_N || "\n")
```

The digest is lowercase 64-character hex. It reuses the existing test-only SHA-256 implementation
in `tests/m5/replay/canonical_text.hpp` (`replay::sha256_hex`).

Digest is computed only after production/reference differential equality is verified for all
events. Differential divergence produces no manifest.

## Workload Set

Four mandatory small workloads using the existing Phase-3 deterministic generator:

| Workload ID | Market | Mode | fixture_hash |
|---|---|---|---|
| `m5-small-core-spot-v1` | Spot | Core | From deterministic generator |
| `m5-small-core-usdm-v1` | USD-M | Core | From deterministic generator |
| `m5-small-adapter-spot-v1` | Spot | Adapter | From deterministic generator |
| `m5-small-adapter-usdm-v1` | USD-M | Adapter | From deterministic generator |

Each workload uses the same underlying replay fixtures as Phase 3:
- `make_spot_small_workload()` (2,048 events, seed `548746690337`)
- `make_usdm_small_workload()` (2,048 events, seed `548746690337`)

The fixture_hash is the canonical replay-log SHA-256.

## Toolchain Metadata

Derived from CMake definitions compiled into the manifest producer:

```text
CMAKE_CXX_COMPILER_ID        -> compiler
CMAKE_CXX_COMPILER_VERSION   -> compiler_version
CMAKE_SYSTEM_NAME            -> os
CMAKE_SYSTEM_PROCESSOR       -> architecture
$<CONFIG>                    -> build_type
```

HEAD SHA is supplied as the required `--head-sha` CLI argument.

In CI the evidence SHA policy is event-aware:

- `pull_request`: exact PR Head SHA (`github.event.pull_request.head.sha`)
- `push`: pushed commit SHA (`github.sha`)

Semantic evidence jobs explicitly checkout and verify the same commit recorded in the manifest.

The invariant is:

```text
manifest head_sha == semantic producer checkout HEAD == EVIDENCE_SHA
```

## Fixture-Set Identity

Deterministic SHA-256 derived from canonical workload ordering:

```text
SHA-256(workload_id_0 "\n" fixture_id_0 "\n" fixture_hash_0 "\n"
        workload_id_1 "\n" fixture_id_1 "\n" fixture_hash_1 "\n"
        ...)
```

Fixed workload order: Core Spot, Core USD-M, Adapter Spot, Adapter USD-M.

## Manifest Format

JSON, schema `M5_SEMANTIC_MANIFEST_V1`:

```json
{
  "schema_version": "M5_SEMANTIC_MANIFEST_V1",
  "head_sha": "<40-char hex>",
  "toolchain": {
    "compiler": "<compiler_id>",
    "compiler_version": "<version>",
    "os": "<system_name>",
    "architecture": "<processor>"
  },
  "build_type": "Release",
  "fixture_set_id": "<64-char hex SHA-256>",
  "workloads": [
    {
      "workload_id": "<string>",
      "fixture_id": "<string>",
      "fixture_hash": "<64-char hex>",
      "semantic_digest": "<64-char hex>"
    }
  ]
}
```

Rules:
- Fixed field order
- Fixed workload order
- Lower-case SHA-256 hex
- LF line endings, final newline
- No timestamps, no random UUID, no temp paths
- No GitHub run ID in semantic identity

## Manifest Producer

Executable: `bmd_projection_m5_semantic_manifest`

CLI:
```text
--output <path>    required: output manifest JSON path
--head-sha <sha>   required: exact HEAD SHA (or "LOCAL" for non-CI)
```

Behaviour:
1. Generates Spot and USD-M small workloads
2. Runs Core-only replay for each (RetainAll)
3. Runs Adapter-enabled replay for each (RetainAll)
4. Fails on any differential divergence
5. Serializes all OperationObservations to canonical text
6. Computes SHA-256 semantic digests
7. Computes fixture-set identity
8. Writes manifest JSON to output

Exit codes:
- 0: success, manifest written
- 1: failure (divergence, I/O error, invalid arguments, digest failure)

## Comparator

Script: `scripts/compare-m5-semantic-manifests.py`

No external PyPI dependencies. Python standard library only.

CLI:
```text
--expected-head SHA    required in CI: expected HEAD SHA
MANIFEST...            paths to manifest JSON files
```

Cross-compiler mode (three Release manifests from different toolchains) verifies:
- exactly three expected manifests
- recognized schema v1
- HEAD SHA identical and matches expected
- build_type == Release for all
- fixture_set_id identical
- workload ID set identical, no duplicates
- fixture_id identical per workload
- fixture_hash identical per workload
- semantic_digest is valid lower-case SHA-256
- semantic_digest identical per workload

Fail-closed on any mismatch with stable diagnostics.

## CI Transport

### Producer jobs (build-matrix, Release)

Each Release build-matrix job:
1. Builds with ProtoAdapter enabled
2. Runs normal Release tests
3. Runs manifest producer
4. Uploads manifest artifact with deterministic name

Artifact names:
- `m5-semantic-manifest-ubuntu-gcc-<short-sha>`
- `m5-semantic-manifest-ubuntu-clang-<short-sha>`
- `m5-semantic-manifest-macos-appleclang-<short-sha>`

Retention: 3 days.

### m5-replay job (Ubuntu Clang)

Builds Debug and Release with ProtoAdapter enabled.
- Runs manifest producer twice for Debug (separate process repeatability)
- Runs manifest producer twice for Release
- Compares Debug run 1 == Debug run 2
- Compares Release run 1 == Release run 2
- Compares Debug run 1 == Release run 1 (build-type determinism)
- Verifies fixture IDs/hashes identical across all runs

### m5-semantic-compare job

Depends on build-matrix. Runs with `if: always()`.
- Downloads three Release artifacts
- Runs comparator
- Uploads comparison report artifact
- Fails if any requirement not met

## Tests

### Serializer tests (`canonical_observation_test.cpp`)

Covers:
- Every enum mapping
- All 10 OperationResultValue alternatives
- Optional absent/present
- Empty/non-empty vectors
- Ordered levels
- Gap evidence
- Adapter errors
- Adapter success
- All dispositions and statuses
- SnapshotOutcome fields
- String handling
- Deterministic repeat serialization

### Digest tests (`digest_test.cpp`)

Proves:
- Same observation stream → same digest
- Mutation → different digest
- Event order change → different digest
- Observation omission → different digest
- Event kind change → different digest
- Disposition change → different digest
- Digest is lowercase hex SHA-256
- Deterministic repeat digest

### Manifest tests (`manifest_test.cpp`)

Proves:
- Valid JSON rendering
- Multiple workload entries
- JSON string escaping
- Fixture-set ID determinism
- Fixture-set ID rejects unordered input
- Schema version frozen

### Comparator tests (`test_compare_manifest.py`)

Positive: three matching manifests pass.

Negative (each exits nonzero):
- Missing manifest
- Invalid JSON
- Unknown schema version
- HEAD mismatch
- Unexpected HEAD
- Non-Release manifest
- Fixture-set mismatch
- Missing workload
- Extra workload
- Duplicate workload ID
- Fixture ID mismatch
- Fixture hash mismatch
- Invalid digest syntax
- Digest mismatch

## Non-Goals

- Phase 5 replay differential fuzzer
- New fuzz corpus categories
- Phase 6 benchmarks
- New performance thresholds
- Phase 7 allocation/live-byte instrumentation
- Phase 8 container spike
- Production code changes
- Recorder or Contracts modification
- Medium corpus in PR CI
- M6 Gateway

## File Layout

```text
tests/m5/semantic/
  canonical_observation.hpp          serializer declarations
  canonical_observation.cpp          serializer implementation
  semantic_digest.hpp                digest declarations
  semantic_digest.cpp                digest computation (reuses replay::sha256_hex)
  semantic_manifest.hpp              manifest model declarations
  semantic_manifest.cpp              manifest JSON rendering, fixture-set identity
  manifest_producer.cpp              producer executable (main)
  canonical_observation_test.cpp     serializer unit tests
  digest_test.cpp                    digest unit tests
  manifest_test.cpp                  manifest model tests
  test_compare_manifest.py           comparator test suite (Python unittest)

scripts/
  compare-m5-semantic-manifests.py   fail-closed comparator

tests/CMakeLists.txt                 added: semantic support library, test target, producer, comparator tests
.github/workflows/ci.yml            added: m5-replay job, m5-semantic-compare job, artifact upload
docs/
  M5_PHASE4_CROSS_COMPILER_SEMANTIC_MANIFESTS.md   this document
  CURRENT_STATE.md, MILESTONES.md, CHANGELOG.md     updated status
```

## Review Status

```text
Initial implementation:
IMPLEMENTED / PENDING INDEPENDENT REVIEW

NOT MERGED
KEEP PR DRAFT
```

Do not merge until independent implementation review.
Do not start Phase 5.
