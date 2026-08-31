# Project instructions

## Purpose

Plazmic Legends is an extensible native Linux companion for the 64-bit
EverQuest Legends `eqgame.exe` running under Wine. Its current client
integration is read-only and its current UI is an independent Qt window, but
those implementation choices are a validated foundation rather than a
permanent limit on future product capabilities. New capabilities must be
introduced through an explicit phase with requirements, provenance, privacy,
security, lifecycle, validation, and rollback gates.

## Read before changing code

1. Read `SPEC.md` for requirements, current exclusions, and acceptance criteria.
2. Read `ROADMAP.md` for phase order, exit criteria, rollback, and approval
   checkpoints.
3. Read `TASKS.md` for the current authorized task and validation status.
4. Read `docs/cleanup-inventory.md` before deleting or retaining imported code.
5. Read `docs/research/legends-baseline.md` before work on the client,
   renderer, process integration, offsets, or compatibility profiles.
6. Read `docs/research/showeq-ui-review.md` before work on the companion
   window, map, spawn model, or Qt dependency.
7. Read `docs/research/everquest-companion-feature-parity.md` before work on
   combat analytics, activity history, alerts, timers, overlays, knowledge
   packs, sharing, or optional online services.

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
  fullscreen above override windows, so that experiment was not selected as
  the primary UI.
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
- Phase 7 is complete and defines the expansion framework and provenance-safe
  feature-parity roadmap.
- Phase 8 combat analytics and the implemented Phase 9 progression, loot,
  inventory, XP, and AA slices are on `main`; their remaining closeout work is
  tracked in `TASKS.md`.
- Phase 10 has its first validated slice on `main`: default-on Mote loot audio
  with an independent opt-out and yellow Mote rows. The wider timer, rule,
  notification, and audio-pack scope remains active.
- Later major capabilities remain separately reviewable and follow the numeric
  order and gates recorded in `ROADMAP.md`.
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
- The currently supported build and runtime are native Linux. A new platform,
  artifact format, or integration mechanism requires its own approved phase
  and must not weaken the Linux release.
- The current product observes the Wine-hosted process externally. Keep every
  process-access mechanism behind a narrow, capability-scoped interface.
- UI surfaces may be docked, detached, or overlay-style independent Linux
  windows. Each window role must declare its focus, stacking, input, privacy,
  persistence, and lifecycle behavior.

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
- `src/activity`: bounded progression, loot, raid, buff, timer, and other
  user-owned activity histories.
- `src/alerts`: local alert rules, notification dispatch, and optional audio.
- Planned `src/knowledge`: validated user-imported or license-compatible knowledge
  packs and planners.
- Planned `src/services`: explicitly enabled network, update, feedback, and sharing
  clients with documented payloads and endpoints.
- `tests`: profile, state-conversion, configuration, and lifecycle tests that
  do not require a live account.

Keep new work inside these established boundaries unless an approved
requirement justifies a focused change.

## Working boundaries

- Phase 0 through Phase 7 are complete on `main`; Phase 8, Phase 9, and the
  first Phase 10 slice are implemented as described above. The current release
  remains `v0.2.0`. Preserve validated lifecycle behavior, exact-profile
  failure rules, privacy guarantees, and package inventory while extending
  them for each new capability.
- RPM, COPR, AppImage, and other package publication are authorized.
- The owner explicitly accepts that this development project operates
  against the Daybreak EULA. Read-only symbol and gameplay-state research may
  proceed after the normal phase checkpoints. The approved UI-file installer
  may replace only validated, user-selected skin and INI files after explicit
  confirmation and a private backup. It may run while the game is active so
  the user can invoke its supported UI reload command. See
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
- The current implementation has no injection, gameplay-state write, input
  synthesis, automation, scripting, multibox, plugin-host, or remote-control
  capability. Adding any such capability requires a dedicated approved phase,
  an updated threat model, clear user control, failure isolation, and evidence
  that it does not bypass a client protection. Authorization for one
  capability does not authorize another.
- Centralize client-specific addresses and signatures in versioned
  compatibility profiles. Do not scatter raw offsets through UI or lifecycle
  code.
- An unknown or partially matched client build must fail closed. Never guess
  offsets.
- Keep the game-data reader separate from rendering so recorded or synthetic
  snapshots can be tested without launching the game.
- Use `plazmic-legends` as the X11 instance and `PlazmicLegends` as the class
  for every Plazmic top-level window. Normal windows use the reference DWM tag
  5 rule. An approved overlay role may request topmost or input-transparent
  behavior only when enabled by the user and must not steal focus or activate
  the game tag.
- Treat the ShowEQ 6.4.25 source as conceptual guidance only. Do not copy its
  GPL implementation, packet decoder, generated data, maps, or mutable
  pointer-based object model. See `docs/research/showeq-ui-review.md`.
- Treat `jmoyers/everquest-companion` as behavior and product research only.
  Its FSL-1.1-MIT code, bundled data, assets, tests, and generated content are
  not copied into this GPL project. See
  `docs/research/everquest-companion-feature-parity.md`.
- Network access, uploads, sharing, and update checks are disabled by default.
  An approved service must disclose its endpoint and payload, require explicit
  user consent, provide a local disable/delete path, and exclude private names,
  paths, logs, and game data unless the user previews and authorizes that exact
  transfer.
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
  --expect-sha256 3451069e63d5118a703a237f121a3ea7c20c973477a69fdd0d66bcdaa7d80b29
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
