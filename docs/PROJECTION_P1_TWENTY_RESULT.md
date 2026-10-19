# Projection P1 — twenty-product offline increment

2026-10-19. The owner explicitly extended the original four-product request to
the v0.4 twenty-key offline attribution increment. The existing
[four-product results](PROJECTION_P1_RESULT.md) and raw archive remain intact.

- [x] Reuse Gateway's existing production configuration, metadata, private owners,
  Projection and Book/Event/Status handlers; finite product bound 20, reject 21.
- [x] Existing integration source also verifies twenty products, all-initial-Live
  startup/rollback, identity/decimal precision, product-local recovery, slow
  consumers, 40 baseline streams and the unchanged process-global 48-context cap.
- [x] Freeze official metadata and 24-hour ticker responses for ten research
  symbols per market; independent raw hashes and selected-field mapping review.
- [x] OFF 26/26, ON 27/27 Release; paired twenty-key smoke and four analyzer tests.
- [x] Fresh independent code/method review APPROVED, no actionable findings.
- [x] ASan+UBSan 25/25, focused TSan 6/6, final metadata addition 1/1 each;
  Gateway source frozen at `8ce41be` and matched OFF/ON binaries recorded by runner.
- [x] Eighteen mixed-load paired trials and machine-readable summaries retained.
- [x] Independent quantitative/raw-evidence review PASS; all hashes, heterogeneous
  counts, weighted stages, p99 and maximum-tail observations verified.
- [x] Local raw evidence archive: 107 data-member SHA-256 checks and tar readback PASS.
- [x] Reviewed tooling/results delivered through Projection PR #55 and Gateway PR #38;
  current automatic CI state is visible in their checks.

## Workload and limits

The research sample is BTC, ETH, BNB, SOL, XRP, DOGE, ADA, AVAX, LINK and LTC
against USDT in each of Spot and USD-M perpetual. This is not an approved final
deployment list. One Book and one Depth Event RPC per key gives 40 concurrent
streams, within the unchanged 48-context limit.

Gateway retains selected official exchangeInfo and ticker objects in
`tests/fixtures/projection-p1-twenty.json`, with raw-response SHA-256 and source
URLs. All selected products were TRADING when fetched. Prices and quantities
use exact integer multiples of the recorded tick/step; no floating-point price
or quantity representation is introduced. All benchmark runs are offline.

For each market let c be the symbol's actual 24-hour trade count and C the
maximum among the ten selected symbols. Normal workload uses per-side depth
500 + floor(1000*c/C), batch 4 + floor(36*c/C), and rate
2 + floor(8*c/C) updates/s for 20 seconds. Deep workload triples each depth
and multiplies each rate by ten for 12 seconds. Batch workload additionally
triples each batch size. The profile records exact counts and integer intervals.
These are deliberately heterogeneous controlled synthetic depth workloads:
24-hour trade count is an activity-tier proxy, **not observed depth frequency**.
It does not reproduce twenty recorded order books or claim live traffic coverage.

Synthetic JSON updates change existing bid quantities using real market-specific
parsers, each product's own sequence IDs and USD-M pu. Each product warms up
32 updates before the measurement window. Source/configuration, workload,
binary hashes, normal/instrumented alternating order and three repetitions
are retained. CPU/RSS and trace boundaries follow the four-product report:
whole-process CPU includes embedded producers/parsers and loopback clients;
RSS is the high-water through load completion including setup/warmup, excluding
later shutdown/export. Core/Adapter production code and package pins are unchanged.

P2 remains conditional on a measured shared-host budget. This offline increment
does not complete Gateway G3's BMD-RL-01, shared-VPS/Recorder co-load, deployment
or production SLA acceptance.

## Measured results

Same local Apple M3/macOS 27.0.1 arm64, eight heterogeneous cores, 24 GiB RAM,
AppleClang 21/libc++ host as the original campaign. No affinity/frequency lock.
Gateway measured commit `8ce41bedf1d3da649d753b04af7a482172cf93c8` stayed clean
and unchanged during all eighteen runs. Projection production package remains
`555dd1f4`; Core/Adapter source and dependency pins did not change.

Binary SHA-256:

- OFF: `1bae8256b2f6182c5e4ac1f89652b1007d5033c67ea36aac0485c52fdc560556`
- ON: `e4b5c8b712d6c42b91a35fa895c570401b0f17f5b8b24f17dd9ee9d590b90a63`

All eighteen process trials passed: **127,920 measured updates / 255,840 client
deliveries**. ON retains **63,960 Applied traces / 127,920 successful T5s**.
No Gap, missing/reordered delivery, final-ID mismatch or Full occurred; ON
has no trace/delivery overflow or clock error. Owner/transport/context shutdown
checks passed. Seed/bootstrap and 32 warmup updates/key are outside these counts.

Each table cell gives the range across three processes; CPU also gives its mean.
Book p99 is each process's worst product p99, then the range across processes,
**not a pooled quantile**. Normal samples/key are 40–200 and deep/batch samples
240–1200; at 40 samples nearest-rank p99 equals the maximum. Different-rate
products do not have equal sample sizes.

| Scenario | Mode | Process CPU cores min–max (mean) | RSS MiB min–max | Worst-product Book p99 ms min–max | Delivered updates/s min–max |
| --- | --- | --- | --- | --- | --- |
| normal | OFF | 0.107–0.127 (0.115) | 26.83–26.88 | 4.466–8.443 | 82.3–82.4 |
| normal | ON | 0.104–0.113 (0.110) | 137.50–137.70 | 6.237–29.901 | 82.4–82.4 |
| deep | OFF | 1.176–1.216 (1.192) | 41.41–41.81 | 3.445–7.611 | 820.2–820.3 |
| deep | ON | 1.144–1.231 (1.178) | 151.83–152.91 | 3.638–10.426 | 820.4–820.5 |
| batch | OFF | 1.189–1.277 (1.242) | 53.02–53.80 | 3.600–40.471 | 820.2–820.4 |
| batch | ON | 1.199–1.267 (1.222) | 163.80–164.53 | 3.672–6.746 | 820.2–820.5 |

Normal offers about 82 updates/s across twenty keys (1640/process); deep/batch
offer about 820 updates/s (9840/process). Normal depths/side range 531–1500,
batches 5–40. Deep/batch depths range 1593–4500; deep batches 5–40 and high
batches 15–120. All delivered-throughput denominators use the actual
receipt-confirmation wall window, including final drain.

The normal ON repetition 3 Spot ADA sample has p99 **29.901 ms** (40 samples),
and batch OFF repetition 3 USD-M LINK has p99 **40.471 ms** (360 samples).
The largest observed Book **maximum** was **102.404 ms** in deep OFF repetition
3 USD-M LINK; several other keys in that run had roughly 97–102 ms maxima.
Those are input-to-client-receipt observations, not server Write latency.
These observations are retained, not dropped or replaced by more favorable
repeats. Background/scheduling causality is not established. These three-repeat
windows do not prove a stable tail-latency SLA or an observer CPU factor.
Instrumentation adds about 110–111 MiB of RSS in this twenty-buffer configuration;
no attempt is made to subtract it.

### Owner/adapter/publication attribution

Means below are **weighted by measured update counts** across all products and
three ON processes, unlike equal-product averaging. Checked apply includes
`AdaptedDepthBatch::apply_to` binding/vector work plus Core, not pure Core.
Owner update-segment CPU means were 0.047 / 0.824 / 0.812 cores (normal/deep/batch),
normalized to each process receipt window; they are cohort costs, not a precisely
additive decomposition of whole-process CPU.

| ON stage, update-weighted mean µs | normal | deep | batch |
| --- | ---: | ---: | ---: |
| Parse/normalize | 82.3 | 42.6 | 68.8 |
| Event publish + ingress submit | 21.8 | 10.9 | 13.8 |
| Ingress queue/wake | 177.4 | 126.2 | 115.5 |
| Adapter before checked call | 10.2 | 6.1 | 9.2 |
| Checked apply | 592.1 | 1017.6 | 990.9 |
| Post-apply owner | 19.6 | 11.3 | 15.2 |
| Book publication | 8.8 | 5.1 | 9.4 |
| Book subscriber wait/materialization | 342.8 | 173.2 | 210.4 |
| Book synchronous Write | 135.3 | 44.8 | 55.0 |

Checked-call share of owner update wall time was **95.2% / 98.3% / 97.6%**.
Maximum ingress/Book/Event occupancy across ON trials was **10 / 10 / 10**,
against capacity 64. Full branch metrics and each product's p99/CPU are retained
in the condensed and per-trial JSON. Server Write return is not client processing.

### Per-product workload and tail ranges

This table records exact normal/deep workload tiers and deep ON Book p99 ranges
across three processes. Batch uses the deep rate/depth and triples the listed
batch. All per-key count/interval fields are retained in the profile and JSON.

| Market/symbol | Normal depth/batch/rate Hz | Deep depth/batch/rate Hz | Deep ON Book p99 ms min–max |
| --- | --- | --- | --- |
| SPOT/ADAUSDT | 531/5/2 | 1593/5/20 | 3.367–4.424 |
| SPOT/AVAXUSDT | 568/6/2 | 1704/6/20 | 2.949–3.762 |
| SPOT/BNBUSDT | 665/9/3 | 1995/9/30 | 3.334–10.426 |
| SPOT/BTCUSDT | 1160/27/7 | 3480/27/70 | 3.315–3.842 |
| SPOT/DOGEUSDT | 642/9/3 | 1926/9/30 | 3.396–9.925 |
| SPOT/ETHUSDT | 1500/40/10 | 4500/40/100 | 3.470–4.525 |
| SPOT/LINKUSDT | 575/6/2 | 1725/6/20 | 3.372–3.697 |
| SPOT/LTCUSDT | 557/6/2 | 1671/6/20 | 3.474–3.964 |
| SPOT/SOLUSDT | 703/11/3 | 2109/11/30 | 3.432–6.089 |
| SPOT/XRPUSDT | 663/9/3 | 1989/9/30 | 3.413–3.945 |
| USD_M_PERPETUAL/ADAUSDT | 639/9/3 | 1917/9/30 | 3.417–3.730 |
| USD_M_PERPETUAL/AVAXUSDT | 656/9/3 | 1968/9/30 | 3.377–3.571 |
| USD_M_PERPETUAL/BNBUSDT | 749/12/3 | 2247/12/30 | 3.479–4.404 |
| USD_M_PERPETUAL/BTCUSDT | 1198/29/7 | 3594/29/70 | 3.557–3.998 |
| USD_M_PERPETUAL/DOGEUSDT | 812/15/4 | 2436/15/40 | 3.404–6.871 |
| USD_M_PERPETUAL/ETHUSDT | 1500/40/10 | 4500/40/100 | 3.638–4.301 |
| USD_M_PERPETUAL/LINKUSDT | 625/8/3 | 1875/8/30 | 3.257–3.874 |
| USD_M_PERPETUAL/LTCUSDT | 591/7/2 | 1773/7/20 | 3.259–3.371 |
| USD_M_PERPETUAL/SOLUSDT | 994/21/5 | 2982/21/50 | 3.407–3.653 |
| USD_M_PERPETUAL/XRPUSDT | 942/19/5 | 2826/19/50 | 3.121–3.777 |

### Allocation, host headroom and disposition

The unchanged pinned Core's separate M5 allocation evidence remains applicable
as algorithmic attribution: existing-price accepted B1/B10/B100 all used
2023 allocations / 161,560 requested bytes at D1000 and
10,029 / 1,004,312 at D5000, with zero persistent live delta. See the
[four-product Core/allocation record](PROJECTION_P1_RESULT.md#core-and-allocation-attribution).
This increment does not relabel those as a direct allocation count for every
heterogeneous depth or a concurrent process-wide malloc/RSS decomposition.
No allocator instrumentation was mixed into timing runs.

Host available memory minimum was **5.81 GiB**; per-trial sampled
host busy-core averages ranged **0.92–2.09**.
Swap-in activity was present, with zero sampled swap-out delta. Other apps
are included. This is not proof of no benchmark faults or four-core VPS reserves.

**Twenty isolated products sustained every finite tested workload with complete
delivery. Full-book checked apply remains the dominant owner-update hotspot.**
The observed tail variability and missing approved target/shared-host budgets
prevent a production SLA or P2 OUT/NOT_NEEDED decision. **P2 stays CONDITIONAL**;
keep Core/Adapter and `std::map` unchanged. G3/R3/BMD-4 still own actual
Recorder/Archive co-load, target symbol approval, shared API budgets and VPS acceptance.

## Evidence and validation

[Condensed results](evidence/projection-p1-twenty-local-20261019.json) and
[all eighteen trial summaries](evidence/projection-p1-twenty-trials-20261019.json)
retain every product, per-update stage aggregation, host samples' identities,
source/binary/profile hashes, queue maxima and all outlier trials.

- Release OFF 26/26, ON 27/27; final affected integration increment 4/4 each.
- ASan+UBSan production 25/25; focused TSan 6/6. Final metadata-error addition
  passed 1/1 in each sanitizer; pinned upstream static packages are unchanged.
- Four Python analyzer tests, including heterogeneous expected counts, PASS.
- Fresh independent code/method/raw-metadata and paired-smoke review APPROVED.
- Gateway Linux CI exposed a missing explicit `g4` namespace alias in the new
  metadata **test**; the test-only correction and stronger 21-distinct-entry
  registry test are validated separately after the measured source freeze.
  Production and measurement-driver source/binaries are unchanged by these fixes;
  the failed CI log is retained. Final corrections passed Release 3/3,
  ASan+UBSan 2/2 and TSan 2/2; independent review approved both test corrections.
- Independent final numeric review matched every raw hash, re-ran all nine trace
  analyses and recalculated weighted stages, all counts, CPU/RSS and latency tails.
- Gateway technical commit `7266c47577d63bc5bff5f09492a8a166b80ae2f5` passed
  [all seven ordinary CI jobs](https://github.com/tomohikoAmada/BinanceMarketDataGateway/actions/runs/39145975891),
  including Linux/GCC13 production **24/24**; manual preparation skipped as intended.
- The fresh reviewer independently verified the raw archive's size/hash, all
  107 data members and measured binary/runner/profile identity; final verdict
  **APPROVED, no actionable findings**.

Local raw archive: `build/projection-p1-twenty-local-20261019.tar.gz`
(26,681,862 bytes), SHA-256
`bf833c5930ba417b8ec8275bfd5e9beb2cd6d61e888b1e6fd33e897ae7d72bee`.
Its manifest hashes all 107 data members; tar readback matched every hash.
It contains all raw paired payloads/resources/traces, original official responses,
measured binaries, profile, runner, source patch, aggregation code and validation
logs, including the pre-correction Linux failure. Public CI links complement the
archive; it is retained locally, not uploaded as a release artifact.

## Reproduction

Use Gateway measured commit `8ce41be` and the recorded pinned toolchain for the
same OFF/ON Release executables. The portable final tree includes test-only
corrections at `7266c47`; its production/measurement-driver code is identical to
`8ce41be`. Projection runner source at `1a35c27` matches the campaign driver SHA.
From Projection:

```sh
.venv-tools/bin/python tests/test_projection_p1.py
.venv-tools/bin/python scripts/measure-projection-p1.py \
  --gateway-source ../BinanceMarketDataGateway \
  --normal ../BinanceMarketDataGateway/build/p1-off/bmd_gateway_projection_p1_measurements \
  --instrumented ../BinanceMarketDataGateway/build/p1-on/bmd_gateway_projection_p1_measurements \
  --profile ../BinanceMarketDataGateway/tests/fixtures/projection-p1-twenty.json \
  --output build/p1-twenty-results
```

The output directory must not exist. The runner checks every profile key/count,
sequence/final ID, build compatibility, binary hash and source/profile freeze;
retains raw payloads, external resources and nine trace files; alternates OFF/ON
order and rejects Full, missing correlation, invalid chronology and overflow.

## Delivery status

Gateway [PR #38](https://github.com/tomohikoAmada/BinanceMarketDataGateway/pull/38)
is ready for review; technical `7266c47` has all seven CI jobs green and Linux
production 24/24. Documentation-only closure `9d92c96` changes no tested code.
Projection [PR #55](https://github.com/tomohikoAmada/BinanceMarketDataProjection/pull/55)
delivers the reviewed runner, four analyzer tests, reports and trial summaries.
Its original four-product technical commit `248c622` passed all 20 automatic
checks. The twenty-key Python-only increment passed its four focused tests and
independent method/evidence review; Core/Adapter and toolchain are unchanged.
The subsequent automatic C++ matrix is tracked by the PR checks and is not
represented as passed while it is still running. Neither PR is merged here.
