#!/usr/bin/env python3
"""Prepare a private, fail-closed compatibility candidate for eqgame.exe."""

from __future__ import annotations

import argparse
import json
import os
import sys
import tempfile
from datetime import datetime, timezone
from pathlib import Path

from inspect_eqgame import InspectionError, inspect_pe, resolve_target

SCHEMA_VERSION = 1
REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
RESOLVERS = (
    "local_player_global",
    "world_data_global",
    "player_position_heading",
    "player_zone",
    "zone_table_entry",
    "spawn_collection",
    "spawn_identity",
    "spawn_level",
    "spawn_position",
    "character_identity",
    "character_vitals",
    "character_equipment",
    "alternate_advancement",
)
REQUIRED_EVIDENCE = (
    "static_data_flow",
    "live_observation_1",
    "live_observation_2",
    "synthetic_success",
    "synthetic_rejection",
)
RESOLVER_STATES = {"pending", "candidate", "passed", "failed"}


class CandidateError(Exception):
    """The candidate cannot be created without weakening a safety boundary."""


def default_output_root() -> Path:
    state_home = os.environ.get("XDG_STATE_HOME")
    if state_home:
        return Path(state_home).expanduser() / "plazmic-legends/profile-candidates"
    return Path.home() / ".local/state/plazmic-legends/profile-candidates"


def path_is_within(path: Path, parent: Path) -> bool:
    try:
        path.resolve().relative_to(parent.resolve())
    except ValueError:
        return False
    return True


def candidate_document(
    identity: dict[str, object], created_at: datetime
) -> dict[str, object]:
    digest = str(identity["sha256"])
    return {
        "schema_version": SCHEMA_VERSION,
        "candidate_id": f"legends-candidate-{digest[:12]}",
        "created_at_utc": created_at.astimezone(timezone.utc).isoformat(),
        "status": "blocked",
        "identity": {
            "sha256": digest,
            "size_bytes": identity["size_bytes"],
            "machine_hex": identity["machine_hex"],
            "pe_format": identity["pe_format"],
            "section_count": identity["section_count"],
            "pe_timestamp_raw": identity["pe_timestamp_raw"],
            "image_size_hex": identity["image_size_hex"],
            "entry_point_rva_hex": identity["entry_point_rva_hex"],
        },
        "runtime": {
            "selectable": False,
            "reason": "candidate has not passed resolver and promotion gates",
        },
        "resolvers": [
            {
                "id": resolver,
                "status": "pending",
                "required_evidence": list(REQUIRED_EVIDENCE),
                "evidence": [],
                "candidate_value": None,
            }
            for resolver in RESOLVERS
        ],
    }


def write_new_candidate(path: Path, document: dict[str, object]) -> None:
    path.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
    os.chmod(path.parent, 0o700)
    descriptor, temporary_name = tempfile.mkstemp(
        dir=path.parent, prefix=".candidate.", suffix=".tmp"
    )
    temporary = Path(temporary_name)
    try:
        os.fchmod(descriptor, 0o600)
        with os.fdopen(descriptor, "w", encoding="utf-8") as handle:
            json.dump(document, handle, indent=2, sort_keys=True)
            handle.write("\n")
            handle.flush()
            os.fsync(handle.fileno())
        os.link(temporary, path)
    except FileExistsError:
        pass
    finally:
        temporary.unlink(missing_ok=True)


def valid_candidate_document(document: object, digest: str) -> bool:
    if not isinstance(document, dict):
        return False
    identity = document.get("identity")
    runtime = document.get("runtime")
    resolvers = document.get("resolvers")
    if (
        document.get("schema_version") != SCHEMA_VERSION
        or document.get("status") != "blocked"
        or not isinstance(identity, dict)
        or identity.get("sha256") != digest
        or not isinstance(runtime, dict)
        or runtime.get("selectable") is not False
        or not isinstance(resolvers, list)
        or len(resolvers) != len(RESOLVERS)
    ):
        return False
    resolver_ids: list[object] = []
    for resolver in resolvers:
        if (
            not isinstance(resolver, dict)
            or resolver.get("status") not in RESOLVER_STATES
            or resolver.get("required_evidence") != list(REQUIRED_EVIDENCE)
            or not isinstance(resolver.get("evidence"), list)
        ):
            return False
        resolver_ids.append(resolver.get("id"))
    return resolver_ids == list(RESOLVERS)


def prepare_candidate(
    target: Path,
    output_root: Path,
    created_at: datetime | None = None,
) -> Path:
    output_root = output_root.expanduser().resolve()
    if path_is_within(output_root, REPOSITORY_ROOT):
        raise CandidateError("candidate output must remain outside the repository")

    identity = inspect_pe(target)
    if identity["pe_format"] != "PE32+" or identity["machine_hex"] != "0x8664":
        raise CandidateError("candidate is not a Windows x86-64 PE32+ executable")

    digest = str(identity["sha256"])
    candidate_directory = output_root / digest
    candidate_path = candidate_directory / "candidate.json"
    document = candidate_document(identity, created_at or datetime.now(timezone.utc))
    write_new_candidate(candidate_path, document)

    try:
        existing = json.loads(candidate_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise CandidateError(f"candidate cannot be read safely: {error}") from error
    if not valid_candidate_document(existing, digest):
        raise CandidateError("existing candidate does not match the fail-closed schema")
    os.chmod(candidate_path, 0o600)
    return candidate_path


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Prepare an owner-only compatibility candidate without enabling it."
        )
    )
    parser.add_argument(
        "target", help="path to eqgame.exe or the EverQuest Legends installation"
    )
    parser.add_argument(
        "--output-root",
        type=Path,
        default=default_output_root(),
        help="private candidate root outside the repository",
    )
    return parser


def main() -> int:
    args = build_parser().parse_args()
    try:
        candidate = prepare_candidate(resolve_target(args.target), args.output_root)
    except (CandidateError, InspectionError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(f"Prepared blocked compatibility candidate: {candidate}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
