# Plazmic Legends tasks

Task status changes only after acceptance criteria and required validation
pass. Record skipped validation and residual risk instead of marking incomplete
work done.

## Current phase

Phase 0 through Phase 5 are complete and merged into `main`. The current
release is `v0.2.0`, with Fedora RPM and x86-64 AppImage assets. Package
publication remains authorized.

Phase 6 is merged and included in `v0.2.0`. It adds a bounded memory-backed
Character dock and an offline combat-log Parse dock without changing the
read-only, fail-closed, privacy, or package boundary. The owner explicitly
authorized release without repeating the final manual live lifecycle gate on
the merged head; that skipped P6-05 check remains a documented residual risk.
That authorization applies only to the merged `v0.2.0` head and does not
authorize merging or releasing a later compatibility profile.

On 2026-08-08, the owner explicitly overrode the M-06 and M-07 merge blocks
and authorized integrating both compatibility profiles into `main` with the
unfinished manual checks retained as documented residual risk. This override
does not authorize a release; the remaining profile-refresh gates must pass
before publishing another version.

On 2026-08-12, the owner authorized removing permanent product-category
prohibitions, researching useful feature parity with
`jmoyers/everquest-companion`, and delivering each major capability as a
separate pull request. This authorizes implementation and publication for
review. It does not authorize merging or releasing the pull requests.

## Phase 7: Extensible product foundation

### P7-01: Record the parity and provenance boundary

- [x] Inventory the upstream public behavior at an exact revision and license.
- [x] Map existing, planned, adapted, and intentionally omitted behavior
  without copying upstream code, data, assets, fixtures, or generated content.
- Acceptance criteria: the research record identifies the source revision,
  provenance constraints, native replacement strategy, and phase for each
  selected capability.

### P7-02: Replace permanent prohibitions with capability gates

- [x] Reconcile product purpose, principles, architecture, security, privacy,
  integration, overlays, services, extensions, automation, and platform scope.
- [x] Preserve exact-profile failure, protection-bypass, provenance, consent,
  lifecycle, privacy, resource, validation, and rollback boundaries.
- [x] Keep network access, uploads, sharing, and update checks disabled by
  default unless an approved capability explicitly enables them.
- Acceptance criteria: product categories are not permanently forbidden, but
  no feature gains implicit authority beyond its approved capability contract.

### P7-03: Establish the expansion pull-request roadmap

- [x] Define one major pull request each for combat/overview, activity,
  alerts/timers/audio, maps, knowledge/planners, overlays, and profiles/services.
- [x] Record dependencies, merge order, exit criteria, validation, rollback,
  and stop conditions for every phase.
- Acceptance criteria: every selected upstream behavior has one clear native
  destination or a documented reason for omission.

### P7-04: Complete the Phase 7 gate

- [x] Run documentation/repository gates and inspect the complete final diff.
- [x] Record executed commands, observable results, skipped checks, and residual
  risks as validation evidence independent of agent review reports.
- [x] Run iterative Codex review and independent review until there are no
  verified actionable findings.
- [ ] Publish the exact reviewed head, require green exact-head CI, verify
  mergeability, and resolve every actionable review thread.
- Acceptance criteria: `AC-19` is satisfied and the foundation pull request is
  mergeable without granting permission to merge it.
- Validation evidence:
  - `cmake --build --preset check -j2` passed Markdown lint, Ruff, Python
    compilation, and all 8 Python tests on 2026-08-12.
  - `ctest --preset dev --output-on-failure` passed all 18 tests, including the
    X11 and performance suites, on 2026-08-12.
  - `git diff --check` passed after the complete documentation rewrite.
  - Manual installed-game and lifecycle checks are skipped because Phase 7
    changes only planning, contribution, and policy documentation. Every
    runtime expansion remains unimplemented and is residual work for its
    separately gated pull request.
  - Network access, upload, sharing, and update checks are not applicable to
    this documentation-only phase; the diff adds no runtime path for them and
    records that they remain disabled by default. Their future implementation
    and verification remain residual risk in Phase 14.
- Review status: iterative Codex and independent CodeRabbit review reached zero
  findings before publication. Exact-head CI, mergeability, and thread state
  are recorded on PR #20 rather than self-certified by a commit that would
  immediately make that evidence stale. Mark the publication task complete
  after merge in the next planning update.

## Expansion phase backlog

### Phase 8: Combat analytics and overview

- [x] Add bounded fight/zone history, drill-down, timelines, healing, and
  overview summaries under `AC-20`.
- [x] Partition persisted history by a stable opaque character-and-selected-log
  key and test switch-away, switch-back, and restart restoration.
- [ ] Complete focused, full, privacy, performance, installed, review, CI, and
  thread-resolution gates in a separate pull request.

### Phase 9: Progression, loot, inventory, and class activity

- [x] Add per-character XP/AA history, loot/inventory reconciliation,
  equipment-change observations, bounded derived rates, and celebrations.
- [x] Consolidate live equipment and imported reconciliation in the Activity
  Inventory tab; keep the Character dock focused on identity and vitals.
- [x] Move DPS, XP, and AA into a full-width summary above Details while
  retaining recent activity in the Activity pane and its tabs.
- [x] Make all three summary columns independently resizable and allocate enough
  initial width to keep the compact XP and AA values readable.
- [ ] Add class-combination, proc, and upgrade insights only after their
  validated catalogs and evidence rules are implemented under `AC-21`.
- [x] Limit Phase 9 inputs to bounded active-log events, existing immutable
  equipment, the exact-profile optional AA snapshot approved in
  `docs/research/phase9-aa-memory-checkpoint.md`, the exact-profile optional
  current-XP snapshot approved in
  `docs/research/phase9-xp-memory-checkpoint.md`, and an explicitly selected
  local inventory-output file; record a separate checkpoint before adding any
  other source.
- [ ] Complete schema, lifecycle, migration, privacy, installed, review, CI,
  and thread-resolution gates in a separate pull request under `AC-21`.

### Phase 10: Buffs, timers, alerts, and audio

- [ ] Add bounded buff/respawn timers, local alert rules, visible notifications,
  and optional rate-limited sound/voice packs under `AC-22`.
- [ ] Limit Phase 10 inputs to bounded local log/activity events,
  user-configured timers, and validated imported duration/audio packs; keep
  observed and reference durations visibly distinct.
- [ ] Complete fake-sink, lifecycle, accessibility, installed, review, CI, and
  thread-resolution gates in a separate pull request.

### Phase 11: Map workflows

- [ ] Add POI search, label declutter, floor slicing, pinned zones, typed
  `/loc`, and local annotations under `AC-23`.
- [ ] Complete parser/renderer/persistence/performance/installed/review/CI gates
  in a separate pull request.

### Phase 12: Knowledge packs, raid history, and planners

- [ ] Add a validated user-imported or license-compatible pack format and
  item, quest, recipe, Plane of Sky, Exaltation, and raid-history views under
  `AC-24`; classify raid kills only against a validated roster from that pack.
- [ ] Complete provenance, schema, integrity, migration, installed, review, CI,
  and thread-resolution gates in a separate pull request.

### Phase 13: Detached and overlay presentation

- [ ] Add user-enabled damage/healing/progression/buff/timer windows with lock,
  topmost, click-through, scope, and focus-neutral behavior under `AC-25`.
- [ ] Complete Xvfb and real-DWM focus/input/stacking/game-invariance gates,
  review, CI, and thread resolution in a separate pull request.

### Phase 14: Profiles, sharing, and optional services

- [ ] Add per-character switching, previewed additive alert import that never
  replaces existing rules, and atomic settings import/export.
- [ ] Add only concretely specified, separately consented services whose
  endpoint, payload, retention, deletion, offline, and threat model gates pass
  under `AC-26`.
- [ ] Keep network access, uploads, sharing, and update checks disabled by
  default; enable each independently only after its approved contract and
  consent gate pass.
- [ ] Complete security/privacy/network-failure/installed/review/CI gates in a
  separate pull request.

## Phase 6: Character and combat column

### P6-01: Establish the character-field evidence boundary

- [x] Expand the normative offset workflow for character identity, current HP,
  maximum HP, current and maximum MP, equipped slots, and equipped item names.
- [x] Record exact-profile static resolvers, explicit bounds, two controlled
  observations per displayed field, and rejection rules in a privacy-safe
  Phase 6 checkpoint.
- Acceptance criteria: no guessed, copied, partial, or UI-local address enters
  the profile, and an unproven equipment field is omitted.

### P6-02: Publish immutable bounded character snapshots

- [x] Add centralized profile fields and bounded readers for approved vitals
  and equipment.
- [x] Add immutable character snapshots and clear them across every existing
  lifecycle transition or inconsistent read.
- [x] Cover valid, boundary, invalid pointer, invalid range, malformed name,
  excessive slot count, torn read, unsupported profile, and lifecycle cases.
- Acceptance criteria: the UI receives values only, and any invalid character
  read publishes no partial or stale character data.

### P6-03: Parse the active local combat log

- [x] Select only the active character's bounded local log without persisting
  its path or runtime name.
- [x] Incrementally parse supported damage lines into immutable current or
  most-recent encounter totals, DPS, percentage, and active duration.
- [x] Handle append, partial lines, inactivity, truncation, rotation,
  unavailable logs, malformed input, and hard byte/count limits.
- Acceptance criteria: parser fixtures are synthetic, diagnostics contain only
  categories, and no upload, account, network, database, or history path exists.

### P6-04: Add the left character and parser column

- [x] Add a Character dock with HP/mana bars, text equipment, and current DPS.
- [x] Add a compact Parse dock below it with participant, damage, DPS,
  percentage, and active-duration columns.
- [x] Preserve default/restored dock placement, theme behavior, unavailable
  states, detached X11 class, and existing map/spawn/detail interactions.
- Acceptance criteria: Character is above Parse in the default left area and
  both consume immutable snapshots without direct file or process access.

### P6-05: Complete the Phase 6 gate

- [x] Run focused, full repository, privacy, performance, package, and staged
  install validation.
- [ ] Complete exact-client HP/mana/equipment, encounter lifecycle, DWM
  placement, and game-invariance manual validation.
- [x] Obtain iterative local review and a fresh independent review, resolve all
  actionable findings, and record skipped checks and residual risks.
- Acceptance criteria: `AC-13` through `AC-15` pass, existing `AC-01` through
  `AC-12` remain green, and the PR is ready for review without known blockers.

## Post-release maintenance

### M-14: Read August 17 AA state and simplify the Details summary

- [x] Record the owner's authorization and exact-client static and bounded
  read-only evidence for August 17 AA progress and banked points in
  `docs/research/phase9-aa-memory-checkpoint.md` without retaining private
  gameplay values or client content.
- [x] Enable the immutable August 17 progression-cache symbols and validate the
  complete float and integer byte ranges inside the exact PE image.
- [x] Preserve the existing double-read, finite 0-100 percent, bounded-point,
  fail-closed, transient, and memory-over-log contracts.
- [x] Remove the duplicated Latest activity column from the Details summary,
  retain recent events in the Activity dock, and migrate saved four-column
  summary widths to the new three-column layout.
- [x] Pass the full repository, package, review, atomic installation, running
  hash, privacy-log, and visible installed-window checks.
- Acceptance criteria: only the exact August 6 or August 17 identity may expose
  these AA fields; invalid, partial, out-of-image, or torn values fail closed;
  the live installed summary renders bounded AA state from memory and contains
  only DPS, XP, and AA columns.
- Rollback: clear the three August 17 progression symbols and restore the
  fourth summary column; existing activity history remains readable.
- Validation: the warnings-as-errors repository gate, 13 Python tests, all 20
  CTest cases, exact-client fingerprint, package metadata, staged install
  inventory, and the CodeRabbit review loop passed. The validated binary was
  atomically installed with the prior hash-addressed rollback and relaunched;
  the build, installed path, and running executable match SHA-256
  `0116c456e8e724df00c50b76f3217e01c0dbdec48b43666c1948a0624957cf74`.
  The visible installed summary rendered bounded AA progress and memory-backed
  banked points instead of unavailable, contained only DPS, XP, and AA panes,
  and retained recent events in the Activity dock. The privacy-safe log
  selected the exact profile and reported `in_world` with the map loaded.

### M-13: Read current player XP from the exact August 17 client

- [x] Record the owner's authorization and exact-client static and bounded
  read-only evidence in `docs/research/phase9-xp-memory-checkpoint.md` without
  retaining private gameplay values or client content.
- [x] Add an immutable August 17 current-XP RVA, validate its exact-profile
  capability, and publish only a finite, stable value in the inclusive range
  0-100 through the character snapshot.
- [x] Keep accumulated combat-log XP as history-derived XP gained and use it
  only for events, rate, and pace; never present it as current player XP.
- [x] Cover supported, unsupported, invalid, profile-bound, and UI precedence
  behavior with synthetic tests, including a retained 133 percent gain total
  that must not render as current XP.
- [x] Pass the full repository, package, review, atomic installation, running
  hash, privacy-log, and visible installed-window checks.
- Acceptance criteria: only the exact August 17 file and mapped PE identity
  may expose current XP; invalid or torn values fail closed; the installed
  summary matches the in-game current XP and does not show accumulated log XP
  as the current position.
- Rollback: clear the August 17 XP RVA and remove the optional snapshot field;
  existing log-derived history remains readable without migration.
- Validation: the warnings-as-errors repository gate, 13 Python tests, all 20
  CTest cases, exact-client fingerprint, package metadata, staged install
  inventory, and two-pass CodeRabbit review passed. The validated binary was
  atomically installed with the prior hash-addressed rollback and relaunched;
  the build, installed path, and running executable match SHA-256
  `da476d1938a3479dd303ace7061e44609117a5d01233419dd3d243691bdb86d6`.
  The visible installed summary showed bounded current XP below 100 percent,
  labeled the log-derived gain rate separately, and did not show the retained
  133 percent gain total as current XP. The privacy-safe log selected the exact
  profile and reported `in_world` with the map loaded.

### M-12: Automate compatibility verification safely

- [x] Define and test a machine-readable contract for every compiled profile,
  including exact identity, bounds, unique selection, and complete-or-disabled
  optional capabilities.
- [ ] Cover every supported data consumer with synthetic success, malformed,
  inconsistent, lifecycle, and unsupported-client tests.
- [x] Add an owner-only candidate preparer that records exact identity and
  every required resolver/evidence gate without copying client content or
  enabling the candidate. Preserve existing candidate evidence atomically.
- [ ] Invoke candidate preparation from the installed app when an executable
  identity is unknown and expose only privacy-safe progress categories.
- [ ] Reproduce the approved static local-player, world, zone, spawn, and
  optional character resolver searches with deterministic evidence checks.
- [ ] Add bounded two-observation live verification and privacy-safe category
  reporting for every candidate capability.
- [ ] Require explicit owner approval to promote a fully passing candidate to
  a new immutable compiled profile, then rebuild, install with rollback, and
  verify the installed and running hashes.
- Acceptance criteria: `AC-27` passes; unknown or incomplete clients remain
  unsupported; no resolver guesses or silently edits an active profile; and
  candidate artifacts remain private, local, bounded, and removable.
- Rollback: remove the automation while preserving every existing immutable
  profile and the manual `docs/profile-refresh.md` workflow.
- Initial validation: repository checks, 13 Python tests, all 20 CTest cases,
  exact-client fingerprinting, atomic local installation, and an isolated
  installed-command smoke test passed. The build and installed SHA-256 both
  match `3504e2e74f416f6421f00ab6d74fd8481a984be1f224ada13643b4f0c2c42a1e`;
  the prior installed build is retained as a hash-addressed rollback. The
  pre-existing visible companion still uses an older deleted inode and must be
  relaunched before it can validate this build.

### M-11: Support the 2026-08-17 Legends client

- [x] Capture the separate exact SHA-256
  `3451069e63d5118a703a237f121a3ea7c20c973477a69fdd0d66bcdaa7d80b29`
  and PE identity without modifying any prior immutable profile.
- [x] Re-establish the exact-client local-player, world-data, and character
  global families from static semantic cross-references. Reject the stale
  carried-forward zone and level fields, then establish zone `0x3b8` and level
  `0x391` through bounded live reads, visible comparison, and matching static
  access evidence.
- [x] Keep the optional Alternate Advancement memory fields disabled because
  their existing approval applies only to `legends-2026-08-06`.
- [x] Pass the fresh warnings-as-errors build, repository checks, 8 Python
  tests, all 19 CTest cases, exact fingerprint, staged install inventory, and
  installed-command smoke test. The build and installed SHA-256 both match
  `702aaf67559afa67f7aa6a3d10ba7dc5d24e3deca0abaac4ab72be64139a82f6`.
- [x] Match the exact mapped PE identity in two isolated companion runs and
  publish the expected character-select and in-world lifecycle states without
  stale snapshots; the installed in-world smoke also loaded the current map.
- [x] Prove consecutive complete bounded player, zone, and spawn snapshots
  against the exact live PE identity while the client is in world.
- [x] Keep the Character snapshot fail-closed until the corrected character
  root, vital maxima, and item-name pointer complete the bounded chain.
- [x] Publish the independently validated bounded local-player identity from
  the spawn anchor so the Character dock can show the player and the local
  combat-log tailer can select the active log without enabling unverified
  vitals, equipment, activity observations, or inventory export.
- [x] Re-establish and prove two consecutive bounded character, vital, and
  equipment snapshots against the exact live PE identity.
- [x] Re-run the fresh warnings-as-errors build, repository gate, 13 Python
  tests, all 20 CTest cases, exact fingerprint, package metadata, and staged
  install inventory after the maintenance correction. Atomically install and
  relaunch SHA-256
  `e3756f23813a12570b718b36ebcd73658e556221490b037336c86ab09b4fd8c4`;
  the build, installed pathname, and running executable match, with
  `root:root` ownership and mode `0755`. The installed window rendered changing
  HP/MP gauges and the corrected bounded spawn levels.
- [ ] Complete two controlled visible observations for every displayed field,
  two zones, lifecycle transitions, DWM placement, game invariance,
  performance, package, and privacy gates from `docs/profile-refresh.md`.
- Acceptance criteria: only the exact file and mapped PE identity select the
  candidate; every displayed field passes `docs/offset-discovery.md`; unknown
  or inconsistent clients fail closed; and no private research artifact enters
  the repository or package.
- Merge exception: on 2026-08-18, the owner explicitly authorized merging all
  completed work and starting the compatibility-automation phase. This
  authorizes merging M-11 with the incomplete manual lifecycle checks retained
  as residual risk; it does not count those checks as passed or authorize a
  release.

### M-10: Support the 2026-08-03 Legends client

- [x] Add a separate immutable profile for exact SHA-256
  `f8af4e704746118f8dd94b688e585bc5c37c3d085da620136bcacad5486145ac`
  without modifying or weakening any newer or prior profile.
- [x] Retain the exact-client player, zone, and spawn fields established from
  static evidence and bounded privacy-safe probes.
- [x] Fail closed for the Character snapshot because this historical profile
  predates the required independently validated maximum-HP field. Do not guess
  that field or expose vitals, equipment, combat identity, or inventory export
  for this client.
- [x] Cover exact selection of all known profiles, rejection of unknown
  digests, and the disabled Character capability in the synthetic repository
  gate.
- [ ] Complete a second zone, second equipment configuration, character select,
  camping, game exit, process reacquisition, DWM placement, and game-invariance
  checks on the exact historical executable.
- Merge exception: on 2026-08-11, the owner explicitly authorized updating and
  merging every open pull request. This authorizes merging M-10 with the
  incomplete manual lifecycle checks and unavailable Character capability
  retained as residual risk; it does not authorize a release.
- Acceptance criteria: only the exact file and live PE identity select the
  profile; approved player, zone, and spawn fields remain bounded; unavailable
  Character data stays fail-closed; and no private artifact enters the tree.

### M-09: Export inventory for EQ Legends Tools

- [x] Add `Export Inventory...` to the User dropdown and enable it only for an
  available immutable character snapshot.
- [x] Serialize the character name and equipped item names into the current EQ
  Legends Tools profile-backup slot and item-ID contract without adding a
  network request or bundled item database.
- [x] Save the JSON atomically with owner-only permissions and reject
  unavailable, duplicate-slot, unknown-slot, or malformed item data.
- [x] Document that race, tri-class, favored stats, Alternate Advancement, and
  Exaltations must be verified after `Import Profile Backup` because Plazmic
  Legends does not read or invent those fields.
- [x] Cover all 23 slot mappings, item-name normalization, upgrade suffixes,
  empty equipment, rejection paths, button lifecycle, and file permissions
  with synthetic tests and run the complete repository and package gate.
- [x] Install the exact validated build with rollback and verify its installed
  hash, ownership, mode, and isolated Xvfb startup. The installed User-menu
  action and private EQ Legends Tools import were not observed because no live
  client or companion process was available.
- Merge exception: on 2026-08-11, the owner explicitly authorized merging all
  open pull requests with necessary updates. This authorizes merging M-09 with
  the missing live click-through retained as residual compatibility risk; it
  does not count that check as passed or authorize a release until an exported
  private file is successfully imported through EQ Legends Tools.
- Acceptance criteria: `AC-18` passes while the existing read-only,
  fail-closed, privacy, lifecycle, and package boundaries remain unchanged.
- Validation: fresh configure and warnings-as-errors build, repository checks,
  8 Python tests, all 18 CTest cases, exact-client fingerprint, package
  metadata and staged-install inventory, and privacy and whitespace scans
  passed. The deployed EQ Legends Tools v1 backup parser and item-ID
  normalization contract were inspected on 2026-08-09 without adding a
  runtime network dependency. Exact head `9f4de39` was installed at
  `/usr/local/bin/plazmic-legends`; the build and installed SHA-256 both matched
  `7bf954207f82fc4d0a6bb316ea98d57ef7580d29330336eb2b2958cc1e2c06af`,
  and the installed-command Xvfb smoke test passed.

### M-08: Complete HP/MP gauges and restore consider-color visibility

- [x] Add a bounded exact-profile maximum-HP field, revalidate it with the
  current HP owner, and reject zero, out-of-range, torn, or current-above-max
  snapshots.
- [x] Render HP as `current / max (percent)` with a red percentage-driven bar
  and MP as `current / max (percent)` with a blue percentage-driven bar.
- [x] Drive each fill from the exact current/maximum ratio, round the displayed
  percentage to the nearest whole number, show `0 / 0 (0%)` explicitly, and
  reserve enough gauge width for the complete MAXHP/MAXMP text.
- [x] Correct the active August 6 MAXMP cache after rejecting `0x2e8` and
  `0x2d8` as unrelated resources and proving that `0x290` tracks current MP.
  A level-up plus depleted-mana observation established `0x678` as the dynamic
  maximum through the bounded read-only path.
- [x] Keep all seven ordinary-NPC consider colors visible on map markers and
  labels and in the spawn table, while preserving named, player, and Other
  presentation.
- [x] Correct the active August 6 spawn level field after live comparison
  proved that `0x64c` published a percentage as level 100 while the game showed
  local-player level 29 and NPC level 33. Use the validated level byte at
  `0x275` and reduce the bounded record span to `0x276`.
- [x] Run the complete repository gate, exact active-client fingerprint and
  read-only smoke, and overwrite the local installed binary with rollback and
  matching artifact/install hashes.
- Acceptance criteria: `AC-13` requires both maxima and percentages; both bars
  reduce with their percentage; every consider band remains distinguishable;
  invalid vitals fail closed; and the installed binary is the validated build.
- Validation: fresh warnings-as-errors build, repository checks, 8 Python
  tests, all 17 CTest cases, exact active-client fingerprint, bounded live
  maximum-HP/MP read, semantic live player/NPC level comparison, and installed
  X11 launch passed. Build and
  `/usr/local/bin/plazmic-legends` both matched SHA-256
  `620ca7e2f25e5cb2e42b3d70da0b47275063dc042dcc2a5fcdb52b68c6d99b39`;
  the prior installed binary is retained in the ignored rollback directory.

### M-07: Support the 2026-08-06 Legends client

- [x] Capture the separate exact SHA-256
  `bf34438c6460acde463692fa09ea28f0d12a204e3445a9da356645fc0d475561`
  and PE identity without modifying either prior immutable profile.
- [x] Add a local candidate profile and prove that exact file and live PE
  identity select one same-user Wine process and publish two consecutive
  complete bounded player, spawn, character, vital, and equipment snapshots.
- [ ] Complete the exact-client semantic static-resolver audit, synthetic
  repository gate, two controlled visible field observations, lifecycle,
  DWM-placement, game-invariance, performance, package, and privacy checks.
- Acceptance criteria: every approved RVA and field passes
  `docs/offset-discovery.md`; unknown or inconsistent clients fail closed; and
  no private research artifact enters the repository or package.
- Release block: the owner authorized merging this candidate with the pending
  checks recorded as residual risk, but it is not release-ready until the
  complete `docs/profile-refresh.md` gate passes.

### M-06: Support the 2026-08-04 Legends client

- [x] Add a separate immutable profile for exact SHA-256
  `d9784a58bb03cb70177d6a494fa71bca2b13ab3c5b8b9d6c26e45bae01597e51`
  without modifying or weakening the prior profile.
- [x] Re-establish the player, zone, spawn, character, vital, and equipment
  fields from exact-client static evidence and bounded privacy-safe probes.
- [x] Cover exact selection of both known profiles and rejection of unknown
  digests, then run the complete synthetic repository gate.
- [ ] Complete two controlled visible observations for position, facing,
  zones, spawn fields, vitals, and equipment configurations, plus lifecycle,
  DWM placement, and game-invariance checks.
- Acceptance criteria: only exact file and live PE identity select the new
  profile; every approved field satisfies `docs/offset-discovery.md`; all
  unsupported, inconsistent, or changed-client states fail closed; and no
  private research artifact enters the repository or packages.
- Release block: the owner authorized merging this profile with the pending
  checks recorded as residual risk. Do not release it until the manual checks
  in `docs/profile-refresh.md` cover character selection,
  entering the world, map selection, zoning, camping to character select,
  game exit, and process reacquisition, in addition to the controlled field,
  DWM-placement, and game-invariance checks above.

### M-05: Export and install private UI files

- [x] Add an ignored 2560x1440 exporter for the installed `plazmic-ui` skin,
  global settings, window layouts, and character filter/UI profiles.
- [x] Derive a cohesive generic 1440p layout only from the current Legends INI,
  preserving unknown sections and changing an allowlisted geometry set.
- [x] Add `User > UI File Install...` with integrity validation, explicit
  source and destination INI selection, optional global settings, live UI
  reload support, private backup, and rollback behavior.
- [x] Install the chosen layout as a reserved `UI_plazmic_1440p.ini` source so
  a running client can import it through `/copylayout` without relying on an
  overwritten active character INI.
- [x] Prove synthetic export/install/tamper/selection behavior and run the full
  repository, privacy, package-inventory, and local-install gates.
- Acceptance criteria: `AC-16` passes; private assets and character/server
  names stay ignored and out of packages, logs, fixtures, and the pull request;
  and the existing gameplay read-only boundary remains unchanged.

### M-01: Persist and discover the Legends client directory

- [x] Save a valid explicit or environment-provided game directory in
  `[client].game_directory`.
- [x] Reuse a valid saved directory without scanning.
- [x] Fall back to a fast, bounded home-directory scan for the exact Daybreak
  installation structure and fail closed on multiple matches.
- Acceptance criteria: a valid `EQ_LEGENDS_DIR` containing `eqgame.exe` never
  scans; one discovered installation is saved automatically; configuration,
  selection precedence, ambiguity, and UI-close persistence have synthetic
  tests.

### M-02: Add spawn map presentation controls

- [x] Classify `#`-prefixed NPC names as named spawns for presentation without
  changing the immutable spawn snapshot or exact-profile reader.
- [x] Draw named spawns with a distinct color and shape, and draw non-player,
  non-NPC snapshots as neutral gray Other markers.
- [x] Add independent named-NPC, player-character, and ordinary-NPC label
  controls plus an Other-marker visibility control.
- [x] Persist all new map controls and expose Other in the spawn-table type
  filter.
- [x] Add a visible Filters / Labels dropdown with independent Named NPC, PC,
  NPC, and Ground / Other marker toggles that also govern map hit-testing.
- [x] Cover classification, rendering controls, filtering, persistence, and
  selection behavior with synthetic tests and the complete local gate.
- Acceptance criteria: named, player, ordinary NPC, and Other markers are
  distinguishable; labels are independently optional; hidden Other markers
  and every independently filtered marker category cannot be selected from the
  map; and no reader/profile behavior changes.
- Validation: repository checks, all 14 CTest cases, the 2,000-spawn label-on
  performance fixture, a synthetic X11 visual inspection, and exact-client
  fingerprint verification passed.

### M-03: Color ordinary NPC dots by consider level

- [x] Publish the local-player level from the already validated spawn anchor
  without adding a client offset or exposing process addresses to the UI.
- [x] Derive the modern gray, green, light-blue, blue, white, yellow, and red
  consider bands from immutable local-player and NPC levels.
- [x] Apply consider colors to ordinary NPC dots, labels, and spawn-table rows
  while preserving named-NPC, player-character, Ground / Other, selection,
  label, and filter behavior.
- [x] Cover every color and level-band boundary with synthetic tests and run
  the complete repository gate.
- Acceptance criteria: every ordinary NPC marker, visible label, and table row
  has the correct consider color for the published local-player level; named
  NPCs retain their distinct diamond treatment.

### M-04: Add top-level view and window controls

- [x] Add an embedded top menu bar with a Views dropdown for every docked
  Character, Parse, Spawns, and Details element.
- [x] Add accessible minimize, maximize/restore, and close controls on the top
  right without changing X11 placement or application lifecycle behavior.
- [x] Keep the Character/Parse column full height and place Details below both
  the map and Spawns panes.
- [x] Cover visibility toggles and all three window controls with the synthetic
  X11 main-window test.
- Acceptance criteria: each Views action stays synchronized with its dock, the
  window buttons perform their advertised action, and dock visibility remains
  part of the existing saved layout.

## Completed Phase 5

### P5-01: Reduce to the supported product boundary

- [x] Remove the historical overlay proof, proof-only tests, and unused
  Xext/Xfixes/XTest dependencies.
- [x] Map every retained source path, executable, test, tool, and runtime
  dependency to a current `SPEC.md` requirement.
- [x] Record source provenance, project-license status, dependency licenses,
  and removal impact without restoring imported implementation.
- Acceptance criteria: the configured build contains only the supported
  product, its deterministic tests, the PE inspector, and required system
  dependencies.

### P5-02: Harden compatibility and privacy-safe diagnostics

- [x] Expose the project version and exact compatibility profile in local
  status and diagnostics.
- [x] Write bounded XDG-state logs containing only approved lifecycle,
  compatibility, integration, and error-category values.
- [x] Detect client executable changes, invalidate live state, and show an
  actionable unsupported-build result without weakening profile matching.
- [x] Document the immutable profile-refresh workflow for a newly fingerprinted
  client.
- Acceptance criteria: a changed or unsupported client never receives old
  offsets, and diagnostics contain no credentials, tokens, runtime names,
  process addresses, memory content, or Wine-prefix data.

### P5-03: Build the Fedora artifact

- [x] Produce a versioned Fedora RPM from the CMake install
  boundary with explicit runtime dependencies.
- [x] Include required project and dependency notices, support matrix, known
  risks, and package status without bundling system libraries.
- [x] Document install, upgrade, removal, troubleshooting, and rollback.
- [x] Generate SHA-256 checksums from a clean artifact build.
- Acceptance criteria: the RPM contains only approved native product files and
  can be installed, upgraded, removed, and rolled back without modifying the
  game installation or Wine prefix.

### P5-04: Establish exact-commit reproducibility gates

- [x] Add Linux CI for configure, warnings-as-errors build, repository checks,
  CTest/Xvfb, and clean package inspection.
- [x] Bind CI and local evidence to the exact release-candidate commit.
- [x] Rebuild from a clean checkout and compare staged contents and checksums.
- Acceptance criteria: local and CI gates produce the same approved file
  inventory from the audited commit.

### P5-05: Complete the release-candidate gate

- [x] Run clean install, upgrade, launch, unsupported-client, and uninstall
  tests on the reference Fedora/Wine/X11 tier.
- [x] Repeat lifecycle, accuracy, accessibility, DWM placement, privacy, and
  performance scenarios against the exact candidate.
- [x] Complete security, privacy, provenance, dependency, license, retained
  path, and package-content audits.
- [x] Obtain an independent review without using CodeRabbit.
- [x] Record checksums, support matrix, known risks, rollback evidence, and all
  skipped validation.
- [x] Confirm package publication authorization.
- Acceptance criteria: the release candidate satisfies `AC-01` through
  `AC-12`, and every unsupported or changed-client path fails closed.

## Completed Phase 4

### P4-01: Establish the exact-profile spawn resolver

- [x] Independently identify and document the smallest collection root,
  traversal shape, and consistency invariants for the exact supported client.
- [x] Resolve only stable ID, approved type, bounded display name, level, and
  finite position fields.
- [x] Require two controlled observations for every displayed field and fail
  closed when any profile or structure invariant fails.
- Acceptance criteria: no copied, guessed, partial, or UI-local address enters
  the compatibility profile.

### P4-02: Read a hard-bounded immutable collection

- [x] Define an evidence-based maximum count and bounded string size before
  enabling a live collection.
- [x] Stage collection, entry, and value reads with canonical-address,
  readable-mapping, overflow, pointer-depth, and consistency checks.
- [x] Reject duplicate IDs, invalid types, malformed names, invalid levels,
  non-finite positions, oversized collections, and collection changes during
  publication.
- [x] Publish immutable spawn snapshots containing no process addresses.
- [x] Cover every success and rejection path with synthetic fixtures that
  contain no Daybreak or player data.
- Acceptance criteria: a failed or inconsistent collection publishes no stale
  or partial spawn values.

### P4-03: Add synchronized spawn presentation

- [x] Replace the placeholder with a sortable and filterable spawn table.
- [x] Draw spawn markers without rebuilding unchanged map geometry.
- [x] Synchronize map, table, and detail selection by stable ID without
  affecting the game target.
- [x] Show explicit empty, unavailable, no-selection, and stale states.
- Acceptance criteria: every presentation surface consumes immutable values
  only and agrees on the selected stable ID.

### P4-04: Complete lifecycle and performance behavior

- [x] Cover add, remove, change, duplicate, oversized, and inconsistent
  collection behavior.
- [x] Invalidate all spawn state on zoning, camping, character select, read
  failure, profile change, and process exit.
- [x] Measure bounded reader, publication, filtering, sorting, selection, and
  large-fixture rendering cost.
- Acceptance criteria: no stale spawn survives a transition and the live game
  shows no visible frame-pacing regression.

### P4-05: Complete the Phase 4 gate

- [x] Run the complete Phase 3 gate plus spawn reader, model, selection,
  lifecycle, privacy, and large-fixture checks.
- [x] Validate approved fields against two controlled live observations.
- [x] Audit logs, fixtures, diffs, package contents, and artifacts for
  character names, spawn names, addresses, and Daybreak content.
- [x] Stop for approval before Phase 5 hardening work.

## Completed Phase 3

### P3-01: Parse local map geometry safely

- [x] Validate zone short names and contain all selected paths under the
  configured maps directory.
- [x] Parse bounded `L` line and `P` label records plus numbered layer files.
- [x] Reject missing base maps, path escapes, symlinks outside the map root,
  oversized input, excessive records, long lines, malformed fields, invalid
  colors, non-finite coordinates, and out-of-range values.
- [x] Cover the parser with synthetic fixtures containing no Daybreak map
  content.
- Acceptance criteria: base and numbered layers produce immutable geometry,
  every rejection has a typed error, and no installed map enters the
  repository or package.

### P3-02: Render the local map model

- [x] Add map bounds and coordinate transforms, pan, zoom, and layer
  visibility.
- [x] Add a default-on player-Z height filter with explicit off/on control,
  independently adjustable below/above ranges, and persisted settings.
- [x] Add persisted player-follow state that exits on manual pan or map fit.
- [x] Replace the Phase 2 map placeholder with a Qt canvas driven only by
  immutable map and player snapshots.
- [x] Add synthetic transform and canvas snapshot tests.
- Acceptance criteria: synthetic geometry and heading render consistently
  without client access.
- Checkpoint evidence: `docs/research/phase3-player-map-checkpoint.md`.

### P3-03: Resolve exact-profile zone and player state

- [x] Discover and document validated resolvers for world/session state, zone
  short name, player coordinates, and heading.
- [x] Enforce module, mapping, pointer-depth, string, finite-number, and
  lifecycle bounds through the existing read-only reader.
- [x] Require two controlled observations for each live field and fail closed
  when any invariant fails.
- Acceptance criteria: no guessed or copied offset reaches the compatibility
  profile.
- Completion evidence: player position and heading matched controlled ground
  truth at multiple locations, and a second-zone transition passed.

### P3-04: Publish lifecycle-safe player snapshots

- [x] Convert validated reads into immutable zone/player snapshots.
- [x] Invalidate state on character select, zoning, camping, read failure, and
  process exit.
- [x] Select map paths only from validated zone short names.
- Acceptance criteria: stale player markers cannot survive a lifecycle
  transition.

### P3-05: Complete the Phase 3 gate

- [x] Run the full Phase 2 gate plus parser, transform, renderer, reader,
  snapshot, and lifecycle tests.
- [x] Validate map orientation, multiple controlled positions and headings,
  layer behavior, missing maps, zoning, camping, and exit against the live
  client.
- [x] Measure parsing, snapshot publication, and UI update cost.
- [x] Stop for approval before Phase 4 spawn collection research.
- Completion evidence: `docs/phase3-completion-report.md`.

### P1-01: Capture the Linux/Wine runtime baseline

- [x] Record host, Wine, prefix, launch, process/module, renderer, X11, window
  mode, and client fingerprint evidence.
- Evidence: `docs/research/phase1-runtime-baseline.md`.
- Acceptance criteria: evidence identifies the exact client process and
  distinguishes static PE clues from modules observed at runtime.
- Automated validation: executable fingerprint and environment probes.
- Manual validation: launched through Lutris; confirmed the live fullscreen
  game window and DXVK/Vulkan process mappings.

### P1-02: Prove least-privilege external process discovery

- [x] Build a native Linux ELF probe that identifies the Wine-hosted Legends
  process and its `eqgame.exe` mappings.
- Evidence: `src/integration/process_discovery.cpp`,
  `docs/phase1-architecture-proof.md`.
- Acceptance criteria: one supported process is selected without root,
  injection, Wine overrides, or host-security changes; zero and ambiguous
  matches fail explicitly.
- Automated validation: synthetic `/proc` fixtures cover exact, absent,
  wrong-identity, and ambiguous targets.
- Manual validation: selected the exact reference client, resolved its image
  mapping, and read only the two-byte PE signature as UID 1000. Process and
  overlay artifacts are native ELF files; the run used no root privilege,
  code injection, Wine override, host-security change, prefix edit, or
  game-installation edit.
- Dependency: P1-01.

### P1-03: Evaluate the external X11 diagnostics overlay

- [x] Build a native X11 overlay showing project, process, profile, and
  compatibility diagnostics only.
- Evidence: `src/overlay/x11_overlay.cpp`,
  `tests/test_x11_overlay_lifecycle.cpp`,
  `tests/test_x11_overlay_hotkey.cpp`, and
  `docs/phase1-architecture-proof.md`.
- Validation: X Shape reported an empty input region; isolated Xvfb/XTest
  validation hid and restored the proof; three child-owned X11 target exits
  cleaned up; and no proof executable remained afterward. A later root-window
  screenshot proved that X11 `IsViewable` did not mean the panel was composed
  above the live true-fullscreen game.
- Result: rejected for the product. The reference DWM deliberately raises true
  fullscreen above override windows. Removing fullscreen exposed the DWM bar,
  changed focus presentation, and produced perceived opacity.
- Automated validation: build, CLI, SHA boundary, synthetic `/proc`, repeated
  X11 lifecycle, isolated hotkey, input-shape, and no-client checks.
- Manual validation: root-window screenshots, X11 tree and property inspection,
  exact client still running after proof exit, restored fullscreen geometry,
  and exact-executable orphan inspection.
- Dependency: P1-02.

### P1-04: Approve the architecture and MVP boundary

- [x] Record the selected process-read mechanism, module resolution, X11
  strategy, dependency licenses, symbol-validation strategy, MVP fields,
  hotkey, distribution intent, and game-rule risk.
- Evidence: `docs/phase1-architecture-proof.md`,
  `docs/research/phase1-symbol-plan.md`,
  `docs/research/phase1-policy-risk.md`, and
  `docs/phase1-codex-review.md`.
- Decision update: this phase approved read-only memory research despite the
  recorded EULA conflict. Each later capability requires approval under its own
  numbered expansion phase; protection bypass remains outside the project
  boundary.
- Acceptance criteria: every unresolved Phase 1 decision in `SPEC.md` is
  answered or explicitly deferred outside MVP.
- Validation: independent Codex review, official-policy review, dependency
  license inspection, local install smoke test, full repository gate, and the
  user's instruction to finish Phase 1.
- Dependency: P1-03.

### P1-05: Prove the bounded external memory reader

- [x] Replace the one-off two-byte probe with a reusable same-user Linux
  reader that rejects unreadable and out-of-range requests.
- Evidence: `src/integration/process_reader.cpp`,
  `tests/test_process_discovery.cpp`, and
  `docs/phase1-architecture-proof.md`.
- Baseline comparison:
  `docs/research/macroquest-boundary-review.md`.
- Acceptance criteria: the live reader recovers PE machine, timestamp,
  relocated image base, and image size from process memory and matches the
  exact file and `/proc` profile.
- Automated validation: self-process reads, mapping-bound checks, invalid PID,
  overflow, and remote PE parsing fixtures.
- Manual validation: exact live Legends process returned machine `0x8664`,
  timestamp `0x6a6a2851`, image size `0x16c1000`, and an ImageBase equal to the
  mapped base. No root, write-capable code path, prefix change, or
  host-security change was used.
- Dependency: P1-02.

## Completed Phase 2

### P2-01: Accept the standalone UI dependency boundary

- [x] Record Qt 6 package versions, license terms, dynamic-linking impact, and
  clean-removal impact.
- [x] Confirm no ShowEQ source, data, maps, protocol definitions, or generated
  tables enter the repository.
- Acceptance criteria: the dependency and provenance audit maps directly to
  `SPEC.md` and `docs/research/showeq-ui-review.md`.

### P2-02: Build the independent application shell

- [x] Replace the product overlay path with a normal Qt 6 `QMainWindow`.
- [x] Set X11 instance `plazmic-legends` and class `PlazmicLegends` on every
  Plazmic top-level window.
- [x] Add compatibility and process status without reading gameplay state.
- [x] Persist geometry and dock state under the XDG configuration directory.
- Acceptance criteria: opening, focusing, moving, minimizing, and closing the
  application does not alter the game's fullscreen, focus policy, geometry,
  opacity, or input configuration.

### P2-03: Integrate DWM tag 5 placement

- [x] Add the live DWM rule
  `{ class="PlazmicLegends", tags=5, monitor=1, noswallow=1 }`.
- [x] Verify the main window and every detached Plazmic dock report EWMH desktop
  index 4 and geometry on HDMI-0.
- [x] Verify launch does not switch the game tag, focus the game, request
  `alwaysontop`, or alter the game window.
- Acceptance criteria: all Plazmic top-level windows open on human tag 5 under
  the current two-monitor DWM layout and remain there after layout restore.

### P2-04: Establish the docked UI and synthetic state boundary

- [x] Add docked map, spawn-list, and detail placeholders plus a status strip.
- [x] Define synthetic compatibility/lifecycle snapshots without game symbols,
  user map loading, or live player/spawn data.
- [x] Test geometry, dock-state persistence, corrupted-state fallback, and
  detached-window class inheritance.
- Acceptance criteria: UI lifecycle and layout tests pass under Xvfb and every
  pane remains inside the standalone application boundary.

### P2-05: Complete the Phase 2 gate

- [x] Run format/lint, warnings-as-errors build, tests, dependency audit, clean
  package inspection, DWM placement checks, and live game-window invariance
  checks.
- [x] Show the independent tag 5 shell and stop at the Phase 2 approval
  checkpoint.

### P2-06: Follow the system color mode

- [x] Detect the active DWM theme and apply a distinct dark or light Qt
  palette.
- [x] Poll for changes while running, with desktop portal and Qt color-scheme
  fallbacks when the reference DWM theme file is absent.
- [x] Test dark and light parsing, palette contrast, live refresh, and a real
  Nord to Catppuccin Latte to Nord transition.
- Acceptance criteria: the open companion follows the system theme without a
  restart and does not change the game window.

## Completed Phase 0

### P0-01: Preserve the imported baseline

- [x] Preserved all 1,431 imported and scaffold files.
- Evidence: commits `e3034e1` and `a6204d9`; annotated tag
  `phase0-import-baseline`.
- Validation: included normally ignored plugin, IDE, and bundled Python files;
  no Daybreak executable, credential file, crash dump, or local log was found.

### P0-02: Finalize the cleanup manifest

- [x] Classified every retained and removed top-level path.
- Evidence: `docs/cleanup-inventory.md`.
- Validation: retained-path purpose review and import provenance audit.

### P0-03: Remove non-product platforms and features

- [x] Removed the inherited Windows build and MacroQuest platform.
- Evidence: cleanup commit `f296591` deleted 1,419 tracked files and 615,570
  lines.
- Validation: removed-path, binary/artifact, credential, and dangling build
  reference scans.

### P0-04: Reduce retained research code

- [x] Removed all inherited implementation and bundled third-party source.
- Evidence: the active tree contains only planning, research documents, the
  independently written PE inspector, and its tests.
- Validation: no imported implementation license marker, binary dependency,
  game asset, source directory, or runtime artifact remains.

### P0-05: Establish the Linux research scaffold

- [x] Replaced the inherited build with Linux CMake/Ninja presets and a
  dependency-free validation target.
- Evidence: `CMakeLists.txt`, `CMakePresets.json`, and
  `tests/test_inspect_eqgame.py`.
- Validation: clean-checkout configure, repository gate, three unit tests,
  CTest, exact client fingerprint, and clean Git status passed.

## Completed pre-phase work

### S-01: Create the AI project scaffold

- [x] Added project instructions, specification, roadmap, and task queue.
- Validation: documents define Linux scope, cleanup-first ordering, acceptance
  criteria, validation, rollback, and hard pause points.

### S-02: Record the reference executable baseline

- [x] Added a read-only PE fingerprint tool and research record.
- Validation: Python checks and exact SHA-256 match against the local reference
  executable.
- Residual risk: static strings suggest Direct3D 9, but active rendering has
  not been confirmed at runtime.
