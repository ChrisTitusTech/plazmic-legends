# Phase 9 progression and activity checkpoint

## Decision

Phase 9 may extend the existing bounded active-character log stream with exact
XP-percentage, AA-point-total, and loot observations. It may combine those
events only with consecutive immutable text-equipment snapshots and an
explicitly selected local EverQuest inventory-output file. The later
owner-approved optional AA memory input is scoped separately in
`phase9-aa-memory-checkpoint.md`. No other process offset, network service,
bundled item database, class catalog, or upstream asset is part of this
checkpoint.

The public behavior inventory at
`docs/research/everquest-companion-feature-parity.md` remains clean-room input.
No upstream implementation, schema, test, database, or generated content was
copied.

## Native contracts

- The existing incremental tailer supplies at most 256 KiB and 4,096 complete
  lines per refresh, with a 4,096-byte line cap and existing truncation,
  rotation, boundary, and active-character selection rules.
- Accepted progression and loot forms are exact synthetic equivalents of the
  locally observed `You gain experience! (<percent>%)`, AA point-total, and
  `--You have looted <item>.--` forms. Ambiguous receive, currency, chat, and
  malformed lines remain unclassified.
- XP rate and 100% pace use only observations from the latest hour. AA rate and
  next-point pace use the same window. Recent loot uses 24 hours.
- Named damage abilities from the active character are observation evidence.
  The UI explicitly says that proc and class identity are unconfirmed.
- Equipment changes require two consecutive immutable snapshots for the same
  character. The first snapshot is a baseline, not an upgrade event.
- Inventory import accepts only a user-selected regular non-symlink file of at
  most 2 MiB, 4,096 rows, and 4,096 bytes per row. Recognized item and key-ring
  header shapes select parsing; repeated slot labels are preserved and
  zero-count empty rows are skipped. It compares names only and never claims
  item stats or ownership beyond the imported file.

## Privacy, persistence, and lifecycle

Activity retention is independently disabled by default. When enabled, schema
1 JSON is stored under an owner-only `activity` directory and owner-only file
whose name is the stable opaque key computed from
`lowercase(active_character) + "\n" +
lowercase(log_path.filename().string())`. Only filenames whose suffix is a
bounded alphanumeric EverQuest server identity (with internal hyphen or
underscore allowed) are accepted; arbitrary `.txt` suffixes fail closed. Only
the lowercased filename is used, not the parent path. The validated filename
includes the server identity and prevents same-named characters on different
servers from sharing a partition. Each partition is capped at 512 events, 512 ability names, 4,096
ability observations, 90 days, and 2 MiB. Stable opaque line
identities preserve genuine repeated lines within one stream. Replay is
suppressed only for same-inode continuity with complete-line evidence;
replacement, rotation to a new inode, and recreation receive fresh identities.
Existing activity directories and files must already be owned by the current
user with exact modes `0700` and `0600`; unsafe retained paths fail closed
before reading, sweeping, or deletion. New and atomically replaced state is
forced to those modes.

Schema 1 also persists a bounded replay boundary containing at most 4,096
opaque source IDs and their bounded source timestamps independently of the 512
displayed events. This lets a continuity-proven stream reset recognize
qualifying lines that have already aged out of the visible event window without
retaining raw log text. A fresh tailer starts at the current end of the selected
log, so existing bytes are not replayed. It intentionally begins a fresh stream
generation because descriptor and prefix continuity do not survive process
restart; matching lines appended afterward are new observations. The oldest
boundary IDs are discarded when the bound is reached. Replay entries expire
with the same 90-day window as their source observations. Legacy string-only
replay entries migrate only when a retained event or ability supplies their
timestamp; unreferenced legacy entries are discarded.

Replay IDs and their timestamps count toward the 2 MiB partition limit. The
bounded field lengths and the 512/512/4,096/4,096 structural caps keep the
maximum serialized schema below that byte limit. Oldest events, observations,
and replay entries are pruned by their independent count and age rules before
serialization; replay entries are not sacrificed merely to admit display
rows. If serialization nevertheless exceeds 2 MiB, persistence fails closed
and leaves the prior owner-only file intact instead of applying another
eviction policy.

Schema 1 persists a monotonically advancing per-partition generation counter.
Every stream generation advances that counter and rejects any 12-hex token
already represented by retained replay metadata. Legacy files without the
counter migrate to zero before allocating and persisting the next generation,
so a restart cannot reuse a token that is still present in the retained replay
boundary. The counter remains monotonic while the partition is retained. It
does not promise lifetime uniqueness after explicit deletion, state loss, or
counter wrap; collision rejection still protects all replay metadata retained
at allocation time.

An identity contains a 12-character stream token derived from a high-resolution
clock value and process-local counter, the 32-character
lowercase opaque hash of the complete accepted bounded log line, and that
fingerprint's zero-based occurrence ordinal. Only activity lines and outgoing
named-damage lines allocate an identity. The first 4,096 distinct fingerprints
keep independent per-fingerprint ordinals. Each qualifying occurrence whose
fingerprint is beyond that distinct-fingerprint cap uses the next bounded
stream-wide overflow ordinal instead. After one tracked fingerprint has used
ordinals 0 through 4,095, or after 4,096 overflow identities have been
allocated, the next qualifying line derives a new stream token and resets the
in-memory counters before allocating its ordinal. An exhausted tracked
fingerprint never falls back to the overflow ordinal pool; it rolls the whole
stream generation first. Truncation, same-path
inode replacement, and a missing-then-recreated
log each derive a new stream token and reset those counters. The construction
makes accidental collisions unlikely in ordinary operation but does not claim
cryptographic uniqueness.
Persisted log-derived events, ability observations, and replay entries must
match that exact `12-hex:32-hex:decimal` shape. The decimal ordinal is canonical
with no leading zero except `0` itself and is limited to `0..4095`; malformed,
missing, or out-of-range identities fail the complete partition closed.

Both the partition key and line fingerprint are the first 128 bits of an
unkeyed, unsalted SHA-256 digest. There is no secret to generate, store, rotate,
or lose; the construction is intentionally deterministic so partitions and
replay boundaries remain stable across restarts. The digest removes raw names
and lines from filenames and replay metadata, but it is not anonymization. An
attacker who can read the owner-only state and enumerate candidate character
names, log filenames, or log lines can test them offline. Owner-only directory
and file permissions, rather than the digest, are the confidentiality boundary.

Replay suppression is separate from identity generation. At a boundary, the
tailer retains an open read-only descriptor for each of at most eight cached
paths. That descriptor keeps an unlinked inode from being recycled while its
continuity state is eligible for reuse. The tailer requires the newly opened
file to match that retained descriptor's device and inode, then proves replay
only from complete lines in the bounded 256 KiB common prefix of the prior and
current view. Inode replacement, recreation, and evicted cache entries have no
continuity signal, so a returning evicted path is reread from its start and
every matching initial line receives a fresh identity
instead of risking suppression of a new observation. The tracker maps
only continuity-proven overlap lines to the persisted source-ID boundary,
bootstrapping older schema-1 files from retained event and ability IDs. Exact
full-line fingerprints consume only the occurrence IDs proven by that overlap;
the proven overlap ordinals remain reserved, and allocation resumes after the
highest retained ordinal for that fingerprint. Further identical occurrences
therefore receive the current generation token with the next ordinal. No raw
line, character, item, or ability text is retained as replay metadata.

For example, two identical loot lines in one uninterrupted stream receive
`0123456789ab:0123456789abcdef0123456789abcdef:0` and
`0123456789ab:0123456789abcdef0123456789abcdef:1` and both remain. When those
two complete lines remain as the proven common prefix after same-inode
truncation, they reuse the two retained IDs and add neither again; a third
identical occurrence after that prefix receives
`fedcba987654:0123456789abcdef0123456789abcdef:2` and remains. A missing log
followed by a recreated log has no identity continuity, so its matching initial
lines and any further identical occurrence all receive fresh identities.
Without both continuity and complete-line prefix overlap, an identical line is
never guessed as replayed. Deterministic tests cover all three boundaries.
Beyond 4,096 tracked fingerprints, repeated lines receive consecutive IDs under
the stream's bounded overflow ordinal and remain distinct. In overflow mode, a
new generation is allocated after 4,096 overflow identities have been consumed.
A new generation is also allocated before the next occurrence when any tracked
fingerprint exhausts ordinals `0..4095`.

The 90-day expiry and inactive-partition sweep run on refresh attempts,
including when the selected log is missing, ambiguous, or unreadable. Each pass
processes at most eight inactive partitions and resumes in filename order one
second later, so the accepted 1,024-file directory cannot create an unbounded
single-refresh pause. Retention therefore does not depend on a successful parse
cycle.

The User menu can export the currently displayed bounded history to an
owner-only JSON file or delete the selected partition after confirmation. An
existing export target must be a regular non-symlink file; directories, FIFOs,
devices, sockets, and other special nodes fail closed. Every state-root parent
component is traversed descriptor-relatively with no-follow directory opens
before reads, writes, sweeps, or deletion; symlinked parents fail closed.
Retained activity directories and files are revalidated for current-user
ownership and exact `0700`/`0600` modes at the operation boundary. New state
uses descriptor-relative creation and atomic replacement. The privacy log uses
the same no-symlink parent-chain rule before append or rotation.
Diagnostics, packages, tests, and network traffic contain no runtime names,
activity values, inventory files, or Wine-prefix data. Synthetic fixtures use
only synthetic names and values.

Character change selects a separate partition before new log lines or
equipment are accepted. The shared lifecycle reset immediately replaces the
consumer's prior Activity value with an unavailable, empty snapshot before the
new partition can publish. Character loss and client loss do the same; recovery
remains empty until the recovered character's partition is selected and a
fresh snapshot is published. Zoning clears the displayed snapshot during the
initial client-snapshot callback, before asynchronous map loading begins.
Player and map work carry the client source, lifecycle generation, expected
character, and expected zone. Completed work is discarded unless those values
still match the current lifecycle state. The lifecycle generation advances at
the initial callback and remains empty until the selected character publishes a
fresh snapshot; retained events then return and new observations use the
recovered zone. Consumers must apply every lifecycle reset and may not continue
displaying the previous snapshot.

Combat and activity refreshes mutate a non-persistent staged tailer. Only a
result whose character and lifecycle generation still match may replace the
active tailer, re-enable the user's retention choices, persist observations,
and run inactive-partition maintenance. Shutdown adopts a completed in-flight
result through the same validation before flushing and clearing the active
tailer.

## Validation and rollback

Focused tests cover exact and rejected log forms, rate windows,
active-character ability filtering, equipment baselines and changes, inventory
reconciliation, schema rejection, owner-only save/export, switch-away and
switch-back restoration, restart restoration, deletion, and the integrated log
tailer. X11 UI tests cover the new dock, actions, tables, summary, and
independent retention preference.

Rollback removes `src/activity/activity_tracker`, the Activity dock, activity
settings, and the Phase 9 User actions while leaving the Phase 8 combat stream
and history intact. Before installing an older build, rollback preflight must
offer `User > Delete Activity History...` for every retained character. That
action is the only supported cleanup path: it traverses parent directories
descriptor-relatively without following links, requires the activity directory
and target file to be owned by the current user with exact `0700` and `0600`
modes, verifies the opened target identity immediately before `unlinkat`, and
fails closed if any check changes. Downgrade and uninstall otherwise retain the
owner-only schema-1 partitions under `activity/` so reinstalling Phase 9 can
restore them; older builds ignore that directory. Do not recursively remove the
state root or sibling `combat` directory. User-selected JSON exports are not
tracked by Plazmic and must be removed separately by the user.
