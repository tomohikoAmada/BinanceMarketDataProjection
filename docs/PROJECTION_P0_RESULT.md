# Projection P0 — CI/CTest repair

2026-10-12; scoped by the owner-provided BinanceMarketData v0.3 overall and
Projection plans. Implementation is submitted for independent review; merge and
post-merge weekly activation remain separate. Baseline:
`555dd1f4d846e19cfd6fbe55676c86504559048e`. Technical commit:
`0430d84e4d269973f207b5e47d301cf73da42713`.

## Changes and diagnosis

- The current repository and Release are public. A fresh anonymous download returned
  HTTP 200 and passed the existing archive, manifest, payload, and fixture verification.
  Asset ID `520530233`, size `49460254` bytes; archive SHA-256 remains
  `5143521fe9728a7c2ce03522b78be4ba2fd91388cdabac800f4a87e970e4adfb`.
  [Historical run 37296257180](https://github.com/tomohikoAmada/BinanceMarketDataProjection/actions/runs/37296257180)
  failed with HTTP 404 at acquisition. The workflow now adds three transient-error
  retries, a 15-second connection timeout, a 180-second transfer timeout, and a
  600-second retry window. Permanent 404 and content mismatch still fail. No token
  dependency or alternative asset store was introduced.
- A fresh Release build with tests and benchmarks enabled reproduced
  `m5_phase6_phase8_registration_separation` as `BAD_COMMAND / permission denied`:
  Python was discovered in the sibling `tests/` scope only. The benchmark directory
  now finds the interpreter before registering its Python test. The script remains
  mode `100644`; no test is disabled.
- The Phase-10 CLI regression checks missing and tampered archives, their nonzero
  status and useful diagnostics, and the absence of extracted output on failure.

Core, public APIs, accepted semantics, dependency pins, and benchmark methodology
are unchanged. No VPS access, Recorder qualification work, Gateway G1/G2, or
cross-repository source change was performed. P1/P2 have not started.

## Validation

Evidence is retained locally under ignored `build/p0-evidence/`.

| Check | Result |
| --- | --- |
| Phase-10 Python suite | 28/28 PASS |
| Fresh Release, tests + benchmarks ON, Core-only, Werror ON | Build PASS; CTest 441/441 PASS |
| Actual acquisition shell block against loopback HTTP fixture | 200 PASS; 503 then 200 PASS (two requests); 404 rejected (exit 22, one request) |
| Fresh anonymous pinned asset | Full distribution verification PASS |
| Original recorded Spot/USD-M canary, three repetitions each | PASS; exact five outputs and source/binary/fixture/result binding PASS |
| `bash scripts/test-quality-toolchain.sh` | 100/100 PASS |
| `bash scripts/quality.sh` | Canonical PASS (clang/clang-tidy/clang-format 18.1.3); 523/523 tests and staged consumer PASS |
| Workflow YAML, shell syntax, `git diff --check` | PASS |

The initial canonical Quality attempt rejected a pre-existing cached Debug Contracts
package carrying source `9c34199f42467b92a4380c6e88617dde0e54ae13` instead of the
current pinned `518880bdfa60948c3b65b6b3525d024526995166`. Only that stale local
binary (`036a2cd0b0b283462d03a5d2eb59f6b09103856a`) was removed. The initial
failure log is retained; no check or pin was weakened.

The recorded benchmark uses a clean local checkout of the technical commit to
preserve the existing source/binary/output binding despite the owner's pre-existing
untracked review patches. It retains the original five-file result contract and
is local exploratory execution evidence, not VPS performance qualification.
The full supplemental `verify.sh` sanitizer/fuzz matrix was not repeated for this
CMake/workflow repair; fresh combined CTest, recorded canary, and canonical Quality
cover the affected paths. Normal PR CI remains enabled.

## Reproduction

```bash
PYTHONPATH="$PWD" .venv-tools/bin/python -m unittest discover \
  -s tests/m5/phase10 -p test_phase10.py -v
bash scripts/configure.sh benchmark \
  -DBMD_PROJECTION_BUILD_PROTO_ADAPTER=OFF \
  -DBMD_PROJECTION_BUILD_TESTS=ON -DBMD_PROJECTION_ENABLE_WERROR=ON
bash scripts/build.sh benchmark
ctest --test-dir build/benchmark/cmake --output-on-failure
bash scripts/test-quality-toolchain.sh
bash scripts/quality.sh
```

The acquisition, full distribution verification, serial recorded benchmark commands,
and five-output validation are the existing steps in
`.github/workflows/m5-performance.yml`; retain a clean checkout for their provenance
checks. The weekly workflow remains main-only and requires no new credentials.
