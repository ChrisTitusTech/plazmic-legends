# Legends 2026-08-25 compatibility candidate

Date: 2026-08-27

Profile: `legends-2026-08-25`

SHA-256:
`6807ac5c672ffee98fcc6a62a5e87d0ec6af1a323251280144a4f1461829f0d4`

This local checkpoint records privacy-safe evidence for a newly patched
client. It contains no game asset, executable byte, disassembly, string table,
screenshot, runtime name, process address, memory value, or Wine-prefix path.

## Exact identity

| Property | Value |
| --- | --- |
| File size | 15,517,304 bytes |
| Format | PE32+ x86-64 |
| Machine | `0x8664` |
| COFF timestamp | `0x6a8e193a` |
| Optional-header magic | `0x20b` |
| Image size | `0x16c3000` |
| Entry RVA | `0x735460` |
| Section count | 7 |

The exact file digest and mapped PE identity must both match before this
candidate can be used. Every older profile remains immutable and selects only
its own digest.

The identity was captured on x86-64 Fedora using Wine 11.0 Staging from
project commit `d3873521445041a6e246063f108990fb340d4cac`. An owner-only
candidate checklist was prepared outside the repository and kept blocked from
runtime selection until the required player, zone, and spawn paths passed.

## Resolver and observation checkpoint

Private exact-client indexing used GNU `objdump` and `strings` from binutils
2.46.1. Repeated null-checked player-lifecycle references resolve the local
player global to image RVA `0x00f09248`. The masked and bounded world-table
lookup resolves the world-data global to image RVA `0x00f08d38`, retains table
offset `0x30`, entry ID `0x0c`, short name `0x10`, mask `0x7fff`, and maximum
zone ID 1000. Grouped player accesses retain X `0x74`, Y `0x78`, Z `0x7c`,
and heading `0x94`.

The carried August 17 zone offset was rejected because it produced no valid
in-world snapshot. Independent local-record and world-table data flow resolves
the current zone field to `0x21c`. That field produced an exact masked table
match and a bounded terminated zone name in consecutive observations.

Spawn traversal retains reciprocal next and previous links at `0x08` and
`0x10`, bounded name storage at `0x0b8`, type byte at `0x139`, ID dword at
`0x178`, and the player coordinate family. The carried August 17 level field
was also rejected. The current target-level formatter and consider paths call
an exact getter whose fallback reads the byte at `0x610`; its alternate path
compares that same field against a derived level before returning the bounded
result. The minimum complete record span is therefore `0x611`.

Two consecutive production-reader observations passed exact mapped-PE
selection, finite player values, the same valid zone, and complete reciprocal
collections of 139 records. Later observations after nearby entities changed
passed complete collections of 141 and 142 records. Every record passed the
name, type, unique nonzero ID, level, position, link, count, cycle, and local
player anchor checks. Runtime names, values, addresses, and zone text were not
retained.

The separately resolved character-owner global is `0x00f09398`, and the
existing descriptor/stats chain still reached bounded current-health and
current-mana fields. However, both carried maximum-vital cache offsets failed
the exact-profile range gate. Because Character is one atomic optional
capability, the August 25 profile sets `character_snapshot_supported` to false
and contains no Character, vital, equipment, current-XP, or current-AA symbols.
Those fields remain unavailable instead of being partially exposed or guessed.

| Profile value | Static resolver | Bounded observations | Candidate value |
| --- | --- | --- | --- |
| Local player | Repeated lifecycle null checks | Exact mapped identity and readable root | `0x00f09248` RVA |
| World data | Masked bounded zone-table lookup | Exact entry match and bounded name | `0x00f08d38` RVA |
| Player position | Grouped finite float access family | Finite and bounded twice | X `0x74`, Y `0x78`, Z `0x7c` |
| Player heading | Heading arithmetic access family | In `[0, 512)` twice | `0x94` |
| Player zone ID | Local-record and world-table match | Exact masked match twice | `0x21c` |
| Zone entry | Bounded table access family | ID and terminated name valid twice | Table `0x30`, ID `0x0c`, name `0x10` |
| Spawn links | List traversal and reciprocal-link paths | Complete reciprocal collections | Next `0x08`, previous `0x10` |
| Spawn identity | Name, type, and ID access families | Every record bounded and unique | Name `0x0b8`, type `0x139`, ID `0x178` |
| Spawn level | Target formatter, getter, and consider paths | Every record in range | `0x610` |
| Spawn position | Player coordinate access family | Every record finite and bounded | X `0x74`, Y `0x78`, Z `0x7c` |
| Character | Maximum-vital fields rejected | Atomic snapshot withheld | Disabled |

No application or analysis step may write target memory, inject code, change
the Wine prefix, weaken host security, synthesize input, or retain a runtime
name, address, memory value, or proprietary derived artifact in Git.

## Automated and installed checkpoint

The fresh warnings-as-errors build, repository gate, 13 Python tests, all 20
CTest cases, exact fingerprint check, staged install inventory, and final
CodeRabbit review passed. The review identified one test fixture that selected
the newly disabled profile for a Character bound; that fixture was corrected
to use the Character-enabled August 17 profile, its focused test passed, and
the repeat review reported zero findings.

The validated build was atomically installed at the shell-resolved unmanaged
path with the previous binary preserved under its SHA-256. Build and installed
SHA-256 both match
`349f442f10c8622e342a32ce1cc97da004d4f975f38d0bf7e97907d142f3016e`;
the installed owner and mode are `root:root` and `0755`. An isolated launch of
that exact installed path selected `legends-2026-08-25` and recorded
`supported`, `running`, `in_world`, and `map state=loaded` in the privacy-safe
log.

The already-open user-visible companion was not interrupted. It continues to
map the prior binary inode, so a user relaunch and matching running-executable
hash remain required before its visible behavior can be attributed to this
replacement.

## Validation status

- Exact file inspection passed for the identity above.
- The private SHA-bound candidate was promoted only after the required player,
  zone, and spawn paths passed production bounds and exact mapped-PE selection.
- Synthetic exact selection and profile-contract tests pass with Character and
  its dependent XP and AA fields explicitly unavailable. The complete
  automated repository and staged-package gates pass, and the exact installed
  path passes an isolated live smoke.
- A controlled changed position and facing, second zone, visible level
  comparison, user-visible relaunch/running-hash check, full lifecycle, DWM,
  and game-invariance gates remain pending.
