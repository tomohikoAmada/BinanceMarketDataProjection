#!/usr/bin/env bash
# M5 Phase-6 formal/manual full evidence driver (OD-M5-P6-020/028/029).
#
# Requires a clean committed source tree and the Release benchmark build with
# ProtoAdapter ON. Runs the complete inventory with >= 5 repetitions, the
# event-latency small-tier evidence, and all validators, then produces a
# human-readable summary derived from the machine-readable results.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

repetitions="${M5_P6_REPETITIONS:-5}"
latency_passes="${M5_P6_LATENCY_PASSES:-5}"
calibration_samples="${M5_P6_CALIBRATION_SAMPLES:-100000}"

if [[ ! "$repetitions" =~ ^[0-9]+$ ]] || (( repetitions < 5 )); then
    echo "formal Phase-6 evidence requires M5_P6_REPETITIONS >= 5" >&2
    exit 2
fi

if [[ -n "$(git status --porcelain)" ]]; then
    echo "WARNING: working tree is dirty; evidence will be labelled exploratory" >&2
fi

if [[ ! -x build/benchmark/cmake/benchmarks/bmd_projection_benchmarks ]]; then
    echo "benchmark binaries are not built; run scripts/configure.sh benchmark -DBMD_PROJECTION_BUILD_PROTO_ADAPTER=ON && scripts/build.sh benchmark" >&2
    exit 1
fi

results_dir="build/benchmark/phase6-full-results"
mkdir -p "$results_dir"
payload="$results_dir/benchmarks.json"
wrapper="$results_dir/benchmarks-wrapper.json"
latency_payload="$results_dir/latency.json"
latency_wrapper="$results_dir/latency-wrapper.json"
summary="$results_dir/evidence-summary.txt"

echo "Phase-6 full benchmark run (repetitions=$repetitions)"
build/benchmark/cmake/benchmarks/bmd_projection_benchmarks \
    --benchmark_format=json \
    --benchmark_out="$payload" \
    --benchmark_repetitions="$repetitions" \
    --benchmark_min_time=0.2s \
    --m5_wrapper_out="$wrapper" \
    --m5_evidence_class=formal

echo "Phase-6 event latency evidence (passes=$latency_passes)"
build/benchmark/cmake/benchmarks/bmd_projection_replay_latency \
    --m5_workload=spot \
    --m5_passes="$latency_passes" \
    --m5_calibration_samples="$calibration_samples" \
    --m5_output="$latency_payload" \
    --m5_wrapper_out="$latency_wrapper" \
    --m5_evidence_class=formal

echo "Validating full-run outputs"
python3 scripts/benchmark_phase6.py validate-inventory "$wrapper" "$payload"
python3 scripts/benchmark_phase6.py validate-latency "$latency_payload" "$latency_wrapper"
echo "Producing human-readable summary"
python3 scripts/benchmark_phase6.py summarize "$wrapper" "$latency_wrapper" > "$summary"
cat "$summary"

echo
echo "Phase-6 full evidence run complete."
echo "  results:        $results_dir"
echo "  payload:        $payload"
echo "  wrapper:        $wrapper"
echo "  latency:        $latency_payload"
echo "  latency wrapper:$latency_wrapper"
echo "  summary:        $summary"
