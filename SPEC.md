# Plazmic Legends specification

## Problem

The imported codebase is a broad MacroQuest platform designed for traditional
EverQuest clients and Windows development. EverQuest Legends uses a different
`eqgame.exe`, the target environment is Linux with Wine, and most inherited
scripting, plugin, login, data-model, Windows build, and service code is outside
this project's purpose.

Plazmic Legends will be a small Linux-built overlay that shows selected
read-only information from the Legends client. Client-version knowledge must
be isolated, lifecycle transitions must be safe, and unknown builds must be
rejected instead of risking invalid memory access.

## Users

The initial user runs EverQuest Legends through Wine on Linux. Contributors
need a reproducible Linux build, deterministic tests that do not require a live
account, and a safe way to identify client drift after patches.

Native Windows users and Windows-hosted development are outside the initial
support tier.

## Product principles

- Read-only: observe and present; never control gameplay.
- Linux-first: configure, build, test, and package on Linux.
- Minimal: retain only code and dependencies used by the approved panel and
  lifecycle.
- Version-aware: client-specific knowledge belongs to an explicit
  compatibility profile.
- Fail closed: an unrecognized or ambiguous build is unsupported.
- Recoverable: failure must not corrupt game state or require editing the game
  installation to recover.
- Offline: no telemetry, account, web service, or updater is required.

## Required behavior

### Target selection and compatibility

- The launcher accepts an explicit Legends installation or discovers the
  configured Wine installation and only targets its `eqgame.exe`.
- Before integration, it records file size, PE machine, PE timestamp, and
  SHA-256 and selects exactly one compatible build profile.
- The MVP supports the Windows x86-64 Legends client under the approved Wine
  version only.
- Missing files, wrong architecture, insufficient process access, multiple
  candidate processes, and unknown fingerprints produce distinct errors.
- An unknown build is never paired with the nearest known offsets.

### Runtime lifecycle

- Startup initializes logging, compatibility state, process integration, the
  read-only game adapter, and the overlay in a defined order.
- Shutdown removes hooks or process access and stops owned work idempotently.
- Character select, entering the world, zoning, camping, and process exit
  invalidate old snapshots and reacquire state without retaining stale
  pointers.
- A failed optional data read marks that field unavailable for the frame. It
  does not crash the client or reuse a prior value as current data.
- Fatal initialization failures disable the overlay and emit a local
  diagnostic; they do not continue in a partial state.

### MVP information panel

The initial panel is deliberately narrow:

- compatibility status and client profile;
- current zone name;
- player coordinates and heading;
- current target name, level, and distance when a target exists.

The panel shows `Not in world`, `Zoning`, `No target`, and `Unavailable`
states explicitly. Field selection is an approval checkpoint at the end of
Phase 1 and may be reduced or amended before game-data implementation.

### Overlay interaction

- The overlay is readable at 1920x1080 and 2560x1440 under X11 in windowed or
  borderless play.
- A configurable hotkey toggles the panel.
- The panel is movable and can be locked in place.
- When hidden or locked, it does not capture mouse or keyboard input.
- Visual settings and position persist per Linux user outside the game
  installation and Wine prefix.
- The default theme has sufficient contrast over bright and dark scenes and
  does not rely on color alone for status.

### Diagnostics

- Logs include project version, compatibility profile, lifecycle transitions,
  integration results, and error categories.
- Logs exclude credentials, session tokens, chat, memory dumps, character
  names, and target names.
- Local status distinguishes not running, unsupported build, integration
  failure, overlay failure, and data-reader failure.

## Architecture and data flow

The Linux launcher fingerprints the PE executable before accessing the
Wine-hosted process:

```text
Linux launcher -> Legends PE fingerprint -> compatible profile
                                                |
                           approved Wine integration boundary
                                                |
                                  read-only game adapter
                                                |
                                      immutable snapshot
                                                |
                                      Linux X11 overlay
```

The implementation is a native Linux process. It discovers the Wine-hosted
client, inspects its Linux process maps, and reads approved state through a
narrow external process-reader interface. It does not inject code into Wine.

Compatibility profiles contain identity metadata and the smallest set of
symbols or signatures needed by approved fields. The runtime resolves one
profile, validates every required symbol, and exposes typed readers. Readers
publish value snapshots; the overlay never traverses client objects or owns raw
game pointers.

Configuration and logs use the XDG base-directory conventions. The game
installation and Wine prefix remain unmodified unless a later explicitly
approved architecture decision requires otherwise.

## Security, privacy, and compliance

- Use the least Linux and target-process access required by the approved
  integration.
- Do not require root, setuid, broad ptrace-policy changes, or disabled system
  security controls. If the design requires one, reject it and revisit the
  architecture.
- Do not inject a PE DLL, add a Wine DLL override, hook DirectX inside the
  client, or patch the Wine prefix.
- Do not add a network listener, remote command channel, telemetry, automatic
  updater, or credential store.
- Do not bundle or redistribute `eqgame.exe`, Daybreak libraries, data files,
  symbols, or other proprietary content.
- Do not bypass authentication, integrity checks, anti-cheat, or client
  protections.
- Memory access is limited to approved panel fields. Gameplay state writes and
  synthesized gameplay input are prohibited.
- Retained MacroQuest-derived code remains subject to GPLv2 notices.
  Third-party dependencies require a documented license and provenance audit.
- The project does not assert that process inspection or overlays are permitted
  by current game rules. Live integration testing and distribution require an
  explicit risk decision.

## Performance and compatibility

- Reference host: Fedora Linux 44 x86-64, X11, and Wine 11.0 Staging.
- The observed client contains Direct3D 9 loader strings, but the active
  renderer and Wine translation path must be confirmed at runtime.
- Wayland is not an MVP target.
- Overlay update and render work should stay below 1 ms p95 per presented frame
  on the reference system and avoid visible frame-pacing regressions.
- Game-state sampling is bounded and does not scan the full process each frame.
- No unbounded queues, detached worker threads, or blocking file/network work
  are allowed on a render or UI event thread.
- A supported profile is immutable. A client patch creates a new profile and
  runs the compatibility gate.

## Non-goals

- Macro, scripting, command, or plugin platforms.
- Automated input, combat, movement, inventory, login, or multibox features.
- Maps, spawn lists, alerts, chat processing, remote control, or web services.
- Support for traditional EverQuest live, test, beta, or emulator clients.
- Native Windows builds, Visual Studio solutions, MSVC, or Windows-hosted CI.
- Windows-format project artifacts, MinGW builds, Wine DLL injection, or
  DirectX hooks.
- Wayland support in the MVP.
- A general-purpose memory editor or debugger.
- Modifying or repackaging the EverQuest Legends installation.
- Preserving source or API compatibility with MacroQuest plugins.
- An in-app updater or crash-report upload service.

## Acceptance criteria

- AC-01: Phase 0 leaves a recoverable baseline and removes imported files,
  information, build paths, and dependencies classified as outside scope.
- AC-02: A clean checkout on Linux has repeatable configure, build, test, and
  package commands with no Visual Studio or Windows-host requirement.
- AC-03: The launcher identifies the reference Legends executable and rejects
  wrong architecture, changed hash, ambiguous target, and unsupported build
  with distinct errors.
- AC-04: The native Linux process displays a diagnostic X11 overlay for the
  reference client without code injection, gameplay-state writes, or Wine
  prefix changes.
- AC-05: Approved MVP fields match controlled ground truth, including loading,
  zoning, no-target, and unavailable states.
- AC-06: Character select, zoning, camping, repeated startup/shutdown, and
  process exit complete without a client crash, stale snapshot, or orphaned
  project process.
- AC-07: Hidden or locked overlays leave game input unaffected; position and
  settings persist after restart.
- AC-08: Tests cover profile matching, rejection paths, state conversion,
  snapshot invalidation, and configuration defaults without a live game.
- AC-09: The release gate includes format/lint, warnings-as-errors build, tests,
  dependency/license audit, clean-package smoke test, and documented Wine
  evidence.
- AC-10: The package contains no Daybreak content, credentials, private Wine
  data, unused inherited services, or unsupported plugins.
- AC-11: Every retained source directory and dependency maps to a requirement
  in this specification.

## Unresolved Phase 1 decisions

- Confirm or amend the proposed MVP information fields.
- Select the least-privilege Linux process-read mechanism and module-address
  resolution strategy.
- Confirm the active renderer, Wine translation path, X11 stacking behavior,
  and supported window modes.
- Select the smallest reproducible Linux toolchain and dependency set.
- Decide whether packages remain private development artifacts or become public
  releases.
- Review current game rules and accept or reject operational risk before live
  integration testing and distribution.
- Choose the default toggle hotkey and final XDG configuration locations.
