# Phase 0 cleanup report

Completed: 2026-07-29

## Outcome

Phase 0 replaced the incomplete Windows/MacroQuest import with a native Linux
research scaffold. No product runtime, live process inspection, overlay, game
state reader, or client modification was implemented.

## Recovery baseline

The complete import is recoverable at annotated tag
`phase0-import-baseline`, which points to commit `a6204d9`.

The baseline contains:

- 1,431 tracked files;
- 55,243,802 bytes of Git blobs;
- the normally ignored root plugin files, bundled Python extension modules, and
  nested IDE metadata;
- no Daybreak executable, credential file, crash dump, or local runtime log.

The baseline-wide whitespace check was not treated as a gate because inherited
and vendored files already contained extensive whitespace findings and
conflict-marker text. The exact import was preserved rather than rewritten
immediately before deletion.

## Cleanup result

Cleanup commit `f296591`:

- deleted 1,419 tracked files;
- added 2 files and modified 7 retained files;
- removed 615,570 lines and added 357 lines;
- removed all inherited implementation and bundled third-party source;
- removed every imported executable, DLL, Python extension, font, archive,
  game database, plugin, macro, and Windows build artifact;
- left 14 tracked files before this evidence report was added.

The retained tree is under 100 KiB of source and documentation. Its only
executable project code is the standard-library Python PE inspector and its
synthetic unit tests.

## Validation

The cleanup commit was checked from an isolated detached worktree, not from the
development checkout:

```bash
cmake --preset dev
cmake --build --preset check
ctest --preset dev
python3 tools/inspect_eqgame.py "$EQ_LEGENDS_DIR" \
  --expect-sha256 97ee793d491930ac97f91e5e26fac16d84d17ff24afcd24d5390d256e7045661
```

Results:

- CMake 4.3.0 and Ninja configured successfully on Fedora Linux 44.
- Markdownlint reported zero errors.
- Ruff lint and format checks passed.
- Three synthetic PE inspection tests passed.
- CTest passed one of one tests.
- The local Legends executable matched the recorded x86-64 PE metadata and
  SHA-256.
- The isolated checkout had a clean Git status after validation.
- No removed source directory, imported runtime binary, credential candidate,
  proprietary game artifact, or implementation license marker remained.

## License and provenance

The exact MacroQuest upstream revision remains unknown. The imported GPLv2 and
third-party material is available only through the recovery tag; none is
retained in the active tree. The license for future original code remained a
Phase 1 distribution decision at this checkpoint. The independently written
active project is now licensed under GPL-3.0-only.

## Residual risk and pause

- The active renderer and Wine translation path have not been observed.
- Linux process-read feasibility has not been tested.
- X11 overlay stacking and input pass-through have not been tested.
- Current game-rule risk has not been reviewed.

These belong to Phase 1. Work stops here until explicit approval to begin that
phase.
