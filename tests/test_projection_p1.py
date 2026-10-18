import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location("measure_p1", Path(__file__).parents[1] / "scripts/measure-projection-p1.py")
P1 = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(P1)


def fixture_rows():
    rows = []
    for product in P1.PRODUCTS:
        campaign = {"record": "campaign", "product": product, "evidence_valid": True, "trace_count": 1}
        for field in ("trace_storage_overflow", "delivery_storage_overflow", "clock_error_count",
                      "ingress_full_count", "order_book_full_terminalization_count", "event_full_terminalization_count"):
            campaign[field] = 0
        rows.append(campaign)
        rows.append(dict(record="trace", product=product, trace_id=1, t3_disposition="applied",
                         t0_ns=10, t1_ns=20, q_ns=40, t2_ns=60, adapted_ns=70, t3_ns=100,
                         owner_done_ns=120, owner_cpu_begin_ns=1000, owner_cpu_end_ns=1040))
        for kind, t4 in (("order_book", 110), ("event", 30)):
            rows.append(dict(record="delivery", product=product, trace_id=1, kind=kind,
                             t4_ns=t4, write_begin_ns=150, t5_ns=180))
    return rows


class TraceAnalysisTests(unittest.TestCase):
    def analyze(self, rows):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "trace.jsonl"
            path.write_text("\n".join(json.dumps(r) for r in rows))
            return P1.analyze_trace(path, 1)

    def test_stage_attribution(self):
        result = self.analyze(fixture_rows())
        for p in result.values():
            self.assertEqual(p["timings"]["checked_apply"]["mean_ns"], 30)
            self.assertEqual(p["timings"]["order_book_subscriber_wait_materialize"]["mean_ns"], 40)
            self.assertEqual(p["timings"]["order_book_server_write"]["mean_ns"], 30)
            self.assertEqual(p["owner_update_cpu_ns"], 40)

    def test_reject_bad_evidence(self):
        original = fixture_rows()
        mutations = [original[1:], original + [original[0]], original + [original[1]], original[:-1]]
        for field, value in (("t3_disposition", "gap_detected"), ("adapted_ns", 200),
                             ("q_ns", None), ("owner_cpu_end_ns", 1)):
            rows = copy.deepcopy(original)
            rows[1][field] = value
            mutations.append(rows)
        for field in ("clock_error_count", "ingress_full_count", "trace_storage_overflow"):
            rows = copy.deepcopy(original)
            rows[0][field] = 1
            mutations.append(rows)
        rows = copy.deepcopy(original)
        rows[2]["write_begin_ns"] = 190
        mutations.append(rows)
        for rows in mutations:
            with self.subTest(rows=rows[:3]), self.assertRaises(ValueError):
                self.analyze(rows)

    def test_heterogeneous_product_counts(self):
        rows = fixture_rows()
        key = rows[0]["product"]
        rows[0]["trace_count"] = 2
        for row in rows[1:4]:
            extra = copy.deepcopy(row)
            extra["trace_id"] = 2
            rows.append(extra)
        counts = {p: (2 if p == key else 1) for p in P1.PRODUCTS}
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "trace.jsonl"
            path.write_text("\n".join(json.dumps(r) for r in rows))
            result = P1.analyze_trace(path, counts)
            self.assertEqual(result[key]["timings"]["checked_apply"]["n"], 2)
            with self.assertRaises(ValueError):
                P1.analyze_trace(path, 1)
            with self.assertRaises(ValueError):
                P1.analyze_trace(path, {**counts, "BINANCE/SPOT/UNKNOWN": 1})

    def test_nearest_rank_percentile(self):
        self.assertEqual(P1.stats(list(range(1, 101)))["p99_ns"], 99)
        self.assertEqual(P1.stats([8])["p99_ns"], 8)


if __name__ == "__main__":
    unittest.main()
