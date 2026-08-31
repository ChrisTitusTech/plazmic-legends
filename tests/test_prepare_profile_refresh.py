"""Tests for fail-closed compatibility candidate preparation."""

from __future__ import annotations

import importlib.util
import json
import sys
import tempfile
import unittest
from datetime import datetime, timezone
from pathlib import Path

from test_inspect_eqgame import make_pe32_plus

TOOLS_DIRECTORY = Path(__file__).resolve().parents[1] / "tools"
MODULE_PATH = TOOLS_DIRECTORY / "prepare_profile_refresh.py"
MODULE_SPEC = importlib.util.spec_from_file_location(
    "prepare_profile_refresh", MODULE_PATH
)
if MODULE_SPEC is None or MODULE_SPEC.loader is None:
    raise RuntimeError(f"cannot load candidate module from {MODULE_PATH}")
sys.path.insert(0, str(TOOLS_DIRECTORY))
prepare_profile_refresh = importlib.util.module_from_spec(MODULE_SPEC)
MODULE_SPEC.loader.exec_module(prepare_profile_refresh)


class PrepareProfileRefreshTests(unittest.TestCase):
    def test_creates_private_blocked_candidate_without_input_path(self) -> None:
        with (
            tempfile.TemporaryDirectory() as input_dir,
            tempfile.TemporaryDirectory(prefix="plazmic-candidate-test-") as output_dir,
        ):
            client = Path(input_dir) / "eqgame.exe"
            client.write_bytes(make_pe32_plus())

            candidate = prepare_profile_refresh.prepare_candidate(
                client,
                Path(output_dir),
                datetime(2026, 8, 18, tzinfo=timezone.utc),
            )
            payload = candidate.read_text(encoding="utf-8")
            document = json.loads(payload)

            self.assertEqual(document["schema_version"], 1)
            self.assertEqual(document["status"], "blocked")
            self.assertFalse(document["runtime"]["selectable"])
            self.assertTrue(document["resolvers"])
            self.assertTrue(
                all(item["status"] == "pending" for item in document["resolvers"])
            )
            self.assertNotIn(input_dir, payload)
            self.assertEqual(candidate.stat().st_mode & 0o777, 0o600)
            self.assertEqual(candidate.parent.stat().st_mode & 0o777, 0o700)

    def test_preserves_existing_candidate_evidence(self) -> None:
        with (
            tempfile.TemporaryDirectory() as input_dir,
            tempfile.TemporaryDirectory(prefix="plazmic-candidate-test-") as output_dir,
        ):
            client = Path(input_dir) / "eqgame.exe"
            client.write_bytes(make_pe32_plus())
            candidate = prepare_profile_refresh.prepare_candidate(
                client, Path(output_dir)
            )
            document = json.loads(candidate.read_text(encoding="utf-8"))
            document["resolvers"][0]["evidence"].append("private-test-evidence")
            candidate.write_text(json.dumps(document), encoding="utf-8")

            same_candidate = prepare_profile_refresh.prepare_candidate(
                client, Path(output_dir)
            )
            preserved = json.loads(same_candidate.read_text(encoding="utf-8"))

            self.assertEqual(
                preserved["resolvers"][0]["evidence"], ["private-test-evidence"]
            )
            self.assertEqual(same_candidate.stat().st_mode & 0o777, 0o600)

    def test_rejects_repository_output(self) -> None:
        with tempfile.TemporaryDirectory() as input_dir:
            client = Path(input_dir) / "eqgame.exe"
            client.write_bytes(make_pe32_plus())
            with self.assertRaisesRegex(
                prepare_profile_refresh.CandidateError,
                "outside the repository",
            ):
                prepare_profile_refresh.prepare_candidate(
                    client,
                    prepare_profile_refresh.REPOSITORY_ROOT / "build/candidate-test",
                )

    def test_rejects_corrupt_existing_candidate(self) -> None:
        with (
            tempfile.TemporaryDirectory() as input_dir,
            tempfile.TemporaryDirectory(prefix="plazmic-candidate-test-") as output_dir,
        ):
            client = Path(input_dir) / "eqgame.exe"
            client.write_bytes(make_pe32_plus())
            candidate = prepare_profile_refresh.prepare_candidate(
                client, Path(output_dir)
            )
            document = json.loads(candidate.read_text(encoding="utf-8"))
            document["resolvers"].pop()
            candidate.write_text(json.dumps(document), encoding="utf-8")

            with self.assertRaisesRegex(
                prepare_profile_refresh.CandidateError,
                "fail-closed schema",
            ):
                prepare_profile_refresh.prepare_candidate(client, Path(output_dir))

    def test_rejects_non_x86_64_candidate(self) -> None:
        payload = bytearray(make_pe32_plus())
        pe_offset = int.from_bytes(payload[0x3C:0x40], "little")
        payload[pe_offset + 4 : pe_offset + 6] = (0x014C).to_bytes(2, "little")
        with (
            tempfile.TemporaryDirectory() as input_dir,
            tempfile.TemporaryDirectory(prefix="plazmic-candidate-test-") as output_dir,
        ):
            client = Path(input_dir) / "eqgame.exe"
            client.write_bytes(payload)
            with self.assertRaisesRegex(
                prepare_profile_refresh.CandidateError,
                "x86-64",
            ):
                prepare_profile_refresh.prepare_candidate(client, Path(output_dir))


if __name__ == "__main__":
    unittest.main()
