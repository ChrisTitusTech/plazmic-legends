# Phase 4 completion report

Date: 2026-07-30

Branch: `phase4/spawn-vertical-slice`

## Result

Phase 4 provides the validated spawn vertical slice. The independent Qt
companion reads one exact-profile, hard-bounded spawn collection and publishes
immutable stable IDs, approved types, bounded names, levels, finite positions,
and locally derived distances. A sortable/filterable table, map markers, and
details dock consume those values and share selection by stable ID.

The adapter remains same-user, external, and read-only. No process address
enters the model or UI. No gameplay write, synthesized input, game-target
selection, injection, Wine override, command, alert, runtime-name log, or
Daybreak asset was added.

## Resolver and reader

Profile `legends-2026-07-29` uses the already validated local-player global as
the spawn-list root. Local disassembly and repeated exact-profile live reads
independently established forward and reverse links, common record identity,
stable ID, approved type, bounded name, position, and level fields. The
detailed provenance, offsets, invariants, and privacy boundary are recorded in
`docs/research/phase4-spawn-checkpoint.md`.

The profile permits at most 2,048 entries and 63 display-name characters plus
the required terminator. The reader stages links and records, rejects cycles,
bad reverse links, changed traversal, excessive counts, unreadable ranges,
duplicate or zero IDs, malformed names, unapproved types, zero levels, and
non-finite or out-of-range coordinates. It repeats the traversal and rechecks
the player and zone before publishing one complete snapshot. Any failure
publishes no partial or previous collection as current.

## Presentation and lifecycle

The spawn table supports case-insensitive name filtering, approved-type
filtering, numeric sorting, and persisted filter, sort, and column settings.
The map draws markers after its unchanged geometry cache. Selecting a visible
table row highlights the same map marker and details; selecting a filtered map
marker reveals and selects its row. Runtime names are HTML escaped in details
and are never written by the reader or diagnostics.

Stable row sequences publish changed values without resetting the table.
Unchanged snapshots avoid table and map reconstruction. Add, remove, stale,
empty, and unavailable snapshots update all presentation surfaces together.
Zoning, camping, character select, read failure, profile change, and process
exit invalidate spawn rows, markers, details, player state, and prior map
geometry through the shared lifecycle boundary.

## Automated validation

The complete local gate passed:

```text
cmake --preset dev
cmake --build --preset dev
cmake --build --preset check
ctest --preset dev --output-on-failure

100% tests passed, 0 tests failed out of 14
```

Coverage includes the complete Phase 3 gate plus:

- successful bounded player/spawn publication;
- invalid profiles, unreadable links, cycles, inconsistent reverse links, and
  excessive collection counts;
- duplicate IDs, malformed names, unapproved types, invalid levels, and
  non-finite positions;
- immutable add, remove, and changed-value publication;
- table filtering, numeric sorting, and stable-ID selection;
- table-to-map and map-to-table selection, including filtered rows;
- escaped runtime names, empty/unavailable/stale states, and selection
  removal;
- settings persistence and detached-dock shutdown;
- lifecycle invalidation and second-zone publication; and
- 2,048-entry reader, publication, interaction, event-loop, and marker-render
  performance.

The owner requested fast local review for this project, so hosted CodeRabbit
review was skipped. Warnings-as-errors compilation, focused tests, the full
gate, final diff inspection, and `git diff --check` passed.

## Performance

The deterministic Phase 4 fixture uses 2,048 synthetic spawns and contains no
Daybreak or player data. One final run on the reference host measured:

| Operation | Observed cost |
| --- | ---: |
| Five maximum-size bounded reader passes | 53.74 ms |
| 100 changed-value model publications | 140.87 ms |
| Filtering, sorting, and selection | 2.69 ms |
| 20 marker renders over 10,000 cached map records | 46.45 ms |
| One-millisecond event-loop heartbeats during publication | 103 |

The full Phase 3 performance fixture also remained within budget: 20,000 map
records parsed in 20.45 ms, the bounded combined reader averaged 21.50 us for
its small fixture, initial map rendering took 24.98 ms, and player-driven UI
updates averaged 4.18 ms.

## Package, profile, and privacy validation

The exact installed client still matched the supported SHA-256, x86-64 PE32+
identity, timestamp, and image size before the final live run. A staged install
contained only:

```text
usr/bin/plazmic-legends
usr/share/applications/plazmic-legends.desktop
```

`file` identified the product as a native x86-64 Linux PIE. `ldd` reported no
missing library, Wine library, or PE DLL dependency.

The final source, fixture, diff, untracked-file, package, and generated-artifact
audit found no captured character name, spawn name, runtime address, game
asset, account data, Wine-prefix content, memory dump, or temporary research
probe. Synthetic names are visibly labeled as synthetic. Profile-local RVAs
and record offsets remain centralized in the exact compatibility profile.

## Live validation

Two privacy-safe live reads published complete collections of 120 and 117
entries, demonstrating bounded acquisition and an observed collection change
without recording names. The live companion was rebuilt and relaunched on the
reference Fedora X11, DWM, and Wine session with the approved X11 class on
human tag 5.

After being asked to compare two visible targets and exercise selection and
the full lifecycle sequence, the owner reported that the result worked
perfectly and specifically confirmed that spawns were displayed and
selectable. This accepts the displayed name, type, level, position/map marker,
distance, shared-selection, zoning, camping, character-select, exit, and
reacquisition gate. The game showed no reported frame-pacing regression or
behavior change.

The manual result was reported as one aggregate acceptance rather than a
per-step transcript. No screenshot or runtime name was retained because the
privacy gate deliberately excludes those artifacts.

## Phase boundary

Phase 4 is complete. Stop here for owner approval before Phase 5 hardening or
release-readiness work.
