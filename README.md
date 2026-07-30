# Plazmic Legends

Plazmic Legends is planned as a minimal, read-only companion application for
the 64-bit EverQuest Legends `eqgame.exe` running under Wine on Linux. It uses
an independent Linux window for maps, spawns, and diagnostics rather than
drawing over the game.

Status: Phase 0 through Phase 2 are complete and squash-merged locally. The
first Phase 3 checkpoint adds installed-map rendering and an exact-profile,
read-only live zone/player marker to the independent Qt 6 companion on DWM tag
5. Map orientation, player position, and heading have been manually verified
against the in-game map. Player-follow and lifecycle-transition validation
remain active Phase 3 work.

## Development and EULA notice

This private development project knowingly operates against the Daybreak EULA
and related published rules. The owner explicitly accepts that risk. This does
not grant permission, prevent account action, or imply affiliation with
Daybreak.

Development remains read-only and external. Memory writes, injection, client
patching, protection bypass, gameplay automation, credentials, and public
distribution are outside the authorized scope.

## Project documents

- [Specification](SPEC.md) defines behavior, non-goals, and acceptance criteria.
- [Roadmap](ROADMAP.md) starts with cleanup and defines validation, rollback,
  and approval checkpoints.
- [Tasks](TASKS.md) identifies the current authorized work.
- [Agent instructions](AGENTS.md) define repository boundaries and commands.
- [Cleanup inventory](docs/cleanup-inventory.md) classifies the imported tree.
- [Phase 0 report](docs/phase0-cleanup-report.md) records cleanup and validation
  evidence.
- [Legends baseline](docs/research/legends-baseline.md) records the reference
  Linux, Wine, and executable evidence.
- [Phase 1 runtime baseline](docs/research/phase1-runtime-baseline.md) records
  the live Lutris, Proton, process, renderer, and X11 evidence.
- [Phase 1 architecture proof](docs/phase1-architecture-proof.md) records the
  implemented safety boundary and remaining manual checks.
- [Phase 1 Codex review](docs/phase1-codex-review.md) records the independent
  implementation review and resolved findings.
- [Phase 2 completion report](docs/phase2-completion-report.md) records the
  standalone UI, DWM, packaging, and game-invariance evidence.
- [Phase 2 dependency audit](docs/phase2-dependency-audit.md) records the Qt,
  X11, removal, and ShowEQ provenance boundaries.
- [Phase 3 map baseline](docs/research/phase3-map-baseline.md) records the
  installed-map inventory, parser bounds, and content-free validation evidence.
- [Phase 3 map/player checkpoint](docs/research/phase3-player-map-checkpoint.md)
  records the exact-profile reader, renderer, controlled manual validation,
  and remaining Phase 3 risks.
- [EULA-risk decision](docs/research/phase1-policy-risk.md) records the
  explicit decision to continue private read-only development despite the
  published restrictions.
- [MacroQuest boundary review](docs/research/macroquest-boundary-review.md)
  records the safe staged-read concept retained and the injection and
  automation paths rejected.
- [ShowEQ UI review](docs/research/showeq-ui-review.md) records the standalone
  map/spawn concepts retained and the packet-capture and GPL implementation
  rejected.
- [Import provenance](docs/research/import-provenance.md) records the recovery
  tag and retained-code audit.

## Validate the scaffold

```bash
cmake --preset dev
cmake --build --preset dev
cmake --build --preset check
ctest --preset dev
```

## Inspect a local Legends client

The repository does not contain or redistribute Daybreak binaries. Point the
read-only inspection tool at a local installation:

```bash
export EQ_LEGENDS_DIR='/path/to/Installed Games/EverQuest Legends'
python3 tools/inspect_eqgame.py "$EQ_LEGENDS_DIR"
```

This reports executable architecture, PE timestamp, image metadata, and
SHA-256 without launching or modifying the game.

## Run the Phase 1 diagnostics proof

With the exact supported client already running:

```bash
build/dev/plazmic-legends-proof \
  --client "$EQ_LEGENDS_DIR/eqgame.exe" \
  --diagnose-only
```

The proof refuses unsupported hashes, missing clients, and ambiguous
processes. Its external X11 panel mode remains research evidence only and is
not the planned product interface.

## Run the companion

With the exact supported client installed:

```bash
build/dev/plazmic-legends \
  --client "$EQ_LEGENDS_DIR/eqgame.exe"
```

The application stores geometry and dock layout in
`$XDG_CONFIG_HOME/plazmic-legends/config.toml`, or the equivalent Qt user
configuration path when `XDG_CONFIG_HOME` is unset. Use `--reset-layout` to
ignore saved placement for one run.

The reference DWM configuration assigns class `PlazmicLegends` to human tag 5
on monitor 1. Other window managers may place the normal application window
according to their own rules.

With the exact supported client in-world, the companion validates the live
zone short name, loads that zone from the installation's `maps` directory, and
displays the player position and heading. Unknown client builds and invalid
live values fail closed. A player-relative height filter is enabled by default
to separate vertical map floors. Right-click the map to toggle the filter,
show all elevations, or independently adjust how far below and above the
player's Z axis the map remains visible. The filter state and ranges persist
with the window settings. The same menu can enable player-follow; manual
panning or fitting the full map disables follow mode, and the choice persists
across launches.

The window follows the active system dark or light preference. On the reference
DWM session it reads `dwm-titus/themes.toml`; on other desktops it falls back
to the standard appearance portal and Qt color-scheme hint. Changes are applied
while the application remains open.

## Platform

- Build host: Linux only.
- Initial runtime: Fedora Linux, X11, and Wine.
- Native Windows and Visual Studio builds: not supported.
- Wayland: deferred beyond the MVP.

The Phase 1 proof and Phase 2 product use native Linux code. The normal Qt 6
Widgets companion window is placed on DWM tag 5 (HDMI-0) by its stable window
class. Project artifacts are native Linux ELF files and do not inject a DLL or
modify the Wine prefix.

## Scope

The proposed MVP shows compatibility status, a zone map, player position and
heading, a filtered spawn list, and selected-spawn details. It excludes
scripting, plugins, automation, gameplay input, remote control, and traditional
EverQuest clients. Phase 2 builds the standalone tag 5 shell, Phase 3 adds local
maps and validated player state, and Phase 4 adds the bounded spawn view.

Static geometry will be parsed read-only from the user's installed Legends
`maps` directory. Dynamic zone, player, and spawn values will come from
exact-profile bounded external reads converted into immutable snapshots.
Plazmic does not use ShowEQ packet capture and does not package game maps.

## Provenance and license

The removed import was derived from MacroQuest and is preserved at
`phase0-import-baseline`. No MacroQuest implementation or bundled dependency is
retained in the active tree. Future code licensing is unresolved and must be
decided before distribution. EverQuest and its assets are the property of
their respective owners and are not part of this repository.
