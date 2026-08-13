# Pull request

## Summary

Describe the problem and the focused solution.

## Validation

List the exact commands run and their results.

- [ ] `cmake --preset dev`
- [ ] `cmake --build --preset dev`
- [ ] `cmake --build --preset check`
- [ ] `ctest --preset dev --output-on-failure`
- [ ] `git diff --check`

## Manual testing

Describe any manual or live-client testing, including the Linux distribution,
display server, Wine runner, installation method, and supported client
fingerprint. Write "Not applicable" when no manual test is needed.

## Safety and scope

Capability phase or maintenance task (required):
<!-- Link the numbered ROADMAP/TASKS contract, or explain why this is not applicable. -->

Owner approval for a new capability:
<!-- Link or quote the approval and include its date, or write "Not applicable". -->

Capability contract for a new capability:
<!-- Link inputs, outputs, side effects, privacy, lifecycle, validation, and
rollback, or write "Not applicable". -->

- [ ] The change maps to an approved capability phase or maintenance task.
- [ ] Unknown or changed clients still fail closed.
- [ ] No game files, maps, Wine-prefix content, credentials, memory captures,
      personal paths, or private runtime data are included.
- [ ] Every new source, sink, side effect, privilege, retained-data path, or
      service has documented consent, privacy, security, lifecycle, bounds,
      validation, and rollback behavior.
- [ ] The change does not bypass authentication, integrity, anti-cheat, or
      another client protection.
- [ ] Network access, uploads, sharing, and update checks remain disabled by
      default unless this approved capability explicitly enables them.
- [ ] New dependencies include license, purpose, and removal-impact notes.
- [ ] Package inventory and user documentation were updated when applicable.

## User-visible changes

Add privacy-safe screenshots or recordings for visual changes. Otherwise write
"Not applicable."
