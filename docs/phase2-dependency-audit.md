# Phase 2 dependency and provenance audit

Date: 2026-07-30

## Accepted runtime dependencies

Phase 2 adds Qt 6 Widgets to build the independent Linux companion window. The
validated Fedora 44 host provides:

| Package | Version | Reported license |
| --- | --- | --- |
| `qt6-qtbase` | 6.11.1-1.fc44 | LGPL-3.0-only or GPL-3.0-only with Qt exception |
| `qt6-qtbase-gui` | 6.11.1-1.fc44 | LGPL-3.0-only or GPL-3.0-only with Qt exception |
| `libX11` | 1.8.13-1.fc44 | MIT and X11 |
| `libXext` | 1.3.6-5.fc44 | permissive X11-family licenses |
| `libXfixes` | 6.0.1-7.fc44 | MIT and HPND-sell-variant |

The product links to the system Qt and X11 shared libraries. Phase 3 also uses
the Qt Concurrent module from the same `qt6-qtbase` package to keep bounded
process discovery and map parsing off the GUI event thread. `readelf -d`
reports `libQt6Concurrent.so.6`, `libQt6Widgets.so.6`, `libQt6Gui.so.6`,
`libQt6DBus.so.6`, `libQt6Core.so.6`, and `libX11.so.6` as needed libraries
and reports no project RPATH or RUNPATH. No Qt or X11 library is copied into
the repository or install image.

Fedora installs the applicable Qt license texts under
`/usr/share/licenses/qt6-qtbase`. Any future distribution must repeat the
license review and provide the notices and replacement rights required by the
selected Qt license. Phase 2 remains private and is not a distribution
approval.

## Removal impact

Qt is isolated to the product UI target and its UI tests. Removing the
`plazmic-legends` executable target, `src/ui`, the UI tests, and the desktop
entry removes the Qt dependency while retaining the Phase 1 native reader and
diagnostics proof. X11 remains a Phase 1 proof dependency until the historical
overlay proof is also removed.

## ShowEQ provenance boundary

The ShowEQ 6.4.25 checkout was used only to identify useful product concepts:
an independent window, a map view, a spawn table, selection details, and
explicit unavailable states. Phase 2 contains independently written code and
does not copy or import:

- ShowEQ source or GPL implementation;
- packet capture, protocol decoding, or generated protocol tables;
- ShowEQ maps, icons, data, configuration, or build files; or
- its mutable pointer-based game model.

The active product also does not load user map files or read gameplay state in
Phase 2. Those capabilities remain behind later phase checkpoints.

## Package boundary

A staged install contains exactly:

```text
bin/plazmic-legends
share/applications/plazmic-legends.desktop
```

The executable is a dynamically linked x86-64 Linux PIE. The Phase 1 proof,
game executable, Wine prefix, maps, local settings, logs, and screenshots are
not installed.
