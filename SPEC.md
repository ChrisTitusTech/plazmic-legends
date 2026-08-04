# Plazmic Legends specification

## Problem

The imported codebase is a broad MacroQuest platform designed for traditional
EverQuest clients and Windows development. EverQuest Legends uses a different
`eqgame.exe`, the target environment is Linux with Wine, and most inherited
scripting, plugin, login, data-model, Windows build, and service code is outside
this project's purpose.

Plazmic Legends will be a small Linux-built companion application that shows
selected read-only information from the Legends client in an independent
window. It does not draw over or inside the game. Client-version knowledge
must be isolated, lifecycle transitions must be safe, and unknown builds must
be rejected instead of risking invalid memory access.

## Users

The initial user runs EverQuest Legends through Wine on Linux. Contributors
need a reproducible Linux build, deterministic tests that do not require a live
account, and a safe way to identify client drift after patches.

Native Windows users and Windows-hosted development are outside the initial
support tier.

## Product principles

- Read-only: observe and present; never control gameplay.
- Linux-first: configure, build, test, and package on Linux.
- Minimal: retain only code and dependencies used by the approved window and
  lifecycle.
- Version-aware: client-specific knowledge belongs to an explicit
  compatibility profile.
- Fail closed: an unrecognized or ambiguous build is unsupported.
- Recoverable: failure must not corrupt game state or require editing the game
  installation to recover.
- Offline: no telemetry, account, web service, or updater is required.

## Required behavior

### Target selection and compatibility

- The launcher selects an explicit `--client`, a valid `EQ_LEGENDS_DIR`, or a
  valid saved `[client].game_directory`, in that order, and only targets its
  `eqgame.exe`.
- Without a valid configured directory, it performs a time-, depth-, and
  count-bounded scan below the current user's home directory for the exact
  `Daybreak Game Company/Installed Games/EverQuest Legends` structure. One
  match is saved to the XDG configuration; multiple matches fail closed.
- Before integration, it records file size, PE machine, PE timestamp, and
  SHA-256 and selects exactly one compatible build profile.
- The MVP supports the Windows x86-64 Legends client under the approved Wine
  version only.
- Missing files, wrong architecture, insufficient process access, multiple
  candidate processes, and unknown fingerprints produce distinct errors.
- An unknown build is never paired with the nearest known offsets.

### Client-update offset discovery

- A changed `eqgame.exe` SHA-256 remains unsupported until a new immutable
  profile completes the normative
  [client offset discovery](docs/offset-discovery.md) and
  [compatibility profile refresh](docs/profile-refresh.md) workflows.
- Every old RVA, field offset, bound, and structure assumption is an untrusted
  hypothesis for the new build. Each approved field requires new static
  instruction/data-flow evidence and two controlled live ground-truth
  observations.
- A new profile records only exact PE identity, profile-local RVAs, bounded
  record offsets, masks, and validation limits. It never replaces or edits a
  prior profile in place.
- Client-update research records provenance and privacy-safe validation while
  excluding proprietary binaries and disassembly, raw live addresses, memory
  content, runtime names, account data, and Wine-prefix paths.
- If any required resolver cannot be independently re-established, the field
  is omitted or the new client remains unsupported. Compatibility checks are
  never weakened to restore apparent functionality.

### Runtime lifecycle

- Startup initializes logging, compatibility state, process integration, the
  read-only game adapter, immutable snapshot model, and companion window in a
  defined order.
- Shutdown removes hooks or process access and stops owned work idempotently.
- Character select, entering the world, zoning, camping, and process exit
  invalidate old snapshots and reacquire state without retaining stale
  pointers.
- A failed optional data read marks that field unavailable for the frame. It
  does not crash the client or reuse a prior value as current data.
- Fatal initialization failures disable live data and emit a local diagnostic
  in the companion window; they do not continue in a partial state.

### MVP companion window

The initial application window is deliberately narrow:

- compatibility status and client profile;
- current zone name and locally installed zone-map geometry;
- player coordinates, heading, and map marker;
- a filtered and sortable spawn list;
- spawn markers synchronized with list selection; and
- selected-spawn name, type, level, coordinates, and distance when available.

The window shows `Client not running`, `Unsupported client`, `Not in world`,
`Zoning`, `No selection`, and `Unavailable` states explicitly. A field that
cannot be isolated and validated safely is omitted rather than guessed.

### Character and combat column

The post-release character/combat feature adds two vertically stacked docks in
the default left column:

- a Character dock showing the active character name, current HP, maximum HP
  and an HP percentage only when separately proven, current and maximum mana,
  the mana percentage, text-only equipped slot and item names, and the
  character's current encounter DPS; and
- a Parse dock showing one condensed current or most-recent encounter with
  participant, total damage, average DPS, percentage, and active duration.

HP, mana, character identity, and equipped item values come only from bounded
exact-profile, same-user process reads. Each field requires independent static
evidence, two controlled live observations, explicit bounds, and lifecycle
invalidation before it may be displayed. Equipment uses slot labels and text
only; the project does not copy or package game icons, item data, or assets.

DPS is derived locally from the user's EverQuest combat log, not from client
memory. The parser follows only the active character's bounded log file,
handles appended and rotated/truncated files, starts a new encounter after an
inactivity boundary, and never persists combatant names or encounter content.
Missing, disabled, ambiguous, malformed, oversized, or unavailable logs show
an explicit unavailable state without affecting memory-backed character data.

The parser is an independent implementation informed only by public product
behavior. It does not copy Loadout Legends code or assets and does not add its
uploads, accounts, leaderboards, saved history, timers, proc tools, resist
tools, database, or network services.

### Window and map interaction

- The application is a normal independent Linux window and does not require a
  relationship with the game window, its focus, its fullscreen state, or its
  render surface.
- The main window contains a central map canvas and dockable spawn and detail
  views inspired conceptually by ShowEQ.
- The default left dock area contains the Character dock above the Parse dock;
  both retain the existing movable, closable, floatable, saved-layout, and X11
  class behavior.
- An embedded top menu bar exposes checkable Views actions for Character,
  Parse, Spawns, and Details, plus minimize, maximize/restore, and close
  controls aligned at the top right.
- The main window and any detached Plazmic tool windows expose
  `WM_CLASS(STRING) = "plazmic-legends", "PlazmicLegends"` so DWM can place
  them predictably.
- On the reference two-monitor DWM session, all Plazmic windows open on tag 5,
  owned by monitor 1 (HDMI-0). They do not request `alwaysontop`, activate the
  game tag, or change game focus.
- The reference DWM rule is
  `{ class="PlazmicLegends", tags=5, monitor=1, noswallow=1 }`.
- The map supports pan, zoom, player-follow, layer visibility, selection, and
  readable labels.
- NPC names beginning with `#` are presented as named spawns with a distinct
  map color and marker shape. Named NPC, player-character, and ordinary-NPC
  labels can be enabled independently.
- Ordinary NPC dots use the consider color derived from the validated
  local-player and NPC levels: gray, green, light blue, blue, white, yellow,
  or red.
  Named NPCs retain their distinct marker color and shape.
- Non-player, non-NPC spawn snapshots are presented as Other with neutral gray
  map markers, including any exact-profile ground records published by the
  reader. A visible Filters / Labels dropdown independently hides named NPC,
  player-character, ordinary-NPC, and Ground / Other markers from rendering
  and map selection, and the spawn table provides an Other type filter.
- Selecting a spawn in either the map or list highlights the same stable ID in
  both views without affecting the game target.
- Window geometry, dock state, columns, filters, map view, and visual settings
  persist per Linux user outside the game installation and Wine prefix.
- The window follows the active system light or dark preference while running.
  The reference DWM theme file is authoritative in that session, with standard
  desktop portal and Qt color-scheme hints as fallbacks elsewhere.
- The default theme has sufficient contrast over bright and dark scenes and
  does not rely on color alone for status.

### Data sources

- Static map lines and labels are parsed read-only from the configured Legends
  installation's `maps/<zone>.txt` and optional numbered layer files. Map
  files are never copied into the project or package.
- The live zone short name selects map files only after bounded validation; it
  is never used as an unchecked path.
- Player and spawn observations come only from exact-profile, same-user,
  bounded external process reads.
- Character HP, mana, identity, equipped slots, and equipped item names come
  only from exact-profile, same-user, bounded external process reads.
- Current encounter damage and DPS come only from bounded incremental reads of
  the selected active character's local EverQuest combat log. Combat logs are
  user-owned runtime inputs and are never copied into the project or package.
- One profile-local game adapter converts validated observations into
  immutable player, zone, and spawn snapshots keyed by stable IDs.
- The UI consumes snapshots and local map geometry only. It never receives
  target addresses or traverses process memory.
- ShowEQ's packet capture, opcode decoding, privileged execution, maps,
  generated data, and implementation are not data sources.

### Diagnostics

- Logs include project version, compatibility profile, lifecycle transitions,
  integration results, and error categories.
- Logs exclude credentials, session tokens, chat, memory dumps, character
  names, and target names.
- Local status distinguishes not running, unsupported build, integration
  failure, window failure, map-file failure, and data-reader failure.
- Parser diagnostics record only state and error categories. They exclude log
  paths, character and combatant names, encounter content, and damage values.

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
                              independent Qt companion window
```

The implementation is a native Linux process. It discovers the Wine-hosted
client, inspects its Linux process maps, and reads approved state through a
narrow external process-reader interface. It does not inject code into Wine.

Compatibility profiles contain identity metadata and the smallest set of
symbols or signatures needed by approved fields. The runtime resolves one
profile, validates every required symbol, and exposes typed readers. Readers
publish value snapshots; the UI never traverses client objects or owns raw game
pointers.

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
  Original Plazmic Legends code is licensed under GPL-3.0-only.
  Third-party dependencies require a documented license and provenance audit.
- The project does not assert that process inspection or a companion
  application is permitted by current game rules. The owner accepted that risk
  for live integration testing and package publication.

## Performance and compatibility

- Reference host: Fedora Linux 44 x86-64, X11, and GE-Proton11-3.
- The reference run used the existing Lutris DXVK 2.6.2 configuration and
  mapped DXGI, Wine Vulkan, the host Vulkan loader, and the NVIDIA GLX library.
- Wayland is not an MVP target.
- Snapshot publication and UI rendering must not cause visible game
  frame-pacing regressions.
- Game-state sampling is bounded and does not scan the full process each frame.
- No unbounded queues, detached worker threads, or blocking file/network work
  are allowed on a render or UI event thread.
- The spawn model has an explicit maximum count, and unchanged snapshots do not
  force full table or map reconstruction.
- Character reads have explicit record, slot-count, and string bounds. The
  combat parser tails only newly appended bounded bytes and publishes immutable
  encounter snapshots without blocking the UI thread.
- A supported profile is immutable. A client patch creates a new profile and
  runs the compatibility gate.

## Non-goals

- Macro, scripting, command, or plugin platforms.
- Automated input, combat, movement, inventory, login, or multibox features.
- Alerts, chat processing, remote control, or web services.
- Map editing, route finding, spawn alerts, audio alerts, or command execution.
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
- AC-04: The native Linux process displays a normal independent Qt companion
  window on DWM tag 5 with the approved class and rule, without changing game
  focus, fullscreen, opacity, geometry, input, active tag, or the Wine prefix.
- AC-05: The local map parser renders approved zone geometry, layers, player
  position, and heading against controlled fixtures.
- AC-06: Validated live spawn snapshots match controlled ground truth, and map
  and table selection agree by stable ID.
- AC-07: Character select, zoning, camping, repeated startup/shutdown, and
  process exit complete without a client crash, stale snapshot, or orphaned
  project process.
- AC-08: Window movement, focus, minimization, and shutdown leave game
  fullscreen, focus policy, geometry, opacity, and input configuration
  unchanged; UI state persists after restart.
- AC-09: Tests cover profile matching, rejection paths, map parsing, state
  conversion, snapshot invalidation, model updates, selection, and
  configuration defaults without a live game.
- AC-10: The release gate includes format/lint, warnings-as-errors build, tests,
  dependency/license audit, clean-package smoke test, and documented Wine
  evidence.
- AC-11: The package contains no Daybreak or ShowEQ content, credentials,
  private Wine data, unused inherited services, or unsupported plugins.
- AC-12: Every retained source directory and dependency maps to a requirement
  in this specification.
- AC-13: Exact-profile character snapshots publish validated current HP,
  current and maximum mana, and text-only equipment. Maximum HP and its
  percentage remain absent until independently proven. Every invalid,
  inconsistent, zoning, camping, character-select, process-exit, and
  unsupported-client path clears stale character data.
- AC-14: The bounded local combat-log parser produces deterministic encounter
  totals, DPS, percentages, and active durations, follows append/rotation
  lifecycle safely, emits no private diagnostics, and performs no network or
  game-state access.
- AC-15: The Character and Parse docks form the default saved left column,
  remain responsive under bounded live updates, retain the approved X11 class,
  and do not alter game focus, fullscreen, input, or state.

## Resolved Phase 1 decisions

- Retain the proposed narrow MVP information fields, but remove any field that
  cannot be isolated and validated safely.
- The technically viable proof uses same-user `process_vm_readv`, exact file
  mappings, and an external Xlib/Xext/Xfixes window.
- The game may remain in its existing true-fullscreen mode because the
  companion window does not stack above or alter it.
- Use GCC C++20, CMake, Ninja, system X11 libraries, and no vendored runtime
  dependency.
- Package publication is authorized.
- Record that the owner knowingly accepts development contrary to the Daybreak
  EULA. This permits read-only research but does not authorize writes,
  injection, automation, or protection bypass.
- Use plain F11 for the proof toggle.
- Reserve `$XDG_CONFIG_HOME/plazmic-legends/config.toml` for configuration and
  `$XDG_STATE_HOME/plazmic-legends/plazmic-legends.log` for logs, with standard
  per-user defaults when the variables are unset.

## Post-Phase 1 UI decision

- The external X11 overlay experiment is not the product UI. On the reference
  DWM, true-fullscreen clients intentionally stack above override windows, and
  removing fullscreen changed the game's focus presentation and opacity.
- Phase 2 uses a normal independent Qt 6 Widgets application inspired by
  ShowEQ's map/spawn layout but independently implemented.
- The application uses one dock-based main window by default and places every
  Plazmic top-level window on DWM tag 5 through a class rule.
- Static geometry comes from the user's installed Legends map files. Dynamic
  player and spawn values come from validated immutable snapshots.
- ShowEQ 6.4.25 is conceptual guidance only; no GPL implementation, packet
  capture, protocol data, map, or generated table is retained.
