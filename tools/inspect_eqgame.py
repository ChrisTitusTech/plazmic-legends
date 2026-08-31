#!/usr/bin/env python3
"""Read identity fields from an EverQuest Legends PE executable."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import BinaryIO

MACHINES = {
    0x014C: "x86",
    0x8664: "x86-64",
    0xAA64: "arm64",
}

SUBSYSTEMS = {
    2: "windows-gui",
    3: "windows-console",
}


class InspectionError(Exception):
    """The target is missing, unreadable, or not a valid PE executable."""


def read_exact(handle: BinaryIO, offset: int, size: int) -> bytes:
    handle.seek(offset)
    data = handle.read(size)
    if len(data) != size:
        raise InspectionError(
            f"file ended while reading {size} bytes at offset 0x{offset:x}"
        )
    return data


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def inspect_pe(path: Path) -> dict[str, object]:
    try:
        stat = path.stat()
    except OSError as error:
        raise InspectionError(f"cannot stat {path}: {error}") from error

    if not path.is_file():
        raise InspectionError(f"target is not a file: {path}")

    try:
        with path.open("rb") as handle:
            if read_exact(handle, 0, 2) != b"MZ":
                raise InspectionError("missing DOS MZ signature")

            pe_offset = struct.unpack("<I", read_exact(handle, 0x3C, 4))[0]
            if read_exact(handle, pe_offset, 4) != b"PE\0\0":
                raise InspectionError("missing PE signature")

            coff = read_exact(handle, pe_offset + 4, 20)
            (
                machine,
                section_count,
                timestamp,
                _symbol_table,
                _symbol_count,
                optional_size,
                characteristics,
            ) = struct.unpack("<HHIIIHH", coff)

            optional = read_exact(handle, pe_offset + 24, optional_size)
            if len(optional) < 72:
                raise InspectionError("optional header is too short")

            optional_magic = struct.unpack_from("<H", optional, 0)[0]
            if optional_magic == 0x20B:
                pe_format = "PE32+"
                image_base = struct.unpack_from("<Q", optional, 24)[0]
            elif optional_magic == 0x10B:
                pe_format = "PE32"
                image_base = struct.unpack_from("<I", optional, 28)[0]
            else:
                raise InspectionError(
                    f"unknown PE optional-header magic 0x{optional_magic:04x}"
                )

            entry_point = struct.unpack_from("<I", optional, 16)[0]
            image_size = struct.unpack_from("<I", optional, 56)[0]
            subsystem = struct.unpack_from("<H", optional, 68)[0]
    except OSError as error:
        raise InspectionError(f"cannot read {path}: {error}") from error

    return {
        "file": path.name,
        "size_bytes": stat.st_size,
        "sha256": sha256_file(path),
        "pe_format": pe_format,
        "machine": MACHINES.get(machine, f"unknown-0x{machine:04x}"),
        "machine_hex": f"0x{machine:04x}",
        "section_count": section_count,
        "pe_timestamp_utc": datetime.fromtimestamp(timestamp, timezone.utc).isoformat(),
        "pe_timestamp_raw": timestamp,
        "image_base_hex": f"0x{image_base:x}",
        "image_size_hex": f"0x{image_size:x}",
        "entry_point_rva_hex": f"0x{entry_point:x}",
        "subsystem": SUBSYSTEMS.get(subsystem, f"unknown-{subsystem}"),
        "characteristics_hex": f"0x{characteristics:04x}",
    }


def resolve_target(value: str) -> Path:
    target = Path(value).expanduser()
    if target.is_dir():
        target = target / "eqgame.exe"
    return target.resolve()


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=("Inspect eqgame.exe identity without launching or modifying it.")
    )
    parser.add_argument(
        "target",
        help="path to eqgame.exe or the EverQuest Legends installation",
    )
    parser.add_argument(
        "--expect-sha256",
        metavar="HEX",
        help="exit with status 2 unless the executable has this SHA-256",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="emit stable JSON instead of the human-readable report",
    )
    return parser


def print_human(result: dict[str, object]) -> None:
    labels = (
        ("file", "File"),
        ("size_bytes", "Size"),
        ("sha256", "SHA-256"),
        ("pe_format", "PE format"),
        ("machine", "Machine"),
        ("machine_hex", "Machine code"),
        ("section_count", "Sections"),
        ("pe_timestamp_utc", "PE timestamp"),
        ("image_base_hex", "Image base"),
        ("image_size_hex", "Image size"),
        ("entry_point_rva_hex", "Entry point RVA"),
        ("subsystem", "Subsystem"),
        ("characteristics_hex", "Characteristics"),
    )
    for key, label in labels:
        print(f"{label}: {result[key]}")


def main() -> int:
    args = build_parser().parse_args()
    target = resolve_target(args.target)

    try:
        result = inspect_pe(target)
    except InspectionError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    expected = args.expect_sha256
    if expected is not None:
        expected = expected.strip().lower()
        is_hex = all(char in "0123456789abcdef" for char in expected)
        if len(expected) != 64 or not is_hex:
            print(
                "error: --expect-sha256 must be 64 hexadecimal characters",
                file=sys.stderr,
            )
            return 1

    if args.json:
        print(json.dumps(result, indent=2, sort_keys=True))
    else:
        print_human(result)

    if expected is not None and result["sha256"] != expected:
        print(
            "error: executable SHA-256 does not match the expected build",
            file=sys.stderr,
        )
        return 2

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
