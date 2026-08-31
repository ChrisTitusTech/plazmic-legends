# Phase 10 Mote loot audio checkpoint

Status: Authorized implementation slice on 2026-08-30. This checkpoint covers
only the local system-sound alert for newly observed Mote loot. Buff timers,
respawn timers, visible notifications, configurable general rules, custom
sounds, voice packs, and imported duration packs remain outside this slice.

## Capability contract

- Input: immutable bounded Phase 9 activity snapshots produced from the active
  character's selected local EverQuest log. Exact personal-loot lines and the
  observed `<actor> looted a/an/the <item>` group-loot form may produce those
  snapshots. Only loot event kind, item label, stable source identity,
  availability, and opaque storage-partition key are consumed.
- Match: `Mote` must appear as a complete ASCII word, case-insensitively. A
  substring such as `Remote`, `Motet`, or `Mote_One` does not match.
- Output and side effect: when independently enabled, one match launches the
  fixed `/usr/bin/paplay` command for the fixed freedesktop `bell.oga` system
  sound without item or character data. Qt's local system bell remains the
  fallback when the executable or sound is unavailable or the command cannot
  start. A matching Activity event row receives a yellow background with black
  text. The capability performs no file write beyond the existing owner-only
  UI setting and performs no process access or network request.
- Default and control: the owner explicitly approved default-on behavior on
  2026-08-30. The User-menu action remains independently available for
  immediate opt-out, and an explicit saved `false` value is restored.
- Retention: the setting is a boolean in the existing bounded owner-only XDG
  configuration. Runtime source identities are held only in bounded memory;
  item, character, zone, and server names are not newly persisted or logged.
- Bounds: at most 1,024 stable source identities are tracked. The Phase 9 input
  remains capped at 512 events. Matching is linear in that bounded snapshot,
  and dispatch is coalesced to at most one sound every two seconds.
- Lifecycle: the first available snapshot and every changed opaque partition
  establish a silent baseline. Unavailable activity resets the baseline. An
  empty source identity never dispatches. Disabling the setting clears a
  pending coalesced alert, so observations made while disabled are not replayed
  after opt-in.
- Privacy and diagnostics: matching and dispatch add no diagnostic fields and
  expose no item label, source identity, storage key, character, server, zone,
  or log path outside the existing in-process snapshot boundary.
- Dependency and provenance: Fedora packages require `pulseaudio-utils` and
  `sound-theme-freedesktop`, both already installed on the validated host. The
  sound remains a distribution-owned system asset and is not linked or bundled
  into the application. No third-party code, voice pack, or game asset is
  bundled. Removing either runtime dependency restores the Qt-bell-only
  behavior.
- Failure behavior: unavailable or unidentified events fail silent. Failure to
  start the desktop player falls back to `QApplication::beep()`. A muted
  desktop audio sink may still produce no audible result; the setting remains
  enabled and the application remains responsive.

## Acceptance and validation

- Unit tests use an injected fake sink and deterministic clock to cover valid
  and rejected word forms, restored-history suppression, exact-once event
  deduplication, two-second coalescing, opt-out behavior, partition switching,
  unavailable recovery, and missing source identity.
- UI tests cover the default-on accessible User-menu action, immediate
  owner-only persistence, fake-sink dispatch from a new immutable loot event,
  restored preference, and readable yellow Mote-row styling without coloring a
  nonmatching row.
- The complete warnings-as-errors build, repository checks, CTest suite,
  privacy inspection, package inventory, and diff checks must pass.
- Installed validation must confirm the action starts enabled, verify its
  immediate opt-out, confirm an audible system alert for a newly logged
  synthetic-or-controlled Mote loot event, confirm no sound for nonmatching
  loot and existing history, and verify that the game window, input, focus,
  and performance remain unchanged.

## Rollback and pause point

Remove `src/alerts`, the User-menu setting, and their tests. Existing activity
snapshots and retained history require no migration. Pause expansion if the
system alert is noisy, dispatch cannot remain bounded, lifecycle replay occurs,
or a future custom sound lacks compatible provenance.

## Validation evidence

On 2026-08-30, `cmake --build --preset check -j2` passed the warnings-as-errors
build, Markdown lint, Ruff checks, Python compilation, and 13 Python tests.
`ctest --preset dev --output-on-failure` passed all 21 tests, including the
fake-sink alert unit test and X11 main-window integration test. RPM spec,
desktop file, and AppStream metadata validation passed; the staged install
matched the approved ten-file package inventory. The exact installed August 25
client fingerprint matched its immutable profile, and `git diff --check`
passed.

CodeRabbit iteratively reviewed the complete tracked and new-file diff. Valid
findings led to explicit locale-independent ASCII matching and a bounded
source-ID rollover/recovery test. Its repeated source-boundary finding was
rejected because `AGENTS.md` explicitly reserves `src/alerts` for local alert
rules, dispatch, and optional audio. Its claimed missing production Qt binding
was rejected after verifying the saved User-menu action, `QApplication::beep()`
sink, immutable snapshot call, and injected-sink X11 integration test.

Installed audible output and live game-invariance validation are skipped in
this worktree because they require atomically replacing and relaunching the
currently installed companion. No running process was disrupted. Exact-head
CI and pull-request review-thread validation also remain pending.

The owner subsequently approved default-on behavior on 2026-08-30. Focused
settings, alert, and X11 UI tests; the complete repository gate; all 21 CTest
cases; package metadata validation; `git diff --check`; and iterative review
passed after the change. The exact build was atomically installed at
`/usr/local/bin/plazmic-legends` with SHA-256
`2a7406935f3688fa227df133ddc179b2d49b1f9dbbb2f96c3cdb97b5111828ec`,
`root:root` ownership, mode `0755`, and a hash-addressed rollback of the prior
Phase 10 binary. The owner-only configuration already records the alert as
enabled. The open companion was not disrupted and still maps the prior Phase
10 binary, so relaunch, exact-running-hash, and audible live validation remain
pending.

The owner's next live observation exposed two acceptance failures: three
recent privacy-redacted Mote entries used the exact
`<actor> looted a/an/the <item>` form rather than the personal-loot form, and
the Qt/X11 bell did not provide audible desktop output. The parser now accepts
that bounded group-loot form while rejecting chat-like or malformed variants.
Production dispatch now starts the fixed Fedora desktop event-sound player and
falls back to the Qt bell only when it cannot start. A direct desktop event
sound completed successfully against the active PipeWire session.

After the repair, the complete warnings-as-errors repository gate, 13 Python
tests, all 21 CTest cases, RPM lint, desktop-file validation, AppStream
validation, and `git diff --check` passed. CodeRabbit reported zero findings on
the tracked repair. The exact repaired build was atomically installed with
SHA-256 `43352a6c0651e26436651ff1f6753402346b4ca120c27cf020e5a6706207529c`,
`root:root` ownership, and mode `0755`; the prior installed hash remains in the
local rollback directory. PID 1673944 still maps the older
`d15882148824717030f8e873ace73f0fc2856ae2b2dc7749fcb8c33b803c3c33`
image, so exact-running-hash and new live-loot audible validation remain
pending until the owner relaunches the companion again.

The subsequent August 25 XP/AA restoration produced a combined installed
binary with SHA-256
`7a94afee87809ee572801aac40da9733c76c50f0f7a589395221f5e4954362ac`.
The complete gate and review remained green, and the desktop-sound dependency
and default-on setting are unchanged. PID 649444 still maps the preceding
`43352a6c0651e26436651ff1f6753402346b4ca120c27cf020e5a6706207529c`
image, so both corrected capabilities require one normal companion relaunch
before visible live acceptance.

A second live Mote observation on 2026-08-30 again produced no audible result.
Privacy-redacted inspection confirmed the two newest entries were exact group
loot forms, arrived after process startup, and the owner-only setting remained
enabled. The low-impact themed message event was replaced with the fixed
freedesktop bell file through `paplay`; that exact command completed
successfully against the active audio session. Matching Activity rows now use
a yellow background with black text as a simultaneous visual alert.

The focused tests, warnings-as-errors repository gate, 13 Python tests, all 21
CTest cases, exact-client fingerprint, RPM lint, desktop-file validation,
AppStream validation, `git diff --check`, and tracked-diff CodeRabbit review
passed. The fixed-path alert module also passed manual input and process-launch
review. The build was atomically installed with SHA-256
`cf073b61ecf41f61c3e1363bc4ac2643c2432c1e6edcc52c31211a1721ece22e`,
`root:root` ownership, and mode `0755`; the previous installed hash remains in
the local rollback directory. PID 649444 still maps the older
`43352a6c0651e26436651ff1f6753402346b4ca120c27cf020e5a6706207529c`
binary, so the new bell and yellow row require a normal relaunch and another
live acceptance observation.

The owner relaunched the exact installed
`cf073b61ecf41f61c3e1363bc4ac2643c2432c1e6edcc52c31211a1721ece22e`
binary and confirmed on 2026-08-30 that everything works. This closes the live
acceptance check for the audible Mote bell, default-on setting, exact group-loot
match, and readable yellow Activity row. The running and installed hashes
matched during final publication preparation.

Independent final review then found three defensive lifecycle edges involving
retained-history loading, preservation of the two-second deadline across an
unavailable snapshot, and rapid opt-out/opt-in between refreshes. The fixes
silently rebaseline those transitions and preserve the dispatch deadline. Their
focused regression coverage, the complete repository gate, all 21 CTest cases,
`git diff --check`, CodeRabbit, and final independent review pass. They do not
change the accepted ordinary group-loot alert path, but the resulting final
source head was not reinstalled for another live observation before
publication; the hash above remains the exact live-acceptance evidence.
