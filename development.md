# Plazmic Legends development

Plazmic Legends completed its first release at version 0.1.2. This document is
entrypoint for contributors and anyone interested in the project's design,
history, validation, and research evidence.

For the contribution workflow, issue expectations, and pull request checklist,
see [CONTRIBUTING.md](CONTRIBUTING.md).

## Product foundation and expansion boundary

The current Plazmic Legends `main` branch is a native Linux Qt 6 application that
observes the supported 64-bit EverQuest Legends client externally through
bounded, read-only process access. It renders a normal independent window. Its
currently implemented game-installation write path is the confirmed private
UI-file installer described below.

The current `main` branch provides:

- one exact compatibility profile at a time;
- locally installed maps only;
- immutable player, zone, spawn, character, and encounter snapshots;
- an independent map, spawn table, details view, character panel, and compact
  local combat-log parser with bounded damage/healing analytics and an
  opt-in owner-only per-character history store capped at 50 encounters, 90
  days, and 2 MiB per partition;
- a tabified Activity dock with bounded XP/AA pace, loot, equipment-change,
  observed-ability, celebration, and explicit local inventory-output views,
  plus an independent opt-in owner-only activity store;
- Linux, Wine, and X11/XWayland support.

This implementation is a foundation, not a permanent product-category limit.
`SPEC.md` and `ROADMAP.md` define capability-scoped phases for analytics,
activity, alerts, audio, maps, knowledge packs, overlays, profiles, sharing,
services, and future integration approaches. Each new trust boundary requires
its own approval, validation, privacy, security, provenance, and rollback
contract.

An unknown or partially matched executable must fail closed. Client-specific
addresses and signatures belong only in versioned compatibility profiles.

## Build requirements

The supported build uses Linux, CMake 3.28 or newer, Ninja, GCC or Clang with
C++20, Qt 6.8 or newer, libX11 development files, Python 3.11 or newer, Ruff,
markdownlint-cli2, and Xvfb.

Configure, build, lint, and test with:

```bash
cmake --preset dev
cmake --build --preset dev
cmake --build --preset check
ctest --preset dev --output-on-failure
```

All configured C++ targets build with warnings as errors and common hardening
flags. Tests use synthetic fixtures and do not require a live account.

## Inspect, install, and run a local client

Set the path without committing it:

```bash
export EQ_LEGENDS_DIR="/path/to/Installed Games/EverQuest Legends"
```

Inspect the executable without launching or modifying it:

```bash
python3 tools/inspect_eqgame.py "$EQ_LEGENDS_DIR"
```

The current supported profile can be verified explicitly:

```bash
python3 tools/inspect_eqgame.py "$EQ_LEGENDS_DIR" \
  --expect-sha256 3451069e63d5118a703a237f121a3ea7c20c973477a69fdd0d66bcdaa7d80b29
```

After the automated gate passes, manual, X11, and live-client testing must
atomically replace the shell-resolved unmanaged local installation with the
exact validated build. Preserve the prior installed binary under
`build/local-install-backup/`, verify the staged and installed SHA-256 values,
and restore `root:root` ownership with mode `0755`. On the reference host, the
installed target is `/usr/local/bin/plazmic-legends`.

Do not treat a direct `build/dev/plazmic-legends` launch as local manual-test
evidence. If a companion is already running, inspect `/proc/<pid>/exe` and
relaunch it before attributing behavior to the replacement; do not terminate it
without explicit authorization.

Run the installed development build:

```bash
plazmic-legends --client "$EQ_LEGENDS_DIR/eqgame.exe"
```

The first valid command-line or `EQ_LEGENDS_DIR` selection is saved as
`[client].game_directory` in the XDG configuration file. With neither input,
the launcher first reuses that saved directory, then performs a one-second,
depth- and count-bounded scan below `$HOME` for the exact
`Daybreak Game Company/Installed Games/EverQuest Legends` structure. A valid
environment or saved path skips the scan, and multiple discovered
installations fail closed.

A changed SHA-256 requires a new immutable profile and a complete validation
cycle. It never authorizes changing old offsets in place.

## Source layout

| Path | Responsibility |
| --- | --- |
| `src/common` | Fingerprinting, logging, and narrow shared utilities |
| `src/integration` | Linux process discovery, mappings, and read-only access |
| `src/game` | Compatibility profiles and bounded game-state readers |
| `src/model` | Immutable status, player, spawn, character, and encounter snapshots |
| `src/launcher` | Compatibility and lifecycle coordination |
| `src/map` | Map parsing, transforms, and renderer-independent state |
| `src/ui` | Qt window, map canvas, spawn table, theme, and settings |
| `tools` | Offline client inspection and private UI-bundle export |
| `tests` | Synthetic lifecycle, parser, UI, privacy, and performance gates |
| `packaging` | RPM, AppImage, desktop metadata, icon, and dependency notices |

The removed MacroQuest import is recoverable from the
`phase0-import-baseline` tag for provenance research only. No MacroQuest or
ShowEQ implementation, bundled dependency, packet decoder, map, or game asset
is retained in the active product.

## Export and install a private 1440p UI bundle

Export the locally installed skin and current on-disk settings:

```bash
tools/export_private_ui_bundle.sh \
  --game-dir "$EQ_LEGENDS_DIR" \
  --resolution 2560x1440
```

This creates ignored `private-bundles/plazmic-ui-2560x1440/` and `.tar.gz`
outputs. They contain Daybreak assets and private character/server filenames;
keep them private and never commit, publish, log, or attach them to a PR.
The bundle also contains a generic `UI_plazmic_1440p.ini` derived from the
newest local Legends layout. It preserves Legends-only sections and adjusts
only an allowlisted geometry set; EQInterface layouts are design references,
not compatible source files or copied assets. See the
[EQInterface layout review](docs/research/eqinterface-layout-review.md) for the
adopted design rules and compatibility boundary.

On another system, extract the archive, start Plazmic Legends with its local
game directory configured, and choose `User > UI File Install...`. The dialog
asks which bundled layout and character/filter profiles to read and which
current INIs to replace. Global `eqclient.ini` filters and 1440p settings are
optional. The installer verifies the bundle SHA-256 inventory and saves
replaced files in a private timestamped rollback directory before activation.
It also writes the selected layout to the reserved
`UI_plazmic_1440p.ini` source, preserving an older copy in the same rollback
directory. When EverQuest is already running, open `/copylayout`, select that
source (shown as `plazmic` on `1440p` in some clients), and copy the window
layout. Then use `/loadskin plazmic-ui 1` to reload the skin while retaining
the imported geometry. Reopen `/copylayout` if it was already open during the
install so EverQuest refreshes the source list.

## Packaging

The CMake install boundary is shared by the Fedora RPM and AppImage. Package
operations and the support matrix are documented in
[docs/package-operations.md](docs/package-operations.md).

Build the AppImage in its pinned Ubuntu 22.04 container:

```bash
packaging/build-appimage.sh
```

Build a source RPM through the COPR entrypoint:

```bash
make -f .copr/Makefile srpm \
  spec=packaging/plazmic-legends.spec \
  outdir="$PWD/build/srpm"
```

Release artifacts must come from an audited exact commit. Inspect the complete
package inventory and verify generated checksums before publication. Packages
must never contain Daybreak files, installed maps, Wine-prefix content,
credentials, settings, logs, memory captures, or account data.

## Validation expectations

Run focused tests while changing code and the complete repository gate before
publishing. A live validation is not complete merely because process attachment
worked. Applicable changes must also verify:

- the independent window and stable X11 class;
- map, player, and spawn presentation;
- zoning, camping, character select, and process exit;
- unsupported and changed-client behavior;
- privacy-safe logging;
- DWM placement without activating the game tag;
- package inventory and dependency licenses; and
- installed, running, and source versions as separate states.

Record skipped checks and remaining risk. Do not weaken a compatibility check
to make a patched client appear supported.

## Project documentation

- [Specification](SPEC.md) defines behavior, current exclusions, and acceptance
  criteria.
- [Roadmap](ROADMAP.md) records the phase order, exit criteria, and rollback.
- [Tasks](TASKS.md) records implementation and validation status.
- [Agent instructions](AGENTS.md) define repository working boundaries.
- [Phase 5 product boundary](docs/phase5-product-boundary.md) maps retained
  source, dependencies, and package contents to requirements.
- [Compatibility profile refresh](docs/profile-refresh.md) documents the
  fail-closed client-update workflow.
- [Client offset discovery](docs/offset-discovery.md) documents how to
  re-establish profile RVAs, record fields, bounds, and live evidence after a
  client patch.
- [Package operations](docs/package-operations.md) covers installation,
  upgrades, removal, troubleshooting, and rollback.
- [Cleanup inventory](docs/cleanup-inventory.md) and the
  [Phase 0 report](docs/phase0-cleanup-report.md) record the import cleanup.
- [Legends baseline](docs/research/legends-baseline.md) and the
  [runtime baseline](docs/research/phase1-runtime-baseline.md) record the
  reference Linux, Wine, renderer, process, and client evidence.
- [Architecture proof](docs/phase1-architecture-proof.md) records the native
  process-access boundary and rejected overlay experiment.
- [Phase 2](docs/phase2-completion-report.md),
  [Phase 3](docs/phase3-completion-report.md), and
  [Phase 4](docs/phase4-completion-report.md) completion reports record the
  product milestones.
- [Phase 3 map baseline](docs/research/phase3-map-baseline.md),
  [player/map checkpoint](docs/research/phase3-player-map-checkpoint.md), and
  [Phase 4 spawn checkpoint](docs/research/phase4-spawn-checkpoint.md) record
  renderer and live-state evidence.
- [Phase 6 character/combat checkpoint](docs/research/phase6-character-combat-checkpoint.md)
  records the bounded memory and local log-parser evidence.
- [Policy-risk decision](docs/research/phase1-policy-risk.md),
  [MacroQuest boundary](docs/research/macroquest-boundary-review.md),
  [ShowEQ review](docs/research/showeq-ui-review.md), and
  [import provenance](docs/research/import-provenance.md) record policy,
  licensing, and clean-room boundaries.

## EULA and safety decision

This project knowingly operates against the Daybreak EULA and related
published rules. The owner explicitly accepts that risk. This decision does not
grant permission, prevent account action, or imply affiliation with Daybreak.

Read-only symbol and gameplay-state research may proceed within the documented
project scope. Later write, injection, automation, service, or control
capabilities require explicit approval under their own numbered phase.
Protection bypass, authentication bypass, guessed offsets, silent private-data
collection, and incompatible or proprietary content remain outside the project
boundary.

## License

Original Plazmic Legends code is licensed under the
[GNU General Public License v3.0 only](LICENSE). The historical MacroQuest
import was GPLv2 and remains isolated at `phase0-import-baseline`; that
historical license does not replace the active project's GPL-3.0-only terms.
Third-party dependencies and packaging tools retain their own licenses as
listed in [packaging/THIRD-PARTY-NOTICES.md](packaging/THIRD-PARTY-NOTICES.md).
