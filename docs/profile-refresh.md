# Compatibility profile refresh

This workflow applies after `eqgame.exe` changes. A changed SHA-256 is a new
client build. Never update an existing profile in place, reuse its offsets
without validation, or select the nearest known profile.

## Runtime behavior after a patch

Plazmic records the validated client file metadata at startup. If the file
size or modification time changes while the companion is open, it recalculates
SHA-256. A mismatch is latched for that run, process access is dropped, live
snapshots are invalidated, and status directs the user to create a new
compatibility profile. Restarting cannot make an unknown hash supported.

Every new Wine process also requires the exact file digest and live PE
identity before profile addresses are used.

## Capture the new identity

Keep the patched installation and all outputs outside the repository:

```bash
export EQ_LEGENDS_DIR='/path/to/Installed Games/EverQuest Legends'
python3 tools/inspect_eqgame.py "$EQ_LEGENDS_DIR"
```

Record file size, SHA-256, machine, PE timestamp, optional-header format, image
size, and entry point in a new privacy-safe research note. Do not record the
absolute Wine-prefix path, launch ticket, account data, memory dump, character
name, spawn name, or game asset.

## Create an immutable profile

1. Add a new `ClientProfile` entry keyed by the new SHA-256 and PE identity.
2. Leave every prior profile unchanged and selectable only by its own hash.
3. Independently re-establish the smallest required zone, player, and spawn
   resolvers from the exact new executable.
4. Use upstream projects only to form conceptual hypotheses. Record their
   revision and provenance; copy no implementation, offsets, generated data,
   maps, or game content.
5. Require bounded static evidence and two controlled live observations for
   every displayed field.
6. Reject any field that cannot be isolated and validated safely.

No raw address belongs in UI, lifecycle, or renderer code. Profile-local RVAs,
record offsets, bounds, and identity metadata remain centralized under
`src/game`.

## Validation cycle

Run the complete synthetic gate before a new profile reaches a live account:

```bash
cmake --preset dev --fresh
cmake --build --preset dev
cmake --build --preset check
ctest --preset dev --output-on-failure
python3 tools/inspect_eqgame.py "$EQ_LEGENDS_DIR" \
  --expect-sha256 NEW_SHA256
git diff --check
```

Synthetic tests must cover exact selection, the old and unknown hashes,
invalid identity, changed-file detection, bounded reads, every field
rejection, duplicate and inconsistent spawn collections, and lifecycle
invalidation.

Then repeat the controlled live gate on the approved Fedora/Wine/X11 tier:

- exact file and live PE identity;
- character select and entering the world;
- two zones and installed-map selection;
- player position and heading;
- two controlled observations for every displayed spawn field;
- add, remove, change, map/table selection, and performance;
- zoning, camping, game exit, and process reacquisition; and
- DWM placement and game-window invariance.

Audit logs, fixtures, screenshots, diffs, packages, and temporary artifacts
for runtime names, addresses, account data, Wine-prefix content, and Daybreak
assets. Retain only privacy-safe aggregate evidence.

## Release handling

A client patch immediately invalidates any release candidate that does
not contain the new validated profile. Withdraw the incompatible artifact
rather than weakening compatibility checks. Retain the prior artifact and
immutable profile for rollback against the corresponding old client only.
