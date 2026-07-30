# Plazmic Legends tasks

Task status changes only after acceptance criteria and required validation
pass. Record skipped validation and residual risk instead of marking incomplete
work done.

## Current phase

Phase 0 is complete. Phase 1 is waiting for explicit user approval. Do not
inspect a running game process, build an overlay proof, or begin product runtime
implementation yet.

## Queued

- [ ] Phase 1: Native Linux/Wine architecture proof.
- [ ] Phase 2: Minimal Linux runtime shell.
- [ ] Phase 3: Read-only game-data vertical slice.
- [ ] Phase 4: Hardening and release readiness.

Break queued phases into reviewable tasks only after the preceding approval
checkpoint.

## Completed Phase 0

### P0-01: Preserve the imported baseline

- [x] Preserved all 1,431 imported and scaffold files.
- Evidence: commits `e3034e1` and `a6204d9`; annotated tag
  `phase0-import-baseline`.
- Validation: included normally ignored plugin, IDE, and bundled Python files;
  no Daybreak executable, credential file, crash dump, or local log was found.

### P0-02: Finalize the cleanup manifest

- [x] Classified every retained and removed top-level path.
- Evidence: `docs/cleanup-inventory.md`.
- Validation: retained-path purpose review and import provenance audit.

### P0-03: Remove non-product platforms and features

- [x] Removed the inherited Windows build and MacroQuest platform.
- Evidence: cleanup commit `f296591` deleted 1,419 tracked files and 615,570
  lines.
- Validation: removed-path, binary/artifact, credential, and dangling build
  reference scans.

### P0-04: Reduce retained research code

- [x] Removed all inherited implementation and bundled third-party source.
- Evidence: the active tree contains only planning, research documents, the
  independently written PE inspector, and its tests.
- Validation: no imported implementation license marker, binary dependency,
  game asset, source directory, or runtime artifact remains.

### P0-05: Establish the Linux research scaffold

- [x] Replaced the inherited build with Linux CMake/Ninja presets and a
  dependency-free validation target.
- Evidence: `CMakeLists.txt`, `CMakePresets.json`, and
  `tests/test_inspect_eqgame.py`.
- Validation: clean-checkout configure, repository gate, three unit tests,
  CTest, exact client fingerprint, and clean Git status passed.

## Completed pre-phase work

### S-01: Create the AI project scaffold

- [x] Added project instructions, specification, roadmap, and task queue.
- Validation: documents define Linux scope, cleanup-first ordering, acceptance
  criteria, validation, rollback, and hard pause points.

### S-02: Record the reference executable baseline

- [x] Added a read-only PE fingerprint tool and research record.
- Validation: Python checks and exact SHA-256 match against the local reference
  executable.
- Residual risk: static strings suggest Direct3D 9, but active rendering has
  not been confirmed at runtime.
