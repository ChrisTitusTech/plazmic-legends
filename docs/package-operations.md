# Package operations

Date: 2026-07-30

Version: 0.1.0

## Distribution

The Fedora RPM is published through `christitustech/copr-fedora`, and the
portable artifact is an x86-64 AppImage. No Daybreak file, installed map,
Wine-prefix content, credential, local setting, or local diagnostic log is
included.

## Support matrix

| Artifact | Supported host | Architecture | Display |
| --- | --- | --- | --- |
| COPR RPM | Fedora 43 and Fedora 44 | x86-64 | X11 |
| AppImage | glibc 2.35 or newer desktop distribution | x86-64 | X11 or XWayland |

The RPM is the reference and fully validated package. The AppImage is built on
Ubuntu 22.04 with the official Qt 6.8.3 Linux binaries and bundles Qt plus
non-base dependencies. It intentionally relies on the host kernel, glibc,
graphics driver, X11 client libraries, and D-Bus. It therefore cannot promise
every distribution, musl system, CPU architecture, old kernel, or pure-Wayland
session. Build on an older glibc and test on each supported target before
claiming additional compatibility.

Both artifacts require the user-owned 64-bit EverQuest Legends installation
and the exact supported `eqgame.exe` profile. They never install or update the
game.

## Fedora COPR install

Enable the owner repository and install the package:

```bash
sudo dnf copr enable christitustech/copr-fedora
sudo dnf install plazmic-legends
```

Launch with an explicit client path:

```bash
plazmic-legends --client "/path/to/EverQuest Legends/eqgame.exe"
```

An unsupported or changed executable fails closed. Refreshing a compatibility
profile is a source-development workflow, not an RPM transaction; see
[Compatibility profile refresh](profile-refresh.md).

## Upgrade and rollback

Refresh repository metadata and upgrade:

```bash
sudo dnf upgrade --refresh plazmic-legends
```

List versions retained by enabled repositories:

```bash
dnf list --showduplicates plazmic-legends
```

Downgrade to a retained known-good build:

```bash
sudo dnf downgrade plazmic-legends
```

If a client patch invalidates the current profile, keep the package installed
only for offline inspection or withdraw it until a newly researched immutable
profile passes the full gate. Never weaken the profile match or reuse old
offsets.

## Removal

Remove the RPM and optionally disable the repository:

```bash
sudo dnf remove plazmic-legends
sudo dnf copr disable christitustech/copr-fedora
```

Removal leaves per-user configuration and bounded logs in place. Remove those
only if their saved window state and local diagnostic history are no longer
needed:

```bash
rm -rf -- \
  "${XDG_CONFIG_HOME:-"$HOME/.config"}/plazmic-legends" \
  "${XDG_STATE_HOME:-"$HOME/.local/state"}/plazmic-legends"
```

No install, upgrade, downgrade, or removal action touches the game
installation or Wine prefix.

## AppImage

Verify the adjacent checksum, make the artifact executable, and launch it:

```bash
sha256sum --check Plazmic-Legends-0.1.0-x86_64.AppImage.sha256
chmod 0755 Plazmic-Legends-0.1.0-x86_64.AppImage
./Plazmic-Legends-0.1.0-x86_64.AppImage \
  --client "/path/to/EverQuest Legends/eqgame.exe"
```

FUSE is optional. On hosts without a usable FUSE setup:

```bash
./Plazmic-Legends-0.1.0-x86_64.AppImage \
  --appimage-extract-and-run \
  --client "/path/to/EverQuest Legends/eqgame.exe"
```

The AppImage has no privileged installer or updater. Remove it by deleting the
AppImage and checksum file.

## Troubleshooting

- Run `plazmic-legends --version` first to confirm the selected artifact.
- Verify the client with `tools/inspect_eqgame.py` from the matching source
  revision. A changed SHA-256 is unsupported until a new profile is validated.
- Read the privacy-safe bounded log under
  `$XDG_STATE_HOME/plazmic-legends/`, or the documented default.
- On a non-X11 desktop, confirm XWayland is available. Native Wayland is not a
  supported MVP path.
- For an AppImage loader error, retry with `--appimage-extract-and-run`; for an
  ELF symbol-version error, the host is older than the declared glibc floor.
- Do not report or attach game binaries, maps, Wine prefixes, memory dumps, or
  unredacted local paths.

## Artifact integrity and content audit

Release-candidate checksums are generated only after clean builds. Inspect an
RPM without installing it:

```bash
rpm -qpl ./plazmic-legends-0.1.0-1.*.x86_64.rpm
rpm -qp --requires ./plazmic-legends-0.1.0-1.*.x86_64.rpm
```

Inspect an AppImage without FUSE:

```bash
./Plazmic-Legends-0.1.0-x86_64.AppImage --appimage-extract
find squashfs-root -type f -print | sort
```

Approved content is limited to the native product, desktop integration,
bundled AppImage runtime dependencies, and the notices installed by the CMake
boundary. Game content, research tools, test fixtures, local data, and Wine
files are prohibited.
