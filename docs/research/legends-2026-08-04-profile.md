# Legends 2026-08-04 compatibility profile

Date: 2026-08-05

Profile: `legends-2026-08-04`

SHA-256:
`d9784a58bb03cb70177d6a494fa71bca2b13ab3c5b8b9d6c26e45bae01597e51`

This checkpoint records privacy-safe evidence for the client update. No game
asset, executable byte, disassembly, string table, screenshot, runtime name,
process address, memory value, or Wine-prefix path is retained.

## Exact identity

| Property | Value |
| --- | --- |
| File size | 15,497,816 bytes |
| Format | PE32+ x86-64 |
| Machine | `0x8664` |
| COFF timestamp | `0x6a725275` |
| Optional-header magic | `0x20b` |
| Image size | `0x16bf000` |
| Entry RVA | `0x731540` |
| Section count | 7 |

The file fingerprint and live mapped PE identity must both match before any
profile address is used. The earlier `legends-2026-07-29` profile remains
immutable and selectable only by its own digest.

## Independently established fields

Static analysis followed instruction and data flow from the exact executable.
The prior profile supplied comparison hypotheses only. Changed values were
re-established against this build instead of adjusting the older profile.

| Boundary | Exact-client evidence and approved value |
| --- | --- |
| Player and world roots | Independent player-state and bounded zone-lookup paths resolve image RVAs `0x00f04ff8` and `0x00f04fe8`. |
| Player state | Position remains at record offsets `0x74`, `0x78`, and `0x7c`; heading remains `0x94`; the zone ID moved to `0x5b0`. Finite-coordinate, heading, zone-mask, and map-name rejection rules are unchanged. |
| Zone table | The world table and entry layout remain `0x30`, `0x0c`, and `0x10`, with a 64-byte short-name bound, `0x7fff` mask, and maximum ID 1000. |
| Spawn list | Reciprocal links, bounded name, type, stable ID, and position retain their meanings. The exact byte-sized level field moved to `0x64c`, so the smallest approved record span is `0x64d`; traversal remains capped at 2,048. |
| Character root | Character-window and stats paths resolve image RVA `0x00f05140`; the active-zone field remains `0x2810`. Stats lookup, current HP, current mana, adjustment, and profile-manager topology retain their proven widths and bounds. |
| Maximum HP | The signed 64-bit local-player maximum-health cache is required at offset `0x2b0`; it is bounded and revalidated with current HP before publication. |
| Maximum mana | Exact-client mana paths and a bounded aligned candidate-family check establish local-player offset `0x2e8`; the prior offset fails the complete vital bounds. |
| Equipment name | The validated equipment container and fixed slot traversal remain bounded. Exact-client item-name paths distinguish the display-name pointer at `0x60` from another printable item text field; the prior offset fails closed. |

No raw address was added outside `src/game/client_profile.cpp`. Unknown digests,
partial identity matches, unreadable mappings, invalid lists, torn snapshots,
invalid values, and malformed text continue to reject the complete snapshot.

## Live checkpoint

On Fedora 44, Linux 7.1.5, GE-Proton11-3, X11, Qt 6.11.1, and the live DXVK
D3D11-over-Vulkan renderer stack with the NVIDIA 610.43.03 driver, two
consecutive full refreshes published the player, installed map, a bounded
159-spawn collection, character vitals, and all 23 equipment slots together.
The aggregate contained one player and 158 NPCs, with 22 occupied equipment
slots. Every reciprocal list, type, stable-ID, level, position, vital,
item-text, and snapshot-revalidation invariant remained valid. Lifecycle
evidence remains pending until the checks listed below and in
`docs/profile-refresh.md` pass.

## Validation evidence

All entries below were observed on 2026-08-05. `$EQ_LEGENDS_DIR` denotes the
private local installation path and is not retained in this checkpoint.

| Step | Exact command | Concrete privacy-safe result |
| --- | --- | --- |
| File identity | `python3 tools/inspect_eqgame.py "$EQ_LEGENDS_DIR" --expect-sha256 d9784a58bb03cb70177d6a494fa71bca2b13ab3c5b8b9d6c26e45bae01597e51` | Exit 0; size 15,497,816, PE32+ x86-64, 7 sections, timestamp `0x6a725275`, image size `0x16bf000`, and entry RVA `0x731540`. |
| Static section and instruction analysis | `objdump -h "$EQ_LEGENDS_DIR/eqgame.exe"` and `objdump -d -M intel "$EQ_LEGENDS_DIR/eqgame.exe"` | Established every RVA and record offset listed above through bounded player, zone, spawn, vital, and equipment paths. Derived output remained private and untracked. |
| Bounded live refresh 1 | `private/candidate_probe "$EQ_LEGENDS_DIR/eqgame.exe"` | Exit 0; complete player, map, 159-spawn, character, and 23-slot snapshots; one player, 158 NPCs, no corpses, and 22 occupied slots. |
| Bounded live refresh 2 | `private/candidate_probe "$EQ_LEGENDS_DIR/eqgame.exe"` | Exit 0 with the same complete snapshot invariants and privacy-safe aggregate counts. |
| Repository-profile live refresh | `private/candidate_probe "$EQ_LEGENDS_DIR/eqgame.exe"` | Digest `d9784a58bb03cb70177d6a494fa71bca2b13ab3c5b8b9d6c26e45bae01597e51` selected `legends-2026-08-04`; an unknown digest selected no profile. Two complete player, map, spawn, character, and equipment snapshots passed with the selected profile. |
| Focused selection test | `build/dev/client_status_test` | Exit 0; both immutable known profiles select only by exact digest and an unknown digest fails closed. |
| Repository gate | `cmake --preset dev --fresh`, `cmake --build --preset dev -j2`, `cmake --build --preset check -j2`, and `ctest --preset dev --output-on-failure` | Exit 0; warnings-as-errors build, repository checks, 8 Python tests, and all 17 CTest cases passed. |

Two visibly controlled positions, facings, zones, spawn-field checks, HP/mana
values, and equipment configurations are not yet complete. Character select,
zoning, game exit, process reacquisition, DWM placement, and game invariance
also remain to be repeated for this profile. Until those checks pass, the
residual risk is a coincidental field match or an unobserved lifecycle failure,
and the profile is not merge-ready.

The complete manual profile-refresh gate is recorded in `TASKS.md`. On
2026-08-08, the owner explicitly authorized merging the profile with the
unfinished checks retained as residual risk. The gate must still be complete
before this profile is released.

## Tooling and provenance

The private workspace used GNU binutils, GCC 16.1.1, CMake 4.3.0, and Ninja
1.13.2. Analysis used only the local user-owned client, repository tools, the
existing bounded same-user reader, and prior repository checkpoints. No
external offset, signature, implementation, generated data, map, or game asset
was copied.
