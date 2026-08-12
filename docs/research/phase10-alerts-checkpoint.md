# Phase 10 alerts and timers checkpoint

## Scope and provenance

Phase 10 implements local timer and alert behavior that fits the native Linux
architecture. The public EverQuest Companion project was inspected only for
product behavior: visible buff and respawn countdowns, rule-driven alerts,
cooldowns, and optional sound. No source, spell database, respawn database,
audio, fixture, image, or other upstream asset is copied.

Qt 6 Multimedia is not available in the supported build environment. This
phase therefore offers an optional local desktop beep through Qt Widgets, not
audio or voice-pack playback. A future voice-pack capability requires its own
dependency, provenance, manifest, and playback checkpoint.

## Rule-pack contract

`User > Import Alert Rules...` accepts a user-selected, owned, regular,
non-symlink JSON file capped at 256 KiB. Schema 1 contains between 1 and 128
rules. Every rule has a unique bounded `id`, display `name`, case-insensitive
literal `match`, `kind`, `durationSeconds`, `cooldownSeconds`, and optional
`sound` boolean. Kinds are `buff`, `crowd-control`, `respawn`, and `custom`.
Identifiers, names, and literals are capped at 64, 128, and 256 UTF-8 bytes.
Durations are capped at seven days and cooldowns at one day. Invalid JSON,
unknown schemas or kinds, duplicate identifiers, control characters, special
files, foreign-owned files, and oversized packs fail closed.

The selected path is stored owner-only in the existing UI settings and is
revalidated on restart. Plazmic does not copy the pack, infer spell ranks, or
claim a rule's duration came from the game. A rule fires only when its bounded
literal appears in an accepted line from the active character's existing local
log stream. This lets users describe exact cast, landing, fade, kill, or other
locally observed sentences without adding a bundled game database.

## Runtime and lifecycle

The engine keeps at most 256 live timers, 128 recent fires, and 4,096 bounded
matching source identities used to reject replayed log lines. The shared log
tracker separately reserves 4,096 indexed memory-only identities for lines
that actually match alert rules, without displacing its 4,096 persistent
activity identities. Both matching identity stores evict in insertion order so
out-of-order timestamps cannot make their replay protection diverge. Unmatched
lines receive a source identity for immediate evaluation but are not retained.
Matching identities are not removed by activity-history deletion or wall-clock
pruning; they remain bounded by insertion-order eviction and explicit lifecycle
invalidation. Cooldowns are per rule. A matching duration rule replaces that
rule's prior timer only when its event timestamp is at least as new; an older
out-of-order match remains a visible fire without rewinding the countdown. An
alert with a zero duration records a visible fire without creating a timer. The
Alerts dock labels the rule kind, zone, remaining seconds, and evidence as an
observed local-log match. It never labels a user duration as measured or
authoritative.

Log timestamps, cooldowns, replay boundaries, and expiry use `system_clock`.
Enabling alerts or replacing the active pack establishes a fresh replay
boundary. A backward correction lowers that boundary to the supplied current
time so newly observed lines remain eligible, but negative elapsed time cannot
rewind a positive cooldown baseline. A line exactly at the cooldown boundary
is eligible; a zero cooldown intentionally permits every unique matching
source. Lines more than 24 hours ahead of the supplied clock fail closed.
Tests cover backward and forward correction, out-of-order and duplicate lines,
replacement, rotation identities, expiry after suspend-like advances, and
restart clearing.

Alert replay identities are independent of retained activity history. Deleting
the selected activity partition preserves them, while character change, loss,
or client invalidation explicitly clears them together with engine replay
state.

An explicit transient reset can retain bounded respawn clocks, but character
loss, character change, and shared client lifecycle invalidation clear all
timers, recent alerts, cooldowns, and replay identities. Expired timers are
removed against the supplied snapshot clock. Timer state itself is memory-only
in Phase 10; restart restores and revalidates the selected rules but does not
invent or replay a pre-restart timer without a new observed line.

Alert processing and sound remain independently disabled by default under
`User > Enable Alerts` and `User > Enable Alert Sounds`. A rule
must request sound and have passed its cooldown before the window invokes the
sound sink. The production sink uses the local desktop beep; tests replace it
with a fake callback. Multiple requests delivered in one 250 ms refresh are
coalesced to one beep. Batch sound intent is retained independently of the 128
visible recent fires, and every delivered sequence is advanced so the batch
cannot replay. No audio device is required for parser or UI tests.

## Bounds, privacy, and rollback

Rule matching shares the combat tailer's 4,096-byte line, 4,096-line refresh,
256-KiB read, rotation, replay, and active-character selection bounds. Zone
and source-identity text are capped at 128 and 64 bytes. Notification text is
derived only from the bounded rule name and zone. Runtime names and matched
text remain in memory and are never added to diagnostics, packages, or network
traffic. The settings file contains only the user-selected local pack path and
independent alert-processing and sound preferences.

Rollback removes `src/alerts/alert_engine`, the alert snapshot, Alerts dock,
and three User actions, then removes the alert fields from UI settings. It leaves
the user-owned rule pack untouched. No audio or database artifact is installed.
