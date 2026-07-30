# Project instructions

## Purpose

Plazmic Legends is a minimal, read-only information overlay for the 64-bit
EverQuest Legends `eqgame.exe` running under Wine on Linux. The repository
began as a MacroQuest-derived source import, but the intended product is not a
scripting or plugin platform.

## Read before changing code

1. Read `SPEC.md` for requirements, non-goals, and acceptance criteria.
2. Read `ROADMAP.md` for phase order, exit criteria, rollback, and approval
   checkpoints.
3. Read `TASKS.md` for the current authorized task and validation status.
4. Read `docs/cleanup-inventory.md` before deleting or retaining imported code.
5. Read `docs/research/legends-baseline.md` before work on the client,
   renderer, process integration, offsets, or compatibility profiles.

If these documents conflict, stop and reconcile them with the user before
implementing the conflicting behavior.

## Current repository state

- The inherited MacroQuest implementation and bundled dependencies were
  removed in Phase 0.
- The full import is recoverable from tag `phase0-import-baseline`.
- The active tree contains planning documents, Linux-native research tooling,
  and no product runtime yet.
- No inherited implementation or third-party source is retained.
- The local reference client is a Windows x86-64 PE executable running through
  Wine 11.0 Staging in an X11 session.

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
- The product observes the Wine-hosted process externally and renders a
  separate X11 overlay. Keep process access behind a narrow Linux interface.

## Planned source boundaries

- `src/launcher`: executable fingerprinting, Wine client startup/monitoring,
  status, and shutdown.
- `src/integration`: Linux process identification, module mapping, and
  read-only process access for the Wine-hosted client.
- `src/game`: build profiles and read-only game-state access.
- `src/overlay`: Linux overlay window and immutable-snapshot rendering.
- `src/common`: narrow configuration, logging, and shared utilities.
- `tests`: profile, state-conversion, configuration, and lifecycle tests that
  do not require a live account.

This layout is a roadmap target, not permission to reorganize ahead of the
active task.

## Working boundaries

- Phase 0 is complete. Do not start Phase 1 until the user explicitly says to
  continue.
- Preserve the imported tree in a recoverable baseline commit before deleting
  files.
- Use `phase0-import-baseline` only as read-only research unless restoration is
  explicitly justified and approved.
- Preserve copyright and license notices for retained third-party or
  MacroQuest-derived code. Record provenance when code is moved.
- Never commit Daybreak executables, game assets, account data, Wine prefix
  contents, crash dumps, logs containing player data, or offsets copied from
  an unknown source.
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
cmake --build --preset check
ctest --preset dev
python3 tools/inspect_eqgame.py "$EQ_LEGENDS_DIR" \
  --expect-sha256 97ee793d491930ac97f91e5e26fac16d84d17ff24afcd24d5390d256e7045661
git diff --check
```

Phase 1 must extend these commands with exact native configure, format, build,
test, and package gates for its proof of life.

## Validation and evidence

- Run focused checks while implementing and the complete gate defined by the
  active roadmap phase before marking a task complete.
- Inspect final status and diff, including generated and untracked files.
- Deletion validation includes reference searches, retained-license checks, and
  an inventory comparison against the recoverable baseline.
- Live-client validation must use the approved fingerprint and record Linux,
  Wine, display-server, renderer, and lifecycle evidence.
- A successful process attachment is not sufficient evidence. Validate
  rendering, zoning, return to character select, game exit, and unsupported
  build handling when the phase requires them.
- Never use an agent report as validation evidence. Record commands and
  observable results.
- Report skipped checks and residual risk explicitly.
