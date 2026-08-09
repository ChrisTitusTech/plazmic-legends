# Maximum vitals and consider-color maintenance checkpoint

Date: 2026-08-08

This privacy-safe checkpoint records the maintenance evidence for required
maximum HP and MP gauges and the consider-color regression. It contains no
runtime name, process address, memory value, executable bytes, screenshot, or
private game path.

## Exact active-client evidence

Static analysis of the exact `legends-2026-08-06` client followed the HP,
MAXHP, HP percentage, mana, and maximum-mana presentation families. A bounded
aligned candidate-family check then used the production same-user discovery,
readable-mapping, and `process_vm_readv` boundary. It established a stable
signed 64-bit local-player maximum-health cache at profile offset `0x2b0`.

An open-application follow-up corrected the active August 6 maximum-mana
finding. The initially selected `0x2e8` value was bounded and stable but did not
match the visible game gauge; its apparent static references were stack and
string-capacity displacements rather than local-player field accesses. A
second bounded candidate at `0x2d8` was rejected in the open companion because
it represented a different resource. A third candidate at `0x290` matched the
game only while mana was full; depleted-mana sampling proved it tracks current
MP. A subsequent level-up changed the visible maximum and isolated the new
value at local-player `0x678`. Repeated bounded reads while current MP changed
confirmed that `0x290` followed current and the signed 64-bit `0x678` value
remained the independent maximum and never fell below current. No private value
or capture was retained.

The production reader now requires both values, bounds each to 0 through
100,000,000, requires each current value not to exceed its maximum, and rereads
both maxima with the current-vital owner before publishing an immutable
snapshot. No value is inferred from a percentage and no target memory is
written.

## UI and consider-color behavior

The Character dock formats both gauges as `current / max (percent)`. HP uses a
red percentage-driven chunk and MP uses a blue percentage-driven chunk, so
each fill shrinks as the corresponding resource falls.

A same-day presentation follow-up confirmed that the active character snapshot
contains a positive maximum-mana value and a bounded percentage. The gauges
now drive their fill from the exact current/maximum ratio, round only the
displayed whole percentage, show `0 / 0 (0%)` explicitly, and reserve the font
width needed for the complete text instead of allowing MAXMP or the percentage
to be clipped by a narrow dock.

The existing seven-band consider classifier remains driven only by immutable
local-player and ordinary-NPC levels. The same color is now applied to map
markers, visible NPC labels, and spawn-table text. Named NPCs retain their
distinct presentation and player and Other categories remain unchanged.

## Validation boundary

Synthetic tests cover both maxima, percentage values, red and blue styling,
current-above-maximum rejection, all seven consider boundaries, distinct map
colors, and spawn-table color publication. A fresh warnings-as-errors build,
repository checks, eight Python tests, all 17 CTest cases, exact active-client
fingerprint, bounded live maximum-HP/MP read, and installed X11 launch passed.
The build and atomically replaced `/usr/local/bin/plazmic-legends` hashes
matched, and the prior installed binary remains available as a private
rollback copy.
