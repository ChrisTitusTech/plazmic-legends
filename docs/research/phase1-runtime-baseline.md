# Phase 1 runtime baseline

Observed on 2026-07-29. This record contains system and executable metadata
only. It excludes command lines, environment variables, account data,
credentials, character data, chat, and memory dumps.

## Reference host

- Distribution: Fedora Linux 44 (x86-64).
- Kernel: 7.1.5-200.fc44.x86_64.
- Session: X11 with `DISPLAY=:0`; no Wayland display is active.
- Window manager: dwm.
- Compositor: picom with the XRender backend.
- Displays: 1920x1080 at `+0+0` and primary 2560x1440 at `+0+1080`.
- Relevant X11 extensions: Composite, RENDER, SHAPE, and XFIXES.
- Process owner: Linux UID 1000, matching the project process.
- Host process policy: `/proc/sys/kernel/yama/ptrace_scope` was already `0`.
  No policy, privilege, package, prefix, or game-installation change was made.

## Launch path

The client was launched through the existing Lutris game entry, not directly
through a generic Wine command:

- Lutris: 0.5.22.
- Game entry: ID 18, EverQuest Legends.
- Prefix: `<HOME>/games/everquest/prefix` on the reference installation.
- Launcher: the installed `LaunchPad.exe`.
- Runner: locally installed GE-Proton11-3.
- Configured translation: DXVK 2.6.2.
- Configured synchronization: fsync enabled and esync disabled.

These local paths identify the reference setup and are not package defaults.
The repository does not modify or redistribute anything from them.

## Exact client

- File: `eqgame.exe` in the configured EverQuest Legends installation.
- Format: PE32+ x86-64.
- Size: 15,509,080 bytes.
- PE timestamp: 2026-07-29T16:20:33Z.
- Image base: `0x140000000`.
- PE image size: `0x16c1000`.
- SHA-256:
  `97ee793d491930ac97f91e5e26fac16d84d17ff24afcd24d5390d256e7045661`.

The supported Phase 1 profile is `legends-2026-07-29`. Any other digest fails
closed.

## Live process and rendering observations

The live process reported `eqgame.exe` in `/proc/PID/comm`, belonged to UID
1000, and mapped the exact installed executable at file offset zero. The
native proof selected one such process and used bounded `process_vm_readv`
calls to read the 64-byte DOS header, 24-byte PE/COFF header, and 64-byte
PE32+ optional-header prefix. Machine, timestamp, magic, relocated ImageBase,
and image size matched the exact file and mapping profile. Wine rewrote the
in-memory ImageBase to the actual mapping base. No gameplay address or
structure was inspected.

The process executable resolved to the GE-Proton11-3 64-bit Wine preloader.
Its live mappings included the prefix `dxgi.dll`, Proton `winevulkan.dll` and
`winevulkan.so`, the host Vulkan loader, and the NVIDIA GLX library. Together
with the Lutris DXVK setting, this establishes a DXVK/Vulkan path for this
run. It supersedes the Phase 0 static Direct3D string inference.

The game window was a normal, managed X11 window with:

- title `EverQuest Legends`;
- `_NET_WM_PID` equal to the selected Linux process;
- class `steam_app_default`;
- fullscreen state;
- geometry 2560x1440 at `+0+1080`;
- `override_redirect` disabled.

The normal managed window combined with `_NET_WM_STATE_FULLSCREEN` and native
monitor geometry is the X11 borderless-fullscreen path. Windowed mode remains
untested and is not required for the MVP's windowed-or-borderless criterion.

## Toolchain and dependencies

- CMake 3.28 or newer and Ninja.
- GCC with C++20 and warnings treated as errors.
- System Xlib, Xext, and Xfixes development packages.
- Python 3.11 or newer, Ruff, and markdownlint-cli2 for repository checks.

The proof adds no vendored code. Fedora package metadata reports `MIT AND X11`
for libX11, an MIT/X11/HPND/ISC family expression for libXext, and
`MIT AND HPND-sell-variant` for libXfixes. The standard C++ and C runtimes use
their Fedora system licenses and GCC runtime exceptions. Removing the X11
packages removes only the Phase 1 overlay; process discovery and hashing have
no non-standard runtime dependency.
