# Legends 2026-08-17 compatibility candidate

Date: 2026-08-18

Profile: `legends-2026-08-17`

SHA-256:
`3451069e63d5118a703a237f121a3ea7c20c973477a69fdd0d66bcdaa7d80b29`

This local checkpoint records privacy-safe evidence for a newly patched
client. It contains no game asset, executable byte, disassembly, string table,
screenshot, runtime name, process address, memory value, or Wine-prefix path.

## Exact identity

| Property | Value |
| --- | --- |
| File size | 15,514,200 bytes |
| Format | PE32+ x86-64 |
| Machine | `0x8664` |
| COFF timestamp | `0x6a837d49` |
| Optional-header magic | `0x20b` |
| Image size | `0x16c2000` |
| Entry RVA | `0x734b00` |
| Section count | 7 |

The exact file digest and mapped PE identity must both match before this
candidate is used. Every older profile remains immutable and selects only its
own digest.

## Static resolver checkpoint

Private exact-client indexing used GNU `objdump` and `strings` from binutils
2.46.1 on x86-64 Fedora with Wine 11.0 Staging. The local-player diagnostic
family resolves repeated null-checked references to image RVA `0x00f08248`.
The zone lookup family masks IDs with `0x7fff`, bounds them to 1 through 1000,
indexes the world table at `0x30`, and validates entry fields at `0x0c` and
`0x10`; its world-data global is image RVA `0x00f07d38`. Character UI access
families resolve the separately null-checked character global at image RVA
`0x00f08390`.

Grouped exact-client accesses retain the player position offsets `0x74`,
`0x78`, and `0x7c` and heading offset `0x94`. The first in-world probe rejected
the carried-forward zone offset `0x5b0`; the world table independently resolved
the active zone and found one matching local-player field at `0x3b8`. The same
probe rejected `0x275` for every spawn level. The direct byte accessor at
`0x20a` matched every bounded live record and tracked a live level transition.
The optional Alternate Advancement cache is deliberately omitted; its prior
authorization applies only to the August 6 profile.

| Profile value | Static resolver | Observation 1 | Observation 2 | Rejection rule | Candidate value |
| --- | --- | --- | --- | --- | --- |
| File and PE identity | PE and COFF headers | Exact file captured | Mapped PE matched twice | Exact digest and all identity fields | Digest and identity above |
| Local player | Player lifecycle diagnostics and repeated null checks | Bounded live read | Bounded live reread | Null outside world or unreadable mapping | `0x00f08248` RVA |
| World data | Masked bounded zone-table lookup | Bounded live read | Bounded live reread | Null, invalid ID, entry mismatch, or invalid name | `0x00f07d38` RVA |
| Player position | Grouped finite float access family | Finite live read | Finite live reread | Non-finite or out of bounds | X `0x74`, Y `0x78`, Z `0x7c` |
| Player heading | Heading arithmetic access family | Bounded live read | Bounded live reread | Non-finite or outside `[0, 512)` | `0x94` |
| Player zone ID | Unique world-table and local-record match | Exact live match | Exact live reread | Zero, masked mismatch, or over 1000 | `0x3b8` |
| Zone entry | Bounded table and entry access family | ID and name matched | ID and name reread | Null, ID mismatch, invalid or unterminated name | Table `0x30`, ID `0x0c`, name `0x10` |
| Spawn links | Reciprocal list validation | Complete collection | Complete reread | Broken reciprocal link, cycle, or over 2048 | Next `0x08`, previous `0x10` |
| Spawn identity | Bounded name, type, and ID fields | All records valid | All records valid | Invalid string/type, zero or duplicate ID | Name `0x0b8`, type `0x139`, ID `0x178` |
| Player identity | Exact local-player anchor name | Bounded live read | Log selection passed | Empty, malformed, or anchor lost | Spawn name at `0x0b8` |
| Spawn level | Direct byte accessor and live transition | All records valid | Transition tracked | Zero, implausible, or visible-level mismatch | `0x20a` |
| Spawn position | Player coordinate access family | All records finite | All records finite | Non-finite or out of bounds | X `0x74`, Y `0x78`, Z `0x7c` |
| Character owner | Character UI access and null checks | Root readable | Later chain failed | Unreadable mapping or lifecycle mismatch | Disabled |
| Character identity | Bounded local-player name candidate | Pending | Pending | Empty, malformed, or changed during staging | `0x0b8`, 64 bytes |
| Current HP and MP | Character-stat lookup access families | Pending | Pending | Missing, negative, torn, or out of range | `0x2770`, `0x2764` |
| Maximum HP and MP | Local-player vital-cache candidates | Pending | Pending | Missing, nonpositive, torn, or below current | HP `0x2b0`, MP `0x678` |
| Equipment | Profile-list, container, slot, and name access families | Pending | Pending | Invalid pointer, count, slot, string, or staged reread | Existing bounded layout; item name `0x60` |

No application or analysis step wrote target memory, injected code, changed
the Wine prefix, weakened host security, synthesized input, or retained a
runtime name, address, memory value, or proprietary derived artifact in Git.

## Local build and installation checkpoint

The fresh warnings-as-errors build and repository gate passed Markdown lint,
Ruff, Python compilation, and all 8 Python tests. All 19 CTest cases passed,
including the X11 and performance suites. The exact fingerprint check and
staged install inventory passed.

Isolated companion runs selected the exact mapped PE identity at character
select and in world. The updated build and installed-command smoke both
published `supported`, `running`, `in_world`, and `map state=loaded` without
stale snapshots or stderr. The installed visual smoke showed the bounded
player identity, player marker, and active local combat-log encounter while
keeping HP and MP unavailable. The validated build was atomically installed
with a hash-addressed rollback of the prior binary. Build and installed
SHA-256 both matched
`702aaf67559afa67f7aa6a3d10ba7dc5d24e3deca0abaac4ab72be64139a82f6`;
owner and mode are `root:root` and `0755`.

The pre-existing companion process still retained the prior deleted inode and
was not disrupted. It must be closed and relaunched before its visible behavior
can count as evidence for this candidate.

## Pending validation

No `eqgame.exe` process was active during the static checkpoint. A later live
session proved the bounded player, zone, and spawn path after rejecting the two
stale carried-forward fields. The Character path remains disabled because its
later pointer chain left readable process mappings. Before this candidate can
be merged or released, it still requires:

- two consecutive complete bounded Character snapshots after separately
  deriving its current layout;
- two controlled visible observations for every displayed field and a second
  zone and equipment configuration;
- character select, entering world, zoning, camping, game exit, and process
  reacquisition;
- DWM placement, map selection, game-window invariance, and live performance;
  and
- complete package and privacy audits with all temporary derived files removed.

On 2026-08-18, the owner explicitly authorized merging all completed work and
starting the compatibility-automation phase. This accepts the unavailable
Character capability and incomplete manual lifecycle checks as merge-time
residual risk; it does not count them as passed or authorize a release.
