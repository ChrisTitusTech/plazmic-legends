from __future__ import annotations

import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
EXPORTER = ROOT / "tools" / "export_private_ui_bundle.sh"


class ExportPrivateUiBundleTests(unittest.TestCase):
    def test_exports_skin_and_selected_ini_profiles(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            game_dir = root / "EverQuest Legends"
            skin_dir = game_dir / "uifiles" / "plazmic-ui"
            skin_dir.mkdir(parents=True)
            (game_dir / "eqgame.exe").write_bytes(b"synthetic")
            (skin_dir / "EQUI.xml").write_text("<XML />\n", encoding="ascii")
            (skin_dir / "asset.tga").write_bytes(b"asset")
            (skin_dir / "stale.crc").write_text("ignored\n", encoding="ascii")
            (game_dir / "eqclient.ini").write_text(
                "[ChatFilters]\nFilter=1\n", encoding="ascii"
            )
            (game_dir / "UI_alpha_test.ini").write_text(
                "[Main]\nXPos=10\n", encoding="ascii"
            )
            (game_dir / "alpha_test.ini").write_text(
                "[HotButtons]\nPage=1\n[ADDITIONALFILTERS]\nFilter=1\n",
                encoding="ascii",
            )
            (game_dir / "voice_test.ini").write_text(
                "[Voice]\nEnabled=1\n", encoding="ascii"
            )
            destination = root / "private bundle"

            completed = subprocess.run(
                [
                    str(EXPORTER),
                    "--game-dir",
                    str(game_dir),
                    "--resolution",
                    "2560x1440",
                    "--destination",
                    str(destination),
                ],
                check=False,
                capture_output=True,
                text=True,
                env={**os.environ, "EQ_LEGENDS_DIR": ""},
            )

            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertTrue((destination / "uifiles/plazmic-ui/EQUI.xml").is_file())
            self.assertFalse((destination / "uifiles/plazmic-ui/stale.crc").exists())
            self.assertTrue((destination / "ini/eqclient.ini").is_file())
            self.assertTrue((destination / "ini/layouts/UI_alpha_test.ini").is_file())
            self.assertTrue((destination / "ini/characters/alpha_test.ini").is_file())
            self.assertFalse((destination / "ini/characters/voice_test.ini").exists())
            self.assertIn(
                "resolution=2560x1440",
                (destination / "bundle.ini").read_text(encoding="ascii"),
            )
            self.assertTrue((destination / "SHA256SUMS").is_file())
            self.assertTrue(Path(f"{destination}.tar.gz").is_file())

    def test_rejects_destination_inside_game_directory(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            game_dir = Path(temp_dir) / "EverQuest Legends"
            skin_dir = game_dir / "uifiles" / "plazmic-ui"
            skin_dir.mkdir(parents=True)
            (game_dir / "eqgame.exe").write_bytes(b"synthetic")
            (game_dir / "eqclient.ini").write_text("[Main]\n", encoding="ascii")
            (skin_dir / "EQUI.xml").write_text("<XML />\n", encoding="ascii")

            completed = subprocess.run(
                [
                    str(EXPORTER),
                    "--game-dir",
                    str(game_dir),
                    "--resolution",
                    "2560x1440",
                    "--destination",
                    str(game_dir / "private bundle"),
                ],
                check=False,
                capture_output=True,
                text=True,
            )

            self.assertNotEqual(completed.returncode, 0)
            self.assertIn("outside the game directory", completed.stderr)

    def test_refresh_restores_directory_and_archive_when_archive_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            game_dir = root / "EverQuest Legends"
            skin_dir = game_dir / "uifiles" / "plazmic-ui"
            skin_dir.mkdir(parents=True)
            (game_dir / "eqgame.exe").write_bytes(b"synthetic")
            (game_dir / "eqclient.ini").write_text(
                "[ChatFilters]\nMode=old\n", encoding="ascii"
            )
            (game_dir / "UI_alpha_test.ini").write_text(
                "[Main]\nXPos=10\n", encoding="ascii"
            )
            (game_dir / "alpha_test.ini").write_text(
                "[HotButtons]\nPage=1\n", encoding="ascii"
            )
            (skin_dir / "EQUI.xml").write_text("<old />\n", encoding="ascii")
            destination = root / "private bundle"
            base_command = [
                str(EXPORTER),
                "--game-dir",
                str(game_dir),
                "--resolution",
                "2560x1440",
                "--destination",
                str(destination),
            ]
            subprocess.run(base_command, check=True, capture_output=True, text=True)
            old_manifest = (destination / "SHA256SUMS").read_bytes()
            old_archive = Path(f"{destination}.tar.gz").read_bytes()
            old_equi = (
                destination / "uifiles" / "plazmic-ui" / "EQUI.xml"
            ).read_bytes()

            (skin_dir / "EQUI.xml").write_text("<new />\n", encoding="ascii")
            fake_bin = root / "fake-bin"
            fake_bin.mkdir()
            fake_tar = fake_bin / "tar"
            fake_tar.write_text("#!/bin/sh\nexit 1\n", encoding="ascii")
            fake_tar.chmod(0o700)
            failed = subprocess.run(
                [*base_command, "--refresh"],
                check=False,
                capture_output=True,
                text=True,
                env={**os.environ, "PATH": f"{fake_bin}:{os.environ['PATH']}"},
            )

            self.assertNotEqual(failed.returncode, 0)
            self.assertEqual(
                (destination / "uifiles" / "plazmic-ui" / "EQUI.xml").read_bytes(),
                old_equi,
            )
            self.assertEqual((destination / "SHA256SUMS").read_bytes(), old_manifest)
            self.assertEqual(Path(f"{destination}.tar.gz").read_bytes(), old_archive)
            self.assertEqual(list(root.glob("private bundle.backup-*")), [])


if __name__ == "__main__":
    unittest.main()
