# Phase 4 spawn checkpoint

Captured: 2026-07-30

This checkpoint establishes the exact-profile spawn adapter, its bounded
immutable model, and the synthetic presentation gate. It contains no game
executable, map asset, account data, character name, spawn name, memory dump,
or captured screenshot.

## Scope

- Traverse one exact-profile, read-only spawn collection.
- Publish stable ID, approved type, bounded display name, level, finite
  position, and locally derived two-dimensional distance.
- Render a sortable and filterable table plus map markers from immutable
  values.
- Synchronize table, map, and detail selection by stable ID without changing
  the game target.
- Fail closed on invalid profiles, links, records, or lifecycle state.

Commands, target writes, input synthesis, injection, game-target selection,
alerts, logging, and persistence of runtime names remain outside the product.

## Research provenance

The exact supported executable remains profile `legends-2026-07-29` with
SHA-256
`97ee793d491930ac97f91e5e26fac16d84d17ff24afcd24d5390d256e7045661`.
The resolver was established from local static x86-64 disassembly and repeated
same-user reads of that exact live image.

MacroQuest EQLib revision
`b5598ba63613cb376addb91c3d52c8647189c91c` was consulted only to form
conceptual field-name and type hypotheses. No EQLib source, layout, address,
generated data, or implementation was copied. Every profile value below was
independently confirmed against the Legends executable and live collection.

## Exact-profile collection

The already validated local-player global at image RVA `0x00f07388` provides
a stable anchor inside the in-world spawn list. An initial controlled run
placed that anchor at the root; a later live run disproved that ordering
assumption when another valid record preceded it. The reader now resolves the
root by following bounded, reciprocal previous links from the anchor. The
supported profile uses this minimum set:

| Value | Exact-profile source |
| --- | --- |
| Next link | Record offset `0x08` |
| Previous link | Record offset `0x10` |
| Display name | NUL-terminated ASCII at `0x0b8`, within 64 bytes |
| Approved raw type | Byte at `0x139`: 0 player, 1 NPC, 2 corpse |
| Stable ID | Nonzero unsigned 32-bit value at `0x178` |
| Position | Finite floats at `0x74`, `0x78`, and `0x7c` |
| Level | Nonzero byte at `0x6b4` |
| Bounded record | Bytes `[0, 0x6b5)` |

The resolved root has a null previous link. Each subsequent entry must have the
same nonzero vtable value and point back to the immediately preceding entry.
The local-player anchor must appear in the resolved forward traversal.
Traversal ends only at a null next link. Static inspection independently found
code consuming the bounded name region, stable ID near type and position, and
a level getter reading offset `0x6b4`. Live reads confirmed one common vtable,
consistent forward and reverse links, unique nonzero IDs, terminated names,
approved types, finite positions, and nonzero levels across the observed
collection.

## Bounds and consistency

The profile maximum is 2,048 entries and the display-name maximum is 63
characters plus the required terminator. Two controlled aggregate reads
observed 120 and 117 live entries. The count ceiling is therefore more than
17 times the larger observation while bounding staged record data to about
3.3 MiB. A profile cannot raise the implementation ceiling above 4,096
entries, enlarge a record above 4,096 bytes, or move link fields outside the
24-byte link stage.

The reader:

1. validates the exact profile and same-user process mapping;
2. resolves the root from the local-player anchor with a hard count,
   visited-address set, vtable check, and reciprocal-link check;
3. traverses forward with the same bound and requires the anchor to appear;
4. reads each bounded record into local storage;
5. validates terminated printable names, approved types, unique nonzero IDs,
   nonzero levels, and finite bounded coordinates;
6. derives distance locally from the immutable player snapshot;
7. repeats the complete traversal and requires the same ordered addresses; and
8. rechecks local-player identity and zone before publishing the combined
   player and spawn snapshot.

Every address addition and read is checked through the existing
canonical-address, overflow, readable-mapping, and exact-length process reader.
Failure publishes neither a partial collection nor a prior collection as
current. Process addresses never enter the model or UI.

## Presentation and lifecycle boundary

The table model and map consume the same `SpawnCollectionSnapshot`. The table
supports case-insensitive name filtering, type filtering, numeric sorting, and
stable-ID selection. The map paints markers after its unchanged geometry cache
and performs hit testing against the same stable IDs. Table selection, map
selection, and the details dock therefore agree without interacting with the
game.

Name filter, type filter, sort order, and column widths persist in the existing
per-user UI settings. Runtime names remain memory-only and are not emitted by
diagnostics. Unavailable, stale, not-in-world, zoning, and process-exit results
clear table rows, markers, details, and the prior zone map together.

## Checkpoint validation

Synthetic coverage includes valid publication plus invalid profile link
bounds, unreadable links, inconsistent reverse links, cycles, excessive
counts, duplicate IDs, malformed names, unapproved types, invalid levels, and
non-finite coordinates. UI coverage includes add, remove, changed values,
filtering, sorting, table-to-map and map-to-table selection, selection removal,
empty state, stale state, and preference restoration.

The 2,048-entry performance fixture measured five full reader passes, 100
model publications, filtering/sorting/selection, and 20 marker renders over
10,000 cached map records while a one-millisecond event heartbeat remained
active. Exact measurements are recorded by `phase4_performance_test` on every
gate run.

Privacy-safe live diagnostics produced two complete aggregate snapshots:

```text
world_snapshot=verified spawn_count=120 spawn_players=4 spawn_npcs=116 spawn_corpses=0
world_snapshot=verified spawn_count=117 spawn_players=3 spawn_npcs=114 spawn_corpses=0
```

These reads prove repeatable bounded collection acquisition and observed
add/remove/type-count change without recording runtime names.

## Subsequent Phase 4 completion

The owner accepted the controlled visible-field, shared-selection, lifecycle,
and game-performance result after the current build was relaunched on the
reference session. The complete automated, package, privacy, performance, and
live evidence is recorded in `docs/phase4-completion-report.md`. Phase 4 is
complete and Phase 5 remains behind its explicit approval checkpoint.
