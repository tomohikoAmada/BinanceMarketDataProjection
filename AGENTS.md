# Agent contribution rules

Before changing this repository, read `README.md`, `ARCHITECTURE.md`, `docs/MILESTONES.md`, and all
relevant files under `docs/adr/`.

## Source-of-truth precedence

Evidence hierarchy for semantic decisions:

1. **External protocol facts** — official upstream Binance protocol documentation,
   changelog/corrective commits, and official maintained reference implementations, inspected
   directly (commit diffs and checked-out files), override local interpretation when they conflict.
2. **Accepted local semantic/design authority** — accepted ADRs and accepted milestone designs,
   unless an upstream conflict is established and corrected explicitly through the repository
   review process.
3. **Implementation evidence** — exact checked-out code, tests, Git history, build results, and CI
   establish what is implemented and whether it conforms to higher authority. Implementation does
   not become semantic authority merely because it exists.
4. **Status/orientation evidence** — PR bodies, `CURRENT_STATE`-style summaries, milestone status
   summaries, Recorder observations, and unmerged branches are supporting evidence only and must
   not define exchange semantics.

Accepted ADRs govern local deliberate behavior unless contradicted by a higher-authority upstream
protocol fact; such a conflict requires an explicit ADR correction (new ADR or amendment) and
independent review. Never silently rewrite an accepted ADR's history.

A live web page is never a build dependency; verified protocol facts are recorded in an ADR before
code changes rely on them.

- Do not manually edit generated files. `conan.lock` is updated through Conan; generated dependency
  and CMake files belong under ignored build/cache directories.
- Do not add networking, storage, threads, system-time reads, logging, or host runtime concerns to
  Core.
- Do not implement a later milestone early. M2 implements the order book core only; it has no
  sequencing, snapshot, protobuf, or projection-state logic.
- Every public API change requires tests and documentation.
- Never represent price or quantity with floating-point types.
- Do not copy Contracts protobuf definitions into this repository.
- Do not add a Protobuf adapter before M4.
- Run `scripts/verify.sh` for every PR; do not disable checks to make CI green.
- Do not modify the Contracts or Recorder repositories from this workspace.
- Do not use system-global package installation. Keep virtual environments, caches, and builds in
  this repository.
- Preserve single-writer determinism and keep Core independent of Gateway and History.

Recommended AI/reviewer reading order: `docs/CURRENT_STATE.md`, `AGENTS.md`, `README.md`,
`ARCHITECTURE.md`, the current milestone/M5 implementation document, relevant accepted ADRs, actual
code/tests, and the active PR body plus exact-head CI. `docs/CURRENT_STATE.md` is orientation only;
accepted ADRs and semantic designs remain authoritative.
