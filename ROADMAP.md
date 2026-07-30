# Plazmic Legends roadmap

The planning scaffold precedes implementation phases. Each phase produces one
reviewable result and stops at its approval checkpoint.

## Phase 0: Inventory-driven cleanup

Status: Complete on 2026-07-29. See
`docs/phase0-cleanup-report.md` for evidence.

### Outcome

The broad, incomplete MacroQuest import becomes a small, recoverable research
base containing only material that may support the Linux/Wine MVP.

### Included work

- Commit the untouched imported tree and planning scaffold as a recovery point.
- Confirm upstream provenance and preserve required notices.
- Turn `docs/cleanup-inventory.md` into an exact file-level keep/remove/replace
  manifest.
- Remove traditional EverQuest, emulator, plugin, macro, scripting, login,
  remote-service, crash-upload, conversion, bundled interpreter, data-asset,
  Visual Studio, MSVC, and Windows build-host paths.
- Remove inherited README text, names, badges, and build instructions that do
  not describe Plazmic Legends.
- Remove Windows process injection, Detours, DirectX renderer backends, PE
  runtime targets, and Wine DLL override concepts.
- Retain backend-independent UI and game-structure code only when it has a
  documented purpose in Phase 1 research.
- Replace the inherited build configuration with a minimal Linux scaffold for
  research tools; do not implement the product runtime.
- Record before/after source, dependency, and disk-size inventories.

### Dependencies and risks

- The current branch has no commits, so deletion is unrecoverable until the
  baseline commit exists.
- `src/eqlib` is already missing; provenance must be resolved without silently
  inventing it.
- Cleanup before the integration spike may remove useful reference code.
  Uncertain candidates should be archived by Git and removed from the working
  tree, not rewritten.
- Retained GPLv2 and third-party notices must survive deletion.

### Exit criteria

- The baseline commit can restore every imported file.
- Every top-level retained path has a Phase 1 purpose.
- Every deletion is represented in the cleanup manifest and has no dangling
  include, build, documentation, or license reference.
- Visual Studio, MSVC, PowerShell build, bundled Windows Python, plugin, macro,
  login automation, and traditional/emu build paths are absent.
- The Linux research-tool configure/check commands work from a clean checkout.
- No live client integration or product feature was implemented.

### Validation

- Compare inventories and disk usage with the baseline commit.
- Search for dangling removed-path references and obsolete MacroQuest build
  instructions.
- Run Linux configure/check commands and the PE fingerprint tool.
- Audit retained license files and third-party notices.
- Inspect the full deletion diff in bounded batches.

### Rollback

Restore any wrongly removed reference from the baseline commit. Do not rewrite
or force-update the baseline.

### Pause point

The final keep/remove inventory and cleanup evidence are recorded. Phase 1 is
waiting for explicit approval before runtime reconnaissance.

## Phase 1: Native Linux/Wine architecture proof

### Outcome

Runtime evidence proves that one native Linux process can observe the
Wine-hosted client and show a separate diagnostic X11 overlay safely.

### Included work

- Record Linux, Wine, launch, process/module, renderer, window-mode, and client
  fingerprint evidence.
- Validate least-privilege Linux process discovery, module maps, and read-only
  access without injection or host-security changes.
- Build an isolated native Linux X11 proof of life.
- Display project/version and compatibility diagnostics only; do not read game
  state.
- Identify the smallest client symbols needed by proposed MVP fields and
  document validation strategies.
- Resolve the MVP fields, hotkey, dependency set, distribution intent, and
  policy risk.

### Dependencies and risks

- Live testing can crash the client and requires explicit approval.
- Linux process-inspection restrictions may make an external reader unsafe or
  impractical.
- X11 focus, stacking, fullscreen, and compositor behavior may limit the
  external overlay.
- Client patching can invalidate observations.

### Exit criteria

- The native reader can identify the correct Wine process and module maps with
  same-user, non-elevated access.
- The proof configures and builds entirely on Linux and emits ELF artifacts
  only.
- The proof renders, toggles, and shuts down cleanly on the exact reference
  hash.
- It requires no root, injection, Wine override, or weakened host security.
- Required data symbols have validation strategies; guessed offsets are
  rejected.
- Every unresolved decision in `SPEC.md` is answered or deferred outside MVP.
- The user approves the architecture and data set.

### Validation

- Capture configure/build/test output for each viable proof.
- Record host, Wine, executable fingerprint, modules, and active renderer.
- Test launch, overlay toggle, input pass-through, controlled failure, and game
  exit.
- Inspect project processes, handles, threads, and Wine state after shutdown.
- Review new dependencies and licenses.

### Rollback

Keep proofs isolated from the cleaned production targets. Remove their output
and restore the Phase 0 tree if process access or overlay shutdown is unsafe.

### Pause point

Present comparison evidence and wait for explicit approval before Phase 2.

## Phase 2: Minimal Linux runtime shell

### Outcome

A clean Linux project builds a launcher, the approved integration boundary, and
a diagnostics overlay without inherited platform features.

### Included work

- Establish the target source layout in `AGENTS.md`.
- Implement typed fingerprint/profile selection with fail-closed errors.
- Implement structured lifecycle, logging, XDG configuration, overlay toggle,
  positioning, input pass-through, and idempotent shutdown.
- Keep only dependencies used by the shell.
- Add unit tests and deterministic fake adapters.
- Document exact Linux configure, format, build, test, and package commands.

### Dependencies and risks

- Phase 1 architecture approval is required.
- Wine process layouts and Linux access policy can change across host updates.
- Overlay focus, stacking, and compositor behavior can interfere with X11 input.

### Exit criteria

- A clean Fedora checkout builds and tests with documented commands.
- An unsupported client is rejected before process integration.
- The diagnostic overlay passes lifecycle and input smoke tests.
- The package contains only required Linux ELF artifacts, license material, and
  defaults.

### Validation

- Run format/lint, warnings-as-errors build, tests, and package inspection.
- Test wrong path, wrong architecture, changed hash, integration failure,
  repeated initialization, and shutdown.
- Repeat the approved Wine/X11 manual smoke test.

### Rollback

Keep integration behind one interface so it can be disabled or reverted
without changing profile, game, or overlay logic.

### Pause point

Show the clean package and lifecycle evidence before reading game state.

## Phase 3: Read-only game-data vertical slice

### Outcome

The approved MVP panel displays trustworthy Legends data through one immutable
snapshot boundary.

### Included work

- Implement the approved profile and symbol validation.
- Add typed readers for approved zone, player, and target fields.
- Convert raw observations into renderer-independent snapshots.
- Implement not-in-world, zoning, no-target, unavailable, and stale handling.
- Add synthetic fixtures containing no proprietary or personal data.

### Dependencies and risks

- Client structures may change independently of visible version metadata.
- Pointer-lifetime mistakes can crash or leak stale data during transitions.

### Exit criteria

- Every displayed value has a documented source and validation method.
- Tests cover conversion, nullability, invalidation, and profile mismatch.
- Controlled manual tests match visible in-game ground truth.
- Zoning, camping, character select, and exit are safe and show no stale data.
- No game-state write or input path exists.

### Validation

- Run the full Phase 2 gate plus reader and snapshot tests.
- Compare fields against controlled ground truth.
- Exercise target acquire/clear and all lifecycle transitions.
- Measure reader and overlay p95 cost on the reference host.

### Rollback

Disable the game adapter at its single integration boundary while retaining the
diagnostic overlay.

### Pause point

Review field accuracy, privacy, and performance before release hardening.

## Phase 4: Hardening and release readiness

### Outcome

A minimal, reproducible Linux package is ready for the approved distribution
model and fails safely after client updates.

### Included work

- Remove any remaining research-only code and dependencies.
- Add client-update detection and a documented profile-refresh workflow.
- Complete security, privacy, provenance, dependency, and license audits.
- Finalize install, upgrade, removal, troubleshooting, and rollback docs.
- Produce checksummed artifacts from a clean checkout.
- Obtain independent review.

### Dependencies and risks

- Distribution requires the Phase 1 policy decision.
- A client patch can invalidate a release candidate immediately.

### Exit criteria

- Every retained path and dependency maps to a `SPEC.md` requirement.
- CI and local gates pass on the exact release commit.
- Artifacts install, run, and uninstall on the approved Linux/Wine/X11 tier.
- Unsupported clients fail closed with actionable status.
- The package contains no Daybreak content, local user data, or research tools.
- Checksums, licenses, support matrix, known risks, and rollback steps accompany
  the artifact.

### Validation

- Rebuild from a clean checkout and inspect package contents.
- Run automated gates and manual install/upgrade/uninstall tests.
- Repeat lifecycle, accuracy, accessibility, and performance scenarios.
- Verify checksums and complete independent review.

### Rollback

Retain the last known-good package and immutable profile. Withdraw an
incompatible artifact rather than weakening compatibility checks.
