# Phase 2 completion report

Date: 2026-07-30

Branch: `phase2/standalone-qt-shell`

## Result

Phase 2 produced a native Linux Qt 6 companion application with one main
window, map and spawn placeholders, selection details, client compatibility
status, persisted dock layout, and live system light and dark modes. It is an
independent managed window, not an overlay, and it performs no gameplay-state
reads.

The executable hashes the configured client once, selects only the exact
supported profile, then refreshes exact Wine process discovery. An absent,
unreadable, changed, or unsupported client fails closed before process
integration.

## Automated validation

The warnings-as-errors build and all eight CTest entries passed:

```text
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure

100% tests passed, 0 tests failed out of 8
```

Coverage includes:

- exact and changed profile selection;
- missing and unsupported client states;
- process-discovery fixtures and bounded reader behavior;
- settings round trip plus corrupt and oversized input rejection;
- map, spawn, detail, and status placeholder construction;
- main and detached-dock X11 class inheritance;
- system dark and light mapping, palette contrast, and live refresh;
- saved geometry and dock-state restoration; and
- shutdown with a detached dock.

The detached-dock test exposed a lifecycle defect during the live gate:
closing the main window could leave the application event loop running. The
main close path now saves state and explicitly ends the Qt event loop, and the
new regression test passes under Xvfb.

A local CodeRabbit review reported three findings. The final implementation
routes timed shutdown through the main close path, verifies saved geometry and
floating-dock restoration, and rejects non-xcb Qt backends before treating a
Qt window ID as an X11 window. A two-second timed run then exited normally and
wrote valid UI state.

The system-theme follow-up reads the active `dark_mode` from the bounded
reference DWM theme configuration and polls for changes. Standard appearance
portal and Qt color-scheme hints remain fallbacks for other desktops. A live
Nord to Catppuccin Latte transition changed the open window background from
dark RGB `(43,47,54)` to light `(242,242,242)` without restart. Restoring Nord
returned it to dark `(41,47,58)`. The game window remained unchanged.

## Package validation

A clean staged install produced only the product ELF executable and desktop
entry. `file` identified the executable as a dynamically linked x86-64 Linux
PIE. `desktop-file-validate` passed without diagnostics. `ldd` resolved the
system Qt 6 and X11 libraries with no missing dependency. The dependency and
provenance decision is recorded in `docs/phase2-dependency-audit.md`.

## Live DWM validation

The active configuration contains this exact rule:

```toml
{ class="PlazmicLegends", tags=5, monitor=1, noswallow=1 },
```

The reference topology was HDMI-0 at `1920x1080+0+0` and DP-0 at
`2560x1440+0+1080`. The main window and a detached spawn dock both reported:

```text
WM_CLASS = "plazmic-legends", "PlazmicLegends"
_NET_WM_DESKTOP = 4
_NET_WM_STATE = not found
_NET_WM_WINDOW_OPACITY = not found
Override Redirect State = no
```

Both windows were viewable inside the HDMI-0 bounds on human tag 5. A saved
layout restored the detached dock with the same class and desktop. The
application did not request always-on-top and did not activate or retag the
game.

## Game-window invariance

Before the companion launched, the game window reported:

```text
_NET_WM_DESKTOP = 0
_NET_WM_STATE = _NET_WM_STATE_FULLSCREEN
_NET_WM_WINDOW_OPACITY = not found
geometry = 2560x1440+0+1080
Map State = IsViewable
Override Redirect State = no
```

The same values were observed after launch, focus and dock movement, minimize
and restore, detached-dock layout restore, and application shutdown. The exact
game process remained running. No game property, tag, geometry, opacity,
fullscreen, or input configuration was changed.

The final tag-5 window was visually inspected from a local root-window capture.
The capture remains under `/tmp` and is intentionally not a repository
artifact because screenshots can contain unrelated private desktop content.

## Phase boundary

Phase 2 is complete. No map parser, map file loading, gameplay symbol,
player-state reader, spawn traversal, injection, write, input, or automation
path was added. Phase 3 remains unstarted and requires the next explicit
checkpoint approval.
