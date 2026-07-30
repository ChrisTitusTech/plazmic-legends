# Phase 1 architecture proof

Status: Complete on local branch `phase1/native-linux-proof`.

## Post-validation correction

A 2026-07-29 root-window screenshot invalidated the earlier inference that
`IsViewable` meant the override-redirect panel was visibly composed over the
game. The reference DWM intentionally raises real fullscreen clients after
override windows, so the game covered the panel. Temporarily removing the
fullscreen property made the panel visible but changed game focus
presentation, exposed shell geometry, and caused perceived opacity.

The fullscreen property and exact 2560x1440 geometry were restored, the proof
process was stopped, and a second root-window screenshot confirmed the game was
restored. The external overlay is rejected as the product UI. The process
discovery, compatibility, and bounded-reader conclusions remain valid.

## Proven boundary

`plazmic-legends-proof` is a native Linux ELF executable. It:

1. hashes the selected installed `eqgame.exe`;
2. accepts only the `legends-2026-07-29` profile;
3. scans numeric `/proc` entries for an exact same-user command and mapping;
4. rejects zero or multiple exact candidates;
5. resolves the image base from the exact file mapping at offset zero;
6. uses a reusable bounded `process_vm_readv` reader to recover the live DOS,
   PE/COFF, and PE32+ identity fields; and
7. creates a separate X11 diagnostics window associated through
   `_NET_WM_PID`.

There is no injection, PE output, DirectX hook, Wine DLL override, target
write, input automation, prefix edit, or gameplay-state reader.

`ProcessMemoryReader` accepts only a non-empty range wholly contained in one
readable mapping captured from `/proc/PID/maps`. It rejects zero, overflow,
cross-boundary, unreadable, missing-process, permission, system, and short-read
conditions with distinct results. A failed or partial kernel read clears the
destination buffer.

## Overlay proof

The X11 proof is an override-redirect window positioned relative to the game
window. It renders project, profile, and read-only status using Xlib. Xfixes
sets its ShapeInput region to empty and X Shape independently confirms zero
input rectangles. The panel uses a passive F11 key grab and follows the target
window every 50 ms. It unmaps itself when the target is not viewable. SIGINT,
SIGTERM, a duration limit, or disappearance of the target window enters one
cleanup path that ungrabs the key, restores prior handlers, and destroys the
window.

Observed live evidence:

- `plazmic-legends-proof --client "$EQ_LEGENDS_DIR/eqgame.exe"
  --duration 3` reported the supported profile, exact digest, one PID, two
  client mappings, `remote_pe_identity=verified`,
  `overlay_input_region=empty`, and exit status zero.
- The final diagnostics run reported machine `0x8664`, timestamp `0x6a6a2851`,
  PE32+ magic, image size `0x16c1000`, and live ImageBase
  `0x6ffffb480000`. Wine rewrites the optional-header ImageBase to the actual
  mapping base; the on-disk preferred base remains `0x140000000`.
- `xwininfo -name "Plazmic Legends Diagnostics" -stats` reported a viewable,
  override-redirect 430x104 window at `+24+1124`; this established X11 map
  state and geometry but did not establish visible fullscreen composition.
- `xprop -name "Plazmic Legends Diagnostics"
  _NET_WM_WINDOW_OPACITY` reported `3650722201`, or `0xd9999999`.
- An exact `/proc/PID/exe` scan against the built proof path returned zero
  processes after the duration test.
- A physical F11 press produced `overlay_visible=false`, proving the live
  passive-grab event path but not visible fullscreen stacking.
- `ctest --preset dev` ran three child-owned X11 target windows through target
  exit; all overlay instances detected exit and cleaned up in 1.26 seconds
  total.
- The isolated `x11_overlay.hotkey` test used Xvfb and XTest to confirm F11
  hide, F11 re-show, and target-exit cleanup without sending input to the
  desktop or game.

Synthetic XTest events on the live desktop carried an existing modifier, so
desktop modifier state was left unchanged while the game was active. The
isolated display supplied the missing re-show evidence. Input pass-through is
established by the independently queried empty ShapeInput region and the fact
that F11 is the only grabbed key; no synthetic gameplay input was used.

Later inspection found three Wine windows with the game PID: the visible
2560x1440 game and two hidden 1x1 helper windows. The proof's first-match search
could select a hidden helper. A local fix and regression test demonstrated
largest-viewable-window selection, but the change was not retained because the
product moved to an independent window with no game-window association.

## Automated coverage

The C++ test executable uses temporary synthetic `/proc` trees and covers:

- SHA-256 padding and block-boundary vectors;
- bounded same-process memory reads;
- zero, overflow, cross-mapping, unreadable, and invalid-PID reads;
- valid and invalid remote PE identity fixtures;
- no candidate;
- one exact same-user candidate;
- wrong command;
- wrong UID;
- same filename at a different path;
- incomplete process metadata;
- deleted executable mappings;
- exact image-base selection; and
- ambiguous exact candidates.

The X11 lifecycle test runs three real child-owned target windows through exit.
The existing Python suite continues to cover PE parsing and client fingerprint
behavior. All C++ targets compile with GCC, C++20, and warnings as errors.

## Current decisions

- Process read: same-user `process_vm_readv`, restricted by a single-mapping
  readable-range gate behind the Linux integration boundary.
- Module resolution: exact normalized file mapping and file offset zero.
- Compatibility: exact SHA-256 profile selection before process access.
- Product UI: normal independent Qt 6 Widgets application placed on DWM tag 5
  by stable X11 class. The external Xlib/Xext/Xfixes window and F11 grab remain
  Phase 1 research only.
- Dependencies: system C++ runtime and system X11 libraries only at runtime.
- XDG locations: configuration under
  `$XDG_CONFIG_HOME/plazmic-legends/config.toml` and logs under
  `$XDG_STATE_HOME/plazmic-legends/plazmic-legends.log`, using the standard
  per-user defaults when those variables are unset.
- MVP fields: retain the narrow field set in `SPEC.md`, subject to removing any
  field that cannot be isolated and validated safely.
- Game symbols: the minimum logical inputs and validation rules are recorded
  in `docs/research/phase1-symbol-plan.md`; no offset or signature was found or
  read.
- Distribution: local private development artifact only. Do not push or
  release the utility.
- EULA risk: the owner explicitly directed private read-only development to
  proceed despite the published restrictions. Writes, injection, automation,
  protection bypass, push, and distribution remain prohibited. See
  `docs/research/phase1-policy-risk.md`.

## Live interaction conclusion

The reference game window is a managed normal window with
`_NET_WM_STATE_FULLSCREEN` at native monitor geometry. The selected DWM policy
places that true-fullscreen window above all override windows. Plazmic will
therefore use a separate normal companion window and will not change the game
window's state.

The live game remained running after repeated proof shutdown. Exact executable
inspection found no remaining proof process. No prefix, game installation, or
host-security setting changed.

Phase 2 has not started. It requires the normal explicit phase approval and
must use the bounded reader without adding writes, injection, automation, or
protection bypass.
