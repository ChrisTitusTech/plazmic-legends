# EverQuest Companion feature-parity review

## Review boundary

This review records public product behavior from
[`jmoyers/everquest-companion`](https://github.com/jmoyers/everquest-companion)
at commit `f6fb532bb4e4bfb353d84d88104061bde97e2dcf`, inspected on
2026-08-12. The latest published release observed during the review was
`v0.22.0`.

The upstream repository is licensed FSL-1.1-MIT and describes bundled or
generated databases, wiki images, portraits, and sound packs with their own
provenance. Plazmic Legends is GPL-3.0-only. This is a clean-room,
behavior-level product review: no upstream implementation, schema, database,
asset, fixture, generated content, or private service detail may be copied or
translated into this repository. Native implementations use Plazmic's current
code, public user-visible behavior, synthetic tests, and independently defined
formats. A data-backed feature waits for a user-supplied or legally compatible
source whose provenance is recorded.

## Selection principles

- Prefer features that naturally extend Plazmic's immutable snapshots, bounded
  log parser, native Qt UI, map model, and local settings.
- Adapt Windows/Electron behavior to Linux/Qt conventions instead of copying
  architecture or installer behavior.
- Keep offline/local behavior useful by itself. Treat every upload, update,
  share, or telemetry path as a separate opt-in service.
- Preserve exact-profile failure and lifecycle invalidation for memory-backed
  fields. Log-derived and imported data must identify their own trust boundary.
- Split each major subsystem into its own pull request so it can be reviewed,
  validated, rolled back, and merged independently in roadmap order.

## Feature matrix

| Upstream public surface | Current Plazmic capability | Native Plazmic plan | Phase |
| --- | --- | --- | --- |
| Live DPS per fight and zone | One bounded current/recent encounter | Capped encounter/zone history, damage/healing totals, attack/spell drill-down, timeline | 8 |
| Overview/current mob/recent activity/rates | Character, parse, map, spawn docks | Phase 8 adds current/recent combat cards; Phase 9 adds recent drops and XP/AA rate/ETA | 8-9 |
| Leveling and AA per character | No progression history | Versioned per-character series from bounded active-log events | 9 |
| Loot history and inventory reconciliation | Live text equipment and inventory export | Bounded loot events plus explicit local inventory-output import | 9 |
| Raid target kills and timestamps | Named spawn presentation | User-owned kill events classified only by a validated imported roster | 12 |
| Class-combination/loadout inference | Character identity and equipment | Bounded summaries only when supported observations are unambiguous | 9 |
| Buff timers and respawn clocks | No timer model | Log/activity observations plus user or validated-pack durations with explicit confidence | 10 |
| Charm/buff/raid/custom alerts | No alert engine | Local bounded rule matcher, visible dispatch, and per-rule controls | 10 |
| Sounds, voice packs, celebration toasts | No audio output | Optional fake-sink-tested audio and provenance-validated user packs | 10 |
| POI search and pinned zones | Installed map display | Search installed labels and view a selected non-live zone | 11 |
| Label declutter and floor slicing | Layer and label visibility | Deterministic declutter policy and coordinate-based floor filtering | 11 |
| Typed `/loc` and user locations | Player-follow map | Parse bounded coordinates and store local annotations | 11 |
| Loot/item/quest/recipe knowledge | No bundled item database | Versioned user-imported or license-compatible knowledge packs | 12 |
| Plane of Sky class Test tracker | No quest tracker | Have/need planner, stats, search, and closest-to-done sort over imported packs | 12 |
| Exaltation planner | Inventory-only profile export omits Exaltations | Native planner over a validated imported pack and user-owned state | 12 |
| Floating damage/healing overlays | Dockable main-window views | Independent Qt tool windows with scope, lock, topmost, and click-through controls | 13 |
| Per-character log switching | Active-character log selection | Explicit profile switcher and isolated per-character retained state | 14 |
| Alert/settings sharing | Inventory JSON export | Additive previewed alert import and atomic settings import/export | 14 |
| Background updates | RPM/COPR/AppImage delivery | Preserve package updates; evaluate an optional signed-release checker separately | 14 |
| Feedback and optional log slices | Privacy-safe local diagnostics | Only with a concrete endpoint, preview/redaction, consent, retention, and deletion contract | 14 |
| Usage analytics | None | Only as a separately consented, schema-disclosed optional service | 14 |

## Behaviors not mirrored directly

- The Electron/React/Windows installer, signing, Start-menu, and desktop
  integration are platform implementation details. Plazmic keeps native Qt,
  RPM/COPR/AppImage, and Linux desktop conventions.
- Upstream owner-only triage and deployment infrastructure is not a user-facing
  companion feature and is not part of parity.
- Upstream databases, schemas, item icons, boss portraits, wiki-derived facts,
  bundled voice packs, and generated assets are not copied. Phase 12 defines an
  independent import contract, and Phase 10 accepts only provenance-validated
  user or compatible packs.
- Exclusive-fullscreen overlays are not promised. Independent Linux overlays
  target windowed or borderless play and must degrade safely under the active
  window manager.
- Public behavior does not justify guessing unavailable game fields. A feature
  remains partial or unavailable until its local observation is validated.

## Pull-request topology

The expansion foundation is Phase 7. Phases 8 through 14 are separate major
pull requests in numeric order: combat/overview; progression/activity;
buffs/timers/alerts/audio; map workflows; knowledge/planners; overlays; and
profiles/sharing/services. While stacked, each targets its predecessor. Before
being called mergeable, it must be based on the exact merged predecessor (or be
rebased onto it), pass its focused and full validation gates, complete iterative
Codex and independent review, have green exact-head CI, and have no unresolved
actionable review thread.

Review and CI readiness do not authorize merging, publishing a release, or
enabling a network service.
