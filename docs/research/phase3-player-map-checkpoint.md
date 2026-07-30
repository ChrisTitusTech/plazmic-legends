# Phase 3 map and player checkpoint

Captured: 2026-07-30

This checkpoint adds the first live Phase 3 vertical slice. It contains no
game executable, map asset, account data, character name, memory dump, or
captured screenshot.

## Scope

- Load bounded base and numbered map layers from the user's installed
  `maps` directory.
- Render lines and labels in the normal Qt companion window.
- Support fit, pan, zoom, per-layer visibility, and a toggleable, adjustable
  player-Z height filter.
- Read only the current zone and local player fields for the exact supported
  client profile.
- Publish an immutable player snapshot to the UI.
- Draw a live player marker and heading without modifying or focusing the
  game.

Player-follow, lifecycle-transition validation, and spawn collection are not
part of this checkpoint.

## Exact-profile resolvers

The supported executable remains profile `legends-2026-07-29` with SHA-256
`97ee793d491930ac97f91e5e26fac16d84d17ff24afcd24d5390d256e7045661`.
Static x86-64 instruction and data-flow inspection identified the following
profile-local values:

| Field | Resolver |
| --- | --- |
| Local player | Image RVA `0x00f07388` contains the player pointer |
| World data | Image RVA `0x00f07378` contains the world pointer |
| Player X, Y, Z | Player offsets `0x74`, `0x78`, and `0x7c` |
| Heading | Player offset `0x94`, bounded to `[0, 512)` |
| Zone ID | Player offset `0x2fc`, masked with `0x7fff` |
| Zone table | World offset `0x30`, indexed by bounded zone ID |
| Zone entry | ID at `0x0c`; bounded short name at `0x10` |

The reader resolves the image base from the validated live module rather than
using a preferred virtual address. Every pointer addition is overflow checked,
every read remains inside a readable mapping through the existing same-user
reader, coordinates must be finite and bounded, the zone table entry must
match the live zone ID, and the zone short name must pass the existing map-path
validator. The local-player pointer and zone ID are re-read before publication
to reject a snapshot that changed during collection.

No value from an unsupported or partially matched client reaches the UI.

## Coordinate and heading conversion

The client UI and live fields identify player X, Y, and Z. Installed map
geometry uses the established EverQuest map conversion:

```text
map X = -player Y
map Y = -player X
```

The raw heading is converted from `[0, 512)` to degrees. The canvas converts it
to its screen-space heading while retaining the approved native map
orientation.

The map background initially had an extra vertical inversion. After removing
that inversion, controlled live comparison found that sign-only player
placement put the marker on the west wall. Comparing the in-game map arrow
with the same installed geometry isolated the required axis swap above. The
owner manually verified the final marker on the correct south wall and the
heading in the correct direction.

Temporary screenshots used for that comparison remained outside the
repository and were not retained as project artifacts.

## UI and update boundary

`MapCanvas` consumes only immutable `ZoneMap` and `PlayerSnapshot` values. It
does not receive process addresses or call the reader. Static map geometry is
cached in a `QPixmap`; the 250 ms player refresh repaints only the live marker
and text unless the viewport, palette, layers, or map changes. Valid label
positions contribute to fitted bounds even when a map contains no line
records, and resize events update viewport dimensions without discarding the
user's center or zoom. The canvas rejects maps above 50,000 total records,
which exceeds the largest observed installed map while bounding full redraw
work during pan and zoom.

The canvas defaults to a player-relative height band from 15 game units below
through 15 units above the player's current Z. Lines remain visible when
either endpoint or the segment between them intersects that band, while labels
use their own elevation. The right-click map menu can disable or re-enable the
filter and opens numeric controls for independently adjusting the below and
above ranges from 0 through 1,000 units. Disabling the filter renders every
elevation. The enabled state and both ranges persist with the user's window
settings. Until a valid player snapshot exists for the loaded zone, the
enabled filter deliberately shows all geometry instead of presenting an empty
or misleading map. Every Z change invalidates the static cache so the band
remains centered on the current player elevation, while X and Y-only player
movement continues to repaint only the marker and text.

Process discovery, live reads, and zone-map parsing run as one serialized Qt
Concurrent task. The GUI timer skips a tick while the prior task is active and
publishes only completed value results on the event thread. Unavailable-client
discovery and attached-process uniqueness checks are limited to once per
second. Each new attachment revalidates both the client file hash and the live
PE identity before applying profile offsets. A completed map load is followed
by another validated snapshot; if the zone changed while parsing, neither the
obsolete map nor snapshot is published.

The application records a handled zone after the worker returns a map result.
A missing or malformed map therefore remains explicitly unavailable instead
of being reparsed four times per second. Every failed or inconsistent snapshot
clears the previous map and marker rather than presenting old zone state as
current.

## Validation evidence

Automated coverage includes:

- bounded synthetic zone/player reads;
- invalid zone names, non-finite player values, and not-in-world state;
- map bounds, native orientation, round-trip transforms, pan/zoom math,
  viewport resize, label-only geometry, player-map conversion, and heading
  conversion;
- asymmetric player-Z band inclusion, crossing segments, exclusion, reversed
  endpoints, invalid and bounded ranges, canvas off/on control, and settings
  persistence;
- rejection of maps above the renderer's global record ceiling;
- Qt main-window publication of synthetic immutable map/player snapshots; and
- the existing parser, process, compatibility, X11, theme, settings, and
  executable-inspection tests.

Live evidence on the reference Fedora X11/Wine session includes:

- exact supported executable and live PE identity;
- validated current-zone selection and installed-map loading;
- player coordinates and heading at multiple controlled positions;
- final map background, marker position, and heading compared with the
  in-game map and manually accepted by the owner;
- the companion on human DWM tag 5 with no always-on-top state; and
- the game remaining a mapped 2560 by 1440 fullscreen window on its own tag.

## Subsequent Phase 3 completion

The continuation added persisted player-follow, lifecycle-safe invalidation,
explicit unavailable states, second-zone coverage, process reacquisition,
performance measurements, and geometry-only default fit. The automated and
live evidence is recorded in `docs/phase3-completion-report.md`. Phase 3 is
complete and Phase 4 remains behind its explicit approval checkpoint.
