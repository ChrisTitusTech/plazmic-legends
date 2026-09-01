# Legends 2026-09-01 compatibility profile

Date: 2026-09-01

Profile: `legends-2026-09-01`

SHA-256:
`bc1eb76dae1a3544f37fe216a0dccc71d2323e9d11d8b612368785e80114c1c5`

This checkpoint records privacy-safe evidence for the September 1 patched
client. It contains no game asset, executable byte, disassembly, string table,
screenshot, runtime name, process address, gameplay value, or Wine-prefix
path.

## Exact identity

| Property | Value |
| --- | --- |
| File size | 15,533,176 bytes |
| Format | PE32+ x86-64 |
| Machine | `0x8664` |
| COFF timestamp | `0x6a96dba9` |
| Optional-header magic | `0x20b` |
| Image size | `0x16c7000` |
| Entry RVA | `0x738540` |
| Section count | 7 |

The on-disk digest and mapped PE identity must both match. All six older
profiles remain immutable and select only their own exact digests.

The identity was inspected on x86-64 Fedora with Wine 11.0 Staging and GNU
binutils 2.46.1. Owner-only candidate material and proprietary derived indexes
were kept outside the repository and blocked from runtime selection while the
candidate was incomplete.

## Resolver and bounded observations

Independent semantic data flow re-established the lifecycle-checked local
player and character-owner globals, the masked world table, player position
and heading, zone selection, reciprocal spawn traversal, identity fields, and
the target-level getter. The patch changed the player zone field and spawn
level field while retaining the bounded zone-entry, coordinate, and spawn-link
layouts.

The character update paths establish 32-bit current and maximum health, a
direct current-health cache with no additional adjustment, 32-bit current
mana, and 64-bit maximum mana. Profile-scoped widths preserve the storage and
adjustment behavior of every older immutable profile. The active-profile
inventory chain remains bounded to 36 slots. Two terminated printable name
candidates were rejected or selected through static display-name use and a
private comparison: the selected display name was consistently the qualified
form, while the other candidate was the shorter base catalog name.

Current XP and AA cache paths moved with the image globals. The active-profile
unallocated-AA accessor still uses offset `0xaa5c`, but its exact operand is a
byte; the reader now records that width in this profile rather than consuming
adjacent state as a dword.

| Profile value | Static resolver | Bounded observations | Exact value |
| --- | --- | --- | --- |
| Local player | Repeated lifecycle null checks | Readable exact root twice | `0x00f0d360` RVA |
| World data | Masked bounded zone-table lookup | Exact entry and bounded name twice | `0x00f0ce50` RVA |
| Player position and heading | Grouped movement and heading paths | Finite and bounded twice | X `0x74`, Y `0x78`, Z `0x7c`, heading `0x94` |
| Player zone ID | Local-record and world-table match | Exact masked match twice | `0x59c` |
| Zone entry | Bounded table access | ID and terminated name twice | Table `0x30`, ID `0x0c`, name `0x10` |
| Spawn links and identity | Traversal, formatter, and identity paths | Complete reciprocal collections twice | Next `0x08`, previous `0x10`, name `0x0b8`, type `0x139`, ID `0x178` |
| Spawn level | Target formatter and exact getter | Every record bounded | `0x4bc`; record span `0x4bd` |
| Character owner | Repeated owner and lifecycle paths | Readable exact root twice | `0x00f0d4b0` RVA |
| Current health | Vital update path | Finite direct 32-bit cache twice | Stats `0x2770`, 4 bytes, no adjustment |
| Maximum health | Vital update and initialization paths | Finite and not below current twice | Player `0x4d0`, 4 bytes |
| Current and maximum mana | Independent mana update paths | Finite and ordered twice | Stats `0x276c`, 4 bytes; player `0x5a8`, 8 bytes |
| Equipment display name | Inventory and display-name paths | 23 bounded slots and complete reread twice | Item pointer `0x68` |
| Current XP | Experience-percent cache path | Finite in `[0, 100]` twice | `0x00faa688` RVA |
| Current AA percent | Progression-cache path | Finite in `[0, 100]` twice | RVA `0x00faa440`, offset `0x24c` |
| Unallocated AA | Active-profile accessor | Bounded byte twice | Offset `0xaa5c`, 1 byte |

Two consecutive observations through the production core passed exact mapped
PE selection, finite player state, the same valid zone, complete reciprocal
spawn collections, Character, all 23 equipment slots, current XP, AA percent,
and unallocated AA. A later observation also accepted a changed complete spawn
collection. No runtime name, gameplay value, address, zone text, or item text
was printed or retained.

## Validation status

The exact fingerprint, profile contract, focused selection and character
reader tests, and two consecutive complete read-only observations pass. A
fresh configure and warnings-as-errors build, the repository check, 13 Python
tests, all 21 CTest cases including X11 and performance coverage, the exact
fingerprint check, and `git diff --check` also pass on this source head.

CodeRabbit 0.7.5 was installed and authenticated, but two review attempts -
including the required local Codex review command - remained in the external
review service without a result beyond the documented 10-minute window. The
task-scoped reviewer processes were terminated after that timeout. No review
result is claimed; an independent review remains validation debt.

The validated binary was atomically installed at the shell-resolved unmanaged
path with SHA-256
`e970fbbef0ced0e6076331ac4f0513dac9617cf0dde28e8f671215177c0b7c79`,
matching the build artifact. The installed owner and mode are `root:root` and
`0755`; the prior binary is preserved in the local hash-addressed rollback
directory. An isolated eight-second launch through the exact installed path
selected `legends-2026-09-01`, reported `supported` and `running`, exited with
status zero, emitted no standard output or error, and left no process or
staging file. The already-open companion was not interrupted and still maps
the prior deleted inode, so its visible behavior cannot count for the new
binary until the owner relaunches it.

The broader controlled live gate remains open: character select, a second
zone, changed position and facing, visibly changed vitals, a second equipment
configuration, explicit spawn add/remove/change, zoning, camping, client exit
and reacquisition, DWM placement, performance, game-window invariance, and an
independent review. Those checks remain release-blocking validation debt in
`TASKS.md`; they are not implied by the two bounded in-world snapshots above.

No analysis or application step wrote target memory, injected code, changed
the Wine prefix, synthesized input, weakened host security, or retained a
private client-derived artifact in Git.
