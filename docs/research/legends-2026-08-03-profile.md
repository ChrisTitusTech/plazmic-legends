# Legends 2026-08-03 compatibility profile

Date: 2026-08-04

Profile: `legends-2026-08-03`

SHA-256:
`f8af4e704746118f8dd94b688e585bc5c37c3d085da620136bcacad5486145ac`

This checkpoint records privacy-safe evidence for the client update. No game
asset, executable byte, disassembly, string table, screenshot, runtime name,
process address, memory value, or Wine-prefix path is retained.

## Exact identity

| Property | Value |
| --- | --- |
| File size | 15,505,496 bytes |
| Format | PE32+ x86-64 |
| Machine | `0x8664` |
| COFF timestamp | `0x6a711da7` |
| Optional-header magic | `0x20b` |
| Image size | `0x16c0000` |
| Entry RVA | `0x732960` |
| Section count | 7 |

The file fingerprint and live mapped PE identity must both match before any
profile address is used. The earlier `legends-2026-07-29` profile remains
immutable and selectable only by its own digest.

## Independently established fields

Static analysis followed instruction and data flow from the exact executable.
The prior profile supplied comparison hypotheses only. Every changed value was
re-established against this build instead of adjusting the older profile.

| Boundary | Exact-client evidence and approved value |
| --- | --- |
| Player and world roots | Independent player-state and bounded zone-lookup paths resolve image RVAs `0x00f05ff8` and `0x00f05fe8`. |
| Player state | Position remains at record offsets `0x74`, `0x78`, and `0x7c`; heading remains `0x94`; the zone ID moved to `0x4a0`. Finite-coordinate, heading, zone-mask, and map-name rejection rules are unchanged. |
| Zone table | The world table and entry layout remain `0x30`, `0x0c`, and `0x10`, with a 64-byte short-name bound, `0x7fff` mask, and maximum ID 1000. |
| Spawn list | Reciprocal links, bounded name, type, stable ID, and position retain their meanings. The exact byte-sized level getter moved to `0x620`, so the smallest approved record span is `0x621`; traversal remains capped at 2,048. |
| Character root | Character-window and stats paths resolve image RVA `0x00f06140`; the active-zone field remains `0x2810`. Stats lookup, current HP, current mana, adjustment, and profile-manager topology retain their proven widths and bounds. |
| Maximum mana | Exact-client mana cache paths and an explicit bounded candidate-family check uniquely establish local-player offset `0x2bc`. Other aligned cache candidates fail the complete vital invariants. |
| Equipment name | The validated equipment container and fixed slot traversal remain bounded. Exact-client item paths plus an explicit pointer-field check uniquely establish the bounded item-name pointer at `0xa8`; all other tested aligned candidates fail closed. |

No raw address was added outside `src/game/client_profile.cpp`. Unknown digests,
partial identity matches, unreadable mappings, invalid lists, torn snapshots,
invalid values, and malformed text continue to reject the complete snapshot.

## Live checkpoint

On Fedora 44, Linux 7.1.5, Wine Staging 11.0, X11, and Qt 6.11.1, three
consecutive full refreshes published the player, installed map, bounded spawn
collection, character vitals, and all 23 equipment slots together. Aggregate
spawn counts changed normally between controlled reads, while every reciprocal
list, type, stable-ID, level, position, vital, item-text, and snapshot
revalidation invariant remained valid. A fresh product window retained the
required X11 instance/class and tag-5 placement without requesting
always-on-top or changing the game window.

## Validation evidence

All entries below were observed on 2026-08-04. `$EQ_LEGENDS_DIR` denotes the
private local installation path and is not retained in this checkpoint.

| Step | Exact command | Concrete privacy-safe result |
| --- | --- | --- |
| File identity | `python3 tools/inspect_eqgame.py "$EQ_LEGENDS_DIR" --expect-sha256 f8af4e704746118f8dd94b688e585bc5c37c3d085da620136bcacad5486145ac` | Exit 0; size 15,505,496, PE32+ x86-64, 7 sections, timestamp `0x6a711da7`, image size `0x16c0000`, and entry RVA `0x732960`. |
| Static section and instruction analysis | `objdump -h "$EQ_LEGENDS_DIR/eqgame.exe"` and `objdump -d -M intel "$EQ_LEGENDS_DIR/eqgame.exe"` | Established every RVA and record offset listed above through bounded player, zone, spawn, vital, and equipment paths. Derived output remained private and untracked. |
| Bounded live refresh 1 | `private/live_probe "$EQ_LEGENDS_DIR/eqgame.exe"` | Exit 0; complete player, map, 149-spawn, character, and 23-slot snapshots; 9 players, 140 NPCs, no corpses, and 22 occupied slots. |
| Bounded live refresh 2 | `private/live_probe "$EQ_LEGENDS_DIR/eqgame.exe"` | Exit 0 with the same complete snapshot invariants and privacy-safe aggregate counts. |
| Bounded live refresh 3 | `private/live_probe "$EQ_LEGENDS_DIR/eqgame.exe"` | Exit 0 with the same complete snapshot invariants and privacy-safe aggregate counts. |
| Fresh product window | `build/dev/plazmic-legends --client "$EQ_LEGENDS_DIR/eqgame.exe"` | The independent window was viewable with instance `plazmic-legends`, class `PlazmicLegends`, tag-5 placement, and no always-on-top request. The game window was unchanged. |
| Repository gate | `cmake --preset dev --fresh`, `cmake --build --preset dev`, `cmake --build --preset check -j2`, and `ctest --preset dev --output-on-failure` | Exit 0; warnings-as-errors build, repository checks, 3 Python tests, and all 16 CTest cases passed. |
| Diff gate | `git diff --check` | Exit 0. The prior profile block matched `main` exactly and no private or generated research artifact appeared in the worktree. |

The two visible positions, facings, and HP/mana values were different and
agreed with the client UI. A second zone, second equipment configuration,
character select, camping, game exit, and process reacquisition are explicitly
not yet complete. Until those checks pass, the residual risk is a coincidental
field match or an unobserved lifecycle failure, and the profile is not
merge-ready.

The complete manual profile-refresh gate is recorded in `TASKS.md`; it must be
complete before this profile is merged.

## Tooling and provenance

The private workspace used GNU binutils 2.46.1, GCC 16.1.1, CMake 4.3.0, and
Ninja 1.13.2. Analysis used only the local user-owned client, repository tools,
the existing bounded same-user reader, and prior repository checkpoints. No
external offset, signature, implementation, generated data, map, or game asset
was copied. The temporary workspace and all derived artifacts are removed
after the gate.
