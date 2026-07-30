#!/usr/bin/env python3
"""Tests for the read-only PE identity inspector."""

from __future__ import annotations

import hashlib
import importlib.util
import struct
import tempfile
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).resolve().parents[1] / "tools" / "inspect_eqgame.py"
MODULE_SPEC = importlib.util.spec_from_file_location("inspect_eqgame", MODULE_PATH)
if MODULE_SPEC is None or MODULE_SPEC.loader is None:
    raise RuntimeError(f"cannot load inspection module from {MODULE_PATH}")

inspect_eqgame = importlib.util.module_from_spec(MODULE_SPEC)
MODULE_SPEC.loader.exec_module(inspect_eqgame)


def make_pe32_plus() -> bytes:
    pe_offset = 0x80
    optional_size = 0xF0
    image = bytearray(pe_offset + 24 + optional_size)
    image[0:2] = b"MZ"
    struct.pack_into("<I", image, 0x3C, pe_offset)
    image[pe_offset : pe_offset + 4] = b"PE\0\0"
    struct.pack_into(
        "<HHIIIHH",
        image,
        pe_offset + 4,
        0x8664,
        7,
        1_753_892_433,
        0,
        0,
        optional_size,
        0x22,
    )

    optional_offset = pe_offset + 24
    struct.pack_into("<H", image, optional_offset, 0x20B)
    struct.pack_into("<I", image, optional_offset + 16, 0x732470)
    struct.pack_into("<Q", image, optional_offset + 24, 0x140000000)
    struct.pack_into("<I", image, optional_offset + 56, 0x16C1000)
    struct.pack_into("<H", image, optional_offset + 68, 2)
    return bytes(image)


class InspectEqgameTests(unittest.TestCase):
    def test_inspects_pe32_plus_identity(self) -> None:
        payload = make_pe32_plus()
        with tempfile.TemporaryDirectory() as temp_dir:
            target = Path(temp_dir) / "eqgame.exe"
            target.write_bytes(payload)

            result = inspect_eqgame.inspect_pe(target)

        self.assertEqual(result["file"], "eqgame.exe")
        self.assertEqual(result["size_bytes"], len(payload))
        self.assertEqual(result["sha256"], hashlib.sha256(payload).hexdigest())
        self.assertEqual(result["pe_format"], "PE32+")
        self.assertEqual(result["machine"], "x86-64")
        self.assertEqual(result["section_count"], 7)
        self.assertEqual(result["image_base_hex"], "0x140000000")
        self.assertEqual(result["image_size_hex"], "0x16c1000")
        self.assertEqual(result["entry_point_rva_hex"], "0x732470")
        self.assertEqual(result["subsystem"], "windows-gui")

    def test_rejects_non_pe_file(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            target = Path(temp_dir) / "eqgame.exe"
            target.write_text("not a PE executable", encoding="utf-8")

            with self.assertRaisesRegex(
                inspect_eqgame.InspectionError, "missing DOS MZ signature"
            ):
                inspect_eqgame.inspect_pe(target)

    def test_resolves_installation_directory(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            install_dir = Path(temp_dir)
            expected = (install_dir / "eqgame.exe").resolve()

            result = inspect_eqgame.resolve_target(str(install_dir))

        self.assertEqual(result, expected)


if __name__ == "__main__":
    unittest.main()
