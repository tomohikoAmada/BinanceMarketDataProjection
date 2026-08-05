# Agent contribution rules

Before changing this repository, read `README.md`, `ARCHITECTURE.md`, `docs/MILESTONES.md`, and all
relevant files under `docs/adr/`.

- Do not manually edit generated files. `conan.lock` is updated through Conan; generated dependency
  and CMake files belong under ignored build/cache directories.
- Do not add networking, storage, threads, system-time reads, logging, or host runtime concerns to
  Core.
- Do not implement a later milestone early. M0 contains no projection business logic.
- Every public API change requires tests and documentation.
- Never represent price or quantity with floating-point types.
- Do not copy Contracts protobuf definitions into this repository.
- Do not add a Protobuf adapter before M4.
- Run `scripts/verify.sh` for every PR; do not disable checks to make CI green.
- Do not modify the Contracts or Recorder repositories from this workspace.
- Do not use system-global package installation. Keep virtual environments, caches, and builds in
  this repository.
- Preserve single-writer determinism and keep Core independent of Gateway and History.
