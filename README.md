# Plazmic Legends

Plazmic Legends is planned as a minimal, read-only information overlay for the
64-bit EverQuest Legends `eqgame.exe` running under Wine on Linux.

Status: Phase 0 cleanup is complete. The active tree is a minimal Linux
research scaffold; no live process integration or product runtime has been
approved or implemented.

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
- [Import provenance](docs/research/import-provenance.md) records the recovery
  tag and retained-code audit.

## Validate the scaffold

```bash
cmake --preset dev
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

## Platform

- Build host: Linux only.
- Initial runtime: Fedora Linux, X11, and Wine.
- Native Windows and Visual Studio builds: not supported.
- Wayland: deferred beyond the MVP.

Phase 1 will validate a least-privilege native Linux reader and external X11
overlay. Project artifacts are native Linux ELF files; the project will not
inject a DLL or modify the Wine prefix.

## Scope

The proposed MVP shows compatibility status, zone, player position and heading,
and limited target information. It excludes scripting, plugins, automation,
gameplay input, remote control, and traditional EverQuest clients. Exact fields
and process integration remain Phase 1 approval decisions.

## Provenance and license

The removed import was derived from MacroQuest and is preserved at
`phase0-import-baseline`. No MacroQuest implementation or bundled dependency is
retained in the active tree. Future code licensing is unresolved and must be
decided before distribution. EverQuest and its assets are the property of
their respective owners and are not part of this repository.
