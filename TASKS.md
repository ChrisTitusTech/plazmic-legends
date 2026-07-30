# Plazmic Legends tasks

Task status changes only after acceptance criteria and required validation
pass. Record skipped validation and residual risk instead of marking incomplete
work done.

## Current phase

Phase 1 and Phase 2 were verified and squash-merged locally into `main` as
commit `8769e35`. The first Phase 3 checkpoint contains the bounded map parser,
Qt map canvas, exact-profile zone/player reader, adjustable player-Z filter,
and persisted player-follow state. The current continuation is lifecycle
invalidation; no spawn work is authorized.

## Active Phase 3

### P3-01: Parse local map geometry safely

- [x] Validate zone short names and contain all selected paths under the
  configured maps directory.
- [x] Parse bounded `L` line and `P` label records plus numbered layer files.
- [x] Reject missing base maps, path escapes, symlinks outside the map root,
  oversized input, excessive records, long lines, malformed fields, invalid
  colors, non-finite coordinates, and out-of-range values.
- [x] Cover the parser with synthetic fixtures containing no Daybreak map
  content.
- Acceptance criteria: base and numbered layers produce immutable geometry,
  every rejection has a typed error, and no installed map enters the
  repository or package.

### P3-02: Render the local map model

- [x] Add map bounds and coordinate transforms, pan, zoom, and layer
  visibility.
- [x] Add a default-on player-Z height filter with explicit off/on control,
  independently adjustable below/above ranges, and persisted settings.
- [x] Add persisted player-follow state that exits on manual pan or map fit.
- [x] Replace the Phase 2 map placeholder with a Qt canvas driven only by
  immutable map and player snapshots.
- [x] Add synthetic transform and canvas snapshot tests.
- Acceptance criteria: synthetic geometry and heading render consistently
  without client access.
- Checkpoint evidence: `docs/research/phase3-player-map-checkpoint.md`.

### P3-03: Resolve exact-profile zone and player state

- [x] Discover and document validated resolvers for world/session state, zone
  short name, player coordinates, and heading.
- [x] Enforce module, mapping, pointer-depth, string, finite-number, and
  lifecycle bounds through the existing read-only reader.
- [ ] Require two controlled observations for each live field and fail closed
  when any invariant fails.
- Acceptance criteria: no guessed or copied offset reaches the compatibility
  profile.
- Checkpoint evidence: player position and heading matched controlled ground
  truth at multiple locations; a second zone transition is still required.

### P3-04: Publish lifecycle-safe player snapshots

- [x] Convert validated reads into immutable zone/player snapshots.
- [ ] Invalidate state on character select, zoning, camping, read failure, and
  process exit.
- [x] Select map paths only from validated zone short names.
- Acceptance criteria: stale player markers cannot survive a lifecycle
  transition.

### P3-05: Complete the Phase 3 gate

- [ ] Run the full Phase 2 gate plus parser, transform, renderer, reader,
  snapshot, and lifecycle tests.
- [ ] Validate map orientation, multiple controlled positions and headings,
  layer behavior, missing maps, zoning, camping, and exit against the live
  client.
- [ ] Measure parsing, snapshot publication, and UI update cost.
- [ ] Stop for approval before Phase 4 spawn collection research.

### P1-01: Capture the Linux/Wine runtime baseline

- [x] Record host, Wine, prefix, launch, process/module, renderer, X11, window
  mode, and client fingerprint evidence.
- Evidence: `docs/research/phase1-runtime-baseline.md`.
- Acceptance criteria: evidence identifies the exact client process and
  distinguishes static PE clues from modules observed at runtime.
- Automated validation: executable fingerprint and environment probes.
- Manual validation: launched through Lutris; confirmed the live fullscreen
  game window and DXVK/Vulkan process mappings.

### P1-02: Prove least-privilege external process discovery

- [x] Build a native Linux ELF probe that identifies the Wine-hosted Legends
  process and its `eqgame.exe` mappings.
- Evidence: `src/integration/process_discovery.cpp`,
  `docs/phase1-architecture-proof.md`.
- Acceptance criteria: one supported process is selected without root,
  injection, Wine overrides, or host-security changes; zero and ambiguous
  matches fail explicitly.
- Automated validation: synthetic `/proc` fixtures cover exact, absent,
  wrong-identity, and ambiguous targets.
- Manual validation: selected the exact reference client, resolved its image
  mapping, and read only the two-byte PE signature as UID 1000. Process and
  overlay artifacts are native ELF files; the run used no root privilege,
  code injection, Wine override, host-security change, prefix edit, or
  game-installation edit.
- Dependency: P1-01.

### P1-03: Evaluate the external X11 diagnostics overlay

- [x] Build a native X11 overlay showing project, process, profile, and
  compatibility diagnostics only.
- Evidence: `src/overlay/x11_overlay.cpp`,
  `tests/test_x11_overlay_lifecycle.cpp`,
  `tests/test_x11_overlay_hotkey.cpp`, and
  `docs/phase1-architecture-proof.md`.
- Validation: X Shape reported an empty input region; isolated Xvfb/XTest
  validation hid and restored the proof; three child-owned X11 target exits
  cleaned up; and no proof executable remained afterward. A later root-window
  screenshot proved that X11 `IsViewable` did not mean the panel was composed
  above the live true-fullscreen game.
- Result: rejected for the product. The reference DWM deliberately raises true
  fullscreen above override windows. Removing fullscreen exposed the DWM bar,
  changed focus presentation, and produced perceived opacity.
- Automated validation: build, CLI, SHA boundary, synthetic `/proc`, repeated
  X11 lifecycle, isolated hotkey, input-shape, and no-client checks.
- Manual validation: root-window screenshots, X11 tree and property inspection,
  exact client still running after proof exit, restored fullscreen geometry,
  and exact-executable orphan inspection.
- Dependency: P1-02.

### P1-04: Approve the architecture and MVP boundary

- [x] Record the selected process-read mechanism, module resolution, X11
  strategy, dependency licenses, symbol-validation strategy, MVP fields,
  hotkey, distribution intent, and game-rule risk.
- Evidence: `docs/phase1-architecture-proof.md`,
  `docs/research/phase1-symbol-plan.md`,
  `docs/research/phase1-policy-risk.md`, and
  `docs/phase1-codex-review.md`.
- Decision update: private read-only memory research is approved despite the
  recorded EULA conflict. Push, release, distribution, writes, injection,
  automation, and protection bypass remain prohibited.
- Acceptance criteria: every unresolved Phase 1 decision in `SPEC.md` is
  answered or explicitly deferred outside MVP.
- Validation: independent Codex review, official-policy review, dependency
  license inspection, local install smoke test, full repository gate, and the
  user's instruction to finish Phase 1.
- Dependency: P1-03.

### P1-05: Prove the bounded external memory reader

- [x] Replace the one-off two-byte probe with a reusable same-user Linux
  reader that rejects unreadable and out-of-range requests.
- Evidence: `src/integration/process_reader.cpp`,
  `tests/test_process_discovery.cpp`, and
  `docs/phase1-architecture-proof.md`.
- Baseline comparison:
  `docs/research/macroquest-boundary-review.md`.
- Acceptance criteria: the live reader recovers PE machine, timestamp,
  relocated image base, and image size from process memory and matches the
  exact file and `/proc` profile.
- Automated validation: self-process reads, mapping-bound checks, invalid PID,
  overflow, and remote PE parsing fixtures.
- Manual validation: exact live Legends process returned machine `0x8664`,
  timestamp `0x6a6a2851`, image size `0x16c1000`, and an ImageBase equal to the
  mapped base. No root, write-capable code path, prefix change, or
  host-security change was used.
- Dependency: P1-02.

## Completed Phase 2

### P2-01: Accept the standalone UI dependency boundary

- [x] Record Qt 6 package versions, license terms, dynamic-linking impact, and
  clean-removal impact.
- [x] Confirm no ShowEQ source, data, maps, protocol definitions, or generated
  tables enter the repository.
- Acceptance criteria: the dependency and provenance audit maps directly to
  `SPEC.md` and `docs/research/showeq-ui-review.md`.

### P2-02: Build the independent application shell

- [x] Replace the product overlay path with a normal Qt 6 `QMainWindow`.
- [x] Set X11 instance `plazmic-legends` and class `PlazmicLegends` on every
  Plazmic top-level window.
- [x] Add compatibility and process status without reading gameplay state.
- [x] Persist geometry and dock state under the XDG configuration directory.
- Acceptance criteria: opening, focusing, moving, minimizing, and closing the
  application does not alter the game's fullscreen, focus policy, geometry,
  opacity, or input configuration.

### P2-03: Integrate DWM tag 5 placement

- [x] Add the live DWM rule
  `{ class="PlazmicLegends", tags=5, monitor=1, noswallow=1 }`.
- [x] Verify the main window and every detached Plazmic dock report EWMH desktop
  index 4 and geometry on HDMI-0.
- [x] Verify launch does not switch the game tag, focus the game, request
  `alwaysontop`, or alter the game window.
- Acceptance criteria: all Plazmic top-level windows open on human tag 5 under
  the current two-monitor DWM layout and remain there after layout restore.

### P2-04: Establish the docked UI and synthetic state boundary

- [x] Add docked map, spawn-list, and detail placeholders plus a status strip.
- [x] Define synthetic compatibility/lifecycle snapshots without game symbols,
  user map loading, or live player/spawn data.
- [x] Test geometry, dock-state persistence, corrupted-state fallback, and
  detached-window class inheritance.
- Acceptance criteria: UI lifecycle and layout tests pass under Xvfb and every
  pane remains inside the standalone application boundary.

### P2-05: Complete the Phase 2 gate

- [x] Run format/lint, warnings-as-errors build, tests, dependency audit, clean
  package inspection, DWM placement checks, and live game-window invariance
  checks.
- [x] Show the independent tag 5 shell and stop at the Phase 2 approval
  checkpoint.

### P2-06: Follow the system color mode

- [x] Detect the active DWM theme and apply a distinct dark or light Qt
  palette.
- [x] Poll for changes while running, with desktop portal and Qt color-scheme
  fallbacks when the reference DWM theme file is absent.
- [x] Test dark and light parsing, palette contrast, live refresh, and a real
  Nord to Catppuccin Latte to Nord transition.
- Acceptance criteria: the open companion follows the system theme without a
  restart and does not change the game window.

## Queued

- [ ] Phase 4: Validated bounded spawn vertical slice.
- [ ] Phase 5: Hardening and release readiness.

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
