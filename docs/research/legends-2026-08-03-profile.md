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
| Preferred ImageBase | `0x140000000` |
| Entry RVA | `0x732960` |
| Section count | 7 |

The file fingerprint and live mapped PE identity must both match before any
profile address is used. The earlier and newer profiles remain immutable and
selectable only by their own digests.

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
| Character snapshot | Historical research established character, vital, and equipment fields for the earlier Character model, but this executable is no longer available for the now-required independent maximum-HP validation. The current profile therefore disables the complete Character snapshot instead of guessing or partially publishing it. |

No raw address was added outside `src/game/client_profile.cpp`. Unknown digests,
partial identity matches, unreadable mappings, invalid lists, torn snapshots,
invalid values, and malformed text continue to reject the complete snapshot.

## Live checkpoint

On Fedora 44, Linux 7.1.5, Wine Staging 11.0, X11, and Qt 6.11.1, three
consecutive historical refreshes published the player, installed map, bounded
spawn collection, character vitals, and all 23 equipment slots together under
the earlier Character model. Aggregate spawn counts changed normally between
controlled reads. A fresh product window retained the required X11
instance/class and tag-5 placement without requesting always-on-top or changing
the game window. Those observations do not establish the maximum-HP field
required by the current model, so current builds publish no Character snapshot
for this profile.

## Validation evidence

All entries below were observed on 2026-08-04. `$EQ_LEGENDS_DIR` denotes the
private local installation path and is not retained in this checkpoint.

| Step | Exact command | Concrete privacy-safe result |
| --- | --- | --- |
| File identity | `python3 tools/inspect_eqgame.py "$EQ_LEGENDS_DIR" --expect-sha256 f8af4e704746118f8dd94b688e585bc5c37c3d085da620136bcacad5486145ac` | Exit 0; size 15,505,496, PE32+ x86-64, 7 sections, timestamp `0x6a711da7`, image size `0x16c0000`, and entry RVA `0x732960`. |
| Static section and instruction analysis | `objdump -h "$EQ_LEGENDS_DIR/eqgame.exe"` and `objdump -d -M intel "$EQ_LEGENDS_DIR/eqgame.exe"` | Established every currently approved RVA and record offset through bounded player, zone, and spawn paths; vital and equipment observations remain historical. Derived output remained private and untracked. |
| Bounded live refresh 1 | `private/live_probe "$EQ_LEGENDS_DIR/eqgame.exe"` | Exit 0; complete player, map, and 149-spawn snapshots. Character and 23-slot results, including 22 occupied slots, were observations under the earlier Character model and are not enabled now. |
| Bounded live refresh 2 | `private/live_probe "$EQ_LEGENDS_DIR/eqgame.exe"` | Exit 0 with the same player, map, and spawn invariants and privacy-safe aggregate counts; Character results were historical. |
| Bounded live refresh 3 | `private/live_probe "$EQ_LEGENDS_DIR/eqgame.exe"` | Exit 0 with the same player, map, and spawn invariants and privacy-safe aggregate counts; Character results were historical. |
| Fresh product window | `build/dev/plazmic-legends --client "$EQ_LEGENDS_DIR/eqgame.exe"` | The independent window was viewable with instance `plazmic-legends`, class `PlazmicLegends`, tag-5 placement, and no always-on-top request. The game window was unchanged. |
| Repository gate | `cmake --preset dev --fresh`, `cmake --build --preset dev`, `cmake --build --preset check -j2`, and `ctest --preset dev --output-on-failure` | Exit 0; warnings-as-errors build, repository checks, 3 Python tests, and all 16 CTest cases passed. |
| Diff gate | `git diff --check` | Exit 0. The prior profile block matched `main` exactly and no private or generated research artifact appeared in the worktree. |

The two visible positions and facings agreed with the client UI. A second zone,
character select, camping, game exit, and process reacquisition are explicitly
not complete. The historical HP, mana, and equipment observations are retained
as research evidence but are not enabled by the current profile. On 2026-08-11,
the owner explicitly authorized updating and merging all open pull requests
with these incomplete checks and the unavailable Character capability recorded
as residual risk. This is a merge exception only; the profile remains blocked
from release until the complete manual profile-refresh gate in `TASKS.md`
passes.

## Tooling and provenance

The private workspace used GNU binutils 2.46.1, GCC 16.1.1, CMake 4.3.0, and
Ninja 1.13.2. Analysis used only the local user-owned client, repository tools,
the existing bounded same-user reader, and prior repository checkpoints. No
external offset, signature, implementation, generated data, map, or game asset
was copied. The temporary workspace and all derived artifacts are removed
after the gate.
