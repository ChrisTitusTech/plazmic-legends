# Phase 5 product and dependency boundary

Date: 2026-07-30

Release: `v0.1.2`

## Decision

Phase 5 prepares Fedora RPM and x86-64 AppImage packages for the reference
Linux, Wine, and X11 tier. Package publication is authorized.

The active project contains independently written Plazmic Legends code. The
removed MacroQuest import remains recoverable only from
`phase0-import-baseline`. No imported implementation, ShowEQ implementation,
game executable, game asset, Wine-prefix content, or account data is restored.

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
| `src/ui` | Independent Qt companion, map, table, selection, theme, settings, and scoped private UI installer |
| `tools` | Offline client fingerprinting, private UI-bundle export, and Legends-derived 1440p layout tooling |
| `tests` | Deterministic AC-03 through AC-09 and performance coverage |
| `packaging` | RPM, AppImage, desktop integration, metadata, and dependency notices |
| `.copr` | Deterministic source-RPM entrypoint for external-SCM COPR builds |
| `.github/workflows` | Exact-commit Linux configure, build, test, and install inspection |
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
| RPM toolchain | Fedora source/binary package and metadata validation | No Fedora artifact |
| Podman or Docker | Reproducible Ubuntu 22.04 AppImage build environment | No portable artifact |
| linuxdeploy and Qt plugin | Checked-by-SHA dependency collection and AppImage creation | No AppImage |

Xext, Xfixes, and XTest are no longer direct project dependencies. Historical
documents may still name them when describing the removed Phase 1 experiment.

## Runtime dependencies

The RPM ELF dynamically links to the system C++/GCC, C, and math runtimes; Qt
6 Concurrent/Core/DBus/Gui/Widgets; libX11; and the system GLX and OpenGL
dispatch libraries selected by Qt. The RPM does not copy those libraries and
declares its Qt and X11 runtime requirements.

The x86-64 AppImage bundles Qt 6.8.3 and non-base runtime dependencies selected
by linuxdeploy. It intentionally retains the host kernel, glibc, graphics
driver, X11 client ABI, and D-Bus boundary. Building on Ubuntu 22.04 establishes
a glibc 2.35 floor; it does not guarantee musl systems, other architectures,
old kernels, or every graphics stack.

Qt remains isolated to the supported product and UI tests. libX11 remains
required because the supported tier is X11 and every Plazmic top-level window
must expose the approved instance and class. Removing Qt and libX11 removes
the product UI but leaves the offline PE inspector.

## Package boundary

The CMake install image contains only:

```text
bin/plazmic-legends
share/applications/plazmic-legends.desktop
share/doc/plazmic-legends/README.md
share/doc/plazmic-legends/THIRD-PARTY-NOTICES.md
share/doc/plazmic-legends/development.md
share/doc/plazmic-legends/package-operations.md
share/doc/plazmic-legends/phase5-product-boundary.md
share/icons/hicolor/512x512/apps/plazmic-legends.png
share/licenses/plazmic-legends/LICENSE
share/metainfo/io.github.ChristitusTech.PlazmicLegends.metainfo.xml
```

The RPM adds no files beyond this install image. The AppImage adds only its
runtime launcher and audited shared-library/plugin closure. Neither artifact
may add the historical proof, research executables, test fixtures, game maps,
runtime names, local settings, logs, Wine files, or game/system data.

The UI installer code is part of the product binary, but private bundles are
never installed by CMake or embedded in a package. The ignored exporter output
remains user-owned input and may contain Daybreak UI assets plus private INI
filenames and settings.

## Validation

The P5-01 gate requires:

- a fresh configure after removing proof targets and dependencies;
- warnings-as-errors build and complete remaining test suite;
- references to removed configured targets absent from current commands;
- staged install inventory unchanged except for later approved package
  metadata;
- direct dynamic dependencies inspected; and
- the final deletion and retained-path diff reviewed against this boundary.
