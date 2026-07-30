# ShowEQ 6.4.25 UI architecture review

Status: Planning reference for the post-Phase 1 standalone-window decision.

## Source reviewed

The local `showeq-6.4.25` source archive was inspected as read-only reference
material. ShowEQ describes itself as a passive EverQuest packet analyzer and
is licensed under GPLv2; individual source files commonly permit GPLv2 or
later.

No ShowEQ source, generated data, map asset, protocol definition, or other
artifact is copied into Plazmic Legends. Reusing implementation would create
GPL obligations and would also import an obsolete packet-decoding architecture
that does not match the approved same-user memory reader.

## Useful concepts

ShowEQ demonstrates a companion application rather than an in-game overlay:

- one normal `QMainWindow` owns the application lifecycle;
- maps, spawn lists, statistics, and other views are dockable windows;
- dock layout and main-window geometry are saved and restored;
- a map manager owns zone-map data independently from the map widget;
- a map widget handles painting, zoom, pan, depth filtering, player heading,
  spawn markers, selection, and tooltips;
- a spawn collection is keyed by stable IDs and publishes add, remove, change,
  clear, and selection events;
- map and list views consume the same spawn collection, so selecting an entry
  can highlight the corresponding marker; and
- UI refresh is timer/event driven rather than tied to the game render loop.

These are product and separation patterns, not code to port.

## Concepts rejected

Plazmic Legends will not adopt ShowEQ's:

- privileged packet capture or setuid/root execution;
- packet reassembly, opcode XML, protocol decoding, or network-device setup;
- mutable game objects shared through raw pointers across views;
- command, alert, audio, logging, map-editing, or packet-inspection features;
- generated EverQuest tables, maps, spells, assets, or configuration files;
- global application object graph with direct cross-component mutation; or
- Autotools and legacy Qt compatibility layers.

The existing exact-profile gate, same-user bounded reader, immutable snapshots,
and fail-closed behavior remain authoritative.

## Plazmic translation

The planned Phase 2 UI is a native Qt 6 Widgets companion application:

```text
exact client profile -> bounded external reader -> immutable snapshot
                                                    |
                             +----------------------+
                             |
                       Qt snapshot model
                             |
              +--------------+--------------+
              |                             |
        zone map widget               spawn table model
              |                             |
              +-------- shared ID selection+
```

The initial shell should contain:

- a compatibility/process/zone status strip;
- a central map canvas with pan, zoom, player marker, and heading;
- a dockable spawn table with text filtering and sortable columns;
- a compact selected-spawn detail view; and
- saved window geometry, dock state, and view preferences under the user's XDG
  configuration directory.

The default layout is one main window with docked panes. All Plazmic top-level
windows use X11 instance `plazmic-legends` and class `PlazmicLegends`. The
reference DWM rule assigns that class to tag 5 on monitor 1 (HDMI-0) with
`noswallow=1`. The application does not use `alwaysontop`, switch the game tag,
or request game focus.

The live two-monitor DWM divides nine tags by logical monitor: DP-0 owns tags
1-4 and HDMI-0 owns tags 5-9. Its TOML rule uses the human tag number `5`,
while EWMH inspection reports zero-based desktop index `4`. Phase 2 must verify
both the tag and HDMI-0 geometry because a changed monitor topology can change
tag ownership.

The map and spawn views consume value snapshots and stable IDs only. They never
receive target-process addresses or traverse client memory. Synthetic snapshot
fixtures drive all Phase 2 UI work; live zone and player acquisition belongs to
Phase 3, and spawn acquisition belongs to Phase 4.

The current reference Legends installation contains 1,710 text files: 570 base
map files and 1,140 numbered layer files. These are user-installed inputs.
Plazmic may parse them in place after validating the zone name, record count,
line length, numeric values, and path containment, but must never copy them
into the repository or package. The current inventory and approved parser
bounds are recorded in `docs/research/phase3-map-baseline.md`.

## Dependency decision

Qt 6.11.1 Widgets, GUI, and Core development packages are present on the
reference Fedora host. Phase 2 should use the system Qt 6 packages and record
their LGPL/GPL/commercial licensing terms and dynamic-linking impact before
the dependency is accepted.

## Open inputs

- The supported map record subset and parser bounds must be finalized against
  synthetic fixtures before user-installed maps are loaded.
- The exact spawn columns and filters need a narrow MVP decision.
- The maximum snapshot size and UI refresh rate require measurement against
  synthetic large-spawn fixtures.
- ShowEQ supports many panes and map-editing features; these remain outside the
  Plazmic MVP unless separately approved.
