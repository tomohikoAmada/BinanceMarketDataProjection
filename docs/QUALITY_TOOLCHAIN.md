# Quality Toolchain Contract (INFRA-TC-001)

## Status

INFRA-TC-001 is **IMPLEMENTED / PENDING FINAL INDEPENDENT RE-REVIEW** (PR #23). A green
exact-head CI run is candidate evidence only and is **not** independent acceptance of this
contract. This document describes the candidate as implemented; it does not declare the
contract accepted.

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
  (`UBUNTU_SNAPSHOT_ID=20260814T120000Z`, format `YYYYMMDDTHHMMSSZ`). The ID must be a **real
  UTC calendar moment** (format alone is never sufficient: `20260230T120000Z`, month 13,
  day 32, hour 24, minute/second 60, and non-leap-day February 29 are all rejected) and MUST
  be historical: the toolchain check rejects any snapshot later than the current UTC time
  (deterministic, before any image construction). The image build rewrites
  `/etc/apt/sources.list.d/ubuntu.sources` (via `scripts/quality-apt-sources.sh`) so that BOTH
  the archive pockets (`noble`, `noble-updates`, `noble-backports`) and the security pocket
  (`noble-security`) resolve exclusively from `https://snapshot.ubuntu.com/ubuntu/<ID>/`. No
  live-archive reference exists anywhere in the build; build logs show only snapshot Get lines
  and the declared snapshot identity.
- **TLS bootstrap is an immutable artifact, not a live-archive install.** The pristine base
  image ships no CA store and the snapshot service requires TLS (the officially documented
  deb822 `Snapshot:` mechanism was evaluated from the pristine base and does not work without
  CA material: index fetches fall back to live hosts and no pinned version resolves). The
  exact `ca-certificates` package payload retrieved from the historical snapshot is committed
  at `.toolchain/bootstrap/ca-certificates.deb` and pinned by
  `CA_CERTIFICATES_BOOTSTRAP_SHA256` in the contract. The Dockerfile verifies the artifact
  bytes before use and extracts only TLS trust material (`Acquire::https::CaInfo`); no package
  is installed and no network contact happens before snapshot mode. The same package version is
  installed properly from the snapshot itself.
- Packages are installed with exact version pins resolved from the snapshot (for example
  `clang-18=1:18.1.3-1ubuntu1`), with archive signature verification (`Signed-By`) intact. If
  the snapshot cannot satisfy any pin, the image build fails closed.
- `clang`, `clang++`, `clang-tidy`, `clang-format` are the versioned `*-18` binaries registered
  through `update-alternatives`. `g++-13` is installed for the libstdc++ headers used by the
  canonical clang build.

## Exact acceptance tool identity

Fail-closed assertions (a mismatch aborts Quality before any build work, printing expected,
actual, and resolved path):

| Tool          | Identity   | Binding                                                    |
| ------------- | ---------- | ---------------------------------------------------------- |
| clang         | 18.1.3     | version output + dpkg provenance (package `clang-18`) + payload md5 |
| clang++       | 18.1.3     | version output + dpkg provenance (package `clang-18`) + payload md5 |
| clang-tidy    | 18.1.3     | version output + dpkg provenance (package `clang-tidy-18`) + payload md5 |
| clang-format  | 18.1.3     | version output + dpkg provenance (package `clang-format-18`) + payload md5 |
| conan         | pinned     | `requirements-tools.txt` (`conan==2.31.2`)                 |
| cmake         | >= 3.24.0  | `CMakePresets.json` `cmakeMinimumRequired`                 |
| python3       | >= 3.10.0  | `PYTHON_MINIMUM_VERSION`                                   |
| snapshot      | historical  | `UBUNTU_SNAPSHOT_ID` format-, real-calendar-, and temporal-validated (never in the future) |

LLVM provenance: the resolved real executable must be owned (per dpkg metadata) by the exact
expected Ubuntu package, that package's installed version must equal the contract pin, and the
executable payload's md5 must match the owning package's recorded md5sum — a payload modified
while the dpkg database (and `--version` output) stays unchanged is detected. Recorded but not
asserted: Ninja.

The canonical C++ standard is C++20; the canonical standard library is libstdc++.

## Source-to-binary freshness

Every canonical run executes from a scratch root rebuilt to exactly reflect the current working
tree (`scripts/quality-work-prep.sh`):

- **`/work` is ephemeral per run** (no persistent /work volume); the scratch root is a fresh
  content copy of the working tree every run, so previously deleted source files cannot survive
  and source identity never depends on mtimes;
- **`build/` never persists**: no CMake/Ninja object, link, or configuration output survives a
  run, so a stale object file cannot produce a false PASS regardless of mtimes;
- persistent named volumes are mounted only at `/work/.cache` and `/work/.venv-tools`, and their
  names are namespaced by the canonical CACHE KEY (`scripts/quality-cache-key.sh`):
  `bmd-projection-quality-cache-${CACHE_KEY}` / `bmd-projection-quality-venv-${CACHE_KEY}`;
- the cache key is the SHA-256 of the normalized `.toolchain/quality.env` plus the exact
  `requirements-tools.txt` contents — covering the base identity, the historical snapshot, every
  exact package pin (including LLVM patch), the bootstrap artifact hash, and the Conan tool
  version. A contract change therefore produces a different namespace: old-contract Conan
  binaries are structurally invisible to a new contract, and no lifecycle-sensitive fingerprint
  file is needed.

Local canonical Quality is **working-tree content acceptance**, not exact Git-commit acceptance.

## Local canonical Quality command

```bash
bash scripts/quality.sh
```

- Linux: uses Docker Engine; macOS: Docker Desktop (backed by Docker Engine; the amd64
  container runs through Rosetta 2). The backend identity is **enforced**, not assumed: a
  `docker` executable alone is insufficient — `quality.sh` verifies the SERVER identity
  (`docker version --format '{{json .Server}}'`) and requires a real Docker Engine backend
  (Components array including the Engine component). **Podman and docker-compatible Podman
  wrappers (podman-docker / libpod backends) are NOT accepted canonical runtimes**, and an
  unknown/unparseable/unreachable backend fails closed. If Docker is absent or its daemon is
  not running, `quality.sh` fails with explicit instructions. Native AppleClang is **never**
  canonical Quality.
- Test-only validation hooks (`BMD_QUALITY_CONTRACT_FILE`, `BMD_QUALITY_REFERENCE_TIME`,
  `BMD_QUALITY_TOOLCHAIN_DIR`, `BMD_QUALITY_DPKG_INFO_DIR`, `BMD_QUALITY_WORK_KEEP`) and
  stale internal orchestration variables (`BMD_CANONICAL_QUALITY_CONTAINER`,
  `BMD_CANONICAL_QUALITY_SRC`, `BMD_CANONICAL_QUALITY_WORK`) are **rejected by the
  canonical entrypoint**: if any is present in the environment, `quality.sh` aborts before
  container image construction. The container execution mode is **not** ambient-selectable,
  and the canonical source/work roots are **fixed** (`/src`, `/work`) — they cannot be
  substituted by the environment. The test hooks exist only for the deterministic tests,
  which invoke the checker directly.
- The working tree is mounted read-only; Quality runs from the fresh scratch copy inside the
  container, so the host tree is never polluted.
- The image is rebuilt whenever the contract fingerprint changes; a cache hit is never trusted
  without identity validation (baked-contract diff + runtime tool checks).
- The internal container mode (`--inside-canonical-container`, generated only by the trusted
  host path) proves the canonical image boundary **before** any source copy or recursive
  execution: the image-baked contract (`/opt/toolchain/quality.env`) must exist and equal the
  mounted `/src` contract; the same proof is re-run against the `/work` scratch tree. A direct
  invocation of the internal mode outside the canonical image fails closed. Threat-model
  scope: this proves the image/runtime boundary as built by the canonical host path; it does
  not claim protection against a malicious privileged host that can rewrite arbitrary
  root-filesystem or container-engine state.

The entrypoint performs, in order:

1. rejection of test-only `BMD_QUALITY_*` hooks and stale `BMD_CANONICAL_QUALITY_*`
   orchestration variables (canonical runs cannot be influenced by test-only overrides, and
   ambient environment can never select the container mode or substitute source/work roots);
2. contract validation (fail closed if malformed, duplicate-keyed, calendar-invalid, or
   future-dated);
3. Docker Engine backend identity validation (fail closed on Podman/libpod/unknown backend);
4. base-reference computation from the contract and image build with
   `--build-arg BMD_CANONICAL_BASE_IMAGE_REF=<authoritative ref>` and the pinned snapshot;
5. container run: internal mode proves the canonical image boundary (baked contract ==
   mounted `/src` contract) before source transfer;
6. image-vs-worktree contract identity re-check on the `/work` scratch tree (fail closed on
   stale images);
7. exact toolchain identity + dpkg provenance check (fail closed);
8. formatting check with canonical clang-format 18.1.3;
9. repository-local Conan bootstrap; 10. pinned Contracts bootstrap;
11. Debug configure with ProtoAdapter ON, clang-tidy ON, Werror ON;
12. build, tests, staged-install consumer;
13. final `CANONICAL QUALITY: PASS` line carrying the exact tool and snapshot identity.

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
- Installed payload modification (real executable md5 differs from the owning package's recorded
  md5sum) → abort, even when dpkg metadata and `--version` output are unchanged.
- Missing tool, unparseable version output, AppleClang resolution, PATH selecting a non-canonical
  installation → abort.
- Malformed contract (missing/empty key, non-numeric version) → abort.
- **Duplicate key assignment in the contract → abort.** A contract may not assign any key
  twice; there is no first-wins or last-wins interpretation. All consumers
  (`quality-toolchain-check.sh`, `quality-base-ref.sh`, `quality-cache-key.sh`) reject the
  file before consuming any value, so the repository parser and the shell sourcing used by
  the Dockerfile can never interpret one file differently.
- **Test-only `BMD_QUALITY_*` hooks or stale `BMD_CANONICAL_QUALITY_*` orchestration
  variables in the canonical entrypoint environment → abort** before image construction: the
  authoritative contract path is always `<repo>/.toolchain/quality.env`, the reference UTC
  time is always the real current clock, the container mode is never ambient-selectable, and
  the source/work roots are always the fixed `/src` and `/work`.
- **Container runtime backend is not a real Docker Engine server → abort**: Podman/libpod
  backends exposed through a `docker` command, unknown/unparseable identities, and
  unreachable daemons fail closed before image construction.
- **Internal container mode outside the canonical image → abort** before any source copy or
  recursive execution: the baked `/opt/toolchain/quality.env` must exist and equal the
  mounted `/src` contract; a direct manual invocation of
  `--inside-canonical-container` on a host fails closed.
- Snapshot ID malformed, calendar-invalid, or **in the future relative to the current UTC
  time** → abort before any image construction (future IDs can never be an acceptance
  snapshot).
- Conan differing from `requirements-tools.txt` → abort.
- Image not built from the current contract → abort.
- Base-image reference missing/empty/mutated → build fails (no default fallback, no second digest
  source).
- TLS bootstrap artifact bytes not matching `CA_CERTIFICATES_BOOTSTRAP_SHA256` → build fails.
- No package-resolution operation ever touches a mutable live archive: the only apt sources are
  the historical snapshot URIs (both pockets), archive signatures verified via `Signed-By`.

Deterministic offline tests: `scripts/test-quality-toolchain.sh` (89 cases, no network; fake
shims generated from the repository's own contract files) — including adversarial stale-object
reuse against the production work-preparation mechanism, source deletion/rollback, base-ref
mutation, snapshot temporal/calendar plumbing, duplicate-key fail-closed (including arbitrary
keys and conflicting values), cache-namespace key changes, payload tamper, entrypoint-level
test-hook isolation proofs, canonical-image-boundary proofs, Docker-Engine-backend identity
proofs (Podman/libpod and unknown backends rejected via fake `docker` shims), and
internal-orchestration-variable adversarial proofs (fake docker shim, no image-build command
reached). Live integration proof comes from the canonical image build itself (CI quality job)
and the documented local failure proofs.

## Intentional upgrade procedure

An LLVM/Quality upgrade is a deliberate act requiring an explicit PR that changes the canonical
version identity:

1. Choose an **already historical** `UBUNTU_SNAPSHOT_ID` (never a future timestamp) and prove it
   resolves the full exact pinned package set (probe `apt-cache policy` inside the pinned base
   image with the artifact-provided TLS trust).
2. Update the single source of truth `.toolchain/quality.env` (LLVM version components, exact
   noble package pins, snapshot ID; re-pin `CA_CERTIFICATES_BOOTSTRAP_SHA256` if the bootstrap
   artifact moves).
3. The cache namespace changes automatically with the contract (cache key), so prior-contract
   Conan/tool caches are structurally invisible to the new contract.
4. Run canonical Quality locally: `bash scripts/quality.sh` (rebuilds the image from the new
   contract; /work is ephemeral, so Projection build outputs are always fresh).
5. Run the normal compatibility CI (GCC, Clang, AppleClang, sanitizers, M4, replay, fuzz,
   benchmarks).
6. Newly introduced diagnostics must be exposed and resolved through the normal review process;
   they must never be silently suppressed to make CI green.
7. The PR receives independent review.

Hosted-runner image updates cannot redefine acceptance: the acceptance toolchain is
repository-owned, and the contract change itself requires the PR process above.

## Retention truthfulness

Clean reconstruction depends on continued availability of external immutable inputs including:

- the pinned Docker base-image digest; and
- the declared Ubuntu Snapshot Service snapshot.

The committed CA bootstrap artifact is repository-owned. No indefinite external-artifact
retention guarantee is claimed. For the snapshot specifically: the official Ubuntu Snapshot
Service retention contract states snapshots are available for any date/time after 1 March 2023,
and Canonical "intend[s] to ensure snapshots are available for dates up to at least 2 years in
the past, which [they] may extend if there is demand". A future long-term archival policy
decision would be a separate task.

## Related files

- `.toolchain/quality.env` — contract, single source of truth (includes
  `CA_CERTIFICATES_BOOTSTRAP_SHA256`).
- `.toolchain/bootstrap/ca-certificates.deb` — committed immutable TLS bootstrap artifact
  (bytes pinned by the contract hash; retrieved from the historical snapshot).
- `.toolchain/Dockerfile` — canonical image definition (FROM bound to the contract-provided
  build argument; artifact-verified TLS bootstrap; snapshot-pinned apt sources).
- `scripts/quality-base-ref.sh` — authoritative `<image>@<digest>` computation.
- `scripts/quality-runtime-check.sh` — Docker Engine backend identity enforcement
  (Podman/libpod and unknown backends rejected).
- `scripts/quality-image-boundary.sh` — canonical image boundary proof for the internal
  container mode (baked contract vs mounted source).
- `scripts/quality-apt-sources.sh` — snapshot-only apt sources generation.
- `scripts/quality-cache-key.sh` — canonical cache namespace identity.
- `scripts/quality-work-prep.sh` — canonical scratch preparation (fresh source, ephemeral
  /work, never-persisted build tree).
- `scripts/quality.sh` — canonical Quality entrypoint (CI and local).
- `scripts/quality-toolchain-check.sh` — deterministic fail-closed identity/provenance/payload/
  snapshot-temporal validation.
- `scripts/test-quality-toolchain.sh` — deterministic contract/negative/adversarial tests.
- `.github/workflows/ci.yml` — CI orchestration (quality job, quality-toolchain-tests job).
