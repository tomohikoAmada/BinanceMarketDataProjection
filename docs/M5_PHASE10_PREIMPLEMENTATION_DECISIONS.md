# M5 Phase-10 Pre-Implementation Decisions

## Status

```text
PHASE9=COMPLETE / DECISION ACCEPTED
PHASE10=AUTHORIZED / NOT STARTED
DECISION_COMPONENT=P10-PRE-P1-001
```

This record is a later Phase-10 pre-implementation addendum. It records the
owner-authorized publication of the exact accepted materialized medium fixtures
and freezes the reviewed future benchmark consumer contract. It does not claim
that Phase-10 implementation or the weekly performance workflow has started.

## P10-PRE-P1-001 — Medium corpus public distribution authority

### Owner authority

```text
PUBLIC_DISTRIBUTION_AUTHORITY=AUTHORIZED_BY_PROJECT_OWNER
AUTHORIZED_FIXTURES=
  M5-REC-SPOT-BTCUSDT-V1
  M5-REC-USDM-BTCUSDT-V1
RAW_SOURCE_DISTRIBUTION=NO
SOFTWARE_LICENSE_CHANGED=NO
```

The project owner authorizes public hosting and download of these two exact,
already-materialized Replay_V1 fixture packages for project CI,
reproducibility, and characterization. This is a corpus-specific project
distribution policy. It does not distribute the Recorder Raw staging archive,
arbitrary Recorder captures, the full large corpus, unrelated Binance data, or
private Recorder artifacts beyond provenance already present in the accepted
fixtures. It does not resolve the repository-wide software-license question,
make a legal conclusion about third-party rights, or grant broader rights over
unrelated source data.

### Exact fixture identities

| Fixture | Event count | Market | Symbol | NumericSpec | Replay-log SHA-256 |
|---|---:|---|---|---|---|
| `M5-REC-SPOT-BTCUSDT-V1` | 100002 | `Spot` | `BTCUSDT` | price 8 / quantity 8 | `9e9831231192938ac1bd21c90b157ec17e8e2d4e8034131eb21ba57c99b2cc9d` |
| `M5-REC-USDM-BTCUSDT-V1` | 100002 | `UsdMPerpetual` | `BTCUSDT` | price 8 / quantity 8 | `d28ffe19e134e4d5d1c4d57a60762e8884dee676c858587224aebf8afed29afc` |

Both fixtures passed Core differential validation, adapter differential
validation, and medium lifecycle validation. Each has one installed baseline,
one applied bridge, 100,001 applied operations, zero gaps, and final
`SYNCHRONIZED` status. No fixture was substituted or synthetically regenerated.

### Source authority

```text
SOURCE_RUN_IDENTITY=preflight/m21-4-24h-20260805T150930Z/
RECORDER_COMMIT=cf1e749c7a533e916dbfb685212e5549a38c70dd
RECORDER_WHEEL_SHA256=926615b09ef46130f49a87fe8ab20acb7cfa6313daa67af5b718931bd95ff329
RECORDER_CONFIG_SHA256=a399e647faaac58b5db24e835f1c29e799c70ad0c94ec77b597cac2647cfb734
SOURCE_INVENTORY_CATALOG_SHA256=f7289fcc3383063c5e3b83e65201df29503cf7c9bba227dbb8298dcdb4805d8c
MATERIALIZER_VERSION=m5-recorder-materializer-v1
```

The formal source was the validated Recorder M21.4 24-hour run. The 2,887
chunk / approximately 505.6 MB Raw staging set remains source and
materialization authority only and is not included in the public asset.

### Immutable release asset

```text
DISTRIBUTION_SCHEMA=M5_MEDIUM_CORPUS_DISTRIBUTION_V1
PACKAGE_ID=M5-MEDIUM-RECORDED-V1
REPOSITORY=tomohikoAmada/BinanceMarketDataProjection
RELEASE_TAG=m5-medium-corpus-v1
RELEASE_TITLE=M5 Medium Recorded Corpus v1
ASSET_NAME=m5-medium-recorded-v1.tar.gz
RELEASE_TARGET_SHA=041a9e8516409552ea9d6eeada5d58d378c40fc7
OUTER_ARCHIVE_SHA256=5143521fe9728a7c2ce03522b78be4ba2fd91388cdabac800f4a87e970e4adfb
DISTRIBUTION_MANIFEST_SHA256=13e4c37119e26f32c60f64f73565363d51ef58f59245f4a6678f4bf016cdba65
```

The deterministic `tar.gz` contains exactly:

```text
distribution-manifest.json
fixtures/M5-REC-SPOT-BTCUSDT-V1/replay.log
fixtures/M5-REC-SPOT-BTCUSDT-V1/manifest.txt
fixtures/M5-REC-SPOT-BTCUSDT-V1/corpus_provenance.json
fixtures/M5-REC-USDM-BTCUSDT-V1/replay.log
fixtures/M5-REC-USDM-BTCUSDT-V1/manifest.txt
fixtures/M5-REC-USDM-BTCUSDT-V1/corpus_provenance.json
```

The manifest binds the package, release, owner authority, source authority,
fixture identities, and every payload-file SHA-256. It intentionally does not
contain its own recursive hash; the outer archive hash binds the manifest.
The archive was independently rebuilt twice from the same staged bytes with
matching hashes.

Release URL:
`https://github.com/tomohikoAmada/BinanceMarketDataProjection/releases/tag/m5-medium-corpus-v1`

### Publication and acquisition evidence

```text
PUBLIC_RELEASE_ASSET=UPLOADED
RAW_SOURCE_UPLOADED=NO
PUBLIC_ACQUISITION_PROOF=PASS
PUBLIC_DOWNLOAD_PATH=versioned GitHub Release download URL
OUTER_SHA_MATCH=PASS
MANIFEST_VALID=PASS
PAYLOAD_HASHES_VALID=PASS
SPOT_REPLAY_SHA_VALID=PASS
USDM_REPLAY_SHA_VALID=PASS
SPOT_CORE_VALIDATION=PASS
USDM_CORE_VALIDATION=PASS
CLEAN_RUNNER_PROOF=PASS
```

The acquisition proof used a fresh temporary directory and an unauthenticated
public Release download, not the local workspace archive and not Actions
artifact storage.

## Frozen future benchmark consumer contract

This contract is authorized for a later Phase-10 implementation, but is not an
implementation record and does not imply that a workflow exists today.

```text
CURRENT_MEDIUM_ENTRYPOINT_EXISTS=NO
SELECTED_SHAPE=dedicated benchmark-only external recorded Replay_V1 executable
PRODUCTION_API_CHANGED=NO
CORE_REPLAY_EXECUTOR_REUSED=YES
FIXTURE_IO_INSIDE_TIMED_REGION=NO
VALIDATION_BEFORE_TIMING=YES
SCHEDULED_CORE_MODE=Spot + USD-M
ADAPTER_SCHEDULED=NO / manual supporting only
CONTAINER_DIAGNOSTIC_REQUIRED=NO

BENCHMARK_SMOKE_RETENTION=3 days
PERFORMANCE_RETENTION=7 days
SCHEDULED_MEDIUM_TIMEOUT=45 minutes
MANUAL_MEDIUM_TIMEOUT=45 minutes
SCHEDULE_FREQUENCY=weekly
PR_BLOCKING=NO
```

The future operation is:

```text
REQUIRED_PHASE10_WEEKLY_OPERATION=.github/workflows/m5-performance.yml
SCHEDULE_SEMANTICS=weekly
TIER=recorded medium v1
FIXTURES=
  M5-REC-SPOT-BTCUSDT-V1
  M5-REC-USDM-BTCUSDT-V1
MODE=Core current-production std::map
RUNNER=standard GitHub-hosted Ubuntu x86_64
TIMEOUT=45 minutes
PR_BLOCKING=NO
EVIDENCE_CLASS=EXPLORATORY / NONBLOCKING REPORTING
RETENTION=7 days
CONTAINER_REDECISION=NO
PRODUCTION_MIGRATION=NO
```

Until that workflow is actually merged and running, the durable state remains:

```text
WEEKLY_M5_PERFORMANCE=NOT IMPLEMENTED / NOT RUNNING
M5_PERFORMANCE_WORKFLOW_CREATED=NO
PHASE10_STARTED=NO
```

When the later workflow lands, its implementation record plus
`docs/CURRENT_STATE.md` and `docs/MILESTONES.md` must be updated to record
the merged/running weekly-performance state, the exact implemented `SCHEDULE_CRON`, and the
runner, fixtures, mode, timeout, nonblocking evidence class, retention,
container-redecision, and production-migration fields above. The running state
must not be written before the workflow is merged.
