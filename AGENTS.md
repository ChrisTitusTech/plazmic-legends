# Project instructions

## Purpose

Plazmic Legends is a minimal companion application for the 64-bit EverQuest
Legends `eqgame.exe` running under Wine on Linux. Its gameplay integration is
read-only, and it uses an
independent Linux window for maps, spawns, and status; it does not draw over or
inside the game. The repository began as a MacroQuest-derived source import,
but the intended product is not a scripting or plugin platform.

## Read before changing code

1. Read `SPEC.md` for requirements, non-goals, and acceptance criteria.
2. Read `ROADMAP.md` for phase order, exit criteria, rollback, and approval
   checkpoints.
3. Read `TASKS.md` for the current authorized task and validation status.
4. Read `docs/cleanup-inventory.md` before deleting or retaining imported code.
5. Read `docs/research/legends-baseline.md` before work on the client,
   renderer, process integration, offsets, or compatibility profiles.
6. Read `docs/research/showeq-ui-review.md` before work on the companion
   window, map, spawn model, or Qt dependency.

If these documents conflict, stop and reconcile them with the user before
implementing the conflicting behavior.

## Current repository state

- The inherited MacroQuest implementation and bundled dependencies were
  removed in Phase 0.
- The full import is recoverable from tag `phase0-import-baseline`.
- Phase 1 is complete and contains an isolated native Linux proof. It may
  fingerprint and select the client, perform bounded read-only process-memory
  access, and validate the live PE identity. Its external X11 overlay
  experiment is retained only as research: the reference DWM raises true
  fullscreen above override windows, so the overlay is not the product UI.
- Phase 2 is complete. The Qt 6 product shell, client status boundary, saved
  dock layout, live DWM tag 5 rule, and game-invariance gate passed.
- Phase 3 is complete. Bounded installed maps, exact-profile zone/player
  reads, player-follow, lifecycle invalidation, and process reacquisition
  passed the complete live gate.
- Phase 4 is complete. The exact-profile bounded spawn reader, immutable
  model, synchronized table/map selection, lifecycle behavior, performance,
  privacy, and live presentation gates passed.
- Phase 5 is complete. Release hardening, privacy-safe diagnostics,
  exact-client update detection, RPM and AppImage packaging, and the first
  official `v0.1.2` release passed their gates.
- Phase 6 is merged and included in `v0.2.0`. It adds bounded exact-profile
  character vitals and text equipment plus an offline local combat-log parser
  in two stacked left docks. The owner explicitly authorized release without
  repeating the final manual live lifecycle gate on the merged head; that
  skipped check remains a documented residual risk.
- No inherited implementation or third-party source is retained.
- The local reference client is a Windows x86-64 PE executable running through
  GE-Proton11-3 in an X11 session.

Do not restore imported code without a documented requirement, provenance
review, license review, and comparison against a native replacement.

## Target platform and architecture

- Build host: Linux only.
- Initial runtime: Fedora Linux, X11, and Wine.
- Build system: Linux-native CMake and Ninja unless Phase 1 evidence supports a
  smaller Linux-native alternative.
- Compiler: GCC or Clang for native Linux code.
- Native Windows and Visual Studio builds are non-goals.
- Project artifacts are native Linux ELF binaries and data only. Do not add a
  PE DLL, MinGW toolchain, Wine DLL override, or Windows injection path.
- The product observes the Wine-hosted process externally and renders a normal
  independent Linux application window. Keep process access behind a narrow
  Linux interface.

## Source boundaries

- `src/launcher`: executable fingerprinting, Wine client startup/monitoring,
  status, and shutdown.
- `src/integration`: Linux process identification, module mapping, and
  read-only process access for the Wine-hosted client.
- `src/game`: build profiles and read-only game-state access.
- `src/model`: immutable player, zone, spawn, and selection snapshots.
- `src/map`: map geometry, transforms, and renderer-independent map state.
- `src/ui`: Qt 6 main window, map canvas, spawn table, status, and preferences.
- `src/common`: narrow configuration, logging, and shared utilities.
- `tests`: profile, state-conversion, configuration, and lifecycle tests that
  do not require a live account.

Keep new work inside these established boundaries unless an approved
requirement justifies a focused change.

## Working boundaries

- Phase 0 through Phase 5 are complete on `main`. The current release is
  `v0.2.0`. Preserve the validated product boundary, lifecycle behavior,
  exact-profile failure rules, privacy guarantees, and package inventory in
  maintenance work.
- RPM, COPR, AppImage, and other package publication are authorized.
- The owner explicitly accepts that this development project operates
  against the Daybreak EULA. Read-only symbol and gameplay-state research may
  proceed after the normal phase checkpoints. The approved UI-file installer
  may replace only validated, user-selected skin and INI files after explicit
  confirmation and a private backup. It may run while the game is active so
  the user can invoke its supported UI reload command. This exception does not
  authorize process-memory or gameplay-state writes,
  injection, automation, input synthesis, or protection bypass. See
  `docs/research/phase1-policy-risk.md`.
- Preserve the imported tree in a recoverable baseline commit before deleting
  files.
- Use `phase0-import-baseline` only as read-only research unless restoration is
  explicitly justified and approved.
- Preserve copyright and license notices for retained third-party or
  MacroQuest-derived code. Record provenance when code is moved.
- Never commit Daybreak executables, game assets, account data, Wine prefix
  contents, crash dumps, logs containing player data, or offsets copied from
  an unknown source.
- Keep private UI bundles, character/server INI names, and their archives
  ignored and local. They are user-supplied installer input and may not enter
  source packages, binary packages, releases, logs, fixtures, or pull requests.
- Do not bypass anti-cheat, integrity checks, authentication, or client
  protections. If one blocks the proposed integration, stop and surface it as
  a product decision.
- Do not inject code, load a DLL into Wine, hook DirectX, or install a Wine DLL
  override.
- Do not write gameplay state, synthesize gameplay input, automate actions, or
  add scripting, macros, plugins, multibox control, or remote control.
- Centralize client-specific addresses and signatures in versioned
  compatibility profiles. Do not scatter raw offsets through UI or lifecycle
  code.
- An unknown or partially matched client build must fail closed. Never guess
  offsets.
- Keep the game-data reader separate from rendering so recorded or synthetic
  snapshots can be tested without launching the game.
- Use `plazmic-legends` as the X11 instance and `PlazmicLegends` as the class
  for every Plazmic top-level window. The reference DWM places that class on
  tag 5, monitor 1, with `noswallow=1`; do not request `alwaysontop` or activate
  the game tag.
- Treat the ShowEQ 6.4.25 source as conceptual guidance only. Do not copy its
  GPL implementation, packet decoder, generated data, maps, or mutable
  pointer-based object model. See `docs/research/showeq-ui-review.md`.
- Add a dependency only when an approved feature requires it. Document the
  license and removal impact in the same phase.

## Local reference target

Set the path for the current Wine installation without committing it:

```bash
export EQ_LEGENDS_DIR='/home/titus/games/everquest/prefix/drive_c/users/Public/Daybreak Game Company/Installed Games/EverQuest Legends'
```

Inspect the client before and after a patch:

```bash
python3 tools/inspect_eqgame.py "$EQ_LEGENDS_DIR"
```

A changed SHA-256 creates a new compatibility profile and validation cycle. It
does not authorize updating old offsets in place.

## Current commands

Configure and validate the Linux research scaffold:

```bash
cmake --preset dev
cmake --build --preset dev
cmake --build --preset check
ctest --preset dev
python3 tools/inspect_eqgame.py "$EQ_LEGENDS_DIR" \
  --expect-sha256 bf34438c6460acde463692fa09ea28f0d12a204e3445a9da356645fc0d475561
git diff --check
```

Run the installed standalone product after following the local replacement
requirements in `SPEC.md`:

```bash
plazmic-legends \
  --client "$EQ_LEGENDS_DIR/eqgame.exe"
```

## Validation and evidence

- Run focused checks while implementing and the complete gate defined by the
  active roadmap phase before marking a task complete.
- Inspect final status and diff, including generated and untracked files.
- Deletion validation includes reference searches, retained-license checks, and
  an inventory comparison against the recoverable baseline.
- Live-client validation must use the approved fingerprint and record Linux,
  Wine, display-server, renderer, and lifecycle evidence.
- Before manual, X11, or live-client testing of a changed build, atomically
  replace the shell-resolved unmanaged local installation with the exact
  validated build, preserve a hash-addressed rollback, and verify the installed
  owner, mode, version, and hash. A build-tree-only launch does not count.
- Inspect `/proc/<pid>/exe` for any running companion and require a relaunch
  before attributing its behavior to the replacement; do not disrupt it without
  explicit authorization.
- A successful process attachment is not sufficient evidence. Validate the
  independent window, snapshot rendering, zoning, return to character select,
  game exit, and unsupported build handling when the phase requires them.
- Never use an agent report as validation evidence. Record commands and
  observable results.
- Report skipped checks and residual risk explicitly.
