#!/usr/bin/env bash
# M5 Phase-8 candidate benchmark/evidence driver.
#
# Smoke checks execution structure and evidence identity only. It applies no
# numeric performance threshold and never selects a container.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

mode="${1:-smoke}"
case "$mode" in
    smoke|full) ;;
    *)
        echo "usage: $0 {smoke|full}" >&2
        exit 2
        ;;
esac

benchmark_exe="build/benchmark/cmake/benchmarks/bmd_projection_m5_phase8_benchmarks"
evidence_exe="build/benchmark/cmake/benchmarks/bmd_projection_m5_phase8_container_evidence"
if [[ ! -x "$benchmark_exe" || ! -x "$evidence_exe" ]]; then
    echo "Phase-8 benchmark binaries are not built; configure/build the benchmark preset first" >&2
    exit 1
fi

results_dir="build/benchmark/phase8-results"
mkdir -p "$results_dir"

if [[ "$mode" == "smoke" ]]; then
    benchmark_filter='^phase8-(std-map-control|sorted-vector-naive|absl-btree-map|sorted-vector-batch-lww)-v1/M2/apply_updates/10/8$'
    evidence_filter="M2/apply_updates/10/8"
    evidence_class="exploratory"
else
    benchmark_filter='^phase8-(std-map-control|sorted-vector-naive|absl-btree-map|sorted-vector-batch-lww)-v1/'
    evidence_filter=""
    evidence_class="exploratory"
fi

echo "Phase-8 Google Benchmark smoke (structural only)"
"$benchmark_exe" \
    --benchmark_filter="$benchmark_filter" \
    --benchmark_format=json \
    --benchmark_out="$results_dir/google-benchmark.json" \
    --m5_wrapper_out="$results_dir/google-benchmark-wrapper.json" \
    --benchmark_repetitions=1 \
    --benchmark_min_time=0.005s

echo "Phase-8 candidate evidence run (class=$evidence_class)"
evidence_args=(
    "--m5_output=$results_dir/evidence.json"
    "--m5_wrapper_out=$results_dir/evidence-wrapper.json"
    "--m5_evidence_class=$evidence_class"
    "--m5_repetitions=${M5_P8_REPETITIONS:-5}"
)
if [[ -n "$evidence_filter" ]]; then
    evidence_args+=("--m5_filter=$evidence_filter")
fi
"$evidence_exe" "${evidence_args[@]}"
python3 scripts/benchmark_phase8.py validate \
    "$results_dir/evidence.json" "$results_dir/evidence-wrapper.json"

echo "Phase-8 benchmark/evidence smoke PASS (no numeric performance conclusion)"
