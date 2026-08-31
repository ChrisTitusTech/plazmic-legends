from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "tools" / "create_cohesive_ui_layout.py"

REQUIRED = [
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
]


def fixture() -> bytes:
    sections = []
    for section in REQUIRED:
        sections.append(
            f"[{section}]\r\n"
            "XRef=left\r\nYRef=top\r\nXPos=99.000000%\r\n"
            "YPos=99.000000%\r\nWidth=100\r\nHeight=100\r\n"
            "LegendsOnly=preserved\r\n"
        )
    sections.append("[PrivateUnrelated]\r\nValue=unchanged\r\n")
    return "".join(sections).encode("ascii")


class CreateCohesiveUiLayoutTests(unittest.TestCase):
    def test_preserves_legends_fields_and_reflows_allowlisted_geometry(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            source = root / "source.ini"
            destination = root / "UI_plazmic_1440p.ini"
            source.write_bytes(fixture())

            completed = subprocess.run(
                [str(TOOL), str(source), str(destination)],
                check=False,
                capture_output=True,
                text=True,
            )

            self.assertEqual(completed.returncode, 0, completed.stderr)
            result = destination.read_bytes()
            self.assertIn(b"[MainChat]\r\nXRef=left\r\nYRef=bottom\r\n", result)
            self.assertIn(b"Width=700\r\nHeight=300\r\n", result)
            self.assertIn(
                b"[PlayerWindow]\r\nXRef=center\r\nYRef=bottom\r\n"
                b"XPos=-15.000000%\r\nYPos=13.250000%\r\n",
                result,
            )
            self.assertIn(
                b"[TargetWindow]\r\nXRef=center\r\nYRef=bottom\r\n"
                b"XPos=13.000000%\r\nYPos=13.250000%\r\n",
                result,
            )
            self.assertIn(b"LegendsOnly=preserved\r\n", result)
            self.assertIn(b"[PrivateUnrelated]\r\nValue=unchanged\r\n", result)
            self.assertNotIn(b"XPos=99.000000%", result)

    def test_rejects_non_legends_layout_without_writing_output(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            source = root / "source.ini"
            destination = root / "output.ini"
            source.write_text("[MainChat]\nXPos=0\n", encoding="ascii")

            completed = subprocess.run(
                [str(TOOL), str(source), str(destination)],
                check=False,
                capture_output=True,
                text=True,
            )

            self.assertNotEqual(completed.returncode, 0)
            self.assertIn("missing sections", completed.stderr)
            self.assertFalse(destination.exists())


if __name__ == "__main__":
    unittest.main()
