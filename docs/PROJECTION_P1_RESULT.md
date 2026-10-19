# Projection P1 — four-product cost attribution

2026-10-18. Scope: the owner-provided v0.3 Projection P1, sharing Gateway's
four-product production path. The offline cost-attribution campaign is complete; final result review and CI delivery
are recorded below. The subsequently revised v0.4 plan adds representative 20-key
mixed-load attribution; the owner authorized that increment on 2026-10-19. Its
[separate twenty-key record](PROJECTION_P1_TWENTY_RESULT.md) preserves this four-product campaign. Target-VPS budget acceptance remains with Gateway G3.
Core, ProtoAdapter, sequence policy, public APIs, dependencies and `KEEP_STD_MAP`
are unchanged. P2 optimization is not implemented.

- [x] Read all six Milestone Plan documents and relevant repository authorities.
- [x] Reuse Gateway G2 JSON/metadata/configured owners/Projection/Book+Event RPC
  fixture, with HTTP/WS replaced only at the existing external-I/O seam.
- [x] Optional trace records now distinguish inbound adaptation, checked apply,
  post-apply owner work, subscriber wait/materialization and synchronous Write.
  Configured export carries each exact product identity, including ETH.
- [x] M5 allocation measurement: 48 accepted M3 cells, 22 Adapter cells and four
  replay cells; three repetitions; existing provenance/invariant validators pass.
- [x] Paired normal/instrumented Release: 18 successful process trials, three scenarios,
  four products, complete ordered Book/Event delivery; representative/replay evidence.
- [x] Fresh independent agent approved the implementation and measurement method;
  corresponding Release/sanitizer/format and Python validation passed.
- [x] Fresh independent agent approved the final 18-trial result, raw hashes,
  trace reanalysis, units, quantile aggregation and conditional decision.
- [x] Technical commit `248c622` exact-head GitHub CI delivery: 20/20 PASS.
- [x] Optimization disposition: hotspot confirmed; P2 stays CONDITIONAL pending
  approved target/shared-host budgets. No production optimization or OUT claim.

## Measurement boundaries

Four keys: Spot BTCUSDT/ETHUSDT and USD-M BTCUSDT/ETHUSDT. Each has its own
production owner/private Projection and one Book plus one Depth Event gRPC
subscription. The benchmark reuses the G2 fixture; it is not a second Gateway
implementation. Synthetic JSON updates mutate existing bid quantities in a fixed
book, using the real market-specific parsers and Spot successor/USD-M pu policies.
The workload is controlled, not a claim of recorded ETH market behavior.

Each run warms up 32 updates/product outside the measurement interval. Frames are
prepared before timing. The paired executable configuration is Release, the same
compiler/Conan toolchain and sanitizers off; instrumentation is the only build
option changed. Repeated OFF/ON order alternates to expose observer cost. No
numeric pass threshold is introduced.

- T0→T1: JSON parse/normalized message construction after the fixture's input
  timestamp; no real socket/TLS read.
- T1→Q: Event publication plus ingress submission/copy.
- Q→T2: ingress queue/wake/dequeue.
- T2→adapted: inbound Adapter plus small owner dispatch/measurement overhead.
- adapted→T3: `AdaptedDepthBatch::apply_to`; includes checked binding and its
  internal vector work plus the Core transaction. It is **not pure Core time**.
  Existing M5 accepted `BookProjection::apply` cells supply separate Core evidence.
- T3→owner_done: post-apply observation, Book publication and state bookkeeping.
- T3→T4-book / T1→T4-event: publication through subscriber queue commit.
- T4→write_begin: subscriber queue/wake and envelope materialization.
- write_begin→T5: synchronous server `Write` return; not client processing.
- T0→client receipt: separate normal/instrumented loopback latency observation.

Thread CPU is the sum of per-update T2→owner_done CPU intervals for each owner;
it excludes mailbox/dequeue, waiting, startup, snapshots and other owner work.
Whole-process CPU **includes the embedded producers/parsers and loopback clients**
as well as the Gateway graph. Its window runs from release of measured producers
through confirmation that all measured client receipts arrived, including final
drain/polling confirmation. RSS is the process high-water through this load-end
sample, including setup/warmup, prebuilt JSON, clients and optional preallocated
trace/delivery buffers; subsequent shutdown/export peaks are excluded. These are
not isolated daemon CPU or order-book resident-byte measurements. Host CPU,
OS-reported available memory and swap activity are sampled externally at 100 ms; they describe
this host during the experiment, not future capacity or target-VPS reserves.

Allocation is measured separately with the existing single-threaded M5 Phase-7
replaceable-global-new instrumentation, never inside the multithreaded timing
campaign. It reports requested bytes/counts, not malloc arenas or RSS. Its
representative and replay workloads have their own identities; their results are
not subtracted from Gateway timing as an exact decomposition.

## Results

Local host: Apple M3, eight logical/physical cores (heterogeneous, no affinity),
macOS 27.0.1 arm64, 24 GiB RAM, AppleClang 21 / libc++. Gateway uses its pinned
Projection `555dd1f4` package; the separate M5 executables use clean Projection
`95fc9181`. Their production `src/` and `include/` trees are identical. Contracts
message source is `518880b`, RREV `53713fb82c27cf6ea0395f6b8a853006`.

The Gateway measured source is base `6927acf` plus the retained patch and
measurement driver, subsequently committed as `255eaea`. Its source stayed
unchanged throughout the campaign. Binary SHA-256:

- OFF: `c43f6a324153f9edbe7c26fde8970216063a37c00a0adbf741657ad6560c8bb1`
- ON: `88d771fa907571561310225124758ff343d43f08e8c378e364eaab8aa781dde2`

All 18 runs passed: **55,200 measured updates and 110,400 client deliveries**
across OFF/ON; ON retained **27,600 Applied traces / 55,200 successful T5s**.
Warmup, bootstrap and seed operations are excluded from those denominators.
No Gap, final-ID mismatch, missing/reordered update, Full/terminalization,
trace/delivery overflow or clock error occurred. Maximum measured ingress /
Book / Event occupancies were 7 / 7 / 8 against capacity 64.

Ranges below are across three processes. Book p99 is the **worst product's
client p99 in each process**, then its min/max across processes; it is not a
pooled quantile. CPU includes the embedded clients/fixture. RSS is the process high-water
through load completion, including setup/warmup; shutdown/export is excluded.

| Depth/side, batch, rate/product | Mode | Process CPU cores min–max (mean) | Peak RSS MiB min–max | Book client p99 ms min–max |
| --- | --- | --- | --- | --- |
| 1000, 10, 10Hz | OFF | 0.055–0.058 (0.057) | 18.50–18.58 | 3.189–5.879 |
| 1000, 10, 10Hz | ON | 0.049–0.058 (0.053) | 40.47–40.55 | 2.940–3.505 |
| 5000, 10, 100Hz | OFF | 0.892–0.945 (0.925) | 27.47–29.63 | 4.179–4.534 |
| 5000, 10, 100Hz | ON | 0.866–0.935 (0.908) | 53.09–55.77 | 4.233–5.109 |
| 5000, 100, 100Hz | OFF | 0.978–0.993 (0.983) | 32.81–34.00 | 3.918–4.424 |
| 5000, 100, 100Hz | ON | 0.965–0.992 (0.982) | 55.17–55.94 | 4.040–6.633 |

The instrumentation's roughly 22–28 MiB extra peak RSS is retained, not
subtracted. CPU/latency ordering varies across paired trials; these runs do not
resolve a reliable observer CPU factor or demonstrate zero observer cost.

The four owners' measured update-segment CPU sums averaged 0.018 / 0.719 /
0.714 cores across the three ON scenarios. These are update-cohort segment CPU
sums normalized to the receipt-confirmation wall window; small boundary-tail
work may cross it, so they are not a precisely additive process-CPU decomposition.
The checked-call span accounted for
94.7% / 99.1% / 98.2% of measured owner update wall time. Arithmetic means below
cover equal-size product/process trials; they are not pure-Core decomposition.

| ON stage mean, µs/update | D1000/B10/10Hz | D5000/B10/100Hz | D5000/B100/100Hz |
| --- | ---: | ---: | ---: |
| Adapter before checked call | 8.3 | 5.1 | 10.3 |
| Checked apply | 487.8 | 1863.3 | 1826.1 |
| Post-apply owner work | 18.9 | 11.1 | 22.3 |
| Ingress queue/wake | 114.0 | 92.2 | 104.1 |
| Book subscriber wait/materialization | 234.9 | 136.1 | 87.3 |
| Book synchronous Write | 169.8 | 48.9 | 39.2 |

The ON Book Write per-product/trial p99 maxima were 986 / 252 / 255 µs;
ingress queue/wake p99 maxima were 919 / 2121 / 2279 µs. All per-product
branch metrics and trial ranges are retained in the machine-readable evidence.

### Core and allocation attribution

Existing M5 accepted `BookProjection::apply` cells, five timing repetitions:

| Per-side depth | Spot B10 mean | USD-M B10 mean | B1..B100 observation |
| --- | ---: | ---: | --- |
| 1000 | 72.8 µs | 73.0 µs | roughly 72–74 µs |
| 5000 | 408.5 µs | 406.0 µs | roughly 406–410 µs |

Existing-price B1/B10/B100 had identical per-operation allocation metrics at
each depth: **2023 allocations / 161,560 requested bytes** at D1000;
**10,029 / 1,004,312 bytes** at D5000, for both markets. Transient requested
peaks above entry were 128,824 / 742,200 bytes; persistent live delta was zero.
All three allocation repetitions agreed. This is direct evidence of full-book
transaction work scaling with book depth while batch size changes little.
It does not count Gateway's concurrent malloc arenas, queues or RPC allocations.

The four concurrent owners' checked-call wall times are substantially higher
than the separate single-thread M5 cells. These are different workloads and
scheduling/allocation environments; no universal multiplier or exact causal
allocator-contention claim is made.

The existing small representative replay passed five latency passes per market,
10,240 samples each: Spot p99 16,417 ns; USD-M 13,250 ns. Timer calibration p99
42 ns was retained and not subtracted. These lifecycle/mixed-operation samples
are not the D5000 accepted-only latency distribution or an ETH recording.
The separate 48/22/4 allocation inventories passed the existing Phase-7 validators.

The existing accepted recorded BTC medium fixtures were also replayed, three
measured repetitions each, with clean source/binary/fixture/checksum binding:
100,002 logical events per fixture. Spot throughput was 6512–6614 events/s
(15.12–15.36 s/pass); USD-M was 1152–1154 events/s (86.69–86.84 s/pass).
The existing Phase-10 exact-five-file validator passed. These are distinct
recorded Core-only workloads, not four-product RPC rates or a comparison
between exchange markets at equal input/depth. Their payloads/wrappers and
canonical hashes are retained. The original unpinned-frequency/affinity
warnings are retained; measurements are local exploratory evidence.

### Host headroom and decision

Host available memory stayed at least 6.63 GiB. Sampled host busy-core averages
were roughly 1.04–2.36 across trial windows on this eight-core machine. Host
swap-in activity was present; no swap-out was observed. Those counters include
other applications, and do not prove this benchmark had zero swap faults or
that the target four-core VPS has equivalent headroom.

**Full-book copying is the dominant accepted-update hotspot and a plausible
optimization target.** Four products sustained every tested finite load with
complete delivery. The result does not establish that copying prevents the
actual four-product production load or consumes approved future reserves:
target hardware, Recorder/Archive co-load, live input distribution and budgets
were not measured here. **P2 remains CONDITIONAL, not triggered and not OUT.**
Keep Core/Adapter and `std::map` unchanged. Gateway G3 can use these workload
identities/metrics for its shared-host budget decision; optimize only if that
evidence requires it.

### Evidence and validation

[Condensed measured evidence](evidence/projection-p1-local-20261018.json) and
[all paired trial summaries](evidence/projection-p1-trials-20261018.json) retain
actual values, source/diff and binary identities. Raw traces, external samples,
M5 payloads/wrappers, source patch and logs are retained in the local evidence
archive identified below.

Local archive: `build/projection-p1-local-20261018.tar.gz` (26792649 bytes),
SHA-256 `04880ece886dc730686400433c18fc7098d2f62023177e11474e050ab0a43a51`. It includes a 151-file member SHA-256 manifest,
raw paired evidence, M5 outputs/wrappers, measured binaries, source patch and
validation/build logs. Reproduction still uses the repositories and pinned packages.

| Validation | Result |
| --- | --- |
| Gateway OFF / ON Release, historical synthetic+G8 enabled | 24/24 / 25/25 PASS |
| Gateway ASan+UBSan production | 23/23 PASS; final driver/assertion increment 2/2 PASS |
| Gateway TSan affected runtime/RPC/P1/instrumentation/four-product groups | 5/5 PASS |
| Gateway Linux/GCC13 exact head `255eaea`, CI `39141119116` | all ordinary jobs PASS; production 22/22 |
| Gateway format / both repositories diff check | PASS |
| Projection technical commit `248c622`, CI `39142012653` | 20/20 PASS, including canonical Quality, ASan/UBSan, fuzz and M5 |
| Projection trace analyzer tests (valid attribution, bad evidence, nearest-rank quantiles) | 3 tests, adversarial subcases PASS |
| M5 Phase-7 validators | 48 Core + 22 Adapter + 4 replay records PASS |
| M5 representative wrapper / two latency validators | PASS, source/binary/payload binding |
| Accepted recorded BTC medium fixtures, three repetitions/market | Phase-10 exact-five-file/source/fixture/binary/checksum PASS |
| Fresh independent agent implementation/method + final main-campaign result review | APPROVED; no actionable findings |

Sanitizer graphs omit the two historical synthetic/G8 groups included by the
Release graphs; pinned upstream static packages were not newly instrumented.
Projection production C++/toolchain is unchanged, so the full
local verify/Quality matrix was not manually repeated for this Python/docs work;
automatic exact-head CI subsequently passed all 20 jobs, including canonical Quality.
No check was disabled. Initial compile-time metadata accessor mistakes and an
obsolete instrumented-preflight test expectation were corrected before measured
binaries were frozen; failed build/test logs are retained.

## Reproduction

Use the Gateway branch/commit recorded with the completed evidence. With its
existing pinned Conan binary toolchain, configure production Release twice with
`BMD_GATEWAY_ENABLE_PERFORMANCE_BASELINE_INSTRUMENTATION=OFF` and `ON`, and build
`bmd_gateway_projection_p1_measurements`. Normal production tests retain their
ordinary configuration; `gateway_projection_p1_smoke` checks the fixture cheaply.

From Projection, use repository-local Python:

```sh
PIP_CACHE_DIR="$PWD/.cache/pip" .venv-tools/bin/python -m pip install psutil==7.2.2
.venv-tools/bin/python tests/test_projection_p1.py
.venv-tools/bin/python scripts/measure-projection-p1.py \
  --gateway-source ../BinanceMarketDataGateway \
  --normal ../BinanceMarketDataGateway/build/p1-off/bmd_gateway_projection_p1_measurements \
  --instrumented ../BinanceMarketDataGateway/build/p1-on/bmd_gateway_projection_p1_measurements \
  --output build/p1-paired-results
```

The driver validates build compatibility, exact four-product identities/counts,
Applied outcomes, complete correlation, chronology, zero overflow/full/clock
errors and final IDs. It retains every trial, raw trace/resource sample and binary
SHA-256. It records source HEAD, diff/untracked file hashes and rejects source
changes during the campaign. Source identity describes the checkout; retained
build logs and binary hashes bind the actual executable.

## Review and delivery

A fresh independent agent approved both implementation/method and the main
campaign result. It independently matched all 18 summaries with raw payloads,
resource files and nine trace hashes; re-ran trace analysis; reconstructed all
update/delivery/T5 counts, CPU/RSS conversions, worst-product p99 ranges and
stage aggregation. No actionable findings remained. It did not repeat the
performance workload. The recorded-medium supplement was independently approved as well: its five-file
contract, hashes, clean source/binary identity and all six repetition time/throughput
conversions matched. Final review: **APPROVED, no actionable findings**.

Gateway support: [PR #38](https://github.com/tomohikoAmada/BinanceMarketDataGateway/pull/38),
commit `255eaeaa82ad576afedc929223f243638dfe8b88`.
[Exact-head CI](https://github.com/tomohikoAmada/BinanceMarketDataGateway/actions/runs/39141119116)
passed all seven ordinary jobs, including Linux production 22/22; the manual
binary-preparation job was skipped as intended. No upstream cold build or
Binance connection was needed.

Projection tooling: [PR #55](https://github.com/tomohikoAmada/BinanceMarketDataProjection/pull/55).
Four-product technical commit `248c622699dc6838553a5d1cc087a598097cb8f1`
passed [all 20 CI jobs](https://github.com/tomohikoAmada/BinanceMarketDataProjection/actions/runs/39142012653).
The subsequent twenty-key increment is tracked separately.

## Decision boundary

There is no approved CPU/RSS/p99 or future-resource reservation budget. This P1
can establish costs and identify the full-book-copy hotspot; it cannot declare
that a four-core VPS with Recorder/Archive meets its budget. Gateway G3 owns
that separately authorized shared-host/network qualification. No VPS was
accessed, no Recorder service/configuration changed, and no API-limit acceptance
was attempted. Until the combined measured budget exists, P2 is conditional;
neither a required optimization nor an OUT decision is justified by a local
microbenchmark alone.
