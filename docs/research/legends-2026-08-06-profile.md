# Legends 2026-08-06 compatibility candidate

Date: 2026-08-08

Profile: `legends-2026-08-06`

SHA-256:
`bf34438c6460acde463692fa09ea28f0d12a204e3445a9da356645fc0d475561`

This local checkpoint records privacy-safe evidence for a newly patched
client. It contains no game asset, executable byte, disassembly, string table,
screenshot, runtime name, process address, memory value, or Wine-prefix path.

## Exact identity

| Property | Value |
| --- | --- |
| File size | 15,501,912 bytes |
| Format | PE32+ x86-64 |
| Machine | `0x8664` |
| COFF timestamp | `0x6a74f50a` |
| Optional-header magic | `0x20b` |
| Image size | `0x16c0000` |
| Entry RVA | `0x731ea0` |
| Section count | 7 |

The file digest and mapped PE identity must both match before the candidate
profile is used. The July 29 and August 4 profiles remain immutable and select
only their own digests.

## Candidate resolver checkpoint

Exact-client static indexing found new player, world, and character global
families at image RVAs `0x00f05ff8`, `0x00f05fe8`, and `0x00f06140`. The exact
binary also contains the expected typed access families for the bounded
player, zone, spawn, vital, and equipment record fields. Prior profile values
were comparison hypotheses only.

A private, read-only candidate probe used the production same-user process
discovery, mapped-PE identity check, readable-mapping checks, bounded
`process_vm_readv` calls, pointer arithmetic, reciprocal spawn traversal,
field validation, and staged snapshot revalidation. Two consecutive refreshes
published complete player, spawn, and character snapshots. Each refresh
contained a bounded 221-entry spawn collection and all 23 equipment slots;
the aggregate type and occupancy counts were stable, and current and maximum
health plus current and maximum mana passed their range checks. The maximum-HP
cache and UI gauge validation are detailed in
`docs/research/max-vitals-2026-08-08.md`.

A same-day open-application check rejected the initially carried maximum-mana
offset `0x2e8`: it produced a stable but semantically impossible gauge value.
That displacement belonged to a stack/string-capacity path, not a validated
local-player vital. A second bounded candidate at `0x2d8` was also rejected by
the open UI because it represented a different resource. A third candidate at
`0x290` matched only while mana was full; a depleted-mana sample proved that it
tracks current MP. After a level-up changed the visible game maximum, a bounded
search found the new value only at local-player `0x678`. Repeated reads while
current mana changed confirmed that `0x290` followed current MP while the
signed 64-bit value at `0x678` remained the independent maximum and never fell
below current. The private captures and raw values were deleted and were not
retained in the repository.

A subsequent open-application con-color check rejected the initially carried
spawn level offset `0x64c`. It published a percentage byte as level 100 for an
ordinary NPC whose visible game target panel reported level 33, and it also
published the local player as level 100 instead of the visible level 29. A
bounded comparison across the 238-entry reciprocal spawn list found the exact
byte at `0x275`: it matched both visible levels and produced a plausible 2-49
range across every record. The active profile now uses `0x275` and reads only
the smallest required `0x276`-byte record span. The older digest-selected
profiles remain unchanged.

No application or probe wrote target memory, injected code, changed the Wine
prefix, weakened host security, synthesized input, or printed a runtime name,
address, memory value, or executable path.

## Pending validation

This checkpoint proves that the local development build can read the active
exact client through the existing fail-closed boundary. It does not complete
the compatibility-profile gate. Before merge or release, the following remain
required:

- finish the semantic static data-flow audit for every carried record field;
- run the complete fresh configure, repository check, CTest, fingerprint,
  package, and privacy gates;
- compare two visibly controlled positions, facings, zones, spawn fields,
  vital values, and equipment configurations;
- exercise character select, entering world, zoning, camping, game exit, and
  process reacquisition; and
- verify DWM placement, game-window invariance, and live performance.

On 2026-08-08, the owner explicitly authorized merging this candidate with the
unfinished checks retained as residual risk. Until those checks pass, it must
not be released.
