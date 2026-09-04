# Legends 2026-09-02 compatibility profile

Date: 2026-09-03

Profile: `legends-2026-09-02`

SHA-256:
`f1c6ab2f07a5d08e62bb936061fd01049fa7b64ce8ddac50c57009162088a9f9`

This checkpoint records privacy-safe evidence for the September 2 patched
client. It contains no game asset, executable byte, disassembly, string table,
screenshot, runtime name, process address, gameplay value, or Wine-prefix
path.

## Exact identity

| Property | Value |
| --- | --- |
| File size | 15,528,056 bytes |
| Format | PE32+ x86-64 |
| Machine | `0x8664` |
| COFF timestamp | `0x6a983aba` |
| Optional-header magic | `0x20b` |
| Image size | `0x16c6000` |
| Entry RVA | `0x737a10` |
| Section count | 7 |

The on-disk digest and mapped PE identity must both match. All seven older
profiles remain immutable and select only their own exact digests.

Owner-only candidate material and proprietary derived indexes were kept
outside the repository and blocked from runtime selection while the candidate
was incomplete.

## Resolver and bounded observations

Independent semantic data flow re-established the lifecycle-checked local
player and character-owner globals, the masked world table, player position
and heading, zone selection, reciprocal spawn traversal, identity fields, and
the target-level getter. The patch retained the player, zone-entry,
coordinate, spawn-link, identity, and level layouts while moving the three
profile roots.

The character update paths retain the validated 32-bit current and maximum
health, direct current-health cache, 32-bit current mana, 64-bit maximum mana,
bounded active-profile inventory chain, and qualified equipment display-name
field. The banked-AA label handler still invokes the exact character-root
accessor and reads the same resolved-record dword. The XP and AA-progress label
handlers now write a different adjacent cache pair; the neighboring AA cache
is not treated as authoritative for banked points.

| Profile value | Static resolver | Bounded observations | Exact value |
| --- | --- | --- | --- |
| Local player | Repeated lifecycle null checks | Readable exact root twice | `0x00f0c360` RVA |
| World data | Masked bounded zone-table lookup | Exact entry and bounded name twice | `0x00f0be50` RVA |
| Player position and heading | Grouped movement and heading paths | Finite and bounded twice | X `0x74`, Y `0x78`, Z `0x7c`, heading `0x94` |
| Player zone ID | Local-record and world-table match | Exact masked match twice | `0x59c` |
| Zone entry | Bounded table access | ID and terminated name twice | Table `0x30`, ID `0x0c`, name `0x10` |
| Spawn links and identity | Traversal, formatter, and identity paths | Complete reciprocal collections twice | Next `0x08`, previous `0x10`, name `0x0b8`, type `0x139`, ID `0x178` |
| Spawn level | Target formatter and exact getter | Every record bounded | `0x4bc`; record span `0x4bd` |
| Character owner | Repeated owner and lifecycle paths | Readable exact root twice | `0x00f0c4b0` RVA |
| Current health | Vital update path | Finite direct 32-bit cache twice | Stats `0x2770`, 4 bytes, no adjustment |
| Maximum health | Vital update and initialization paths | Finite and not below current twice | Player `0x4d0`, 4 bytes |
| Current and maximum mana | Independent mana update paths | Finite and ordered twice | Stats `0x276c`, 4 bytes; player `0x5a8`, 8 bytes |
| Equipment display name | Inventory and display-name paths | Bounded slots and complete reread twice | Item pointer `0x68` |
| Current XP | Exact XP label handler and cache write | Finite in `[0, 100]` twice | `0x00faa538` RVA |
| Current AA percent | Exact AA-progress label handler and cache write | Finite in `[0, 100]` twice | RVA `0x00faa538`, offset `0x04` |
| Unallocated AA | Exact banked-AA character-root accessor | Bounded stable dword twice | Resolved record offset `0xaa64` |

Two consecutive observations through the production core passed exact mapped
PE selection, finite player state, the same valid zone, complete reciprocal
spawn collections, Character, bounded equipment slots, XP, AA progress, and
banked AA. No runtime name, gameplay value, address, zone text, or item text
was printed or retained.

## Validation status

The exact fingerprint, profile contract, focused selection and character
reader tests, and two consecutive complete read-only observations pass. A
fresh configure and warnings-as-errors build, the repository check, Python
tests, all CTest cases including X11 and performance coverage, the exact
fingerprint check, and `git diff --check` also pass on this source head.

The validated binary was atomically installed at the shell-resolved unmanaged
path with SHA-256
`bde4a84e772b2c65e0b7d4aefb19339c2f8233da4b991fcb55fb726bc72bb884`,
matching the build artifact. The installed owner and mode are `root:root` and
`0755`; the superseded binary with SHA-256
`af8d5963bd44a2ee4078a31d3c939df36438c122c8459a6f113b2c0c2612df90`
is preserved in the local hash-addressed rollback directory. No companion was
running during replacement, so the next launch will use the validated binary.

The broader controlled live gate remains open: character select, a second
zone, changed position and facing, visibly changed vitals, a second equipment
configuration, explicit spawn add/remove/change, zoning, camping, client exit
and reacquisition, DWM placement, performance, game-window invariance, and an
independent review. Those checks remain validation debt in `TASKS.md`; they
are not implied by the two bounded in-world snapshots above.

No analysis or application step wrote target memory, injected code, changed
the Wine prefix, synthesized input, weakened host security, or retained a
private client-derived artifact in Git.
