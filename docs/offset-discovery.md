# Client offset discovery

This is the normative offset-discovery workflow for a patched EverQuest
Legends `eqgame.exe`. Use it together with
[Compatibility profile refresh](profile-refresh.md). A changed SHA-256 is a
new client build: keep every existing profile immutable and treat every old
RVA, field offset, bound, and structure assumption as an untrusted hypothesis
until it passes this workflow again.

This workflow approves only the current read-only fields: local-player and
world state, zone identity and short name, player position and heading, the
bounded spawn collection, character identity, current and maximum HP, current
and maximum mana, and bounded equipped slot and item-name values. A different
read or integration technique requires its own capability phase and discovery
procedure; this document neither approves nor permanently prohibits it.
Protection bypass remains outside the project boundary.

## Discovery map

```text
new executable identity
        |
        v
static strings and x86-64 cross-references
        |
        v
candidate globals, field offsets, and invariants
        |
        v
same-user bounded read-only observations
        |
        v
two controlled ground-truth confirmations per field
        |
        v
synthetic success and rejection coverage
        |
        v
new immutable ClientProfile and complete live gate
```

An old offset, a familiar nearby string, a matching timestamp, or one
successful memory read is never enough. A candidate reaches a profile only
after its static data flow, runtime type and range, lifecycle behavior, and
controlled visible result agree.

## Inputs and private workspace

Required inputs are:

- the newly patched, user-owned `eqgame.exe` outside the repository;
- the prior immutable `ClientProfile` and its research checkpoints;
- the new executable fingerprint;
- GNU `file`, `objdump`, `strings`, and `rg`;
- the existing same-user `ProcessMemoryReader`; and
- controlled in-game observations on the approved Fedora/Wine/X11 tier.

If the previous executable is still available locally, it can make function
matching easier. It remains proprietary local input and must never enter the
repository, a package, an issue, or a shared artifact. The workflow must still
work from the prior profile and privacy-safe checkpoints when that binary is
not available.

Keep disassembly, string tables, screenshots, temporary probes, runtime
addresses, and observation notes in a private temporary directory. Do not
commit them. A committed profile research note contains only the executable
identity, profile-local RVAs and record offsets, validation invariants,
privacy-safe aggregate results, and tool/provenance versions.

## Step 1: Capture and freeze the new identity

Set the installation directory without recording its absolute Wine-prefix
path:

```bash
export EQ_LEGENDS_DIR='/path/to/Installed Games/EverQuest Legends'
python3 tools/inspect_eqgame.py "$EQ_LEGENDS_DIR"
```

Record the following in a new `docs/research/` profile note before examining
gameplay fields:

- file size and SHA-256;
- PE machine and optional-header format;
- PE timestamp, image base, image size, and entry point;
- Wine version and host architecture; and
- the date and exact project commit used for research.

Do not add the new hash to `src/game/client_profile.cpp` merely to make the
application attach. First create a research branch and keep the candidate
profile unavailable to release builds until the remaining steps pass.

## Step 2: Build a static comparison workspace

Generate private, local-only indexes for the new executable:

```bash
readonly NEW_EQ_CLIENT="$EQ_LEGENDS_DIR/eqgame.exe"
OFFSET_WORK_DIR="$(mktemp -d -t plazmic-offsets.XXXXXXXX)"

file "$NEW_EQ_CLIENT"
objdump -f "$NEW_EQ_CLIENT" > "$OFFSET_WORK_DIR/new.file.txt"
objdump -h "$NEW_EQ_CLIENT" > "$OFFSET_WORK_DIR/new.sections.txt"
objdump -x "$NEW_EQ_CLIENT" > "$OFFSET_WORK_DIR/new.headers.txt"
strings -a -t x -n 8 "$NEW_EQ_CLIENT" \
  > "$OFFSET_WORK_DIR/new.strings.txt"
objdump -d -M intel -j .text "$NEW_EQ_CLIENT" \
  > "$OFFSET_WORK_DIR/new.text.asm"
```

These files are derived from the proprietary client. Keep them outside Git
and delete the private workspace after the research record is complete.

When a prior local executable is available, produce the same four indexes with
an `old.` prefix. Compare semantic anchors and instruction shapes, not absolute
instruction addresses or whole-file diffs. Compiler and linker changes can
move every function while preserving the relevant data flow.

### Address vocabulary

Keep these values distinct:

| Value | Meaning | May enter a profile? |
| --- | --- | --- |
| File offset | Byte position in the PE file | No |
| Preferred VA | PE image base plus RVA | No |
| Live VA | Wine mapping base plus RVA | Never commit |
| Image RVA | Offset from the validated live image base | Yes, for globals |
| Record offset | Offset from a validated object or entry pointer | Yes |

`strings -t x` reports a file offset. Convert it through the containing PE
section before treating it as a virtual address:

```text
string VA = section VMA + (string file offset - section file offset)
image RVA = candidate VA - PE image base
live VA = validated Wine image base + image RVA
```

For an x86-64 RIP-relative instruction, resolve the referenced target from the
address of the next instruction plus its signed displacement. GNU `objdump`
normally prints the calculated target in the instruction comment. Verify that
the target lies in the expected PE section before classifying it as code,
read-only data, or a writable global.

## Step 3: Recover semantic cross-references

Use strings only as navigation landmarks. Search the private index for
diagnostics associated with these behaviors:

- local-player unavailable or player lifecycle;
- zone conversion, lookup, switching, or short-name handling;
- player add/remove lifecycle;
- bounded name handling; and
- level, type, or identity accessors.

Do not commit literal client strings. Convert a selected string file offset to
its VA, then locate instructions that reference that VA:

```bash
rg -n -i 'player|zone|name|level' "$OFFSET_WORK_DIR/new.strings.txt"
rg -n -C 12 'TARGET_VA_WITHOUT_0x' "$OFFSET_WORK_DIR/new.text.asm"
```

Old record offsets may also be searched as comparison hints after a semantic
function has been located. Never promote a numeric match by itself:

```bash
rg -n -C 8 '0xOLD_RECORD_OFFSET' "$OFFSET_WORK_DIR/new.text.asm"
```

Classify every match by its base register, operand width, surrounding control
flow, and relationship to the validated semantic landmark. Common constants
can appear in unrelated functions.

A displacement is not a record offset until the base register is proven to be
the validated record owner. In particular, stack-frame, string-capacity, and
virtual-table displacements can share the same numeric value as a plausible
field and must be rejected even when a bounded live read happens to look
stable.

For each candidate function, record privately:

1. the semantic landmark used to reach it;
2. the RIP-relative global or record field it accesses;
3. operand width and signedness;
4. surrounding branch, mask, indexing, and null-check behavior; and
5. the equivalent old-profile behavior, if one exists.

A field is stronger when several independent functions access it with the same
width and meaning. A nearby string without matching instruction data flow is
not evidence for an offset.

## Step 4: Re-establish the player and world globals

### Local-player global

Find code paths that guard player-dependent work with a RIP-relative pointer
load and null check. The candidate must:

- resolve to a pointer-sized value in writable image data;
- be referenced by more than one player-related code path;
- be null at character select and non-null only in world;
- point inside one readable live mapping; and
- lead to independently validated position, heading, zone, and spawn fields.

Store the final value as an image RVA, never a preferred or live VA.

### World-data global

Start from zone conversion or zone-entry lookup code. Identify the
RIP-relative world pointer, then follow the bounded table-indexing data flow.
The candidate must connect the live player's zone ID to an entry whose ID and
short name agree. Confirm this chain in at least two zones.

Do not infer the world pointer merely because it is adjacent to the old local
player global. Adjacency is a comparison hint, not validation.

## Step 5: Re-establish player fields

Use the validated local-player pointer as the only record base. Static
evidence and live evidence must agree as follows:

| Field | Static evidence | Controlled live evidence |
| --- | --- | --- |
| X, Y, Z | Three floating-point accesses used together by movement or position code | Values match visible coordinates at multiple positions; directional movement changes the expected axes |
| Heading | Floating-point access with client heading arithmetic or bounds | At least two controlled facings agree after conversion from `[0, 512)` |
| Zone ID | Integer access participating in masking or zone lookup | ID is stable in one zone, changes on zoning, and resolves through the world table |

Validate finite values and the implementation coordinate bound. Reconfirm the
map transform independently:

```text
map X = -player Y
map Y = -player X
```

The transform is renderer logic, not a client offset. Do not alter it merely
because a client update moves player fields.

## Step 6: Re-establish the zone table and entry

Trace the validated zone ID through the world-data lookup routine. Recover and
validate:

- any zone-ID mask;
- the maximum accepted zone ID;
- the table offset from world data;
- pointer indexing width;
- the entry's zone-ID field;
- the short-name field and maximum byte count; and
- the null, bounds, and path-character rejection rules.

The entry ID must equal the masked live zone ID. The short name must be
NUL-terminated, pass `valid_zone_short_name`, and select the installed
`maps/<zone>.txt` file for two controlled zones. A plausible string elsewhere
in the entry is insufficient.

## Step 7: Re-establish the spawn collection

The validated local-player record is an anchor inside the spawn collection.
It is not guaranteed to be the root. The original Phase 4 research initially
observed the player at the root; a later live run found a valid predecessor.
The durable invariant is a bounded reciprocal list containing the player
anchor.

### Links and root

Locate player add/remove or traversal code and identify pointer-sized next and
previous accesses. A candidate list must satisfy all of these:

- the player anchor has the same nonzero record identity as other entries;
- following previous links reaches a null-previous root within the bound;
- every `previous->next` and `next->previous` relationship is reciprocal;
- forward traversal contains the original player anchor;
- traversal ends at a null next link without a cycle; and
- a second traversal returns the same ordered addresses.

Reject any candidate that works only while the local player happens to be the
first entry.

### Spawn fields

Recover each field independently from instructions that access the validated
record base:

| Field | Required evidence |
| --- | --- |
| Display name | Static bounded string use plus a terminated printable live value within the approved byte limit |
| Type | Byte access plus controlled examples for player, NPC, and corpse when available; unknown values reject the snapshot |
| Stable ID | Unsigned 32-bit access, nonzero and unique across a collection, stable across repeated reads |
| Level | Byte-sized getter or use, nonzero, and visibly confirmed twice |
| Position | The same three finite floating-point semantics used by player position, confirmed against map placement |
| Record bound | Smallest byte span containing every approved field; it is not permission to read an entire unknown structure |
| Maximum count | Evidence-based ceiling above controlled observations and no higher than the implementation cap |

## Step 7a: Re-establish character vitals and equipment

Use the validated local-player identity as the lifecycle anchor, but do not
assume that vitals or inventory live in the spawn record. Follow independent
character-window, health-bar, mana-bar, and equipment-slot instruction paths to
their owning object and prove every pointer hop, index, width, and bound.

| Field | Required evidence |
| --- | --- |
| Character identity | Bounded character-name use tied to the validated local player; the value selects exactly one local combat log and is never logged or persisted |
| Current HP | Signed or unsigned operand width and health-bar data flow; the value is nonnegative and independently confirmed at two visible values |
| Maximum HP | A separately owned raw or stable cached value with proven width and bar/percentage data flow; the character snapshot is unavailable if this required value cannot be established |
| Current and maximum mana | Independent mana-bar or spell-resource data flow with the same range and lifecycle validation |
| Equipment container | Pointer or inline array reached from an equipment UI or gameplay accessor, with an explicit fixed slot count and readable-mapping checks |
| Equipped item | Null for empty or a validated readable object for occupied; no whole-object dump or unbounded graph traversal |
| Item name | Bounded terminated printable text reached from the validated item object; text is UI-only and excluded from diagnostics and evidence |

Confirm HP and mana at two visibly different values and validate maximum values
against the client UI. Confirm each supported occupied slot against two
controlled equipment configurations; empty slots must remain explicitly empty.
Re-read the local-player identity, vitals owner, equipment container, and slot
pointers after staging the snapshot. Reject the entire character snapshot if a
required identity, pointer, range, slot, string, or consistency invariant
changes during the read.

Upstream or historical projects may suggest field names and types only. Record
the exact upstream revision consulted, then prove the Legends instruction and
live behavior independently. Never copy an upstream offset, structure, byte
pattern, generated table, or implementation into the profile.

Every supported character profile requires a separately validated maximum-HP
cache. Never infer it from a displayed percentage, current HP, a historical
structure, or another client's profile. If the cache cannot be re-established
for an exact digest, keep character snapshots unavailable for that digest.

## Step 8: Perform bounded live confirmation

Do not use a general-purpose dump or unbounded scan. Candidate reads must go
through the same same-user discovery and `ProcessMemoryReader` mapping checks
used by the product. A temporary research probe, if needed, stays outside the
repository and must:

- accept the exact PID selected by `discover_client_process`;
- use only proposed profile RVAs and bounded record offsets;
- call `read_exact` for known-size fields;
- print no names, raw addresses, memory bytes, or Wine-prefix paths;
- report only field validity and privacy-safe aggregates; and
- contain no write-capable process API.

For each displayed field, capture two controlled ground-truth observations.
Use different positions, facings, zones, or visible spawn examples so the
second observation can disprove a coincidental match. Exercise character
select, entering world, zoning, camping, process exit, and reacquisition.

Reject the entire candidate snapshot when a pointer, mapping, identity, link,
string, type, ID, level, coordinate, collection count, or consistency check
fails. Never publish a partially validated field or keep the previous snapshot
as current.

## Step 9: Record provenance and implement the new profile

Create a new privacy-safe research note modeled on the existing
[Phase 3 player/map checkpoint](research/phase3-player-map-checkpoint.md) and
[Phase 4 spawn checkpoint](research/phase4-spawn-checkpoint.md). Include an
evidence table with one row per profile value:

| Profile value | Static resolver | Observation 1 | Observation 2 | Rejection rule | Final RVA or offset |
| --- | --- | --- | --- | --- | --- |
| Example field | Function/data-flow category, no proprietary bytes | Privacy-safe result | Privacy-safe result | Exact validation | Profile-local value |

Then add a new `ClientProfile` keyed by the new SHA-256 and PE identity. Keep
the old profile unchanged. Profile code may contain only:

- identity metadata;
- image RVAs for validated globals;
- record offsets for approved fields;
- string, record, and collection bounds; and
- masks or numeric bounds required by validation.

Raw live addresses, process IDs, names, screenshots, memory bytes, personal
paths, and temporary probe output do not belong in source, tests, logs, or the
research note.

## Step 10: Run the complete acceptance gate

Before live testing, add synthetic fixtures for the new profile and every
rejection path. Run:

```bash
cmake --preset dev --fresh
cmake --build --preset dev
cmake --build --preset check
ctest --preset dev --output-on-failure
python3 tools/inspect_eqgame.py "$EQ_LEGENDS_DIR" \
  --expect-sha256 NEW_SHA256
git diff --check
```

The live gate then requires:

- exact file SHA-256 and live PE identity;
- zero and ambiguous-process rejection;
- character select, entering world, and two-zone validation;
- multiple player positions and headings;
- two controlled observations for every displayed spawn field;
- spawn add, remove, change, and selection behavior;
- zoning, camping, process exit, and process reacquisition;
- missing and malformed map behavior;
- reader and UI performance without visible game regression;
- DWM placement and game-window invariance; and
- a privacy audit of logs, fixtures, diffs, packages, screenshots, and
  temporary artifacts.

Only after every automated and live criterion passes may the new profile enter
a release. If any required field cannot be re-established, keep the updated
client unsupported. Never weaken matching or carry an old offset forward to
restore apparent compatibility.

## How the current profile was established

The current checkpoints demonstrate the workflow and preserve the reusable
reasoning:

| Resolver | How it was found | Durable lesson |
| --- | --- | --- |
| Local player | Static x86-64 cross-references from player lifecycle diagnostics to a RIP-relative global, followed by coherent live position and lifecycle checks | Store an image RVA and require null/non-null lifecycle behavior |
| World data and zone | Static zone-conversion and lookup data flow, followed by live ID-to-entry-to-short-name confirmation | Confirm entry ID and short name in two zones |
| Player position and heading | Grouped floating-point field accesses plus controlled movement, facing, and installed-map comparison | Static type and live ground truth must agree |
| Spawn links | Player record as a live anchor, reciprocal link inspection, bounded traversal, and repeated collection reads | The anchor may be in the middle; walk backward to the validated root |
| Name, type, ID, level, and position | Conceptual field hypotheses, independent Legends disassembly, operand-width checks, and repeated controlled live observations | Upstream layouts are hypotheses only; prove every Legends field independently |
| Map geometry | User-installed map text files selected by the validated zone short name | Map geometry has no process-memory offset |

The exact current values and invariants remain in the Phase 3 and Phase 4
checkpoint documents and `src/game/client_profile.cpp`. This runbook explains
how to find their successors after a patch rather than treating the current
numbers as reusable.
