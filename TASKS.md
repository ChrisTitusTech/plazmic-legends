# Plazmic Legends tasks

Task status changes only after acceptance criteria and required validation
pass. Record skipped validation and residual risk instead of marking incomplete
work done.

## Current phase

The planning scaffold is complete. Phase 0 cleanup is waiting for the user to
say to begin. Do not delete imported files or begin runtime implementation yet.

### P0-01: Preserve the imported baseline

- [ ] Create a recoverable commit containing the untouched imported tree and
  validated planning scaffold.
- Acceptance criteria: the commit restores the complete pre-cleanup inventory,
  including known incomplete paths.
- Validation: clean-checkout inventory, file counts, disk usage, and status.
- Blocker: the current branch has no commits.

### P0-02: Finalize the cleanup manifest

- [ ] Expand `docs/cleanup-inventory.md` into exact keep/remove/replace paths.
- Acceptance criteria: every top-level path and retained dependency has a Phase
  1 purpose; uncertain files are explicitly classified.
- Validation: source/build/reference searches and manual license review.
- Dependency: P0-01.

### P0-03: Remove non-product platforms and features

- [ ] Remove Visual Studio/MSVC/vcpkg build-host files, PowerShell conversion
  tools, bundled Windows Python, plugins, macros, login automation, remote
  services, emulator/live variants, Windows injection/Detours, DirectX
  backends, and unrelated data/assets.
- Acceptance criteria: removed paths match the manifest and no dangling
  reference remains.
- Validation: bounded deletion diffs, reference scans, and before/after
  inventory.
- Dependency: P0-02.

### P0-04: Reduce retained research code

- [ ] Retain only backend-independent code needed to evaluate Linux process
  reads, game profiles, snapshots, and an X11 overlay in Phase 1.
- Acceptance criteria: each retained directory has an owner and stated Phase 1
  research question.
- Validation: include/dependency graph review and license/provenance audit.
- Dependency: P0-03.

### P0-05: Establish the Linux research scaffold

- [ ] Replace inherited Windows build metadata with the smallest Linux-native
  configure/check setup needed for Phase 1.
- Acceptance criteria: clean Fedora checkout runs documented checks without
  Visual Studio, MSVC, Windows PowerShell, or a Windows build host.
- Validation: clean configure/check, fingerprint tool, diff check, and package
  inventory.
- Dependency: P0-04.

## Queued phases

- [ ] Phase 1: Native Linux/Wine architecture proof.
- [ ] Phase 2: Minimal Linux runtime shell.
- [ ] Phase 3: Read-only game-data vertical slice.
- [ ] Phase 4: Hardening and release readiness.

Break queued phases into reviewable tasks only after the preceding approval
checkpoint.

## Completed pre-phase work

### S-01: Create the AI project scaffold

- [x] Added project instructions, specification, roadmap, and task queue.
- Validation: documents define Linux scope, cleanup-first ordering, acceptance
  criteria, validation, rollback, and hard pause points.
- Residual risk: Phase 1 integration and product decisions remain unresolved.

### S-02: Record the reference executable baseline

- [x] Added a read-only PE fingerprint tool and research record.
- Validation: Python compile check and exact SHA-256 match against the local
  reference executable.
- Residual risk: static strings suggest Direct3D 9, but active rendering has
  not been confirmed at runtime.
