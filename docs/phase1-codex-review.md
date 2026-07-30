# Phase 1 Codex review

Review date: 2026-07-29.

## Scope

Codex independently reviewed the complete `main...phase1/native-linux-proof`
diff after the external review service reached its rate limit. The review
covered:

- executable hashing and compatibility rejection;
- `/proc` parsing, identity selection, and remote-read behavior;
- X11 window selection, input shape, hotkey, lifecycle, and resource cleanup;
- compiler and dependency configuration;
- synthetic and live-test coverage;
- documentation claims and phase-boundary compliance; and
- the current official Daybreak rules relevant to continued work.

No reviewer-provided command or code was executed without checking it against
the repository requirements.

## Findings resolved

### Process and compatibility

- Changed an empty `DiscoveryResult` to fail by default.
- Normalized `/proc`, `/proc/`, and `/proc/.` before selecting live-read
  behavior.
- Rejected executable mappings marked deleted.
- Distinguished inaccessible `eqgame.exe` metadata from an ordinary
  non-match.
- Kept process liveness inside the Linux integration boundary.
- Rejected invalid address ranges, permission fields, and overflowing UIDs.
- Replaced stale-`errno` reporting for short `process_vm_readv` reads with a
  deterministic error.
- Replaced the special-case two-byte probe with a reusable reader that enforces
  readable single-mapping bounds and clears partial-read output.
- Added live DOS, PE/COFF, and PE32+ identity validation, including Wine's
  relocated ImageBase invariant.

### Hashing

- Replaced whole-file buffering with incremental 64-byte SHA-256 processing.
- Added vectors at 0, 55, 56, 63, 64, and 65 bytes to exercise both padding
  branches and the block boundary.

### X11 lifecycle

- Preserved setup errors instead of clearing them before the F11 stage.
- Switched both initial and follow-up placement to root-relative
  `XTranslateCoordinates`.
- Restored prior SIGINT, SIGTERM, and X11 error handlers after each run.
- Hid the overlay whenever its target window is not viewable.
- Added three-cycle X11 target-exit coverage using real child-owned windows.
- Added isolated Xvfb/XTest coverage for F11 hide, re-show, and cleanup.
- Added target-window visibility handling.
- Enabled PIE, immediate binding, RELRO, a non-executable stack, and strong
  stack protection for the proof executable.

### Evidence and scope

- Replaced a user-specific prefix in documentation with a neutral reference.
- Added reproducible commands and observable results for live evidence.
- Made least-privilege evidence explicit in the task ledger.
- Kept the architecture described as a proof pending the phase decision.

## Validation

The corrected code passed:

- GCC C++20 compilation with warnings as errors;
- Markdownlint and Ruff;
- Python PE-inspector tests;
- C++ hashing and synthetic `/proc` tests;
- three real X11 target-window exit cycles;
- isolated F11 hide and re-show;
- exact supported-client fingerprinting;
- live two-byte PE-header verification;
- live bounded PE identity reads;
- X11 empty-input-region verification;
- timed overlay exit with no orphan process;
- a one-file local install smoke test containing only the native ELF; and
- ELF hardening inspection showing PIE, BIND_NOW, RELRO, and a non-executable
  stack.

ASan and UBSan were attempted but skipped because the reference host does not
have Fedora's `libasan` or `libubsan` runtime packages installed. No package or
host configuration was changed for an optional review check.

## Review conclusion

No unresolved critical or major implementation finding remains in the Phase 1
proof. The owner explicitly accepted the documented EULA conflict for private
read-only development. That decision does not authorize writes, injection,
automation, protection bypass, push, or distribution.
