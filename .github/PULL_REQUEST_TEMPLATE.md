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

- [ ] The change remains external and read-only.
- [ ] Unknown or changed clients still fail closed.
- [ ] No game files, maps, Wine-prefix content, credentials, memory captures,
      personal paths, or private runtime data are included.
- [ ] No injection, memory writes, automation, protection bypass, telemetry,
      scripting, plugins, or remote control were added.
- [ ] New dependencies include license, purpose, and removal-impact notes.
- [ ] Package inventory and user documentation were updated when applicable.

## User-visible changes

Add privacy-safe screenshots or recordings for visual changes. Otherwise write
"Not applicable."
