# Phase 1 minimal symbol plan

This document identifies logical data boundaries only. It contains no address,
offset, byte pattern, disassembly result, copied structure, or proprietary
client data.

## Minimum logical inputs

The proposed private-development MVP needs only:

1. a world/session state that distinguishes character select, loading, zoning,
   in-world, and exit;
2. a current-zone record containing a bounded display name;
3. a local-player record containing position and heading; and
4. a bounded spawn collection containing stable ID, type, bounded display name,
   level, and finite position for each validated entry.

Spawn distance is derived locally from the approved player and spawn
positions. The UI needs immutable values only and never receives client
pointers.

Static map geometry does not require a client symbol; it comes from read-only
parsing of the configured installation's zone map files. No chat, inventory,
combat, command, input, or write symbol is required. If an approved input
cannot be isolated from a broader structure, the associated field is removed
from the MVP.

## Required validation strategy

Each logical input requires all of:

- exact executable profile match before process access;
- one profile-local resolver, never a raw address in UI code;
- mapped-module bounds and readable-page validation;
- stable signature plus instruction or data-reference invariants;
- pointer alignment, canonical-address, and maximum-depth checks;
- a hard maximum spawn count and duplicate-ID rejection;
- bounded string length and encoding validation;
- numeric range and finite-value checks;
- two independent controlled ground-truth observations;
- invalidation on zoning, character select, profile change, or read failure;
  and
- synthetic fixtures containing no Daybreak or account data.

A timestamp, nearby string, copied legacy offset, or one successful read is not
sufficient validation. Unknown and partial matches fail closed.

## Current result

No gameplay symbol discovery was performed in Phase 1. The owner explicitly
accepted the EULA conflict for private read-only development. Phase 2 uses
synthetic lifecycle snapshots only. Live zone and player symbol work belongs to
Phase 3; spawn collection work belongs to Phase 4. Each requires the preceding
approval checkpoint and must use the bounded memory reader and validation rules
above without adding writes, injection, automation, or protection bypass.
