# Phase 5 product and dependency boundary

Date: 2026-07-30

Branch: `phase5/release-hardening`

## Decision

Phase 5 prepares a private/local Fedora package for the reference Linux,
Wine, and X11 tier. It does not authorize a public release, repository
publication beyond the existing development workflow, or distribution to
other users.

The active project contains independently written Plazmic Legends code. The
removed MacroQuest import remains recoverable only from
`phase0-import-baseline`. No imported implementation, ShowEQ implementation,
game executable, game asset, Wine-prefix content, or account data is restored.

The new project's license remains unresolved. Until the owner selects a
license and separately approves distribution, the Phase 5 artifact is private
and local only. This is a release blocker for public distribution, not for
local package validation.

## Removed research boundary

Phase 5 removes the Phase 1 overlay proof from the configured tree:

- `plazmic-legends-proof`;
- `src/proof`;
- `src/overlay`;
- the overlay lifecycle and F11/XTest tests; and
- direct Xext, Xfixes, and XTest build dependencies.

The proof established the native reader and rejected overlay architecture. It
is historical evidence, not a supported executable or product requirement.
Its source remains recoverable from Git history. Removing it does not change
the bounded process reader or the independent Qt companion.

## Retained path mapping

| Retained path | Current requirement |
| --- | --- |
| `src/common` | Exact SHA-256 fingerprinting and narrow shared utilities |
| `src/game` | Immutable exact-build profiles and bounded read-only game adapter |
| `src/integration` | Same-user Linux process discovery, mappings, and exact reads |
| `src/launcher` | Typed compatibility and lifecycle state |
| `src/map` | Bounded installed-map parsing, transforms, and renderer state |
| `src/model` | Immutable status, player, and spawn snapshots |
| `src/ui` | Independent Qt companion, map, table, selection, theme, and settings |
| `tools/inspect_eqgame.py` | Offline client fingerprint and profile-refresh input |
| `tests` | Deterministic AC-03 through AC-09 and performance coverage |
| `packaging` | Desktop integration for the installed native product |
| `CMakeLists.txt` and `CMakePresets.json` | Linux configure, build, test, and install gate |
| `SPEC.md`, `ROADMAP.md`, `TASKS.md`, `AGENTS.md` | Scope, safety, phase, and validation contract |
| `docs` | Provenance, risk decisions, research evidence, audits, and operations |
| `README.md` | Supported user and contributor entrypoint |

No retained source directory exists solely for an abandoned feature.

## Build and test dependencies

| Dependency | Purpose | Removal impact |
| --- | --- | --- |
| CMake 3.28 or newer | Configure, compile, test, install, and package entrypoint | No supported build |
| Ninja | Reproducible local build executor used by the presets | Presets require replacement |
| GCC or Clang with C++20 | Native Linux compilation | No product binary |
| Python 3.11 or newer | PE inspector and deterministic inspector tests | No profile inspection gate |
| Ruff | Python lint and format gate | Python style gate is unavailable |
| markdownlint-cli2 | Project-document gate | Documentation format gate is unavailable |
| Qt 6.8 or newer | Concurrent worker, DBus theme fallback, GUI, widgets, settings, and its OpenGL/GLX link closure | No product UI |
| libX11 development files | Stable X11 class and live DWM placement | No supported X11 product tier |
| Xvfb | Headless Qt/X11 lifecycle, theme, and performance tests | GUI tests cannot run headlessly |

Xext, Xfixes, and XTest are no longer direct project dependencies. Historical
documents may still name them when describing the removed Phase 1 experiment.

## Runtime dependencies

The installed ELF dynamically links to the system C++/GCC, C, and math
runtimes; Qt 6 Concurrent/Core/DBus/Gui/Widgets; libX11; and the system GLX and
OpenGL dispatch libraries selected by Qt. The package does not copy those
libraries. Fedora package metadata will express the required runtime ABI
dependencies.

Qt remains isolated to the supported product and UI tests. libX11 remains
required because the supported tier is X11 and every Plazmic top-level window
must expose the approved instance and class. Removing Qt and libX11 removes
the product UI but leaves the offline PE inspector.

## Package boundary

Before RPM metadata and notices are added, the CMake install image contains
only:

```text
bin/plazmic-legends
share/applications/plazmic-legends.desktop
```

Phase 5 may add only version, license/status notices, support documentation,
and package metadata required by P5-03. It must not add the historical proof,
research executables, test fixtures, game maps, runtime names, local settings,
logs, Wine files, or system libraries.

## Validation

The P5-01 gate requires:

- a fresh configure after removing proof targets and dependencies;
- warnings-as-errors build and complete remaining test suite;
- references to removed configured targets absent from current commands;
- staged install inventory unchanged except for later approved package
  metadata;
- direct dynamic dependencies inspected; and
- the final deletion and retained-path diff reviewed against this boundary.
