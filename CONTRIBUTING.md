# Contributing to Plazmic Legends

Thank you for helping improve Plazmic Legends. The application has a validated
native Linux foundation and an expansion roadmap. Contributions may propose
major new capabilities as well as fixes, compatibility maintenance,
accessibility, documentation, and focused refinements.

## Before opening an issue

- Read the [README](README.md), [package operations](docs/package-operations.md),
  and existing issues.
- Confirm that the problem occurs with the latest release.
- Run `plazmic-legends --version` and record the result.
- Confirm your Linux distribution, display server, Wine runner, and installation
  method.
- Do not upload game executables, maps, Wine prefixes, memory captures,
  credentials, character or spawn names, or unredacted personal paths.

Use the repository's issue forms so reports include the information needed to
reproduce the problem. Security vulnerabilities must follow the private
reporting instructions in [.github/SECURITY.md](.github/SECURITY.md).

## Project scope

Every new capability must follow the phase, acceptance, provenance, privacy,
security, lifecycle, validation, and rollback contracts in `SPEC.md` and
`ROADMAP.md`. A capability with writes, input, automation, injection, plugins,
remote access, telemetry, or another new trust boundary needs explicit owner
approval for its own phase; approval of one capability does not approve another.

Protection bypass, silent private-data collection, guessed client offsets, and
incompatible or proprietary third-party content remain outside the project
boundary.

An unknown or changed client must continue to fail closed. Never weaken the
fingerprint match or reuse unvalidated offsets to make a patched client appear
supported.

## Development workflow

1. Fork the repository and create a focused branch from the latest `main`.
2. Read [development.md](development.md) and the repository instructions in
   [AGENTS.md](AGENTS.md).
3. Make the smallest change that addresses the issue.
4. Add or update deterministic tests when behavior changes.
5. Run the complete local gate:

   ```bash
   cmake --preset dev
   cmake --build --preset dev
   cmake --build --preset check
   ctest --preset dev --output-on-failure
   git diff --check
   ```

6. Inspect the complete diff for unrelated files, secrets, generated output,
   local paths, and private game data.
7. Open a pull request and complete every applicable section of the template.

For visible UI changes, include a screenshot or recording that contains no
game, account, character, spawn, or local-path data. For live-client changes,
record the exact supported fingerprint and observable test results without
publishing proprietary artifacts.

## Code and documentation style

- Use C++20 and preserve warnings-as-errors builds.
- Keep process access behind the narrow Linux integration boundary.
- Pass immutable snapshots to the UI; do not expose process addresses to
  rendering code.
- Use simple ASCII punctuation unless a file format requires otherwise.
- Keep documentation lines within 100 characters where practical.
- Do not add a dependency without documenting its license, purpose, and
  removal impact.

## License

By contributing, you agree that your contribution is licensed under the
project's [GNU General Public License v3.0 only](LICENSE).
