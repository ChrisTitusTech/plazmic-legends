# Phase 0 cleanup inventory

This is the planning-level inventory for the imported tree. Phase 0 must expand
it to exact paths after creating the recoverable baseline commit. A category is
not permission to delete files before that commit.

## Remove

These capabilities are outside the Linux/Wine MVP:

| Area | Initial paths | Reason |
| --- | --- | --- |
| Visual Studio metadata | `src/**/*.sln`, `src/**/*.vcxproj*`, `src/**/*.props`, `tools/**/*.vcxproj*` | Windows-hosted builds are a non-goal. |
| Windows build conversion | `batch-convert.ps1`, `gen_solution.ps1`, `cmake/`, `tools/build_scripts/`, `tools/conversions/` | The target build is Linux-native and will not generate Visual Studio projects. |
| Bundled Windows Python | `tools/python/` | Do not ship an obsolete embedded Windows interpreter. |
| Plugin authoring | `plugins/`, `tools/mkplugin/`, `extras/mkplugin_old/`, `src/plugins/` | Plugins and plugin API compatibility are non-goals. |
| Macros and game data | `data/macros/`, inherited item, spell, zone, and command databases | The MVP does not provide a macro or general data platform. |
| Login automation | `src/login/`, loader auto-login code and data | Automated login is prohibited. |
| Remote messaging | `src/routing/`, PostOffice, named-pipe tests and clients | Remote control and messaging are non-goals. |
| Windows process integration | injector, Detours, remote-thread, DLL loader, and module-hook code | The project reads the Wine process externally from native Linux. |
| Windows rendering | DirectX 9/11 hooks and Win32 ImGui backends | The product uses a separate native X11 overlay. |
| Editor and developer platform | `contrib/zep/`, developer tools, window inspectors, macro console | The project has one fixed information panel. |
| Crash upload and news | Crashpad upload, changelog/news fetching, update services | The product is offline and keeps diagnostics local. |
| Emulator and legacy variants | emu sources, Win32 presets, and traditional-client build variants | Traditional, test, beta, and emulator clients are unsupported. |
| Conversion and comment tools | `tools/comment-update/`, obsolete resource/conversion helpers | They do not build, test, inspect, or package the Linux product. |
| Optional legacy extras | `extras/` | Inherited optional plugins and samples are outside scope. |

## Replace

| Imported area | Replacement |
| --- | --- |
| Root `CMakeLists.txt` and `CMakePresets.json` | Small Linux CMake/Ninja project after the Phase 0 inventory. |
| MacroQuest README and badges | Plazmic Legends scope, status, and Linux commands. |
| MacroQuest loader UI | Minimal Linux launcher/status surface. |
| Broad MQ2Main runtime | Native Linux lifecycle, process reader, game reader, and snapshot boundaries. |
| Scattered offsets/build types | Immutable Legends compatibility profiles. |
| MacroQuest settings and logging | XDG configuration and local privacy-safe logs. |
| Windows package scripts | Reproducible Linux packaging commands. |

## Retain temporarily for Phase 1 research

These are candidates, not permanent dependencies:

| Research area | Candidate source | Question it may answer |
| --- | --- | --- |
| PE identity | selected portable PE parsing concepts | Which file identity checks should the Linux launcher reproduce? |
| Overlay UI | backend-independent ImGui concepts, if smaller than replacement | Can any UI code be retained without Win32 or DirectX dependencies? |
| Lifecycle | backend-independent state-machine concepts | Which transition hazards should the external reader model? |
| Game structures | retained public declarations with known provenance | Which minimal Legends fields need new profiles or typed readers? |
| Shared utilities | narrow string, config, and logging helpers | Is reuse smaller than a Linux-native replacement? |
| Licenses | `LICENSE.md` and dependency notices | What obligations apply to each retained source fragment? |

Temporary research code must be removed in Phase 4 unless it maps to a product
requirement.

## Retain as project infrastructure

- `AGENTS.md`, `SPEC.md`, `ROADMAP.md`, and `TASKS.md`.
- `docs/cleanup-inventory.md` and `docs/research/legends-baseline.md`.
- `tools/inspect_eqgame.py`.
- `LICENSE.md` until the retained-source license audit determines the final
  notice set.
- `.gitignore` and the project README.

## Required deletion evidence

For each cleanup batch, record:

- exact paths removed and retained;
- baseline and resulting file counts and disk usage;
- searches for includes, build references, docs, commands, and generated files;
- retained copyright and dependency notices;
- validation run and anything skipped;
- rollback commit.
