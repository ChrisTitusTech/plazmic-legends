# MacroQuest baseline boundary review

Reviewed from the read-only `phase0-import-baseline` tag on 2026-07-29. No
baseline source was restored or copied into the active tree.

## Safe concept retained

`src/loader/RemoteOps.cpp` performs staged remote-module validation:

1. resolve a remote module base;
2. read and validate the DOS signature;
3. follow the bounded PE-header offset;
4. read and validate the NT signature and file header; and
5. select and validate the matching optional-header format.

The native Linux `ProcessMemoryReader` independently implements that general
read-and-validate sequence with `process_vm_readv`. It adds `/proc` readable
mapping bounds, exact-read results, overflow rejection, partial-buffer
clearing, same-user selection, and an exact Legends profile. It does not reuse
Windows code, types, offsets, or error handling.

## Unsafe concepts rejected

The preserved baseline also contains Windows paths using remote allocation,
`WriteProcessMemory`, remote threads, DLL loading, APC injection, Detours,
AutoLogin, and other automation. Those paths conflict with the Linux-native,
external, read-only project boundary and are not mirrored.

The active project contains no memory-write primitive, injection target,
remote-thread creation, gameplay input, macro engine, login automation, or
anti-cheat/integrity bypass.

## Future use

Phase 2 may consult the baseline only for high-level separation of process,
profile, typed-reader, snapshot, and presentation responsibilities. Any
client-specific structure or symbol must be independently established against
the exact Legends profile and pass the validation rules in
`docs/research/phase1-symbol-plan.md`.
