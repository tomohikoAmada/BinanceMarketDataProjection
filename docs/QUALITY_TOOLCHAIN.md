# Quality Toolchain Contract (INFRA-TC-001)

This document is the reference for the canonical Quality acceptance environment. It records the
exact toolchain identity, the single repository-owned entrypoint, how CI and local developers
invoke it, the distinction between canonical acceptance and supplemental local checks, and the
intentional upgrade procedure.

The single source of truth for the identity is `.toolchain/quality.env`. No version literal of an
acceptance-critical LLVM tool exists anywhere else in the repository.

## Canonical Quality environment

Canonical Quality runs inside a repository-owned container image:

- Base image: `ubuntu:24.04` amd64, pinned by immutable digest
  `sha256:019e8eb29a85e74d64925745884f2ec79aa27e3feab36353d24656f4d6b89467`
  (resolved 2026-08-15 from the official Docker Hub repository; the digest, not the mutable tag,
  is the identity).
- Installed from the Ubuntu noble archive with **exact** package versions declared in
  `.toolchain/quality.env` (for example `clang-18=1:18.1.3-1ubuntu1`). If the archive cannot
  satisfy the exact pin, the image build fails closed.
- `clang`, `clang++`, `clang-tidy`, `clang-format` are the versioned `*-18` binaries registered
  through `update-alternatives`, mirroring the hosted-runner layout the CI previously relied on.
- `g++-13` is installed for the libstdc++ headers used by the canonical clang build (and matches
  the historical CI compiler mix). CMake, Ninja, and Python 3.12 are installed with exact pins for
  image determinism.

## Exact acceptance tool identity

Fail-closed assertions (a mismatch aborts Quality before any build work, printing expected,
actual, and resolved path):

| Tool          | Identity   | Source in contract                          |
| ------------- | ---------- | ------------------------------------------- |
| clang         | 18.1.3     | `LLVM_MAJOR` / `LLVM_MINOR` / `LLVM_PATCH`  |
| clang++       | 18.1.3     | same                                        |
| clang-tidy    | 18.1.3     | same                                        |
| clang-format  | 18.1.3     | same                                        |
| conan         | pinned     | `requirements-tools.txt` (`conan==2.31.2`)  |
| cmake         | >= 3.24.0  | `CMakePresets.json` `cmakeMinimumRequired`  |
| python3       | >= 3.10.0  | `PYTHON_MINIMUM_VERSION`                    |

Recorded but not asserted (printed in the check output): Ninja.

The canonical C++ standard is C++20; the canonical standard library is libstdc++.

## Local canonical Quality command

```bash
bash scripts/quality.sh
```

- Linux: uses Docker or Podman; the canonical amd64 image is built from the pinned digest and
  exact package pins, then Quality runs inside it.
- macOS: Docker Desktop runs the amd64 container through Rosetta 2. If Docker or Podman is
  missing or its daemon is not running, `quality.sh` fails with explicit instructions. Native
  AppleClang is **never** presented as CI-equivalent canonical Quality.
- The working tree is mounted read-only; Quality executes from a fresh scratch copy inside the
  container (`/work`), so the host tree is never polluted with Linux build artifacts.
- Caches (Conan cache, pip cache, tools venv, build trees) live in the container volume
  `bmd-projection-quality-work`; they are invalidated automatically whenever the contract
  fingerprint changes. A cache hit is never trusted without identity validation: the running
  image must be baked from the same contract as the working tree, and the tool versions are
  re-asserted on every run.

The entrypoint performs, in order:

1. contract file validation (fail closed if malformed);
2. image build from the pinned base digest and exact package pins (fail closed if apt cannot
   install the exact versions);
3. image-vs-worktree contract identity check (fail closed on stale images);
4. exact toolchain identity check (fail closed);
5. formatting check with canonical clang-format 18.1.3;
6. repository-local Conan bootstrap;
7. pinned Contracts bootstrap;
8. Debug configure with `BMD_PROJECTION_BUILD_PROTO_ADAPTER=ON`,
   `BMD_PROJECTION_ENABLE_CLANG_TIDY=ON`, `BMD_PROJECTION_ENABLE_WERROR=ON`;
9. build, tests, staged-install consumer;
10. final `CANONICAL QUALITY: PASS` line carrying the exact tool identity.

## Canonical acceptance vs. supplemental local checks

- `bash scripts/quality.sh` is the **only** canonical Quality command. Its result is authoritative
  acceptance semantics.
- `bash scripts/verify.sh` is broad developer verification (formatting, warnings-as-errors,
  sanitizers, M4, fuzz, benchmark smoke, consumers, `git diff --check`). It never claims
  canonical acceptance: it prints `CANONICAL QUALITY: NOT RUN` with the canonical command, unless
  `BMD_PROJECTION_RUN_CANONICAL_QUALITY=1` was set, in which case it ends with
  `CANONICAL QUALITY: PASS`/failure from `quality.sh`.
- A local opportunistic clang-tidy run inside `verify.sh` is supplemental; it can never present a
  pass as CI-equivalent acceptance.
- Local developers may run newer LLVM versions as forward-compatibility checks; those results are
  supplemental and never authoritative.

## How CI invokes it

The `quality` job in `.github/workflows/ci.yml` is orchestration only: it checks out the code,
prints the contract, and runs `bash scripts/quality.sh`. All Quality semantics live in the
repository entrypoint, not in the workflow. The runner label is the concrete `ubuntu-24.04`
(the acceptance toolchain cannot drift with it, because the toolchain is container-owned).

The `quality-toolchain-tests` job runs the deterministic contract tests
(`scripts/test-quality-toolchain.sh`) on every PR/push.

## Fail-closed guarantees

- Wrong major/minor/patch of any of clang, clang++, clang-tidy, clang-format -> Quality aborts.
- Missing tool -> Quality aborts.
- PATH selecting another installation (resolved path outside `/usr/bin` or
  `/usr/lib/llvm-18/bin`) -> Quality aborts.
- AppleClang resolving as the canonical clang -> Quality aborts.
- Malformed contract (missing/empty key, non-numeric version) -> Quality aborts.
- Conan differing from `requirements-tools.txt` -> Quality aborts.
- Image not built from the current contract -> Quality aborts.
- `ubuntu:24.04` mutable tag never used; only the pinned digest is accepted.

Deterministic tests for all of the above live in `scripts/test-quality-toolchain.sh` (no network
access; fake tool shims generated from the repository's own contract files).

## Intentional upgrade procedure

An LLVM/Quality upgrade is a deliberate act requiring an explicit PR that changes the canonical
version identity:

1. Update the single source of truth `.toolchain/quality.env` (new LLVM major/minor/patch and the
   matching exact noble package pins; resolve candidate pins inside the pinned base image with
   `apt-cache policy`).
2. Run canonical Quality locally: `bash scripts/quality.sh` (this rebuilds the image from the new
   contract and invalidates stale caches via the fingerprint).
3. Run the normal compatibility CI (GCC, Clang, AppleClang, sanitizers, M4, replay, fuzz,
   benchmarks).
4. Newly introduced diagnostics must be exposed and resolved through the normal review process;
   they must never be silently suppressed to make CI green.
5. The PR receives independent review.

Hosted-runner image updates cannot redefine acceptance: the acceptance toolchain is
repository-owned, and the contract change itself requires the PR process above.

## Related files

- `.toolchain/quality.env` — contract, single source of truth.
- `.toolchain/Dockerfile` — canonical image definition (sources the contract).
- `scripts/quality.sh` — canonical Quality entrypoint (CI and local).
- `scripts/quality-toolchain-check.sh` — deterministic fail-closed identity validation.
- `scripts/test-quality-toolchain.sh` — deterministic contract/negative tests.
- `.github/workflows/ci.yml` — CI orchestration (quality job, quality-toolchain-tests job).
