# Plazmic Legends

Plazmic Legends is planned as a minimal, read-only information overlay for the
64-bit EverQuest Legends `eqgame.exe` running under Wine on Linux.

Status: planning is complete and Phase 0 cleanup has not started. The imported
MacroQuest-derived source is not currently a buildable Plazmic Legends product,
and no live process integration has been approved or implemented.

## Project documents

- [Specification](SPEC.md) defines behavior, non-goals, and acceptance criteria.
- [Roadmap](ROADMAP.md) starts with cleanup and defines validation, rollback,
  and approval checkpoints.
- [Tasks](TASKS.md) identifies the current authorized work.
- [Agent instructions](AGENTS.md) define repository boundaries and commands.
- [Cleanup inventory](docs/cleanup-inventory.md) classifies the imported tree.
- [Legends baseline](docs/research/legends-baseline.md) records the reference
  Linux, Wine, and executable evidence.

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

The initial import is derived from MacroQuest and includes GPLv2 code and
third-party components. Retained code must preserve its notices and remain
compatible with [LICENSE.md](LICENSE.md). EverQuest and its assets are the
property of their respective owners and are not part of this repository.
