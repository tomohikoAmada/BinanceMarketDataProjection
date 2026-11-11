# Projection M-P2.A-C Result

- Date: 2026-11-11
- Base commit: `bb4dfd69b2b2346a262d73faa8fa3979faefd428`
- Status: **COMPLETE** (implementation, local validation, and documentation)
- Scope: Projection M-P2.A, M-P2.B, M-P2.C, and historical-document cleanup

This record summarizes the changes made after the base commit. Accepted ADR history is preserved;
where an accepted decision changed, the repository uses an amendment note or a new ADR rather than
rewriting the original text.

## M-P2.A — in-place accepted apply

### What changed

- `BookProjection::apply` no longer copies the full book into a candidate `OrderBook`.
  - Empty batches skip book work.
  - Non-empty batches call `OrderBook::apply_updates` directly on the live book.
- `OrderBook::copy_from_map` reserves the expected output size.
- Same-price insert/update uses one map lookup (`try_emplace`), preserving
  `Inserted`/`Updated`/`Unchanged`.
- Deleted the two synthetic global-`operator new` allocation-failure test targets and their source
  files.
- Added [ADR-0010](adr/ADR-0010-in-place-accepted-apply.md), superseding only ADR-0005's
  copy-on-apply transaction decision. Baseline installation remains transactional; accepted apply
  keeps the basic exception guarantee.

### Evidence

- Former D5000 accepted apply: 10,029 allocations / 1,004,312 requested bytes.
- D5000 existing-price accepted B10/B100 after the change: 0 allocations / 0 bytes.
- D0/B10 empty-book insertion: 10 allocations / 480 bytes (one node per new price level).
- New regression coverage:
  - `AllocationInstrumentationAdversarial.M3AcceptedApplyMutatesInPlace`
  - `AllocationInstrumentationAdversarial.M3EmptyAcceptedBatchDoesNoBookWork`

## M-P2.B — ProtoAdapter moves, standard setters, trusted fast path

### What changed

- Bid and ask depth updates are appended into one pre-reserved `std::vector<LevelUpdate>` instead of
  two vectors plus a second merge allocation.
- Adapted level vectors and metadata are moved into the returned owner instead of copied.
- Snapshot/gap output uses standard Protobuf `set_*`, `mutable_*`, repeated-field `Add()`, and
  `Reserve()` APIs. Manual `set_allocated_*` ownership assembly and the `std::bad_alloc`
  catch/clear branch are removed.
- Added same-process trusted entries:
  - `adapt_trusted_exchange_depth_snapshot`
  - `adapt_trusted_depth_update`
  - `AdaptedBookBaseline::install_into_trusted`
  - `AdaptedDepthBatch::apply_to_trusted`

The trusted entries skip repeated fixed-metadata UTF-8/identity scans and target
`NumericSpec`/policy comparisons. They still validate level decimals, sequence ranges, metadata
presence, and quality enums. Checked entries remain the default external boundary.

### Evidence

Local Release benchmark means:

| Cell | Before | After |
|---|---:|---:|
| `M4/AdaptDepthUpdate/Spot/10` | 0.2743 us | 0.2572 us |
| `M4/AdaptDepthUpdate/Spot/10` allocations | 2 / 480 bytes | 1 / 240 bytes |

An exploratory standalone loop measured the same 10-level update at approximately 256 ns/op checked
versus 232 ns/op trusted. Snapshot timing remains dominated by per-level decimal formatting and
string allocation; its timing change is small.

New tests:

- `TrustedAdapterTest.SkipsRepeatedFixedMetadataValidation`
- `TrustedAdapterTest.SkipsRepeatedTargetBindingCheck`

## M-P2.C — CI deduplication and script removal

### What changed

- Removed the duplicate Ubuntu GCC and Ubuntu Clang Release entries from the `m4-static` matrix.
  Release builds remain covered by the `build-matrix` jobs.
- Deleted the 1,248-line `scripts/test-quality-toolchain.sh` fake-environment suite.
- Removed the `quality-toolchain-tests` CI job.
- Removed the deleted suite from `scripts/verify.sh`, `AGENTS.md`,
  `docs/QUALITY_TOOLCHAIN.md`, and P0 reproduction instructions.
- `scripts/quality.sh` remains the single canonical Quality entrypoint.

### Validation

- Workflow YAML parses successfully.
- Current workflow has 10 jobs; `m4-static` has four entries:
  Ubuntu GCC Debug, Ubuntu Clang Debug, macOS AppleClang Debug, macOS AppleClang Release.
- Every `scripts/*.sh` path referenced by the workflow exists.
- `bash -n` passes for changed shell scripts.

## Documentation cleanup

- Compressed `docs/M5_PHASE7_PREIMPLEMENTATION_DECISIONS.md` from 2,429 lines to a concise current
  record; the original text remains in Git history at `bb4dfd6`.
- Compressed `docs/CURRENT_STATE.md` to current orientation and added M-P2.A/B/C status.
- Corrected stale full-book, strong-guarantee, allocation-failure-test, P2-conditional, and
  old-allocation-count statements in README, ARCHITECTURE, MILESTONES, M2/M3/M4/M5 design,
  P1 results, CHANGELOG, and Quality documentation.
- Added explicit amendment notes to ADR-0005, ADR-0006, and ADR-0008; original accepted text is
  retained.

## Final local validation

| Check | Result |
|---|---|
| Release CTest (ProtoAdapter ON) | **514/514 PASS** |
| ASan CTest (ProtoAdapter ON) | **515/515 PASS** |
| UBSan CTest (ProtoAdapter ON) | **514/514 PASS** |
| `clang-format --dry-run --Werror` on changed C++ files | PASS |
| `git diff --check` | PASS |
| Workflow YAML parse | PASS |

`scripts/quality.sh` was not run on this host because Docker Engine was unavailable. The GitHub
Actions `quality` job is unchanged and remains containerized. This record was prepared before the
push; the pushed commit triggers the normal GitHub Actions CI run.
