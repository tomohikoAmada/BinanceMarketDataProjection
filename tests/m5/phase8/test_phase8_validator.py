import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location("phase8_validator", ROOT / "scripts/benchmark_phase8.py")
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class Phase8ValidatorTests(unittest.TestCase):
    def test_duplicate_json_key_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "duplicate.json"
            path.write_text('{"schema": 1, "schema": 2}', encoding="utf-8")
            with self.assertRaises(ValueError):
                MODULE.load(path)

    def test_malformed_wrapper_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            payload = Path(directory) / "payload.json"
            wrapper = Path(directory) / "wrapper.json"
            payload.write_text("{}", encoding="utf-8")
            wrapper.write_text(json.dumps({"schema": "wrong"}), encoding="utf-8")
            with self.assertRaises(ValueError):
                MODULE.validate(payload, wrapper)

    def test_candidate_set_is_exact(self):
        self.assertEqual(len(MODULE.CANDIDATES), 4)
        self.assertIn("phase8-std-map-control-v1", MODULE.CANDIDATES)
        self.assertIn("phase8-sorted-vector-batch-lww-v1", MODULE.CANDIDATES)


if __name__ == "__main__":
    unittest.main()
