#!/usr/bin/env bash
# M5 Phase-6 benchmark smoke driver (OD-M5-P6-013/020/024/029).
#
# Structural execution evidence only: Release, ProtoAdapter ON, 1 repetition,
# fail-closed inventory/wrapper/latency validation. No numeric performance
# gate is applied. The smoke filter matches the exact expected executed set;
# the validator rejects zero matches, SkipWithError, error_occurred, missing
# required inventory, and missing required M4 names.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

mode="${1:-full}"
case "$mode" in
    full|quick) ;;
    *)
        echo "usage: $0 {full|quick}" >&2
        exit 2
        ;;
esac

if [[ ! -x build/benchmark/cmake/benchmarks/bmd_projection_benchmarks ]]; then
    echo "benchmark binaries are not built; run scripts/configure.sh benchmark -DBMD_PROJECTION_BUILD_PROTO_ADAPTER=ON && scripts/build.sh benchmark" >&2
    exit 1
fi

results_dir="build/benchmark/phase6-smoke-results"
mkdir -p "$results_dir"
payload="$results_dir/benchmarks.json"
wrapper="$results_dir/benchmarks-wrapper.json"
latency_payload="$results_dir/latency.json"
latency_wrapper="$results_dir/latency-wrapper.json"

min_time="0.02s"
if [[ "$mode" == "quick" ]]; then
    min_time="0.005s"
fi

# Locked smoke filter: M1 (all), M2 representatives, the 8-cell accepted-live
# M3 subset (D{8,1000} x B{0,10} x {Spot,UsdMPerpetual}), all classification
# cells, component/proxy depth-8 cells, M4 depth-100 cells, Core/Adapter
# replays, and the infrastructure smoke benchmark.
filter='^(M1/|M2/(apply_level/(insert|update|delete|missing_delete)/8|apply_updates/(10/8|update_mix/8)|replace_all/8|best_bid/8|best_ask/8|quantity_at/(hit|miss)/8|top_levels/5/8|all_levels/8)|M3/(LiveApply/Accepted/(Spot|UsdMPerpetual)/D(8|1000)/B(0|10)/|Classification/(Stale|Duplicate|Gap|Reset|BaselineInstall)/(Spot|UsdMPerpetual)|Component/AllLevelsBothSides/8|Proxy/(CandidateRebuildFromVectors|CandidateApplyUpdates|OrderBookMoveCommit)/8)|M4/.*/100/|CoreNormalizedReplay/(Spot|UsdMPerpetual)|AdapterWireReplay/(Spot|UsdMPerpetual)|BM_LibraryVersionAccess)'

echo "Phase-6 benchmark smoke (filter=$filter)"
build/benchmark/cmake/benchmarks/bmd_projection_benchmarks \
    --benchmark_filter="$filter" \
    --benchmark_format=json \
    --benchmark_out="$payload" \
    --benchmark_repetitions=1 \
    --benchmark_min_time="$min_time" \
    --m5_wrapper_out="$wrapper" \
    --m5_evidence_class=exploratory

echo "Phase-6 event latency smoke (spot, 1 pass)"
build/benchmark/cmake/benchmarks/bmd_projection_replay_latency \
    --m5_workload=spot \
    --m5_passes=1 \
    --m5_calibration_samples=10000 \
    --m5_output="$latency_payload" \
    --m5_wrapper_out="$latency_wrapper" \
    --m5_evidence_class=exploratory

echo "Validating benchmark smoke outputs"
python3 scripts/benchmark_phase6.py validate-smoke "$payload" "$wrapper" --repetitions 1
python3 scripts/benchmark_phase6.py validate-inventory "$wrapper"
echo "Validating latency smoke outputs"
python3 scripts/benchmark_phase6.py validate-latency "$latency_payload" "$latency_wrapper"

echo "Phase-6 benchmark smoke PASS (structural execution evidence only)"
