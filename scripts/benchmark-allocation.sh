#!/usr/bin/env bash
# M5 Phase-7 allocation/memory characterization driver — EXPLORATORY ONLY.
#
# This is the local/exploratory Phase-7 driver. It is NOT the formal
# Phase-7 canonical Release runner (scripts/benchmark-allocation-formal.sh),
# which does not exist in this PR. Nothing produced here is formal evidence;
# every payload is labelled evidence_class=exploratory.
#
# Modes:
#   smoke         locked reduced subset (fast structural execution evidence)
#   full          the complete Phase-7 measurement inventory
#   determinism   run the complete inventory twice in separate process
#                 invocations and require exact normalized-metric equality
#
# Before measuring, the Phase-6 workload identity regression verifies that
# the measurement executables carry bit-identical accepted Phase-6 workload
# identities.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

mode="${1:-smoke}"
case "$mode" in
    smoke|full|determinism) ;;
    *)
        echo "usage: $0 {smoke|full|determinism}" >&2
        exit 2
        ;;
esac

repetitions="${M5_P7_REPETITIONS:-3}"
case "$repetitions" in
    [0-9]*) ;;
    *)
        echo "M5_P7_REPETITIONS must be a positive integer" >&2
        exit 2
        ;;
esac

benchmark_bin="build/benchmark/cmake/benchmarks/bmd_projection_benchmarks"
m2m3_bin="build/benchmark/cmake/benchmarks/bmd_projection_allocation_m2_m3"
m4_bin="build/benchmark/cmake/benchmarks/bmd_projection_allocation_m4"
replay_bin="build/benchmark/cmake/benchmarks/bmd_projection_allocation_replay"
footprint_bin="build/benchmark/cmake/benchmarks/bmd_projection_allocation_footprint"

if [[ ! -x "$m2m3_bin" ]]; then
    echo "Phase-7 allocation executables are not built; run:" >&2
    echo "  bash scripts/configure.sh benchmark -DBMD_PROJECTION_BUILD_PROTO_ADAPTER=ON" >&2
    echo "  bash scripts/build.sh benchmark" >&2
    exit 1
fi

results_dir="build/benchmark/phase7-results"
mkdir -p "$results_dir"

echo "Phase-7 allocation driver: EXPLORATORY (mode=$mode, repetitions=$repetitions)"
echo "This is NOT the formal runner and produces no formal evidence."

# 1. Phase-6 workload identity regression (bit-for-bit against the committed
#    golden identity set).
echo "Checking Phase-6 workload identity preservation"
python3 tests/m5/benchmark/check_phase6_workload_identity.py \
    "$benchmark_bin" "$results_dir/identity-check"

run_one() {
    local suffix="$1"
    local m2m3_filter="$2"
    local m4_filter="$3"
    local replay_filter="$4"
    local footprint_filter="$5"

    "$m2m3_bin" \
        --m5_output="$results_dir/m2m3-$suffix.json" \
        --m5_wrapper_out="$results_dir/m2m3-$suffix-wrapper.json" \
        --m5_evidence_class=exploratory \
        --m5_repetitions="$repetitions" \
        --m5_filter="$m2m3_filter"
    "$footprint_bin" \
        --m5_output="$results_dir/footprint-$suffix.json" \
        --m5_wrapper_out="$results_dir/footprint-$suffix-wrapper.json" \
        --m5_evidence_class=exploratory \
        --m5_repetitions="$repetitions" \
        --m5_filter="$footprint_filter"
    if [[ -x "$replay_bin" ]]; then
        "$replay_bin" \
            --m5_output="$results_dir/replay-$suffix.json" \
            --m5_wrapper_out="$results_dir/replay-$suffix-wrapper.json" \
            --m5_evidence_class=exploratory \
            --m5_repetitions="$repetitions" \
            --m5_filter="$replay_filter"
    fi
    if [[ -x "$m4_bin" ]]; then
        "$m4_bin" \
            --m5_output="$results_dir/m4-$suffix.json" \
            --m5_wrapper_out="$results_dir/m4-$suffix-wrapper.json" \
            --m5_evidence_class=exploratory \
            --m5_repetitions="$repetitions" \
            --m5_filter="$m4_filter"
    fi
}

validate_one() {
    local suffix="$1"
    local inventory="${2:-}"

    python3 scripts/benchmark_phase7.py validate-records \
        "$results_dir/m2m3-$suffix.json" "$results_dir/m2m3-$suffix-wrapper.json" \
        ${inventory:+--require-inventory m2_m3}
    python3 scripts/benchmark_phase7.py validate-records \
        "$results_dir/footprint-$suffix.json" "$results_dir/footprint-$suffix-wrapper.json" \
        ${inventory:+--require-inventory footprint}
    if [[ -f "$results_dir/replay-$suffix.json" ]]; then
        python3 scripts/benchmark_phase7.py validate-records \
            "$results_dir/replay-$suffix.json" "$results_dir/replay-$suffix-wrapper.json" \
            ${inventory:+--require-inventory replay}
    fi
    if [[ -f "$results_dir/m4-$suffix.json" ]]; then
        python3 scripts/benchmark_phase7.py validate-records \
            "$results_dir/m4-$suffix.json" "$results_dir/m4-$suffix-wrapper.json" \
            ${inventory:+--require-inventory m4}
    fi
}

if [[ "$mode" == "smoke" ]]; then
    echo "Running locked exploratory smoke subset"
    # M2 representatives, one accepted M3 cell (B=0 preserved),
    # classification cells, proxies at depth 8, footprint depth 100, and one
    # replay identity.
    run_one smoke \
        "M2/apply_level/" \
        "M4/CheckedInstall/8" \
        "CoreNormalizedReplay/Spot" \
        "Depth/100"
    validate_one smoke ""
    echo "Phase-7 allocation smoke PASS (exploratory structural evidence only)"
    exit 0
fi

if [[ "$mode" == "full" ]]; then
    echo "Running the complete Phase-7 exploratory inventory"
    run_one full "" "" "" ""
    validate_one full "--require-inventory"
    echo
    echo "Phase-7 exploratory allocation characterization complete (NOT formal evidence)."
    echo "  results: $results_dir"
    exit 0
fi

# determinism: two complete runs in separate process invocations; the
# validator requires exact normalized-metric equality (OD-M5-P7-015).
echo "Determinism run A (complete inventory)"
run_one a "" "" "" ""
echo "Determinism run B (complete inventory)"
run_one b "" "" "" ""

python3 scripts/benchmark_phase7.py check-determinism \
    "$results_dir/m2m3-a.json" "$results_dir/m2m3-b.json"
python3 scripts/benchmark_phase7.py check-determinism \
    "$results_dir/footprint-a.json" "$results_dir/footprint-b.json"
if [[ -f "$results_dir/replay-a.json" ]]; then
    python3 scripts/benchmark_phase7.py check-determinism \
        "$results_dir/replay-a.json" "$results_dir/replay-b.json"
fi
if [[ -f "$results_dir/m4-a.json" ]]; then
    python3 scripts/benchmark_phase7.py check-determinism \
        "$results_dir/m4-a.json" "$results_dir/m4-b.json"
fi
echo "Phase-7 exploratory determinism PASS (normalized metrics identical across "
echo "separate process invocations; NOT formal evidence)"
