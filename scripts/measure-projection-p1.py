#!/usr/bin/env python3
"""Paired offline configured-product campaigns using the existing Gateway G2 graph.

Requires repository-local psutil (7.2.2 used for the retained campaign).
No network access, daemon deployment, threshold, or production qualification.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import platform
import subprocess
import time
from pathlib import Path

import psutil

PRODUCTS = {
    "BINANCE/SPOT/BTCUSDT", "BINANCE/SPOT/ETHUSDT",
    "BINANCE/USD_M_PERPETUAL/BTCUSDT", "BINANCE/USD_M_PERPETUAL/ETHUSDT",
}


def require(condition, reason):
    if not condition:
        raise ValueError(reason)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def stats(values):
    require(bool(values), "empty measurement")
    values = sorted(values)
    return {"n": len(values), "mean_ns": sum(values) / len(values),
            "p50_ns": values[math.ceil(len(values) * .5) - 1],
            "p99_ns": values[math.ceil(len(values) * .99) - 1], "max_ns": values[-1]}


def analyze_trace(path, count):
    counts = count if isinstance(count, dict) else {p: count for p in PRODUCTS}
    products = set(counts)
    rows = [json.loads(line) for line in Path(path).read_text().splitlines()]
    campaigns = [r for r in rows if r["record"] == "campaign"]
    require(len(campaigns) == len(products) and {r["product"] for r in campaigns} == products,
            "trace product campaigns do not match expected identities")
    result = {}
    for campaign in campaigns:
        product = campaign["product"]
        count = counts[product]
        require(campaign["evidence_valid"] and campaign["trace_count"] == count,
                f"invalid/count-mismatched campaign: {product}")
        for field in ("trace_storage_overflow", "delivery_storage_overflow", "clock_error_count",
                      "ingress_full_count", "order_book_full_terminalization_count",
                      "event_full_terminalization_count"):
            require(campaign[field] == 0, f"{product}: {field}")
        selected_traces = [r for r in rows if r["record"] == "trace" and r["product"] == product]
        require(len(selected_traces) == count, f"{product}: duplicate/missing trace rows")
        traces = {r["trace_id"]: r for r in selected_traces}
        require(set(traces) == set(range(1, count + 1)), f"{product}: incomplete traces")
        metrics = {name: [] for name in ("parse", "event_publish_and_ingress_submit", "ingress_wait",
                    "adapter_before_checked_call", "checked_apply", "post_apply_owner",
                    "input_to_applied", "owner_update_cpu")}
        for t in traces.values():
            require(t["t3_disposition"] == "applied", f"{product}: non-Applied trace")
            stages = [t[k] for k in ("t0_ns", "t1_ns", "q_ns", "t2_ns", "adapted_ns", "t3_ns", "owner_done_ns")]
            require(all(isinstance(v, int) and v > 0 for v in stages) and stages == sorted(stages),
                    f"{product}: invalid stage chronology")
            pairs = (("parse", "t0_ns", "t1_ns"),
                     ("event_publish_and_ingress_submit", "t1_ns", "q_ns"),
                     ("ingress_wait", "q_ns", "t2_ns"),
                     ("adapter_before_checked_call", "t2_ns", "adapted_ns"),
                     ("checked_apply", "adapted_ns", "t3_ns"),
                     ("post_apply_owner", "t3_ns", "owner_done_ns"),
                     ("input_to_applied", "t0_ns", "t3_ns"),
                     ("owner_update_cpu", "owner_cpu_begin_ns", "owner_cpu_end_ns"))
            for name, begin, end in pairs:
                require(t[end] >= t[begin], f"negative {name}")
                metrics[name].append(t[end] - t[begin])
        deliveries = [r for r in rows if r["record"] == "delivery" and r["product"] == product]
        require(len(deliveries) == 2 * count, f"{product}: delivery count")
        for branch in ("order_book", "event"):
            selected = [r for r in deliveries if r["kind"] == branch]
            require(len(selected) == count and {r["trace_id"] for r in selected} == set(traces),
                    f"{product}: {branch} correlation incomplete")
            for metric in ("publication", "subscriber_wait_materialize", "server_write", "input_to_write_completed"):
                metrics[f"{branch}_{metric}"] = []
            for d in selected:
                t = traces[d["trace_id"]]
                start = t["t3_ns"] if branch == "order_book" else t["t1_ns"]
                require(isinstance(d["t5_ns"], int) and start <= d["t4_ns"] <= d["write_begin_ns"] <= d["t5_ns"],
                        f"{product}: {branch} invalid write chronology")
                for metric, value in (("publication", d["t4_ns"] - start),
                                      ("subscriber_wait_materialize", d["write_begin_ns"] - d["t4_ns"]),
                                      ("server_write", d["t5_ns"] - d["write_begin_ns"]),
                                      ("input_to_write_completed", d["t5_ns"] - t["t0_ns"])):
                    metrics[f"{branch}_{metric}"].append(value)
        result[product] = {"timings": {k: stats(v) for k, v in metrics.items()},
                           "owner_update_cpu_ns": sum(metrics["owner_update_cpu"]), "queues": campaign}
    return result


def run_trial(binary, out, depth, batch, count, interval, profile=None, scenario=None):
    command = ([str(binary), "--profile", str(profile), scenario, str(out)] if profile else
               [str(binary), str(depth), str(batch), str(count), str(interval), str(out)])
    expected = ({f"BINANCE/{p['market']}/{p['symbol']}": p["workloads"][scenario]
                 for p in json.loads(profile.read_text())["products"]} if profile else
                {p: {"depth": depth, "batch": batch, "count": count, "interval_us": interval} for p in PRODUCTS})
    before_sha = sha(binary)
    samples = []
    psutil.cpu_percent(percpu=True)
    swap_start = psutil.swap_memory()
    with out.with_suffix(".log").open("w") as log:
        process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT)
        child = psutil.Process(process.pid)
        while process.poll() is None:
            memory = psutil.virtual_memory()
            try:
                rss = child.memory_info().rss
            except psutil.NoSuchProcess:
                rss = 0
            samples.append({"monotonic_ns": time.monotonic_ns(), "host_cpu_percent_per_logical_cpu": psutil.cpu_percent(percpu=True),
                            "host_available_bytes": memory.available, "child_rss_bytes": rss})
            time.sleep(.1)
        require(process.returncode == 0, f"campaign failed: {command}, see {log.name}")
    require(sha(binary) == before_sha, "measurement binary changed during campaign")
    result = json.loads(out.read_text())
    require(len(result["products"]) == len(expected), "summary product count mismatch")
    observed_products = {f"BINANCE/{p['market'].removeprefix('MARKET_')}/{p['symbol']}" for p in result["products"]}
    require(observed_products == set(expected), "summary product identity mismatch")
    for p in result["products"]:
        key = f"BINANCE/{p['market'].removeprefix('MARKET_')}/{p['symbol']}"
        w = expected[key]
        actual = p if profile else result
        require((actual["depth_per_side"], actual["batch_levels"], actual["events_per_product"], actual["interval_us"])
                == (w["depth"], w["batch"], w["count"], w["interval_us"]), "summary workload mismatch")
        require(p["last_update_id"] == 134 + w["count"], "final ID mismatch")
        for branch in ("book_client_latency", "event_client_latency"):
            require(p[branch]["n"] == w["count"], "client delivery count")
    swap_end = psutil.swap_memory()
    resource_path = out.with_suffix(".resources.json")
    resource_path.write_text(json.dumps({"samples": samples, "swap_in_delta_bytes": swap_end.sin - swap_start.sin,
                                         "swap_out_delta_bytes": swap_end.sout - swap_start.sout}, indent=2) + "\n")
    entry = {"file": out.name, "payload_sha256": sha(out), "binary_sha256": before_sha,
             "resource_file": resource_path.name, "resource_sha256": sha(resource_path),
             "process_cpu_cores": result["process_cpu_us"] * 1000 / result["wall_ns"],
             "peak_rss_process_bytes": result["peak_rss_process_bytes"],
             "host_min_available_bytes": min(s["host_available_bytes"] for s in samples),
             "host_mean_busy_cores": sum(sum(s["host_cpu_percent_per_logical_cpu"]) / 100 for s in samples) / len(samples),
             "summary": result}
    if result["instrumented"]:
        trace = Path(str(out) + ".traces.jsonl")
        entry["trace_file"] = trace.name
        entry["trace_sha256"] = sha(trace)
        entry["trace_analysis"] = analyze_trace(trace, {p: w["count"] for p, w in expected.items()})
    return entry


def source_identity(root):
    def git(*args):
        return subprocess.check_output(["git", "-C", str(root), *args])
    return {"head": git("rev-parse", "HEAD").decode().strip(),
            "diff_sha256": hashlib.sha256(git("diff", "HEAD", "--binary")).hexdigest(),
            "untracked": {name: sha(root / name) for name in git("ls-files", "--others", "--exclude-standard").decode().splitlines()}}


def cache_settings(binary):
    cache = binary.parent / "CMakeCache.txt"
    values = {}
    for line in cache.read_text().splitlines():
        if not line.startswith(("#", "//")) and "=" in line and ":" in line.split("=", 1)[0]:
            name, value = line.split("=", 1)
            values[name.split(":", 1)[0]] = value
    return cache, values


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--normal", type=Path, required=True)
    parser.add_argument("--instrumented", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--gateway-source", type=Path, required=True)
    parser.add_argument("--repetitions", type=int, default=3)
    parser.add_argument("--smoke", action="store_true")
    parser.add_argument("--profile", type=Path, help="Frozen twenty-key mixed workload JSON")
    args = parser.parse_args()
    require(args.repetitions > 0, "positive repetitions required")
    args.gateway_source = args.gateway_source.resolve()
    source = source_identity(args.gateway_source)
    if args.profile:
        args.profile = args.profile.resolve()
    profile_sha = sha(args.profile) if args.profile else None
    normal_cache, normal_settings = cache_settings(args.normal.resolve())
    on_cache, on_settings = cache_settings(args.instrumented.resolve())
    for setting in ("CMAKE_BUILD_TYPE", "CMAKE_CXX_COMPILER", "CMAKE_CXX_FLAGS", "CMAKE_CXX_FLAGS_RELEASE", "CMAKE_TOOLCHAIN_FILE",
                    "BMD_GATEWAY_ENABLE_ASAN", "BMD_GATEWAY_ENABLE_UBSAN", "BMD_GATEWAY_ENABLE_TSAN"):
        require(normal_settings.get(setting) == on_settings.get(setting), f"build mismatch: {setting}")
    require(normal_settings["CMAKE_BUILD_TYPE"] == "Release", "Release required")
    require(normal_settings["BMD_GATEWAY_ENABLE_PERFORMANCE_BASELINE_INSTRUMENTATION"] == "OFF" and
            on_settings["BMD_GATEWAY_ENABLE_PERFORMANCE_BASELINE_INSTRUMENTATION"] == "ON", "expected OFF/ON pair")
    require(all(normal_settings.get(k) == "OFF" for k in ("BMD_GATEWAY_ENABLE_ASAN", "BMD_GATEWAY_ENABLE_UBSAN", "BMD_GATEWAY_ENABLE_TSAN")), "sanitizers must be off")
    args.output.mkdir(parents=True, exist_ok=False)
    scenarios = [("normal_depth", 1000, 10, 300, 100000), ("deep_stress", 5000, 10, 1000, 10000),
                 ("batch_stress", 5000, 100, 1000, 10000)]
    if args.profile:
        scenarios = [(name, None, None, None, None) for name in ("normal", "deep", "batch")]
    if args.smoke:
        scenarios = [("smoke", 8, 1, 8, 1000)]
    report = {"gateway_source": source, "driver_sha256": sha(__file__),
              "normal_cmake_cache_sha256": sha(normal_cache), "instrumented_cmake_cache_sha256": sha(on_cache),
              "common_build_settings": {k: v for k, v in normal_settings.items() if k in ("CMAKE_BUILD_TYPE", "CMAKE_CXX_COMPILER", "CMAKE_CXX_FLAGS", "CMAKE_CXX_FLAGS_RELEASE", "CMAKE_TOOLCHAIN_FILE")},
              "schema": "projection-p1-paired-campaign.v1", "profile_sha256": profile_sha, "platform": platform.platform(),
              "processor": platform.processor(), "logical_cpus": psutil.cpu_count(),
              "physical_cpus": psutil.cpu_count(logical=False), "host_memory_bytes": psutil.virtual_memory().total,
              "psutil_version": psutil.__version__, "scope": "offline local host, embedded loopback clients, no numeric acceptance gate", "trials": []}
    for name, depth, batch, count, interval in scenarios:
        for repetition in range(args.repetitions):
            pair = [("off", args.normal), ("on", args.instrumented)]
            if repetition % 2:
                pair.reverse()
            for mode, binary in pair:
                print(f"{name} repetition={repetition + 1} instrumentation={mode}", flush=True)
                entry = run_trial(binary.resolve(), args.output / f"{name}-{repetition + 1}-{mode}.json", depth, batch, count, interval, args.profile, name)
                require(entry["summary"]["instrumented"] == (mode == "on"), "wrong instrumentation binary")
                entry.update(scenario=name, repetition=repetition + 1, mode=mode)
                report["trials"].append(entry)
                (args.output / "campaign.json").write_text(json.dumps(report, indent=2) + "\n")
    require(not args.profile or sha(args.profile) == profile_sha, "profile changed during campaign")
    require(source_identity(args.gateway_source) == source, "Gateway source changed during campaign")
    print(f"complete: {args.output / 'campaign.json'}", flush=True)


if __name__ == "__main__":
    main()
