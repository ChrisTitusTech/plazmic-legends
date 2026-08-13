# Phase 9 AA memory checkpoint

## Decision

The owner approved verifying Phase 9 AA tracking with the existing bounded,
read-only process-memory boundary used by the character snapshot. This
checkpoint adds current AA progress and banked points only for the exact
`legends-2026-08-06` profile. It does not authorize writes, input synthesis,
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

## Native contract

- The three new symbols live only in the immutable August 6 profile. Earlier
  profiles leave the optional fields unsupported rather than borrowing these
  offsets.
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
multi-point log gains, and UI unavailable states. The exact installed build
must then be compared with the in-game AA progress and banked-point displays
in at least two controlled observations before this semantic gate is closed.

Rollback clears the three August 6 profile symbols and removes the two
optional snapshot fields. Existing log-derived history remains readable and
no migration or retained-data deletion is required.
