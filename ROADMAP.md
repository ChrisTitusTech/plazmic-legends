# Plazmic Legends roadmap

This roadmap records product sequence, current phase status, exit criteria,
and rollback boundaries. `TASKS.md` is the active-only execution ledger;
completed task checklists belong in Git history and the completion reports
under `docs/`.

## Milestone status

| Phase | Status | Durable evidence |
| --- | --- | --- |
| 0 - Inventory cleanup | Complete | `phase0-import-baseline`, `docs/phase0-cleanup-report.md` |
| 1 - Linux/Wine proof | Complete | `docs/phase1-architecture-proof.md`, `docs/phase1-codex-review.md` |
| 2 - Qt and DWM shell | Complete | `docs/phase2-completion-report.md` |
| 3 - Map and player slice | Complete | `docs/phase3-completion-report.md` |
| 4 - Spawn slice | Complete | `docs/phase4-completion-report.md` |
| 5 - Release readiness | Complete | `docs/phase5-product-boundary.md`, release `v0.1.2` |
| 6 - Character and combat | Complete with residual live debt | release `v0.2.0`, `docs/research/phase6-character-combat-checkpoint.md` |
| 7 - Expansion foundation | Complete | PR #20, `docs/research/everquest-companion-feature-parity.md` |
| 8 - Combat analytics | Implemented; closeout open | PR #21, `AC-20` |
| 9 - Progression and activity | Partially complete | PRs #22 and #24, `docs/research/phase9-activity-checkpoint.md` |
| 10 - Timers and alerts | First slice complete; remainder active | `docs/research/phase10-mote-audio-checkpoint.md` |
| 11 - Map workflows | Planned | `AC-23` |
| 12 - Knowledge and planners | Planned | `AC-24` |
| 13 - Detached and overlay UI | Planned | `AC-25` |
| 14 - Profiles and services | Planned | `AC-26` |

The current public release remains `v0.2.0`. Later work on `main` does not
become a release until its required automated checks, installed, live, privacy,
review, and publication gates pass. Unfinished gates and owner-approved merge
exceptions are listed in `TASKS.md`.

## Sequencing rules

- Capability phases proceed in numeric order unless the roadmap is explicitly
  amended. Compatibility maintenance may proceed independently because an
  unsupported client blocks all live validation.
- Each major capability is independently reviewable and records its source,
  side effects, privacy, license, lifecycle, resource, validation, and rollback
  contract under `AC-19`.
- A successor may be developed while an earlier closeout remains open, but it
  does not inherit permission to merge, release, use a new data source, or
  weaken a fail-closed boundary.
- Passing CI or review is evidence, not authorization to publish or release.
- Unknown and partially validated clients remain unsupported. Compatibility
  profiles are immutable and exact-identity selected.

## Completed foundation

Phases 0 through 7 established the recoverable import baseline, removed the
inherited implementation, proved same-user bounded read-only Wine process
access, selected the independent Qt application, implemented map, player,
spawn, character, and combat slices, shipped Linux packages, and defined the
capability-gated expansion model.

Those phases are intentionally summarized here instead of retaining hundreds
of completed checklist items. Their requirements remain normative in
`SPEC.md`; their decisions and validation remain in the linked completion and
research reports. Phase 6's skipped merged-head live lifecycle check remains
open validation debt and is not rewritten as a pass.

## Maintenance: Compatibility verification automation

Status: Active and allowed to proceed alongside capability work.

### Outcome

Turn the manual exact-client refresh workflow into a bounded, owner-controlled
verification pipeline without guessing offsets or enabling an incomplete
profile.

### Remaining scope

- Complete synthetic coverage for every supported consumer and failure mode.
- Integrate private candidate preparation into the installed application for
  unknown executable identities.
- Reproduce approved static resolvers deterministically and add bounded
  two-observation live verification.
- Require explicit promotion approval, immutable profile creation, rebuild,
  rollback-capable installation, and installed/running hash verification.

### Exit, rollback, and pause conditions

`AC-27`, `docs/offset-discovery.md`, and `docs/profile-refresh.md` must pass.
Remove the automation without changing existing compiled profiles if it cannot
produce deterministic, privacy-safe evidence. Pause on ambiguity, incomplete
capabilities, guessed values, private-data leakage, or any attempt to modify an
active profile silently.

## Phase 8: Combat analytics and overview

Status: Core implementation is on `main`; closeout evidence remains open.

### Outcome

Provide capped encounter and zone history, attack, spell, damage, and healing
drill-down, a timeline, current log-derived encounter context, and overview
cards for current and recent fights.

The phase does not infer a true game target from memory. Adding one requires a
separate exact-profile input checkpoint.

### Exit, rollback, and pause conditions

Histories must satisfy `AC-20`: explicit count, byte, age, persistence,
partition, lifecycle, corruption, and privacy bounds with synthetic and
installed validation. Remove the history store and overview views while
retaining the Phase 6 parser if closeout fails. Pause when a supported log form
cannot be categorized deterministically.

## Phase 9: Progression, loot, inventory, and class activity

Status: Progression, loot, inventory reconciliation, equipment observations,
XP, AA, rates, and celebrations are on `main`. Catalog-backed class, proc, and
upgrade insights and final closeout remain active.

### Outcome

Provide per-character progression and activity history with bounded derived
rates, inventory reconciliation, and evidence-carrying class, proc, and
upgrade observations. Keep inventory in the Activity view and the resizable
DPS, XP, and AA summary above Details.

Approved inputs are bounded active-log lines, immutable text equipment, the
optional exact-profile AA and current-XP snapshots approved by their Phase 9
checkpoints, and an explicitly selected local inventory-output file. No other
memory field, network source, or bundled game database is authorized.

### Exit, rollback, and pause conditions

`AC-21` and all schema, lifecycle, migration, corruption, privacy,
performance, installed, review, and CI gates must pass. Remove the activity
store and views without changing raw inputs if the model cannot remain
bounded and recoverable. Pause rather than infer an ambiguous character, loot,
XP, AA, class, proc, or upgrade event.

## Phase 10: Buffs, timers, alerts, and audio

Status: The default-on Mote loot audio and yellow-row slice is complete. The
broader phase remains active.

### Outcome

Add bounded buff and respawn timers, local text rules, rank-upgrade and custom
kill alerts, visible notifications, optional compatible sound or voice packs,
rate limits, and independent controls.

The implemented first slice matches new bounded loot activity whose item label
contains `Mote` as a complete case-insensitive word. It provides an immediate
independent opt-out, a silent restored-history baseline, stable activity source
identity, lifecycle reset behavior, two-second coalescing, an injected fake
sink, fixed desktop bell playback with Qt fallback, and readable yellow loot
rows. Its exact contract is recorded in
`docs/research/phase10-mote-audio-checkpoint.md`.

Future inputs are limited to bounded local log or activity events,
user-configured timer values, and schema and provenance-validated imported
duration or audio packs. Observed and imported reference durations must remain
visibly distinct. This phase authorizes no new process-memory or network
source.

### Exit, rollback, and pause conditions

`AC-22` and rule, timer, fake-sink, lifecycle, accessibility, performance,
privacy, installed audio, review, and CI gates must pass. Disable dispatch and
remove alert or timer views while preserving migratable user data if rollback
is required. Pause on noisy, unbounded, inaccessible, or privacy-unsafe
matching.

## Phase 11: Map workflows

Status: Planned after Phase 10.

### Outcome

Add point-of-interest search, label decluttering, floor slicing, pinned-zone
viewing, typed `/loc` navigation, and local user annotations without copying
third-party maps or databases.

### Exit, rollback, and pause conditions

`AC-23` and parser, renderer, transform, search, persistence, accessibility,
performance, installed, review, and CI gates must pass. Remove new controls and
annotation storage while retaining the current map renderer. Pause if a
feature requires redistributing unlicensed map content.

## Phase 12: Knowledge packs, raid history, and planners

Status: Planned after Phase 11.

### Outcome

Add a versioned import format and native item, quest, recipe, Plane of Sky,
Exaltation, have/need, and raid-history views. Classify bounded local raid kills
only against a validated roster from an explicitly selected pack.

### Exit, rollback, and pause conditions

`AC-24` and schema, version, size, integrity, path, provenance, migration,
performance, installed, review, and CI gates must pass. Remove the importer and
views while leaving user-owned packs untouched. Pause until a legally
compatible source or user-supplied pack is available.

## Phase 13: Detached and overlay presentation

Status: Planned after Phase 12.

### Outcome

Add user-enabled floating damage, healing, progression, buff, and timer
windows with fight or zone scope, geometry persistence, lock, topmost, and
click-through controls where the Linux window manager supports them.

### Exit, rollback, and pause conditions

`AC-25` and synthetic UI, Xvfb, real-DWM focus, input, stacking, lifecycle,
accessibility, performance, game-invariance, review, and CI gates must pass.
Disable overlay roles and restore docked views without losing settings if
rollback is required. Pause on unexpected game input, focus, opacity,
geometry, or tag changes.

## Phase 14: Profiles, sharing, and optional services

Status: Planned after Phase 13.

### Outcome

Add per-character switching, atomic settings import and export, previewed
additive alert import that never replaces existing rules, and an isolated
service boundary for separately enabled updates, feedback, sharing, or
telemetry only when concrete endpoints and retention policies exist.

### Exit, rollback, and pause conditions

`AC-26` and schema, migration, corruption, network-failure, TLS, consent,
redaction, deletion, privacy, security, installed, review, and CI gates must
pass. Online capabilities remain independently disabled by default, show their
exact endpoint and payload, and provide disable and deletion behavior. Remove
a service client without affecting the offline product. Pause until its
operator, endpoint, retention, deletion, and threat model are documented.
