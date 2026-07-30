# Phase 0 cleanup record

Phase 0 reduced the active tree from the complete imported baseline at
`phase0-import-baseline` to a Linux-native research scaffold. Git retains every
removed file; the active branch does not carry unused implementation forward.

## Removed paths

The following tracked paths were deleted in full:

- `LICENSE.md`;
- `batch-convert.ps1`;
- `cmake/`;
- `contrib/`;
- `data/`;
- `extras/`;
- `gen_solution.ps1`;
- `include/`;
- `plugins/`;
- `src/`;
- `tools/build_scripts/`;
- `tools/comment-update/`;
- `tools/conversions/`;
- `tools/mkplugin/`;
- `tools/python/`.

This removes the inherited Visual Studio and MSVC build, vcpkg overlays,
PowerShell conversion tools, bundled Windows Python, Windows process injection,
Detours, DirectX backends, MacroQuest runtime, plugins, macro engine, login
automation, messaging, editor, crash upload, emulator variants, game databases,
and bundled third-party source.

`LICENSE.md` governed the removed GPLv2 import. No inherited implementation
remains in the active tree. Its license and all dependency notices remain
recoverable from the baseline tag. The new project's license is unresolved.

## Replaced paths

- `CMakeLists.txt` is now a dependency-free Linux research-tool project.
- `CMakePresets.json` now configures Ninja on Linux.
- `README.md` describes Plazmic Legends rather than MacroQuest.
- The former implementation is represented by explicit future source
  boundaries in `AGENTS.md`; Phase 0 does not create placeholder product code.

## Retained paths and purpose

| Path | Phase 1 purpose |
| --- | --- |
| `.gitignore` | Exclude Linux build, cache, diagnostic, and private output. |
| `.markdownlint-cli2.jsonc` | Enforce project documentation formatting. |
| `AGENTS.md` | Define execution and safety boundaries. |
| `CMakeLists.txt` | Provide the Linux configure and validation entrypoint. |
| `CMakePresets.json` | Make the Linux/Ninja gate reproducible. |
| `README.md` | Explain project state, scope, and commands. |
| `SPEC.md` | Define observable product requirements and non-goals. |
| `ROADMAP.md` | Define phase ordering, exit criteria, and pause points. |
| `TASKS.md` | Track authorized work and validation status. |
| `docs/cleanup-inventory.md` | Record the exact cleanup scope. |
| `docs/research/import-provenance.md` | Record source recovery and license audit. |
| `docs/research/legends-baseline.md` | Record the Linux/Wine/client baseline. |
| `tools/inspect_eqgame.py` | Fingerprint the target PE without launching it. |
| `tests/test_inspect_eqgame.py` | Validate PE inspection without game content. |

No imported source, binary dependency, game asset, offset, or account data is
retained.

## Recovery

Inspect or restore removed material without changing the active branch:

```bash
git show phase0-import-baseline:path/to/file
git worktree add /tmp/plazmic-import phase0-import-baseline
```

Restoring source to the active tree requires a requirement mapping, provenance
and license audit, and evidence that reuse is smaller than a native
replacement.
