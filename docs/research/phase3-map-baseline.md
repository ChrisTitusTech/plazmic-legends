# Phase 3 local-map baseline

Captured: 2026-07-30

## Scope

This record describes read-only inspection of the user's installed Legends
maps. No map line, label, asset, or file is copied into the repository,
fixtures, package, or logs. Tests use independently written synthetic records.

## Installed input inventory

The current installation contains 1,710 text files:

- 570 base map files;
- 563 layer-1 files; and
- 577 layer-2 files.

The largest observed file is approximately 1.9 MiB and contains 26,383
records. Observed non-empty records use the established `L` line and `P` label
forms. Numbered layers currently stop at layer 2, while the parser permits
bounded layer indices 1 through 9.

## Approved parser boundary

The Phase 3 parser accepts only:

- ASCII zone names up to 64 bytes containing letters, digits, `_`, or `-`;
- one required `<zone>.txt` base file and optional numbered layer files;
- `L` records containing two finite 3D points and an RGB color; and
- `P` records containing one finite 3D point, RGB color, bounded size, and a
  bounded UTF-8 label.

Default limits are 8 MiB per file, 4,096 bytes per line, 100,000 records per
layer, 512 bytes per label, absolute coordinates no greater than 1,000,000,
and numbered layers no greater than 9.

Paths are canonicalized and must remain below the canonical maps root.
Missing base maps, symlink escapes, inaccessible files, oversized input,
excessive records, invalid UTF-8, malformed fields, non-finite coordinates,
out-of-range coordinates, invalid colors, and invalid sizes return typed
errors.

## Validation evidence

Synthetic fixtures cover base and numbered layers, line and label records,
Windows line endings, normalized label separators, empty labels, missing
roots and base files, invalid zone paths, malformed records, non-finite and
out-of-range coordinates, invalid colors, long lines and labels, invalid
UTF-8, record and file limits, and symlinks outside the map root.

The focused and full automated gates passed:

```text
cmake --build --preset dev
ctest --test-dir build/dev -R map_parser.unit --output-on-failure
ctest --preset dev --output-on-failure
cmake --build --preset check

map_parser.unit passed
100% tests passed, 0 tests failed out of 9
repository gate: 0 Markdown errors; Python checks passed
```

The content-free installed-map checks used:

```bash
maps_dir="$EQ_LEGENDS_DIR/maps"
build/dev/map_parser_test "$maps_dir" qeynos
build/dev/map_parser_test "$maps_dir" everfrost

while IFS= read -r filename; do
  zone=${filename%.txt}
  build/dev/map_parser_test "$maps_dir" "$zone"
done < <(
  find "$maps_dir" -maxdepth 1 -type f -name '*.txt' \
    ! -name '*_[0-9].txt' -printf '%f\n' | sort
)
```

The build-only checker loaded a representative layered map with 4,086 records
and the largest installed map with 27,045 records across its available layers.
An audit attempted all 570 installed base zones. It accepted 569 and rejected
one layer containing an RGB component above 255. The rejection is intentional;
the parser does not clamp or guess malformed user input.

The first CodeRabbit pass found two documentation gaps and no parser-code
finding. Both documentation findings were corrected. The requested clean rerun
was rate limited, so the owner-approved Codex fallback reviewed path
containment, parser bounds, typed failures, fixture isolation, and the final
diff without finding another actionable defect.

Map rendering, automatic zone selection, coordinate-orientation checks, and
live player validation were skipped because they belong to P3-02 through
P3-04. The remaining parser risk is format drift in future map updates and the
single currently malformed installed layer, which remains unavailable rather
than partially loaded.

## Next boundary

P3-02 may consume the immutable parsed values to implement bounds, transforms,
and rendering. The parser has no client-process access. Live zone and player
symbol research remains P3-03 and must not place raw addresses in map or UI
code.
