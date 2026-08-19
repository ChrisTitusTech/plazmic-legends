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
families resolve the separately null-checked character-owner global at image
RVA `0x00f08398`. The adjacent `0x00f08390` global belongs to a different UI
lifecycle path and was rejected when its descriptor chain left readable
mappings.

Grouped exact-client accesses retain the player position offsets `0x74`,
`0x78`, and `0x7c` and heading offset `0x94`. The first in-world probe rejected
the carried-forward zone offset `0x5b0`; the world table independently resolved
the active zone and found one matching local-player field at `0x3b8`. A later
visible consider-level check rejected `0x20a`: it held percentage-like values
in the 70s rather than levels. Independent update and getter paths establish
the byte at `0x391`; two complete bounded collections produced plausible
levels and the local-player value matched the visible level 42.

The exact `LABELTYPE_EXPPCT` display path reads the character owner, calculates
the current player experience position, scales and clamps it to 0-100, and
caches the resulting float at image RVA `0x00fa6688`. Two consecutive bounded
read-only observations were finite, stable, and in range. The private value is
not retained. This field is authorized independently of the separately
approved Alternate Advancement fields by
`docs/research/phase9-xp-memory-checkpoint.md`.

The separately authorized `LABELTYPE_AAEXPPCT` and `LABELTYPE_AA_PTS` paths
write current AA progress and banked points to adjacent RVAs `0x00fa668c` and
`0x00fa6690`. Two separate bounded read-only probes each produced stable,
in-range values. The shared progression-cache base is RVA `0x00fa6440`, with
profile-local offsets `0x24c` and `0x250`. Private values are not retained.

The corrected character root completed the existing bounded type-descriptor,
stats-record, profile-list, and equipment-container paths. Current HP and MP
remain signed 64-bit `0x2770` and signed 32-bit `0x2764` fields in the resolved
stats owner, with the signed health adjustment at character-zone `0x28`.
Exact-client update paths separately establish the signed 64-bit local-player
maximum-HP cache at `0x310` and maximum-MP cache at `0x238`. Visible gauges
matched at two different current HP and MP observations, including a maximum-
HP change. The 23 bounded equipment slots retained their container layout; the
terminated printable item-name pointer moved to `0x98`.

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
| Spawn level | Byte update and getter paths | All records valid; player 42 | Second complete collection; visible player 42 | Zero, implausible, or visible-level mismatch | `0x391` |
| Spawn position | Player coordinate access family | All records finite | All records finite | Non-finite or out of bounds | X `0x74`, Y `0x78`, Z `0x7c` |
| Character owner | Character UI access and null checks | Complete chain | Complete reread | Unreadable mapping or lifecycle mismatch | `0x00f08398` RVA |
| Character identity | Bounded local-player anchor | Valid | Stable reread | Empty, malformed, or changed during staging | `0x0b8`, 64 bytes |
| Current HP and MP | Character-stat lookup and update paths | Visible gauges matched | Different live values matched | Missing, negative, torn, or out of range | HP `0x2770` plus `0x28`; MP `0x2764` |
| Maximum HP and MP | Local-player vital update and gauge paths | Visible maxima matched | HP maximum changed and reread matched | Missing, nonpositive, torn, or below current | HP `0x310`, MP `0x238` |
| Current XP | `LABELTYPE_EXPPCT` calculation and UI-cache write | Finite bounded read | Stable bounded reread | Missing, non-finite, outside 0-100, or torn | `0x00fa6688` RVA |
| Current AA | AA percent and point label-cache writes | Finite and bounded | Stable bounded reread | Missing, non-finite, outside bounds, or torn | Base `0x00fa6440`; percent `0x24c`, points `0x250` |
| Equipment | Profile-list, container, slot, and name access families | 23 bounded slots valid | Complete reread valid | Invalid pointer, count, slot, string, or staged reread | Existing bounded layout; item name `0x98` |

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

That installed checkpoint has since been superseded by the maintenance build
described below.

## Level and Character maintenance correction

On 2026-08-18, the visible installed companion disproved the candidate mob
level and showed Character vitals as unavailable. The exact live client was
still mapped from the profile digest above, and the running companion exactly
matched its then-installed binary, excluding a stale-process explanation.

Read-only static analysis and bounded `process_vm_readv` checks established the
corrected fields recorded above. Two consecutive complete Character snapshots
passed with one bounded stats record, one bounded profile-list record, all 23
equipment slots, and 22 occupied slots. A visible game capture independently
matched the current and maximum HP and MP values while those values changed;
the capture remained private and was deleted after comparison.

The fresh warnings-as-errors build, repository gate, 13 Python tests, all 20
CTest cases, exact-client fingerprint, package metadata, and staged install
inventory passed. The validated binary was atomically installed with the prior
binary retained under its SHA-256, then relaunched through the installed path.
The build, installed pathname, and running executable all matched SHA-256
`e3756f23813a12570b718b36ebcd73658e556221490b037336c86ab09b4fd8c4`;
the installed file remained `root:root` and mode `0755`. The installed window
rendered changing HP and MP gauges, a live map, the local player, and corrected
bounded spawn levels. Its privacy-safe log selected the exact supported
profile and reached `in_world` with the map loaded and no read failure.

## Current-XP maintenance correction

On 2026-08-18, the installed summary exposed that accumulated retained log XP
was mislabeled as the player's current XP and could exceed 100 percent. The
separate checkpoint in `docs/research/phase9-xp-memory-checkpoint.md` approved
the exact memory-backed field above. The Activity summary now uses that
transient character snapshot for current XP and labels the combat-log-derived
gain rate separately.

The warnings-as-errors repository gate, 13 Python tests, all 20 CTest cases,
exact fingerprint, package metadata, staged install inventory, and the review
loop passed. The binary was atomically installed with the prior
hash-addressed rollback and relaunched. Build, installed, and running SHA-256
all match
`da476d1938a3479dd303ace7061e44609117a5d01233419dd3d243691bdb86d6`.
The visible installed window showed bounded current XP below 100 percent and
did not render the retained 133 percent gain total as current XP. The
privacy-safe log selected the exact profile and reached `in_world` with the map
loaded.

## AA and summary maintenance correction

On 2026-08-18, the installed summary still showed current AA progress as
unavailable. The owner authorized the exact August 17 progression fields
recorded above and requested removal of the duplicated Latest activity pane.
The implementation preserves memory-over-log precedence for current AA while
retaining log events for rate and history, migrates saved four-column widths,
and leaves recent activity in the Activity dock.

The warnings-as-errors repository gate, 13 Python tests, all 20 CTest cases,
exact fingerprint, package metadata, staged install inventory, and the review
loop passed. The binary was atomically installed with the prior
hash-addressed rollback and relaunched. Build, installed, and running SHA-256
all match
`0116c456e8e724df00c50b76f3217e01c0dbdec48b43666c1948a0624957cf74`.
The visible installed window rendered bounded AA progress and memory-backed
banked points instead of unavailable, with only DPS, XP, and AA panes in the
Details summary. The private screenshot was deleted. The privacy-safe log
selected the exact profile and reached `in_world` with the map loaded.

## Pending validation

The current session proved the bounded player, zone, spawn, Character, vital,
and equipment paths after rejecting the stale fields. Before this candidate
can be released, it still requires:

- a second controlled equipment configuration and a second visibly confirmed
  NPC level;
- character select, entering world, zoning, camping, game exit, and process
  reacquisition;
- DWM placement, map selection, game-window invariance, and live performance;
  and
- the remaining controlled privacy review. All temporary derived screenshots
  and staged-install files from this correction were removed.

On 2026-08-18, the owner explicitly authorized merging all completed work and
starting the compatibility-automation phase. This accepts the incomplete
manual lifecycle checks as merge-time residual risk; it does not count them as
passed or authorize a release.
