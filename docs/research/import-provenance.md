# Import provenance

## Recovery point

The complete pre-cleanup import is preserved by:

- commit `a6204d9`;
- annotated tag `phase0-import-baseline`.

The tag includes 1,431 tracked files across the initial root commit and its
ignored-file completion commit. It includes the imported source, bundled
dependencies, Windows binaries, nested IDE metadata, and the pre-phase planning
scaffold.

Restore or inspect a path without weakening the cleaned branch:

```bash
git show phase0-import-baseline:path/to/file
git worktree add /tmp/plazmic-import phase0-import-baseline
```

Do not copy an imported file back into the current tree without documenting why
it is smaller than a native replacement and carrying forward its copyright and
license obligations.

## Known provenance

- Source headers and the inherited README identify the implementation as
  MacroQuest, copyright MacroQuest Authors, licensed under GPLv2.
- Bundled third-party projects carried their own notices in `contrib/` and
  `tools/python/`.
- The imported root CMake files were generated or customized beyond the
  standard README instructions.

## Unknown provenance

The exact upstream MacroQuest commit is unknown:

- the repository arrived on an unborn branch with no source history;
- there was no `.gitmodules` file;
- `src/eqlib` was empty despite being required by the inherited build;
- no durable upstream revision was recorded in the imported files.

The cleanup therefore makes no claim that the import was a complete or
reproducible upstream snapshot.

## Retained-code audit

Phase 0 retains no MacroQuest implementation or bundled third-party source in
the active tree. The only executable project code is the independently written
standard-library Python inspection tool and its tests.

Because no inherited implementation remains, the imported GPLv2 license file
was removed from the active tree with the source it governed. It remains
available from `phase0-import-baseline`. The independently written active
Plazmic Legends project is licensed separately under GPL-3.0-only in the root
`LICENSE` file.
