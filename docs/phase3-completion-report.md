# Phase 3 completion report

Date: 2026-07-30

Branch: `phase3/lifecycle-completion`

## Result

Phase 3 provides a bounded local-map and player vertical slice. The
independent Qt companion loads installed base and numbered map layers, renders
validated player position and heading, supports player-relative height
filtering and player-follow, and fails closed across client lifecycle changes.

The UI consumes immutable values only. Process addresses remain inside the
exact-profile game reader. No spawn traversal, gameplay write, synthesized
input, injection, Wine override, game-installation edit, or packaged map was
added.

## Lifecycle implementation

`PlayerLifecycle` converts every read result into an explicit value state:

- client not running;
- not in world;
- zoning;
- stale snapshot rejected;
- unavailable; or
- in world.

Every unsuccessful or inconsistent read publishes an empty player value and
requests map invalidation. The handled zone is cleared so a later valid
snapshot must reload its map. Missing and malformed maps clear prior geometry
without invalidating an otherwise valid player, and their diagnostic persists
instead of being overwritten by the next player refresh.

The worker confirms the zone after loading a new map. A zone change during
that load publishes `Zoning` and discards both the obsolete map and snapshot.
Client exit clears the cached process. A later exact-profile process is
fingerprinted and has its live PE identity revalidated before reads resume.

Default fit prefers base-layer line geometry rather than numbered-layer
legends or off-map labels. Maps without base lines fall back to all line
geometry, then to label positions.

## Automated validation

The complete local gate passed:

```text
cmake --preset dev
cmake --build --preset dev
cmake --build --preset check
ctest --preset dev

100% tests passed, 0 tests failed out of 13
```

Coverage includes the complete Phase 2 gate plus:

- bounded map parser records, layers, path containment, and typed failures;
- map transforms, base-geometry fit, all-line and label-only fallbacks, and
  off-map legends;
- player-map coordinate and heading conversion;
- player-relative height filtering and persisted player-follow;
- exact-profile synthetic zone and player reads;
- two-zone publication and map replacement;
- character-select, zoning, stale, read-failure, and process-exit
  invalidation;
- missing and malformed map presentation;
- map, reader, snapshot, and UI performance; and
- the existing X11, settings, theme, lifecycle, and PE inspection tests.

The owner requested fast local review for this project, so no hosted
CodeRabbit review was used. Warnings-as-errors compilation, focused tests,
the full gate, final diff inspection, and `git diff --check` passed.

## Performance

The deterministic performance test used a synthetic 20,000-record map and no
Daybreak content. One run on the reference host measured:

| Operation | Observed cost |
| --- | ---: |
| Parse and load 20,000 map records | 20.35 ms |
| One bounded game-state read | 14.33 us |
| One lifecycle snapshot publication | 0.51 us |
| Initial 20,000-record map render | 19.54 ms |
| Player-driven UI update average | 4.29 ms |

The reader measurement uses the real bounded read path against controlled
self-process memory. The UI measurement includes player-Z cache invalidation,
making it representative of the more expensive player updates.

## Package and profile validation

The exact installed client still matched profile `legends-2026-07-29`:

```text
SHA-256: 97ee793d491930ac97f91e5e26fac16d84d17ff24afcd24d5390d256e7045661
PE format: PE32+
Machine: x86-64
Image size: 0x16c1000
```

A staged install contained only:

```text
usr/bin/plazmic-legends
usr/share/applications/plazmic-legends.desktop
```

`file` identified the product as a native x86-64 Linux PIE. `ldd` reported no
missing library, Wine library, or PE DLL dependency.

## Live validation

The reference Fedora X11, DWM, and Wine session passed these observations:

- the supported live PE identity was accepted before profile addresses were
  used;
- installed base and numbered layers loaded without entering the repository;
- player position, heading, and map orientation matched controlled visible
  ground truth at multiple positions;
- a normal transition to a second zone replaced the map and player snapshot;
- geometry-only fit removed the excessive zoom caused by off-map legends;
- camping to character select removed the map and marker while both the game
  and companion remained healthy;
- normal game exit invalidated the player and left the companion running;
- relaunch produced a different Wine process, which the existing companion
  reacquired and revalidated without restart;
- an empty validation map root showed persistent `Map missing` status without
  stale geometry;
- a synthetic malformed validation map failed closed without changing the
  installation; and
- the owner confirmed the zoning, camping, exit, and relaunch behavior.

After relaunch, the game window still reported desktop `0`, fullscreen state,
geometry `2560x1440+0+1080`, no opacity property, and a normal managed
`InputOutput` window. Plazmic remained on desktop `4` (human tag 5), exposed
the approved X11 class, and requested no always-on-top state.

Temporary screenshots and synthetic validation maps were created under
`/tmp`, removed after validation, and never became repository artifacts.

## Phase boundary

Phase 3 is complete. The companion contains no spawn collection root,
traversal, table population, marker publication, or selection path. Stop here
for owner approval before Phase 4 spawn research.
