# Phase 9 AA memory checkpoint

## Decision

The owner approved verifying Phase 9 AA tracking with the existing bounded,
read-only process-memory boundary used by the character snapshot. This
checkpoint originally added current AA progress and banked points for the
exact `legends-2026-08-06` profile. On 2026-08-18, the owner separately
authorized the same two read-only fields for the exact
`legends-2026-08-17` profile. It does not authorize writes, input synthesis,
injection, automation, a network source, or offsets for any other client.

## Exact-client evidence

The approved executable has SHA-256
`bf34438c6460acde463692fa09ea28f0d12a204e3445a9da356645fc0d475561`.
Static inspection shows its progression-cache base at RVA `0x00fa43d0`.
The cache population path writes AA progress as a bounded float at offset
`0x24c` after calling the client's AA-experience routine. The adjacent value
at offset `0x250` is populated by the same integer accessor used to refresh the
Alternate Advancement window's banked-point control; that accessor resolves
the active profile and reads its banked-point field.

The public MacroQuest [`eqlib`](https://github.com/macroquest/eqlib) layout at
commit `3eafc8217e690b3c93201a17ec205410db1cac82` was used only as conceptual
corroboration that the corresponding cache fields are adjacent and ordered as
experience, AA progress, and banked points. Its offsets describe a different
client and were not copied. No upstream implementation, fixture, generated
data, or game asset is retained.

Live read-only inspection on the approved client produced a finite fractional
AA progress value and a bounded banked-point value. The values are private
gameplay state and are not recorded here. A manual comparison with the visible
in-game AA window remains the final semantic gate.

The August 17 executable has SHA-256
`3451069e63d5118a703a237f121a3ea7c20c973477a69fdd0d66bcdaa7d80b29`.
Its exact `LABELTYPE_AAEXPPCT` path calls the AA-experience routine, scales and
clamps the result to 0-100, and writes the float to image RVA `0x00fa668c`.
The exact `LABELTYPE_AA_PTS` path calls the banked-point accessor and writes
the result to adjacent RVA `0x00fa6690`. The shared cache base is therefore
RVA `0x00fa6440`, with the same profile-local offsets `0x24c` and `0x250`.
Two separate bounded probes each produced stable, finite, in-range AA progress
and stable bounded points. The private values are not retained.

## Native contract

- Each approved immutable profile owns its exact progression-cache base and
  field offsets. All other profiles leave the optional fields unsupported
  rather than borrowing them.
- Each field is read and re-read through `ProcessMemoryReader`. AA progress
  must be finite and in the inclusive range 0-100; banked points must be at
  most 10,000,000.
- A missing, invalid, torn, or unsupported optional value is omitted for that
  frame without invalidating the already bounded health, mana, and equipment
  snapshot.
- Current memory-backed values are transient. Activity persistence and export
  retain only the existing bounded log-derived events and ability evidence.
- The Activity dock prefers the current memory-backed banked total over the
  latest retained log total. Exact log AA events remain the source for AA gain
  rate, and numeric multi-point gains contribute their awarded amount.

## Validation and rollback

Synthetic tests cover supported and unsupported profiles, valid and invalid
optional values, zero banked points, memory precedence over stale log totals,
multi-point log gains, and UI unavailable states. The original August 6 field
checkpoint still requires comparison with the in-game AA progress and
banked-point displays in at least two controlled observations before its
separate semantic gate is closed.

For the August 17 extension, the warnings-as-errors repository gate, 13 Python
tests, all 20 CTest cases, exact-client fingerprint, package metadata, staged
install inventory, and the review loop passed. The binary was atomically
installed with a hash-addressed rollback and relaunched. Build, installed, and
running SHA-256 all match
`0116c456e8e724df00c50b76f3217e01c0dbdec48b43666c1948a0624957cf74`.
The visible installed summary rendered bounded AA progress and memory-backed
banked points instead of unavailable. The private values and validation
screenshot were not retained. The summary contained only DPS, XP, and AA
panes, while recent events remained in the Activity dock. The privacy-safe log
selected the exact profile and reached `in_world` with the map loaded.

Rollback clears the three progression symbols only from the affected exact
profile. Removing both profiles' symbols may additionally remove the two
optional snapshot fields. Existing log-derived history remains readable and
no migration or retained-data deletion is required.
