# Plazmic Legends specification

## Problem

The imported codebase was a broad MacroQuest platform designed for traditional
EverQuest clients and Windows development. Phase 0 removed that inherited
implementation so Plazmic Legends could establish a native Linux foundation
for the EverQuest Legends client running under Wine.

Plazmic Legends is now an extensible companion rather than a permanently
minimal viewer. The current `main` branch provides read-only client
observations, local combat-log parsing, maps, and user-directed file tools. Future phases may
add history, analytics, planners, alerts, audio, overlays, sharing, optional
services, extensions, and other useful capabilities. Each capability must have
an explicit contract for provenance, consent, privacy, security, lifecycle,
resource bounds, compatibility, validation, and rollback. Unknown client builds
remain unsupported instead of being paired with guessed memory layouts.

## Users

The initial user runs EverQuest Legends through Wine on Linux. Contributors
need a reproducible Linux build, deterministic tests that do not require a live
account, and a safe way to identify client drift after patches.

Native Windows users and Windows-hosted development are outside the initial
support tier.

## Product principles

- Capability-scoped: every new source, sink, side effect, and privilege is
  independently specified and reviewable.
- Linux-first: configure, build, test, and package on Linux.
- Extensible: the current architecture is a foundation, not a permanent limit
  on product surfaces or integration techniques.
- Version-aware: client-specific knowledge belongs to an explicit
  compatibility profile.
- Fail closed: an unrecognized or ambiguous build is unsupported.
- Recoverable: failure must not corrupt game state or require editing the game
  installation to recover.
- Privacy by default: local/offline behavior is the baseline; transfers and
  remote services are optional, disclosed, and explicitly enabled.
- Provenance safe: incompatible code, data, assets, fixtures, and generated
  content are not copied into the project.

## Required behavior

### Expansion capability model

- Existing capabilities retain their validated behavior unless a later phase
  explicitly supersedes it with migration and rollback instructions.
- A major capability is delivered in its own pull request. Stacked work must
  state its base, merge order, and independent validation surface.
- Every capability documents input sources, outputs and side effects, retained
  data, resource bounds, lifecycle invalidation, privacy behavior, optional
  dependencies, license provenance, failure states, and removal path.
- Client memory, log files, user-imported packs, local files, audio devices,
  and network services are distinct trust boundaries. Approval of one does not
  imply approval of another.
- Capabilities that write files, communicate over a network, display overlays,
  emit audio, automate work, control input, or modify a process are disabled by
  default until the user enables that exact behavior.
- No capability may bypass authentication, integrity checks, anti-cheat, or
  other client protections. Encountering such a protection is a stop condition
  for that design.

### Planned product surfaces

- Combat and overview: bounded encounter and zone history, attack and spell
  drill-down, damage and healing timelines, log-derived active-encounter target
  context, recent activity, and rate/ETA summaries. A true game-target field is
  a separate exact-profile input that this surface does not approve.
- Progression and activity: per-character XP and AA history, loot history,
  inventory reconciliation, class-combination summaries, proc analytics, and
  upgrade observations derived from bounded active-log events, existing
  immutable equipment, and explicit local inventory imports.
- Buffs, timers, and alerts: bounded buff state, duration and respawn timers,
  local rule matching, visible notifications, optional sounds or voice packs,
  and celebration events derived from bounded local log/activity events, user
  timer values, and provenance-validated imported duration or audio packs.
- Map workflows: point-of-interest search, label decluttering, floor slicing,
  pinned zones, typed `/loc` navigation, and user-owned annotations.
- Presentation: normal, detached, and overlay-style Linux windows for damage,
  healing, progression, buffs, and timers, with explicit stacking,
  click-through, lock, focus, and privacy controls.
- Knowledge and planning: schema-validated, versioned user-imported or
  license-compatible packs for item, quest, recipe, Plane of Sky, Exaltation,
  raid-target identity and history, and related planning views.
- Profiles and services: per-character switching, settings and alert
  import/export, and separately consented update, feedback, sharing, or
  telemetry services when a phase demonstrates their value and safeguards.

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

- a Character dock showing the active character name, current and maximum HP,
  rounded whole HP percentage, current and maximum MP, rounded whole MP
  percentage;
- a Parse dock showing one condensed current or most-recent encounter with
  participant, total damage, average DPS, percentage, and active duration.

A full-width summary bar above the Details content shows current encounter DPS,
XP rate and pace, current AA state and rate, and the latest bounded activity.
Its four columns are independently resizable and initially allocate extra width
to XP and AA so those values remain readable.
The Activity dock retains its progression, evidence, and inventory tabs without
duplicating this summary.

The Activity dock's Inventory tab consolidates the text-only equipped slot and
item names with explicitly imported inventory-output reconciliation. The
Character dock does not duplicate the equipment table.

HP, mana, character identity, and equipped item values come only from bounded
exact-profile, same-user process reads. Each field requires independent static
evidence, two controlled live observations, explicit bounds, and lifecycle
invalidation before it may be displayed. Equipment in the Activity Inventory
tab uses slot labels and text only; the project does not copy or package game
icons, item data, or assets.

DPS is derived locally from the user's EverQuest combat log, not from client
memory. The parser follows only the active character's bounded log file,
handles appended and rotated/truncated files, starts a new encounter after an
inactivity boundary, and may persist bounded encounter content, including
combatant and target names, in an owner-only local state file for the History
view only after the user enables `User > Retain Combat History`. Retention is
off by default. It retains at most 50 encounters for 90 days, caps each
partition at 2 MiB, and never includes those values in diagnostics or network
traffic.
Missing, disabled, ambiguous, malformed, oversized, or unavailable logs show
an explicit unavailable state without affecting memory-backed character data.

The same bounded active-character stream may recognize exact XP percentage,
AA-point-total, and loot forms for the Activity dock. It may also aggregate
the active character's named damage abilities as observation evidence and
record changes between consecutive immutable equipment snapshots. It does not
turn an observed ability into an asserted proc or class, and it does not infer
a class combination without a validated catalog. Rates use only the latest
hour of retained progress observations; the level pace is the time to earn
100% at that rate, not a claim about the character's current level position.
Recent loot means the latest 24 hours.

For the exact `legends-2026-08-06` client only, the immutable character
snapshot may additionally publish bounded current AA progress and banked
points from the client's read-only progression cache. These optional fields
are read twice, validated as 0-100 percent and at most 10,000,000 points, and
discarded for the frame if unavailable, invalid, or inconsistent. They are
never inferred for another profile, persisted in activity history, exported,
logged, or sent over the network. The Activity dock prefers this current
snapshot over an older log total while retaining exact log events for rates.

Activity retention is independently opt-in and partitioned by the same stable
opaque character-and-selected-log key. The selected log filename must contain a
bounded validated server-identity suffix and separates same-named characters on
different servers. The
unkeyed digest prevents raw names from appearing in filenames but does not
resist candidate-name guessing; owner-only permissions are the confidentiality
boundary. Each owner-only schema-1 partition is
capped at 512 events, 512 ability/category pairs, 4,096 observations, 90 days,
and 2 MiB. Timestamped replay metadata shares the same 90-day limit. Expiry
maintenance also runs in bounded batches while the selected combat log is
missing or unreadable. Replay suppression requires both same-file identity and
complete-line evidence from a bounded common prefix; replacement and recreation
lines receive fresh identities. A persisted generation counter prevents token
reuse across restart. Persisted log-derived observations require the exact bounded source-identity shape;
malformed partitions fail closed. The User menu
can export the displayed bounded JSON or delete the selected partition.
Diagnostics and network traffic contain none of its names or values.

An explicitly selected local EverQuest inventory-output file may be parsed as
a regular non-symlink file capped at 2 MiB, 4,096 rows, and 4,096 bytes per row.
Recognized item and key-ring section headers define row shape; repeated slot
labels and zero-count empty rows are valid. Quantities are bounded.
Reconciliation compares case-insensitive item names with the current text
equipment snapshot; it does not infer item stats, upgrades, inventory
ownership, or class requirements.

The parser is an independent implementation informed only by public product
behavior. It does not copy third-party code, data, fixtures, or assets. Later
phases may build bounded histories, timers, proc or resist analytics,
knowledge stores, sharing, and optional services on top of this parser under
the expansion capability model.

The User menu can export the current immutable character name and text-only
equipment snapshot as an offline JSON profile backup compatible with the EQ
Legends Tools character sheet. The export derives the site's stable
item identifiers from bounded item names, writes only to a user-selected local
file, and performs no upload or network request. Race, tri-class, favored
stats, and Exaltations are not read by Plazmic Legends. The optional live AA
snapshot is intentionally outside this inventory-only export. Those fields are
therefore not invented in the export; users must verify them after importing
the backup.

### Window and map interaction

- The default application is a normal independent Linux window. Approved
  presentation phases may add independent overlay-style Linux windows without
  drawing inside the game process.
- The main window contains a central map canvas and dockable spawn and detail
  views inspired conceptually by ShowEQ.
- The default left dock area contains Character above tabified Parse and
  Activity docks. They retain movable, closable, floatable, saved-layout, and
  X11 class behavior. Details spans below the central map and right-side Spawns
  dock, with the DPS, XP, AA, and latest-activity summary fixed directly above
  its content at the same width.
- An embedded top menu bar exposes checkable Views actions for Character,
  Parse, Activity, Spawns, and Details, plus minimize, maximize/restore, and
  close controls aligned at the top right.
- Its User menu exposes `UI File Install...` for an extracted private bundle.
- The main window and any detached Plazmic tool windows expose
  `WM_CLASS(STRING) = "plazmic-legends", "PlazmicLegends"` so DWM can place
  them predictably.
- On the reference two-monitor DWM session, normal Plazmic windows open on tag
  5, owned by monitor 1 (HDMI-0). Overlay roles may request topmost and
  click-through behavior only when explicitly enabled; no Plazmic role may
  activate a game tag or steal game focus.
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
  The exact-client level field must be semantically checked against the visible
  game level for both the local player and an NPC; a plausible percentage or
  other bounded byte is not an acceptable substitute.
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

Configuration and logs use the XDG base-directory conventions. Runtime
observation leaves the game installation and Wine prefix unmodified. The
private UI-file installer is the currently implemented write capability and is
constrained by its allowlist, confirmation, backup, and rollback contract.
Future side effects use similarly explicit capability contracts.

## Security, privacy, and compliance

- Use the least Linux and target-process access required by the approved
  integration.
- Do not require root, setuid, broad ptrace-policy changes, or disabled system
  security controls. If the design requires one, reject it and revisit the
  architecture.
- The current release does not inject a PE DLL, add a Wine DLL override, hook
  DirectX, or patch the Wine prefix. A future integration mechanism must be
  isolated in a dedicated phase and cannot bypass a client protection.
- Network listeners, remote commands, telemetry, updates, feedback, accounts,
  and credential storage require separate threat models. They are disabled by
  default, disclose endpoints and payloads, minimize retention, and provide a
  local disable and deletion path.
- Do not bundle or redistribute `eqgame.exe`, Daybreak libraries, data files,
  symbols, or other proprietary content.
- Do not bypass authentication, integrity checks, anti-cheat, or client
  protections.
- Memory access is limited to fields approved by exact compatibility profiles.
  A write, input, automation, or control capability requires explicit user
  authorization, a dedicated phase, a least-privilege boundary, visible state,
  an immediate disable path, and failure-isolation evidence.
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
  encounter snapshots without blocking the UI thread. Combat snapshots retain
  at most 4,096 aggregate attack/spell detail rows, and the UI independently
  enforces the same display cap.
- Activity parsing shares the combat stream's byte, line, rotation, and
  lifecycle bounds. Activity memory and storage cap events and observed
  abilities at 512 each and inventory imports at 2 MiB and 4,096 rows.
- A supported profile is immutable. A client patch creates a new profile and
  runs the compatibility gate.

### Local development installation and testing

- Automated build and synthetic test commands may run artifacts directly from
  `build/`; they do not modify the installed application.
- Any manual, X11, or live-client test intended to validate a newly built
  version must first overwrite the shell-resolved unmanaged local installation
  with that exact validated artifact. On the reference host, the target is
  `/usr/local/bin/plazmic-legends`. Running only `build/dev/plazmic-legends`
  does not satisfy local manual-test evidence.
- Before replacement, resolve the command with `command -v`, inspect package
  ownership, and preserve the current installed executable under
  `build/local-install-backup/`, identified by its SHA-256. If the resolved
  target differs or is package-owned, stop instead of overwriting it without
  explicit approval.
- Stage the new executable beside the installed target, verify that its SHA-256
  matches the validated build, and atomically replace the installed file.
  Restore and verify `root:root` ownership and mode `0755`, then confirm the
  installed version and SHA-256 match the build.
- Inspect any running Plazmic process separately from the installed pathname.
  A process retaining the prior inode must be closed and relaunched before its
  behavior counts as evidence for the new build; do not terminate a running
  process unless that disruption was explicitly authorized.
- Finish with a non-disruptive smoke test through the installed command and
  verify that no unintended process or staging file remains.

### Private UI file installation

- `tools/export_private_ui_bundle.sh` copies the locally installed
  `uifiles/plazmic-ui` skin, `eqclient.ini`, all `UI_*.ini` window layouts, and
  character INIs containing HotButtons or additional-filter state into an
  ignored private bundle with a resolution manifest and SHA-256 inventory.
- The exporter also derives `UI_plazmic_1440p.ini` from the newest installed
  Legends layout. It preserves every unknown Legends section and value while
  changing only allowlisted anchor, position, width, and height keys for a
  cohesive bottom command strip, paired chat columns, left combat rail, and
  top-right map. It does not import layouts or assets from another EQ client.
- The bundle and archive retain private character/server filenames and
  proprietary game assets. They remain local user-owned inputs and are never
  committed, packaged, logged, uploaded, or attached to a pull request.
- `User > UI File Install...` accepts an extracted bundle only after validating
  its format, bounded file inventory, SHA-256 hashes, required skin files, and
  absence of symbolic links or path traversal.
- The user selects the source and destination layout INIs and the source and
  destination character/filter INIs. Installing bundled `eqclient.ini` global
  filters and 1440p settings is a separate, explicit choice.
- Source choices are allowlisted to regular, non-symlink `*.ini` files in the
  bundle's `ini/layouts` and `ini/characters` directories. Destination choices
  are allowlisted to regular, non-symlink `UI_*.ini` layouts and character INIs
  with recognized UI/filter sections directly under the canonical selected
  game directory; `eqclient.ini` is the only optional global target. Every
  path outside these exact roots and patterns is rejected.
- Installation requires a valid selected Legends directory but may run while
  EverQuest is active. In addition to the selected destination, the installer
  writes the chosen layout to the reserved `UI_plazmic_1440p.ini` live source.
  The success dialog tells the user to select that source with `/copylayout`,
  then apply the skin with `/loadskin plazmic-ui 1`. Before replacement,
  selected INIs, an existing live source, and any existing
  `uifiles/plazmic-ui` are preserved in a private, timestamped rollback
  directory inside the selected game directory.
- Activation is transactional: if any replacement fails, every already
  replaced INI and `uifiles/plazmic-ui` target is restored from rollback before
  failure is reported. A normal failure may not leave a mixed old/new state;
  an independently failed rollback is reported as a critical recovery error.
- This capability is limited to the confirmed skin and INI targets. It does
  not implicitly authorize any other write, automation, input, integration, or
  control capability.

## Current exclusions

These exclusions protect the project rather than permanently limit its product
categories:

- bypassing authentication, integrity checks, anti-cheat, or client
  protections;
- silently collecting, retaining, uploading, or sharing private user data;
- copying incompatible or unverified third-party code, databases, assets,
  fixtures, generated content, or proprietary game files;
- guessing client offsets or weakening exact-profile failure rules;
- unbounded work, hidden side effects, or capabilities without a disable and
  rollback path; and
- claiming support for an unvalidated platform, client build, or integration.

Native Windows, Wayland, scripting, extensions, automation, remote control,
alternate clients, injection, update services, and other new product areas are
not implemented by the current release. They may be proposed through the
expansion capability model; none is implicitly approved by this roadmap.

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
- AC-13: Exact-profile character snapshots publish validated current and
  maximum HP, current and maximum MP, both percentages, and text-only
  equipment. Every invalid,
  inconsistent, zoning, camping, character-select, process-exit, and
  unsupported-client path clears stale character data.
- AC-14: The bounded local combat-log parser produces deterministic encounter
  totals, DPS, percentages, and active durations, follows append/rotation
  lifecycle safely, emits no private diagnostics, and performs no network or
  game-state access.
- AC-15: The Character and Parse docks form the default saved left column,
  remain responsive under bounded live updates, retain the approved X11 class,
  and do not alter game focus, fullscreen, input, or state.
- AC-16: The private UI exporter produces a bounded, integrity-inventoried
  2560x1440 bundle without tracking private data, and `User > UI File
  Install...` asks which layout and character INIs to replace, optionally
  replaces `eqclient.ini`, installs a dedicated `/copylayout` source for live
  activation, preserves a private rollback, and does not implicitly authorize
  gameplay-state modification.
- AC-17: Local manual or live-client evidence for a changed build uses the
  atomically replaced shell-resolved local installation, whose hash matches the
  validated build and whose prior binary has a verified rollback copy. A
  build-tree-only launch or process retaining the old inode is not accepted as
  evidence for the newly built version.
- AC-18: `User > Export Inventory...` exports an available immutable character
  snapshot to a user-selected, owner-only JSON file accepted by EQ Legends
  Tools' `Import Profile Backup` action. Every supported equipment slot maps
  deterministically, empty slots are omitted, unavailable or malformed
  snapshots fail closed, and the export adds no network access, game-state
  write, private logging, or bundled item database.
- AC-19: Every expansion feature has an independently reviewable pull request
  and a recorded source, side-effect, privacy, license, lifecycle, resource,
  validation, and rollback contract.
- AC-20: Combat and overview histories are immutable, bounded, deterministic,
  lifecycle-aware, partitioned by a stable opaque character-and-selected-log
  key, and testable with synthetic logs. Character switching and restart
  restore only that character's owner-only local history. The history may
  contain combatant and target names for display, while diagnostics and network
  traffic retain no private names or encounter values.
- AC-21: Progression, loot, inventory, class, proc, upgrade, and
  celebration activity is scoped per character, bounded on disk and in memory,
  and reset or restored predictably across log rotation, zoning, character
  changes, and restart.
- AC-22: Buffs, timers, alerts, and audio are user-configurable, disabled
  independently, rate-limited, privacy-safe, lifecycle-aware, and testable
  without a live client or sound device.
- AC-23: POI search, label decluttering, floor slicing, pinned zones, typed
  locations, and annotations are bounded, path-safe, deterministic, local,
  recoverable, and testable without a live client.
- AC-24: A knowledge pack is accepted only after schema, version, size,
  integrity, path, and provenance validation. Raid-target classification and
  history require a validated roster from such a pack. The package and
  repository do not embed incompatible upstream databases or assets.
- AC-25: Overlay windows preserve focus and input unless the user explicitly
  chooses interaction, expose their active state, and degrade safely when the
  window manager cannot provide requested stacking or click-through behavior.
- AC-26: Imports, exports, feedback, sharing, updates, and telemetry preview or
  document their exact payload and destination, require capability-specific
  consent, fail closed, and provide disable, deletion, and rollback behavior.

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
  EULA. This authorizes the capabilities accepted in their individual phases;
  it never authorizes protection bypass or silently expands one capability's
  scope into another.
- Use plain F11 for the proof toggle.
- Reserve `$XDG_CONFIG_HOME/plazmic-legends/config.toml` for configuration and
  `$XDG_STATE_HOME/plazmic-legends/plazmic-legends.log` for logs, with standard
  per-user defaults when the variables are unset.

## Post-Phase 1 UI decision

- The Phase 1 external X11 overlay experiment was not suitable as the primary
  UI. On the reference DWM, true-fullscreen clients intentionally stack above
  override windows, and removing fullscreen changed the game's focus
  presentation and opacity. This is a historical constraint of that
  experiment, not a permanent ban on independent overlays for windowed or
  borderless use.
- Phase 2 uses a normal independent Qt 6 Widgets application inspired by
  ShowEQ's map/spawn layout but independently implemented.
- The application uses one dock-based main window by default and places every
  Plazmic top-level window on DWM tag 5 through a class rule.
- Static geometry comes from the user's installed Legends map files. Dynamic
  player and spawn values come from validated immutable snapshots.
- ShowEQ 6.4.25 is conceptual guidance only; no GPL implementation, packet
  capture, protocol data, map, or generated table is retained.
