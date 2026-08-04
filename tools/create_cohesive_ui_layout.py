#!/usr/bin/env python3

from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys


REQUIRED_SECTIONS = {
    "MainChat",
    "Chat 1",
    "GroupWindow",
    "ExtendedTargetWnd",
    "MapViewWnd_6",
    "TargetWindow",
    "HotButtonWnd",
    "HotButtonWnd2",
    "PlayerWindow",
    "CastSpellWnd",
}

# Percent positions retain the Legends anchor model and scale cleanly at
# 2560x1440. Widths and heights match the current Legends UI schema.
OVERRIDES: dict[str, dict[str, str]] = {
    "MainChat": {
        "XRef": "left",
        "YRef": "bottom",
        "XPos": "0.312500%",
        "YPos": "0.555556%",
        "Width": "700",
        "Height": "300",
    },
    "Chat 1": {
        "XRef": "right",
        "YRef": "bottom",
        "XPos": "13.046875%",
        "YPos": "0.555556%",
        "Width": "700",
        "Height": "300",
    },
    "Chat 2": {
        "XRef": "left",
        "YRef": "bottom",
        "XPos": "0.312500%",
        "YPos": "22.500000%",
        "Width": "700",
        "Height": "220",
    },
    "Chat 3": {
        "XRef": "right",
        "YRef": "bottom",
        "XPos": "13.046875%",
        "YPos": "22.500000%",
        "Width": "700",
        "Height": "220",
    },
    "GroupWindow": {
        "XRef": "left",
        "YRef": "top",
        "XPos": "2.656250%",
        "YPos": "0.555556%",
    },
    "ExtendedTargetWnd": {
        "XRef": "left",
        "YRef": "top",
        "XPos": "2.656250%",
        "YPos": "4.166667%",
        "Width": "155",
        "Height": "326",
    },
    "CastSpellWnd": {
        "XRef": "left",
        "YRef": "top",
        "XPos": "0.312500%",
        "YPos": "0.555556%",
        "Width": "52",
        "Height": "456",
    },
    "MapViewWnd_6": {
        "XRef": "right",
        "YRef": "top",
        "XPos": "0.312500%",
        "YPos": "0.555556%",
        "Width": "500",
        "Height": "420",
    },
    "PlayerWindow": {
        "XRef": "center",
        "YRef": "bottom",
        "XPos": "-15.000000%",
        "YPos": "13.250000%",
    },
    "TargetWindow": {
        "XRef": "center",
        "YRef": "bottom",
        "XPos": "13.000000%",
        "YPos": "13.250000%",
    },
    "TargetOfTargetWindow": {
        "XRef": "center",
        "YRef": "bottom",
        "XPos": "0.000000%",
        "YPos": "17.500000%",
    },
    "HotButtonWnd": {
        "XRef": "center",
        "YRef": "bottom",
        "XPos": "0.000000%",
        "YPos": "0.555556%",
        "Width": "525",
        "Height": "52",
    },
    "HotButtonWnd2": {
        "XRef": "center",
        "YRef": "bottom",
        "XPos": "0.000000%",
        "YPos": "4.444444%",
        "Width": "525",
        "Height": "52",
    },
    "StanceWnd": {
        "XRef": "center",
        "YRef": "bottom",
        "XPos": "0.000000%",
        "YPos": "8.333333%",
    },
}

SECTION_PATTERN = re.compile(rb"^\[([^\]]+)\][ \t]*\r?\n$")
KEY_PATTERN = re.compile(rb"^([A-Za-z0-9_.]+)=")


def cohesive_layout(source: bytes) -> bytes:
    newline = b"\r\n" if b"\r\n" in source else b"\n"
    lines = source.splitlines(keepends=True)
    output: list[bytes] = []
    sections: set[str] = set()
    current_section: str | None = None
    seen_keys: set[str] = set()

    def finish_section() -> None:
        if current_section not in OVERRIDES:
            return
        for key, value in OVERRIDES[current_section].items():
            if key not in seen_keys:
                output.append(f"{key}={value}".encode("ascii") + newline)

    for line in lines:
        section_match = SECTION_PATTERN.match(line)
        if section_match:
            finish_section()
            current_section = section_match.group(1).decode("ascii")
            sections.add(current_section)
            seen_keys = set()
            output.append(line)
            continue

        key_match = KEY_PATTERN.match(line)
        if current_section in OVERRIDES and key_match:
            key = key_match.group(1).decode("ascii")
            seen_keys.add(key)
            if key in OVERRIDES[current_section]:
                output.append(
                    f"{key}={OVERRIDES[current_section][key]}".encode("ascii") + newline
                )
                continue
        output.append(line)

    finish_section()
    missing = sorted(REQUIRED_SECTIONS - sections)
    if missing:
        raise ValueError(
            "source is not a compatible Legends 1440p layout; missing sections: "
            + ", ".join(missing)
        )
    return b"".join(output)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Create a cohesive 2560x1440 layout from a Legends UI INI."
    )
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        source = args.source.resolve(strict=True)
        destination = args.destination.resolve(strict=False)
        if source == destination:
            raise ValueError("source and destination must differ")
        transformed = cohesive_layout(source.read_bytes())
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(transformed)
        destination.chmod(0o600)
    except (OSError, UnicodeError, ValueError) as error:
        print(f"create-cohesive-layout: {error}", file=sys.stderr)
        return 1
    print("Created private cohesive 2560x1440 Legends layout")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
