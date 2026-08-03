# Phase 6 character and combat checkpoint

Date: 2026-08-03

Client profile: `legends-2026-07-29`

Client SHA-256:
`97ee793d491930ac97f91e5e26fac16d84d17ff24afcd24d5390d256e7045661`

This checkpoint records privacy-safe evidence for the Character and Parse
docks. No runtime name, target, item value, damage value, process address, log
path, memory content, screenshot, or Wine-prefix content is retained here.

## Product boundary

The feature remains read-only and local:

- character identity, vitals, and equipped item text use bounded same-user
  `ProcessMemoryReader` reads selected by the immutable exact client profile;
- combat damage uses only newly appended bytes from one active-character local
  log, outside the process reader;
- the UI receives immutable value snapshots, never target addresses;
- no write API, injection, packet capture, input, automation, upload, account,
  network service, database, saved encounter history, or game asset was added;
  and
- diagnostics retain only existing state and error categories.

## Independently established character fields

Static analysis followed the exact client executable's character-window,
gauge, profile-manager, and inventory access paths. Public projects were used
only to identify concepts and plausible line forms. No external offset,
structure, signature, parser implementation, generated data, or asset was
copied.

The exact profile centralizes these bounded values:

| Value | Exact-client evidence and bound |
| --- | --- |
| Character identity | The validated local-player record supplies a printable, terminated name bounded to 64 bytes. It is used only in the UI and to select one matching log. |
| Current HP | A signed 64-bit current-health field plus the signed 32-bit active-zone adjustment used by the exact client's gauge path. The checked result is limited to 0 through 100,000,000. |
| Maximum HP | Omitted. The exact client's gauge path reaches a dynamic calculation; no separate raw or stable cached value passed the evidence gate. |
| Current mana | A signed 32-bit stats value used by the exact client's mana path, bounded to the validated maximum. |
| Maximum mana | A signed 64-bit local-player value independently used by the mana presentation path; it must be nonnegative and no greater than 100,000,000. Zero is accepted only with zero current mana. |
| Equipment | The current profile manager selects one bounded inventory container. Its count must be 23 through 36 and only the first 23 known equipment slots are read. |
| Equipped item text | Empty pointers remain empty. Occupied entries use one bounded item pointer and one printable, terminated name of at most 64 bytes. |

Every pointer addition is overflow checked, list traversal is capped, string
decoding is bounded, invalid values reject the complete character snapshot,
and the character root is reread before publication. Player lifecycle failure,
zoning, character select, camping, process exit, or reacquisition clears the
character snapshot instead of retaining partial or stale values.

Two separate live application runs against the exact client confirmed current
HP, current and maximum mana, the 23 slot labels, empty-slot handling, and all
displayed occupied item text against the game UI. The observations were made
visually and were not recorded with their private values. Maximum HP remained
absent in both runs as required.

## Combat-log parser boundary

The parser accepts English timestamped outgoing direct melee, spell,
damage-over-time, frenzy, and pet damage forms. Incoming damage and unknown or
malformed lines are ignored. It enforces these limits:

- 4,096 bytes per line;
- 256 KiB read per refresh;
- 4,096 completed lines per refresh;
- 4,096 directory entries while selecting the active log;
- 256 encounter participants; and
- a 10-second inactivity boundary.

The tailer chooses exactly one case-insensitive
`eqlog_<active-character>_*.txt` regular file below `Logs` or `logs`, starts at
its current end, and handles partial lines, append, truncation, replacement,
and disappearance without blocking the Qt UI thread. Missing or ambiguous
logs fail closed. Participant names exist only in the transient snapshot used
by the local UI.

An encounter is anchored by the active character's first outgoing event and
then accepts participant damage only against that same defender until the
inactivity boundary. This prevents nearby incoming combat and quoted chat from
inflating the parse without requiring unapproved actor-type memory. As a
deliberate compact-parser limitation, simultaneous damage against secondary
targets is not included in that single-target encounter.

With EverQuest logging enabled, a fresh live run observed newly appended
outgoing damage populate the current encounter, Parse table, and Character
dock DPS. The parser then retained the most recent encounter after inactivity.
The evidence recorded only pass/fail state; no log content or private value was
copied.

Public behavioral references consulted:

- Loadout Legends Parsing Tools, viewed 2026-08-03:
  <https://www.loadoutlegends.com/parsing-tools>
- EQL Meter website and MIT-licensed source, commit
  `69b7fbb7fc65ba4dfd72783398c42492e595d919`, viewed 2026-08-03:
  <https://eqlmeter.com/> and <https://github.com/kpxcoolx/eql-meter>

These references influenced only the compact presentation fields and the
synthetic coverage categories. The implementation is independent and adds no
third-party source or runtime dependency, so package license and removal
impact are unchanged.

## Validation status

The following validation passed on 2026-08-03:

- configure, warnings-as-errors build, repository checks, and all 16 CTest
  cases;
- focused bounded character-reader, parser, player-lifecycle, performance, and
  Xvfb UI coverage;
- exact-client fingerprint verification against the profile SHA-256;
- complete tracked-and-untracked source-manifest comparison;
- staged native ELF install, version, inventory, and private-path or synthetic
  fixture string inspection;
- iterative local review followed by independent whole-diff reviews, with
  actionable findings fixed and regression tested;
- live character rendering across two controlled observations and a newly
  appended real encounter through completion; and
- live X11 instance/class and absence of an always-on-top state.

No screenshot or raw live log evidence is retained because it would disclose
private runtime data. The exact-client zoning, camping, process-exit, and
reacquisition scenarios were not exercised in this work session. Synthetic
lifecycle tests cover their invalidation states, but the pull request remains
draft and Phase 6 remains incomplete until those manual transitions pass.
