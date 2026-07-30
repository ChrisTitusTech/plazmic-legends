# EverQuest Legends baseline

Captured: 2026-07-29

This record describes the local reference used to plan the migration. It
contains no game binary, asset, account, or memory content.

## Host

| Property | Observed value |
| --- | --- |
| Distribution | Fedora Linux 44 x86-64 |
| Kernel | Linux 7.1.5-200.fc44.x86_64 |
| Session | X11 |
| Wine | Wine 11.0 Staging |
| CMake | 4.3.0 |
| Ninja | 1.13.2 |
| Native compiler | GCC 16.1.1 |
| MinGW C++ compiler | Not installed |

MinGW is not required because project artifacts are native Linux ELF files.

## Executable identity

| Property | Observed value |
| --- | --- |
| File | `eqgame.exe` |
| Size | 15,509,080 bytes |
| Format | PE32+ Windows GUI executable |
| Machine | x86-64 (`0x8664`) |
| PE timestamp | 2026-07-29 16:20:33 UTC |
| Image base | `0x140000000` |
| Image size | `0x16c1000` |
| SHA-256 | `97ee793d491930ac97f91e5e26fac16d84d17ff24afcd24d5390d256e7045661` |

The installation is inside a Wine prefix. Repository commands refer to its
directory through `EQ_LEGENDS_DIR`; the absolute prefix is not a portable
project setting.

## Static observations

- The client is a 64-bit Windows executable. Legacy 32-bit EverQuest paths are
  irrelevant.
- Its import table does not statically import a Direct3D DLL.
- Embedded strings include `D3d9.dll` and `Direct3DCreate9`, suggesting runtime
  Direct3D 9 loading.
- The installation includes `EQGraphics.dll`, `eqmain.dll`, Coherent UI
  libraries, Miles audio libraries, and Visual C++ runtime libraries.
- Static evidence does not prove the active API, Wine translation path, hook
  point, loaded modules, or safe process integration.

## Historical imported repository observations

- Initial inventory: 487 files under `src`, 61 under `include`, and 736 under
  `contrib`.
- Approximate imported sizes: 12 MiB `src`, 23 MiB `contrib`, 14 MiB `tools`,
  and 9.4 MiB `data`.
- Root CMake still declares MacroQuest core, launcher, plugin, and test targets.
- `src/eqlib` is empty even though root CMake adds it as required.
- There is no `.gitmodules`, and the branch has no commits. Missing source
  cannot be recovered from local repository history.
- The imported build was migration input, not a validated baseline. It is now
  preserved at `phase0-import-baseline` and absent from the active tree.

## Repeat the inspection

```bash
export EQ_LEGENDS_DIR='/path/to/Installed Games/EverQuest Legends'
python3 tools/inspect_eqgame.py "$EQ_LEGENDS_DIR" \
  --expect-sha256 97ee793d491930ac97f91e5e26fac16d84d17ff24afcd24d5390d256e7045661
```

After a patch, run without `--expect-sha256`, retain the old profile unchanged,
and create a new research record before runtime testing.

## Runtime evidence still required

- Wine launch command and prefix environment.
- `eqgame.exe` process and child-process topology.
- Loaded graphics modules, active API, and Wine translation layer.
- Windowed and borderless behavior under X11.
- Feasibility of native Linux process reads without elevated privileges.
- Character select, enter world, zoning, camping, and exit transitions.
- Explicit risk decision before process integration testing or distribution.
