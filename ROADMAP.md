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

Status: Complete on 2026-07-29 on local branch
`phase1/native-linux-proof`. The bounded reusable reader and live in-memory PE
identity proof passed after the owner explicitly accepted the documented EULA
conflict.

### Outcome

Runtime evidence proves that one native Linux process can observe the
Wine-hosted client with bounded same-user access. The separate X11 overlay
experiment also established that the reference DWM raises true-fullscreen
clients above override windows, so an overlay is not the product UI.

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
- Resolve the MVP fields, UI direction, dependency set, distribution intent, and
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
- The proof window lifecycle and input-shape behavior pass in isolation, and
  live true-fullscreen stacking is documented accurately.
- It requires no root, injection, Wine override, or weakened host security.
- Required data symbols have validation strategies; guessed offsets are
  rejected.
- Every unresolved decision in `SPEC.md` is answered or deferred outside MVP.
- The user approves the architecture and data set.

### Validation

- Capture configure/build/test output for each viable proof.
- Record host, Wine, executable fingerprint, modules, and active renderer.
- Test launch, proof-window lifecycle, controlled failure, and game exit.
- Inspect project processes, handles, threads, and Wine state after shutdown.
- Review new dependencies and licenses.

### Rollback

Keep proofs isolated from the cleaned production targets. Remove their output
and restore the Phase 0 tree if process access is unsafe.

### Pause point

The technical comparison and EULA-risk decision must be recorded. Stop for the
normal Phase 1 approval checkpoint before Phase 2.

## Phase 2: Standalone Qt and DWM shell

Status: Complete on 2026-07-30 on local branch
`phase2/standalone-qt-shell`. See `docs/phase2-completion-report.md` and
`docs/phase2-dependency-audit.md`.

### Outcome

A clean Linux project builds a launcher and independent Qt 6 companion window.
One main window contains docked map, spawn, and detail placeholders and opens
on DWM tag 5 without changing the game window.

### Included work

- Establish the target source layout in `AGENTS.md`.
- Accept system Qt 6 only after its license and removal impact are recorded.
- Implement typed fingerprint/profile selection with fail-closed errors.
- Implement structured lifecycle, logging, XDG configuration, a normal
  `QMainWindow`, dockable placeholder views, and idempotent shutdown.
- Set X11 instance `plazmic-legends` and class `PlazmicLegends` for every
  Plazmic top-level window.
- Add the reference DWM rule
  `{ class="PlazmicLegends", tags=5, monitor=1, noswallow=1 }`.
- Persist main-window geometry and dock state.
- Follow system dark and light mode at startup and while the window is open.
- Add deterministic synthetic status snapshots; do not discover gameplay
  symbols or load user map files.
- Document exact Linux configure, format, build, test, and package commands.

### Dependencies and risks

- Phase 1 architecture approval is required.
- Qt 6 is a new system dependency and requires a license/removal-impact record.
- The current two-monitor DWM assigns tags 1-4 to DP-0 and tags 5-9 to HDMI-0;
  monitor-topology changes require placement revalidation.
- Detached Qt docks create additional top-level windows and must retain the
  same class and tag rule.
- ShowEQ concepts must be independently implemented without copying GPL code or
  importing its packet/data pipeline.

### Exit criteria

- A clean Fedora checkout builds and tests with documented commands.
- An unsupported client is rejected before process integration.
- The independent main window and any detached Plazmic window report the
  approved X11 class and DWM desktop index 4 (human tag 5).
- Plazmic windows are assigned to HDMI-0 tag 5 without switching the currently
  viewed tags or focusing the game.
- Opening, focusing, moving, minimizing, restoring, and closing Plazmic does
  not alter the game's fullscreen, focus policy, geometry, opacity, or input
  state.
- Saved geometry and dock state restore without moving a window off-screen.
- Dark and light system-theme changes update the open window without restart.

### Validation

- Run format/lint, warnings-as-errors build, tests, and package inspection.
- Test wrong path, wrong architecture, changed hash, integration failure,
  repeated initialization, and shutdown.
- Test synthetic status, dock layout, configuration defaults, and corrupted
  saved-state fallback under Xvfb.
- Test dark and light palette contrast, theme-file parsing, and live refresh.
- Use `xprop`, `wmctrl`, and `xwininfo` to verify class, tag, monitor geometry,
  focus, and game-window invariance on the live DWM session.

### Rollback

Remove the DWM class rule and Qt product target while retaining the Phase 1
reader proof and planning documents.

### Pause point

Show the standalone tag 5 window and game-invariance evidence before map or
gameplay-state work.

## Phase 3: Local map and player vertical slice

Status: In progress. The first checkpoint contains bounded local-map parsing,
the Qt map canvas with toggleable player-relative height filtering, and
exact-profile zone/player snapshots. Map orientation, player position, and
heading passed controlled live comparison against the in-game map.
Persisted player-follow is implemented on the continuation branch. Lifecycle
transitions, a second-zone observation, and the final Phase 3 gate remain.

### Outcome

The tag 5 companion window loads the current zone's installed map geometry and
displays a validated live player marker and heading.

### Included work

- Implement a bounded parser for approved line and label records in the user's
  installed `maps/<zone>.txt` and numbered layer files.
- Implement map transforms, pan, zoom, layer visibility, toggleable and
  adjustable player-relative height filtering, player-follow, and synthetic
  renderer tests.
- Discover and validate only world/session state, zone short name, player
  position, and heading for the exact client profile.
- Convert reads into immutable zone and player snapshots.
- Select map paths from a strictly validated zone short name.
- Implement not-running, not-in-world, zoning, missing-map, malformed-map,
  unavailable, and stale states.

### Dependencies and risks

- Local map files are proprietary user-installed inputs and cannot be packaged.
- Client structures may change independently of visible version metadata.
- Coordinate axes and heading transforms must be proven against controlled
  in-game and map ground truth.

### Exit criteria

- Parser fixtures cover supported records, layers, bounds, malformed input, and
  path escape rejection.
- Every live player/zone value has a documented resolver and validation method.
- Controlled manual tests match zone, player position, heading, and map
  orientation.
- Character select, zoning, camping, and exit invalidate the player marker
  without stale data or a client crash.
- No spawn traversal, game-state write, or input path exists.

### Validation

- Run the full Phase 2 gate plus map, reader, transform, and snapshot tests.
- Compare multiple controlled positions and headings against visible ground
  truth.
- Exercise base maps, numbered layers, missing files, and lifecycle
  transitions.
- Measure reader, snapshot publication, map loading, and UI update cost.

### Rollback

Disable the map/player adapter while retaining the tag 5 shell and explicit
unavailable state.

### Pause point

Review map accuracy, player-field isolation, and lifecycle evidence before any
spawn collection research.

## Phase 4: Validated spawn vertical slice

### Outcome

The tag 5 companion window displays a trustworthy bounded spawn table and map
markers synchronized by stable ID.

### Included work

- Discover and validate the smallest spawn-collection root for the exact client
  profile.
- Read a hard-bounded collection using staged reads and consistency checks.
- Publish immutable spawn values containing only approved ID, type, bounded
  name, level, position, and locally derived distance.
- Implement sortable/filterable table, map markers, detail view, and shared
  selection without changing the game target.
- Implement add, remove, change, zoning, unavailable, duplicate-ID, oversized,
  and inconsistent-read behavior.

### Dependencies and risks

- Spawn collection structure and lifetime are unknown and may change per client
  build.
- Large or rapidly changing collections can produce torn reads or excessive UI
  churn.
- Names are private runtime data and must never enter logs or fixtures.

### Exit criteria

- Every displayed spawn field has a documented source and two controlled
  ground-truth validations.
- Tests cover bounds, invalid pointers, duplicate IDs, inconsistent staged
  reads, invalidation, filtering, sorting, and selection.
- Map and table selection agree by stable ID and never control the game.
- Zoning, camping, character select, and exit show no stale spawns.
- Reader and UI performance stay within measured budgets without visible game
  regression.

### Validation

- Run the full Phase 3 gate plus spawn-reader, model, selection, and
  large-fixture tests.
- Compare spawn count and approved fields against controlled visible ground
  truth.
- Exercise spawn add/remove/change and every lifecycle transition.
- Audit logs, fixtures, and artifacts for character and spawn names.

### Rollback

Disable the spawn adapter while retaining the validated map/player view.

### Pause point

Review spawn accuracy, privacy, performance, and scope before hardening.

## Phase 5: Hardening and release readiness

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

- Hardening requires the Phase 4 approval checkpoint.
- Public distribution remains prohibited without a separate explicit decision.
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
