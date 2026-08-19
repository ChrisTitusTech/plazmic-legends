# Phase 9 current-XP memory checkpoint

## Decision

On 2026-08-18, the owner explicitly requested that Plazmic Legends read the
player's current XP from memory. This checkpoint adds that single optional
field only for the exact `legends-2026-08-17` profile. It does not authorize
writes, input synthesis, injection, automation, a network source, Alternate
Advancement offsets for this client, or offsets for any other client.

## Exact-client evidence

The approved executable has SHA-256
`3451069e63d5118a703a237f121a3ea7c20c973477a69fdd0d66bcdaa7d80b29`.
Static inspection resolved the `LABELTYPE_EXPPCT` display case to a handler
that loads the exact character-owner global, invokes the client's current-XP
calculation, scales the result to a percentage, clamps it to 100, and writes a
float to image RVA `0x00fa6688`.

Two consecutive bounded read-only observations through the existing
same-user process reader produced the same finite value in the inclusive
range 0-100. The value is private gameplay state and is not recorded here. No
client bytes, disassembly, screenshot, process address, runtime name, or Wine
prefix content is retained.

## Native contract

- The new symbol lives only in the immutable August 17 profile. Every other
  profile leaves current XP unsupported rather than borrowing the offset.
- The float is read and re-read through `ProcessMemoryReader`. It must be
  finite and in the inclusive range 0-100.
- A missing, invalid, torn, or unsupported value is omitted for that frame
  without invalidating bounded identity, vitals, equipment, or activity data.
- Current XP is transient. It is not persisted, exported, logged, included in
  diagnostics, or sent over the network.
- Exact combat-log XP events remain the source for history, gain rate, and
  pace. Their retained sum is XP gained, not the character's current XP
  position, and the UI must never label that sum as current XP.

## Validation and rollback

Synthetic tests cover exact-profile capability selection, invalid bounds,
unsupported profiles, character-snapshot publication, and UI precedence over
a retained XP-gain total greater than 100 percent. The fresh repository and
package gates, two-pass code review, atomic local installation, exact running
hash, privacy-safe log, and visible installed summary passed. The installed UI
showed bounded current XP below 100 percent with log-derived gain rate labeled
separately, and did not show the retained 133 percent gain total as current XP.
The build, installed path, and running executable match SHA-256
`da476d1938a3479dd303ace7061e44609117a5d01233419dd3d243691bdb86d6`.

Rollback clears the August 17 current-XP RVA and removes the optional snapshot
field. Existing activity history remains readable and requires no migration or
retained-data deletion.
