<!-- markdownlint-disable-next-line MD033 -->
# <img src="packaging/plazmic-legends.png" alt="Plazmic Legends icon" width="52"> Plazmic Legends

Plazmic Legends is a feature-complete, native Linux companion for the 64-bit
EverQuest Legends client running under Wine. It provides a dedicated map and
spawn window without drawing over the game, injecting code, or modifying the
Wine prefix.

[Download the latest release](https://github.com/ChrisTitusTech/plazmic-legends/releases/latest)

<!-- markdownlint-disable-next-line MD013 -->
![Plazmic Legends map, spawn list, and details](https://github.com/user-attachments/assets/2409ad54-0612-40ec-944e-7c0f158f29c8)

## Features

- Displays locally installed zone maps with pan, zoom, geometry-aware fit, and
  optional player follow.
- Shows live player position, heading, and configurable elevation filtering.
- Presents a sortable and filterable spawn list with synchronized map markers
  and selection details, including distinct named-spawn and Other markers.
- Shows bounded live character HP, mana, and text-only equipped items in a
  left Character dock.
- Condenses newly appended local combat-log damage into a current or
  most-recent encounter Parse dock when EverQuest combat logging is enabled.
- Handles zoning, camping, character select, process exit, and client changes
  without leaving stale data visible.
- Follows the active system light or dark theme.
- Keeps all processing local with no telemetry, updater, account service, or
  bundled game content.

## Requirements

- An x86-64 Linux desktop using X11 or XWayland.
- A user-owned 64-bit EverQuest Legends installation running through Wine.
- The exact supported client build. Unknown or changed clients fail closed
  instead of using unverified offsets.
- Fedora 43 or 44 for the COPR package, or a glibc 2.35-or-newer distribution
  for the AppImage.

## Install

### Fedora COPR

```bash
sudo dnf copr enable christitustech/copr-fedora
sudo dnf install plazmic-legends
```

### AppImage

Download the AppImage and its adjacent checksum file from the
[latest release](https://github.com/ChrisTitusTech/plazmic-legends/releases/latest),
then run:

```bash
sha256sum --check Plazmic-Legends-0.2.0-x86_64.AppImage.sha256
chmod 0755 Plazmic-Legends-0.2.0-x86_64.AppImage
```

See [package operations](docs/package-operations.md) for direct RPM
installation, upgrades, rollback, removal, and FUSE-free AppImage launch.

## Run

On first launch, Plazmic Legends looks for the game in this order:

1. An explicit `--client /path/to/eqgame.exe`.
2. `EQ_LEGENDS_DIR`, when it contains `eqgame.exe`.
3. The saved `[client]` configuration.
4. A short, bounded scan below your home directory for
   `Daybreak Game Company/Installed Games/EverQuest Legends`.

A valid explicit, environment, or discovered path is saved automatically, so
later launches need no client argument:

```bash
plazmic-legends
```

The scan is skipped when `EQ_LEGENDS_DIR` or the saved configuration points to
an existing `eqgame.exe`. If more than one installation is found, select one
explicitly instead of guessing:

```bash
plazmic-legends --client "/path/to/EverQuest Legends/eqgame.exe"
```

You can also edit
`$XDG_CONFIG_HOME/plazmic-legends/config.toml` (normally
`~/.config/plazmic-legends/config.toml`) directly:

```toml
[client]
game_directory = "/path/to/EverQuest Legends"
```

The AppImage uses the same selection and saved configuration behavior. Launch
it with `./Plazmic-Legends-0.2.0-x86_64.AppImage`.

The companion opens as a normal independent desktop window. Its map reads the
zone files already present in your game installation; no maps are included in
the package.

## Using the companion

- Drag the map to pan and use the mouse wheel to zoom.
- Use the map's Filters / Labels dropdown to independently show or hide named
  NPC, player, ordinary-NPC, and Ground / Other markers and to toggle labels
  for named NPCs, players, and ordinary NPCs.
- Right-click the map to fit the zone geometry, toggle player follow, show all
  elevations, or adjust the visible range above and below the player.
- Select a spawn from either the map or table to keep both views synchronized.
- Use the status area when the client is not running, unsupported, zoning, or
  outside the world.
- Enable EverQuest combat logging to populate the Parse dock. Plazmic Legends
  starts at the active log's current end and does not upload or save encounter
  history.

The selected client directory, window layout, and map preferences are stored in
`$XDG_CONFIG_HOME/plazmic-legends/config.toml`, with the standard Qt user
configuration path used when `XDG_CONFIG_HOME` is unset. Pass
`--reset-layout` to ignore saved placement for one launch.

Privacy-safe diagnostics are stored under
`$XDG_STATE_HOME/plazmic-legends/`, or
`$HOME/.local/state/plazmic-legends/` by default. Logs are bounded, readable
only by the owner, and exclude client paths, process IDs, character or spawn
names, memory contents, and process addresses.

## Why Plazmic Legeneds Name?

The original Macroquest was released open source by a developer named Plazmic.
He only wanted to extend the tradeskill automation, macro capabilities, and other
limitations the original Everquest had. He passed in 2007, and this project
is a homage to him.

## Important notice

Plazmic Legends is an independent project and is not affiliated with or
endorsed by Daybreak Game Company. It performs external, read-only process
inspection. The project owner knowingly accepts that this may conflict with
Daybreak's EULA and published rules. Use it at your own risk.

The project does not write game state, inject code, automate gameplay, bypass
client protections, or modify the game installation or Wine prefix.

## Development

Build instructions, architecture, project history, validation requirements,
and research references are in [development.md](development.md). Before
submitting a change, read the
[contribution guide](https://github.com/ChrisTitusTech/plazmic-legends/blob/main/CONTRIBUTING.md).

## License

Plazmic Legends is licensed under the
[GNU General Public License v3.0 only](LICENSE). Third-party components retain
their respective licenses; see
[third-party notices](packaging/THIRD-PARTY-NOTICES.md).

EverQuest and its assets are the property of their respective owners and are
not included in this repository.
