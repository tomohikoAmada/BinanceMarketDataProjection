# Quality Toolchain Contract (INFRA-TC-001)

This document is the reference for the canonical Quality acceptance environment. It records the
exact toolchain identity, the single repository-owned entrypoint, how CI and local developers
invoke it, the distinction between canonical acceptance and supplemental local checks, and the
intentional upgrade procedure.

The single source of truth for the identity is `.toolchain/quality.env`. No version literal of an
acceptance-critical LLVM tool exists anywhere else in the repository, and no base-image digest
literal exists in the Dockerfile.

## Canonical Quality environment

Canonical Quality runs inside a repository-owned container image:

- **Base image identity** is declared in `.toolchain/quality.env`
  (`CANONICAL_QUALITY_BASE_IMAGE=ubuntu:24.04`, `CANONICAL_QUALITY_BASE_IMAGE_DIGEST=sha256:019e8eb2…`,
  resolved 2026-08-15 from the official Docker Hub repository). `scripts/quality-base-ref.sh`
  computes the single authoritative reference `<image>@<digest>`; `scripts/quality.sh` passes it
  as the `BMD_CANONICAL_BASE_IMAGE_REF` build argument; `.toolchain/Dockerfile` uses it
  verbatim as `FROM ${BMD_CANONICAL_BASE_IMAGE_REF}`. The Dockerfile contains no independent
  digest literal and the build argument has no default: an empty or mutated reference fails the
  build (`base name should not be blank`) or fails the pull. Drift between the contract and the
  actual base image is therefore impossible.
- **Ubuntu archive state** is pinned to one official Ubuntu Snapshot Service identity
  (`UBUNTU_SNAPSHOT_ID=20260815T120000Z`, format `YYYYMMDDTHHMMSSZ`). The image build rewrites
  `/etc/apt/sources.list.d/ubuntu.sources` (via `scripts/quality-apt-sources.sh`) so that BOTH
  the archive pockets (`noble`, `noble-updates`, `noble-backports`) and the security pocket
  (`noble-security`) resolve exclusively from `https://snapshot.ubuntu.com/ubuntu/<ID>/`. No
  live-archive reference exists in the final sources; build logs show the snapshot Get lines and
  the declared snapshot identity.
- **TLS bootstrap before snapshot mode**: the official base image ships no CA store, and the
  snapshot service requires TLS. The single pre-snapshot apt step installs the exact pinned
  `ca-certificates` package from the base image's own shipped default sources (plain HTTP with
  GPG-verified indexes). Every later apt operation runs against the snapshot only, and the
  final image's ca-certificates version is asserted against the contract pin.
- Packages are installed with exact version pins resolved from the snapshot (for example
  `clang-18=1:18.1.3-1ubuntu1`). If the snapshot cannot satisfy any pin, the image build fails
  closed.
- `clang`, `clang++`, `clang-tidy`, `clang-format` are the versioned `*-18` binaries registered
  through `update-alternatives`. `g++-13` is installed for the libstdc++ headers used by the
  canonical clang build.

## Exact acceptance tool identity

Fail-closed assertions (a mismatch aborts Quality before any build work, printing expected,
actual, and resolved path):

| Tool          | Identity   | Binding                                                    |
| ------------- | ---------- | ---------------------------------------------------------- |
| clang         | 18.1.3     | version output + dpkg provenance (package `clang-18`)      |
| clang++       | 18.1.3     | version output + dpkg provenance (package `clang-18`)      |
| clang-tidy    | 18.1.3     | version output + dpkg provenance (package `clang-tidy-18`) |
| clang-format  | 18.1.3     | version output + dpkg provenance (package `clang-format-18`) |
| conan         | pinned     | `requirements-tools.txt` (`conan==2.31.2`)                 |
| cmake         | >= 3.24.0  | `CMakePresets.json` `cmakeMinimumRequired`                 |
| python3       | >= 3.10.0  | `PYTHON_MINIMUM_VERSION`                                   |
| snapshot      | exact ID   | `UBUNTU_SNAPSHOT_ID` (format-validated)                    |

LLVM provenance: the resolved real executable must be owned (per dpkg metadata) by the exact
expected Ubuntu package, and that package's installed version must equal the contract pin. A
correct `--version` text emitted by an arbitrary replacement binary is not sufficient by itself.
Recorded but not asserted: Ninja.

The canonical C++ standard is C++20; the canonical standard library is libstdc++.

## Source-to-binary freshness

Every canonical run executes from a scratch root rebuilt to exactly reflect the current working
tree (`scripts/quality-work-prep.sh`):

- the scratch root is cleaned of every entry except the explicitly permitted persistent caches
  (`.cache` — Conan and pip — and `.venv-tools`, both reconciled against exact pins on every
  run);
- **`build/` never persists**: no CMake/Ninja object, link, or configuration output survives a
  run, so a stale object file cannot produce a false PASS regardless of mtimes;
- previously deleted source files cannot survive a run; source identity is a fresh content copy,
  never mtime-based;
- caches are additionally invalidated whenever the contract fingerprint changes.

Local canonical Quality is **working-tree content acceptance**, not exact Git-commit acceptance.

## Local canonical Quality command

```bash
bash scripts/quality.sh
```

- Linux: uses Docker or Podman; macOS: Docker Desktop (the amd64 container runs through
  Rosetta 2). If no container runtime is present or its daemon is not running, `quality.sh`
  fails with explicit instructions. Native AppleClang is **never** canonical Quality.
- The working tree is mounted read-only; Quality runs from the fresh scratch copy inside the
  container, so the host tree is never polluted.
- The image is rebuilt whenever the contract fingerprint changes; a cache hit is never trusted
  without identity validation (baked-contract diff + runtime tool checks).

The entrypoint performs, in order:

1. contract validation (fail closed if malformed);
2. base-reference computation from the contract and image build with
   `--build-arg BMD_CANONICAL_BASE_IMAGE_REF=<authoritative ref>` and the pinned snapshot;
3. image-vs-worktree contract identity check (fail closed on stale images);
4. exact toolchain identity + dpkg provenance check (fail closed);
5. formatting check with canonical clang-format 18.1.3;
6. repository-local Conan bootstrap; 7. pinned Contracts bootstrap;
8. Debug configure with ProtoAdapter ON, clang-tidy ON, Werror ON;
9. build, tests, staged-install consumer;
10. final `CANONICAL QUALITY: PASS` line carrying the exact tool and snapshot identity.

## Canonical acceptance vs. supplemental local checks

- `bash scripts/quality.sh` is the **only** canonical Quality command. Its result is authoritative
  acceptance semantics.
- `bash scripts/verify.sh` is broad developer verification (formatting, warnings-as-errors,
  sanitizers, M4, fuzz, benchmark smoke, consumers, `git diff --check`). It never claims
  canonical acceptance: it prints `CANONICAL QUALITY: NOT RUN` with the canonical command, unless
  `BMD_PROJECTION_RUN_CANONICAL_QUALITY=1` was set, in which case it ends with
  `CANONICAL QUALITY: PASS`/failure from `quality.sh`.
- A local opportunistic clang-tidy run inside `verify.sh` is supplemental; it can never present a
  pass as CI-equivalent acceptance. Newer LLVM versions run locally are forward-compatibility
  checks only.

## How CI invokes it

The `quality` job in `.github/workflows/ci.yml` is orchestration only: it checks out the code,
prints the contract, and runs `bash scripts/quality.sh`. All Quality semantics live in the
repository entrypoint, not in the workflow. The runner label is the concrete `ubuntu-24.04`
(the acceptance toolchain cannot drift with it, because the toolchain is container-owned).

The `quality-toolchain-tests` job runs the deterministic contract tests
(`scripts/test-quality-toolchain.sh`) on every PR/push.

## Fail-closed guarantees

- Wrong/missing major, minor, or patch of clang, clang++, clang-tidy, clang-format → abort.
- dpkg provenance mismatch (wrong owning package, wrong installed package version, unowned real
  executable) → abort.
- Missing tool, unparseable version output, AppleClang resolution, PATH selecting a non-canonical
  installation → abort.
- Malformed contract (missing/empty key, non-numeric version, malformed `UBUNTU_SNAPSHOT_ID`) →
  abort.
- Conan differing from `requirements-tools.txt` → abort.
- Image not built from the current contract → abort.
- Base-image reference missing/empty/mutated → build fails (no default fallback, no second digest
  source).
- Snapshot identity malformed or unresolvable → build fails; no fallback to the live archive
  (the final sources contain only snapshot URIs, asserted by tests).

Deterministic offline tests: `scripts/test-quality-toolchain.sh` (32 cases, no network; fake
shims generated from the repository's own contract files) — including adversarial stale-object
reuse against the production work-preparation mechanism, source deletion/rollback, base-ref
mutation, snapshot plumbing, and provenance negatives. Live integration proof comes from the
canonical image build itself (CI quality job) and the documented local failure proofs.

## Intentional upgrade procedure

An LLVM/Quality upgrade is a deliberate act requiring an explicit PR that changes the canonical
version identity:

1. Update the single source of truth `.toolchain/quality.env` (LLVM version components, exact
   noble package pins, and — when the archive state must move — a new `UBUNTU_SNAPSHOT_ID`
   that demonstrably contains the full exact pin set; verify candidate pins and snapshots with
   `apt-cache policy` inside the pinned base image).
2. Run canonical Quality locally: `bash scripts/quality.sh` (rebuilds the image from the new
   contract, invalidates stale caches via the fingerprint).
3. Run the normal compatibility CI (GCC, Clang, AppleClang, sanitizers, M4, replay, fuzz,
   benchmarks).
4. Newly introduced diagnostics must be exposed and resolved through the normal review process;
   they must never be silently suppressed to make CI green.
5. The PR receives independent review.

Hosted-runner image updates cannot redefine acceptance: the acceptance toolchain is
repository-owned, and the contract change itself requires the PR process above.

## Retention truthfulness

Reproducibility is guaranteed **against the declared snapshot identity while that official
snapshot remains available on the Ubuntu Snapshot Service**. The official retention contract is:
snapshots are available for any date/time after 1 March 2023, and Canonical "intend[s] to ensure
snapshots are available for dates up to at least 2 years in the past, which [they] may extend if
there is demand". This project makes no stronger or indefinite retention claim; a future
long-term archival policy decision would be a separate task.

## Related files

- `.toolchain/quality.env` — contract, single source of truth.
- `.toolchain/Dockerfile` — canonical image definition (FROM bound to the contract-provided
  build argument; snapshot-pinned apt sources; TLS bootstrap step).
- `scripts/quality-base-ref.sh` — authoritative `<image>@<digest>` computation.
- `scripts/quality-apt-sources.sh` — snapshot-only apt sources generation.
- `scripts/quality-work-prep.sh` — canonical scratch preparation (fresh source, never-persisted
  build tree).
- `scripts/quality.sh` — canonical Quality entrypoint (CI and local).
- `scripts/quality-toolchain-check.sh` — deterministic fail-closed identity/provenance
  validation.
- `scripts/test-quality-toolchain.sh` — deterministic contract/negative/adversarial tests.
- `.github/workflows/ci.yml` — CI orchestration (quality job, quality-toolchain-tests job).
