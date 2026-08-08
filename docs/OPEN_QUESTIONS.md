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

## O-P004: Representative transcript corpus

M5 medium/large workload tiers require externally recorded, provenance-verified Binance-style
transcripts (source, market, symbol, capture time range, event count, conversion version, SHA-256,
schema identity). Selection of the exact corpus is a blocking open decision
(OD-M5-001) requiring a data-acquisition plan; it does not block the small-tier CI validation.

## O-P005: Scheduled medium-tier CI budget

Medium-tier scheduled runs and the container spike need a documented runner/artifact budget.
This blocking decision (OD-M5-002) is resolved during M5 implementation planning, before the
scheduled workflow is enabled.

## O-P006: Semantic manifest granularity

The cross-compiler semantic manifest digest granularity (per-event versus per-checkpoint) is
resolvable during the performance spike (OD-M5-003). The default is per-event for tiny fixtures
and per-checkpoint for small. The cross-job transport architecture is fixed; the granularity
choice does not affect manifest schema identity.

## O-P007: Benchmark threshold and artifact retention

Regression-threshold tuning (noise-floor study) and benchmark artifact retention duration are
open decisions resolvable during the performance spike (OD-M5-005, OD-M5-007).
