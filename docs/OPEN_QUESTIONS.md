# Open questions

## O-P001: Repository license selection

No explicit common license was found in the private Contracts or Recorder repositories. Select a
license (or an explicit proprietary policy) before any public distribution. This does not block the
private engineering baseline.

## O-P002: TSan platform coverage

Confirm which hosted and deployment toolchains provide a stable ThreadSanitizer runtime. The
repository keeps a separate TSan preset and reports actual platform results without treating
unsupported runtimes as a pass.

## O-P003: Order-book container performance decision

M2 uses `std::map` as the correctness baseline for bid/ask storage. The M5 design
(`docs/M5_DIFFERENTIAL_VALIDATION_AND_PERFORMANCE_DESIGN.md`) defines a benchmark-only container
spike comparing `std::map`, a sorted contiguous vector, Abseil `btree_map`, and a flat-map-style
container against a semantic conformance gate and an explicit decision matrix. Until benchmark
evidence exists, no third-party container is introduced. The Public API is isolated from container
choice via PIMPL; changing the internal map type does not require API changes. A production
migration would require a separate decision record/ADR, full M2-M5 regression, and independent
review. `KEEP std::map` is a valid outcome.

## O-P004: Representative transcript corpus — CLOSED (OD-M5-001)

M5 recorded corpus v1 is sourced from the Recorder M21.4 validated live Raw capture
(2026-08-05T15:09:30.200566Z → 2026-08-06T15:09:30.200566Z, BTCUSDT Spot + USD-M perpetual).
Primary medium fixtures: M5-REC-SPOT-BTCUSDT-V1 and M5-REC-USDM-BTCUSDT-V1, each targeting
100,000 valid replay operations after synchronization. Large corpus: full 24h window, manual
only. Corpus materialization is pending M5 implementation. See
`docs/M5_PREIMPLEMENTATION_DECISIONS.md` for provenance, selection rules, and storage policy.

## O-P005: Scheduled medium-tier CI budget — CLOSED (OD-M5-002)

Free standard GitHub-hosted runners only. Docs-only changes skip full CI via `paths-ignore`.
Superseded runs cancel automatically with `concurrency.cancel-in-progress`. Scheduled medium:
<= weekly, 1 primary Ubuntu timing job, 45 min timeout, non-blocking results. Container spike:
manual only, 60 min timeout. Artifact ceiling: 200 MiB/run, 7-day retention. Larger runners:
NOT ALLOWED. Self-hosted public-repo runner: NOT APPROVED. See
`docs/M5_PREIMPLEMENTATION_DECISIONS.md`.

## O-P006: Semantic manifest granularity

The cross-compiler semantic manifest digest granularity (per-event versus per-checkpoint) is
resolvable during the performance spike (OD-M5-003). The default is per-event for tiny fixtures
and per-checkpoint for small. The cross-job transport architecture is fixed; the granularity
choice does not affect manifest schema identity.

## O-P007: Benchmark threshold and artifact retention

Regression-threshold tuning (noise-floor study) and benchmark artifact retention duration are
open decisions resolvable during the performance spike (OD-M5-005, OD-M5-007).
