# Plazmic Legends tasks

This ledger contains only unfinished work and unresolved validation debt.
Completed implementation and validation are recorded in Git history, release
tags, the completion reports under `docs/`, and the milestone summary in
`ROADMAP.md`.

Task status changes only after the acceptance criteria and required validation
pass. An owner-approved merge exception does not turn a skipped check into a
pass and does not authorize a release.

## Current baseline

- Phases 0 through 7 are complete on `main`; the current release is `v0.2.0`.
- Phase 8 combat history and overview functionality is implemented, but its
  remaining closeout evidence is tracked below.
- Phase 9 progression, loot, inventory, XP, and AA functionality is
  implemented. Catalog-backed class, proc, and upgrade insights and the final
  closeout gate remain open.
- Phase 10 has shipped its first narrow slice: default-on Mote loot audio and
  yellow Mote rows with an independent immediate opt-out. The wider timer,
  notification, rule, and audio-pack scope remains open.
- Network access, uploads, sharing, and update checks remain disabled by
  default.

## Active work

### M-12: Compatibility verification automation

- [ ] Cover every supported data consumer with synthetic success, malformed,
  inconsistent, lifecycle, and unsupported-client tests.
- [ ] Invoke owner-only candidate preparation from the installed application
  when an executable identity is unknown, exposing only privacy-safe progress
  categories.
- [ ] Reproduce the approved static local-player, world, zone, spawn, and
  optional Character resolver searches with deterministic evidence checks.
- [ ] Add bounded two-observation live verification and privacy-safe category
  reporting for every candidate capability.
- [ ] Require explicit owner approval before promoting a fully passing
  candidate to a new immutable compiled profile; then rebuild, install with
  rollback, and verify the installed and running hashes.

Acceptance criteria: `AC-27` passes; unknown or incomplete clients remain
unsupported; automation never guesses or edits an active profile; and private
candidate artifacts remain local, owner-only, bounded, and removable.

Rollback: remove the automation while preserving every compiled immutable
profile and the manual `docs/profile-refresh.md` workflow.

### Phase 8 closeout: Combat analytics and overview

- [ ] Reconcile and record the remaining focused, full, privacy, performance,
  installed, review, CI, and review-thread evidence for the implementation
  already on `main`.
- [ ] Confirm character switch-away, switch-back, restart restoration,
  truncation, rotation, and corruption behavior against `AC-20`.

Rollback: remove the bounded history store and overview views while retaining
the Phase 6 local parser.

### Phase 9 closeout: Progression, loot, inventory, and class activity

- [ ] Add class-combination, proc, and upgrade insights only after validated
  catalogs, provenance, and evidence/confidence rules are approved.
- [ ] Complete schema, lifecycle, migration, privacy, installed, review, CI,
  and review-thread gates under `AC-21`.

Approved inputs remain limited to bounded active-log events, immutable text
equipment, the optional exact-profile AA and current-XP snapshots documented
in `docs/research/phase9-aa-memory-checkpoint.md` and
`docs/research/phase9-xp-memory-checkpoint.md`, and an explicitly selected
local inventory-output file. Any new input requires a separate checkpoint.

Rollback: disable or remove the activity store and views without changing the
raw inputs or weakening existing exact-profile behavior.

### Phase 10 remainder: Buffs, timers, alerts, and audio

- [ ] Add bounded buff and respawn timers, local alert rules, visible
  notifications, and optional rate-limited sound or voice packs under `AC-22`.
- [ ] Limit inputs to bounded local log and activity events, user-configured
  timers, and validated imported duration or audio packs; keep observed and
  reference durations visibly distinct.
- [ ] Complete fake-sink, lifecycle, accessibility, performance, privacy,
  installed, review, CI, and review-thread gates.

The accepted Mote slice contract remains in
`docs/research/phase10-mote-audio-checkpoint.md`. It does not authorize new
process-memory fields, network inputs, or bundled audio packs.

Rollback: disable dispatch and remove new alert or timer views while
preserving user data for migration.

### Phase 11: Map workflows

- [ ] Add POI search, label decluttering, floor slicing, pinned zones, typed
  `/loc`, and local annotations under `AC-23`.
- [ ] Complete parser, renderer, persistence, accessibility, performance,
  installed, review, CI, and review-thread gates.

Rollback: remove the new controls and annotation storage while retaining the
existing map parser and renderer.

### Phase 12: Knowledge packs, raid history, and planners

- [ ] Add a validated user-imported or license-compatible pack format and
  item, quest, recipe, Plane of Sky, Exaltation, and raid-history views under
  `AC-24`; classify raid kills only against a validated roster from that pack.
- [ ] Complete provenance, schema, integrity, migration, installed, review,
  CI, and review-thread gates.

Rollback: remove the importer and views while leaving user-owned packs
untouched.

### Phase 13: Detached and overlay presentation

- [ ] Add user-enabled damage, healing, progression, buff, and timer windows
  with lock, topmost, click-through, scope, and focus-neutral behavior under
  `AC-25`.
- [ ] Complete Xvfb and real-DWM focus, input, stacking, accessibility,
  performance, game-invariance, review, CI, and review-thread gates.

Rollback: disable overlay roles and restore docked views without losing
settings.

### Phase 14: Profiles, sharing, and optional services

- [ ] Add per-character switching, previewed additive alert import that never
  replaces existing rules, and atomic settings import and export.
- [ ] Add only concretely specified, separately consented services whose
  endpoint, payload, retention, deletion, offline, and threat-model gates pass
  under `AC-26`.
- [ ] Keep network access, uploads, sharing, and update checks disabled by
  default; enable each independently only after its approved contract and
  consent gate passes.
- [ ] Complete schema, migration, corruption, security, privacy,
  network-failure, installed, review, CI, and review-thread gates.

Rollback: disable and remove a service client without affecting the offline
product or local data.

## Residual validation debt

These checks were explicitly retained when their implementation was merged.
They are release blocks where noted by the governing checkpoint or profile
workflow.

- [ ] Phase 6 merged-head live gate: repeat exact-client HP, mana, equipment,
  encounter lifecycle, DWM placement, and game-invariance validation from
  `docs/research/phase6-character-combat-checkpoint.md`.
- [ ] The 2026-08-04 profile: complete controlled observations for position,
  facing, zones, spawns, vitals, and equipment plus lifecycle, DWM placement,
  and game-invariance checks.
- [ ] The 2026-08-06 profile: complete the semantic resolver audit, synthetic
  gate, controlled field observations, lifecycle, DWM placement,
  game-invariance, performance, package, and privacy checks.
- [ ] The historical 2026-08-03 profile: complete its second-zone and
  lifecycle checks while keeping its unavailable Character capability
  fail-closed.
- [ ] The 2026-08-17 profile: complete two controlled observations for every
  displayed field, two zones, lifecycle, DWM placement, game invariance,
  performance, package, and privacy checks.
- [ ] The 2026-08-25 profile: complete the controlled character-select,
  in-world, two-zone, changed-position and facing, visible field, zoning,
  camping, exit, reacquisition, DWM placement, performance, and
  game-invariance gate.
- [ ] The 2026-09-01 profile: complete the controlled character-select,
  two-zone, changed-position and facing, visibly changed vital, equipment,
  spawn add/remove/change, zoning, camping, exit, reacquisition, DWM
  placement, performance, game-invariance, and independent-review gates. Two
  consecutive bounded in-world snapshots and the automated repository gate
  are recorded in `docs/research/legends-2026-09-01-profile.md`.
- [ ] Inventory export: successfully import a private exported file through EQ
  Legends Tools and verify the documented fields that Plazmic Legends does not
  read or invent.
- [ ] Phase 10 exact-head follow-up: reinstall the final lifecycle-hardened
  source head and repeat the installed live Mote observation. The earlier
  installed build remains valid evidence for audible playback and yellow row
  presentation, but it is not the final source hash.

All exact-client work follows `docs/offset-discovery.md` and
`docs/profile-refresh.md`. Private observations, paths, logs, client content,
and candidate artifacts must not enter the repository or packages.

## Completion policy

When an item passes, remove it from this file in the same change that records
its durable evidence in the applicable checkpoint, completion report, release
record, or Git history. Do not grow this file into another completed-work
archive.
