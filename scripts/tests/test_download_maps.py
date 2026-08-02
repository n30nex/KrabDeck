"""CLI and resume-contract tests for the offline map downloader."""

from __future__ import annotations

import io
import json
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

from scripts import download_maps


class DownloadMapsTests(unittest.TestCase):
    @staticmethod
    def _valid_tile(marker: bytes) -> bytes:
        return (
            download_maps.PNG_SIGNATURE
            + b"\x00\x00\x00\x0dIHDR"
            + (256).to_bytes(4, "big")
            + (256).to_bytes(4, "big")
            + marker
        )

    def test_documented_coordinate_mode_is_reachable(self) -> None:
        args = download_maps.parse_args(
            [
                "--name", "teesside",
                "--lat1", "54.45", "--lon1", "-1.45",
                "--lat2", "54.65", "--lon2", "-1.05",
                "--zoom", "8", "14",
                "--dry-run",
            ]
        )
        self.assertEqual(args.bounds, (54.45, -1.45, 54.65, -1.05))

    def test_generic_source_url_is_redacted_from_logs(self) -> None:
        secret = "do-not-print-this-token"
        output = io.StringIO()
        with redirect_stdout(output):
            download_maps.main([
                "--name", "private-source",
                "--bbox", "43.6,-79.4,43.61,-79.39",
                "--zoom", "0", "0",
                "--server", "generic",
                "--url-template",
                f"https://tiles.example/{{z}}/{{x}}/{{y}}.png?key={secret}",
                "--attribution", "Example",
                "--dry-run",
            ])

        rendered = output.getvalue()
        self.assertNotIn(secret, rendered)
        self.assertNotIn("tiles.example", rendered)
        self.assertIn("custom HTTPS source; URL redacted", rendered)

    def test_bbox_and_city_modes_match_documented_commands(self) -> None:
        bbox = download_maps.parse_args(
            ["--name", "london", "--bbox", "51.3,-0.5,51.7,0.3"]
        )
        self.assertEqual(bbox.bounds, (51.3, -0.5, 51.7, 0.3))
        city = download_maps.parse_args(["--city", "London", "--zoom", "10", "14"])
        self.assertEqual(city.name, "london")
        self.assertEqual(city.bounds, download_maps.CITIES["london"])

    def test_partial_mixed_and_missing_modes_are_rejected(self) -> None:
        invalid = (
            ["--name", "partial", "--lat1", "1", "--lon1", "2"],
            [
                "--name", "mixed", "--bbox", "1,2,3,4",
                "--lat1", "1", "--lon1", "2", "--lat2", "3", "--lon2", "4",
            ],
            ["--name", "missing"],
            ["--bbox", "1,2,3,4"],
        )
        for argv in invalid:
            with self.subTest(argv=argv), self.assertRaises(SystemExit):
                download_maps.parse_args(argv)

    def test_resume_skips_existing_tile_but_default_refreshes(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            tile = root / "1" / "0" / "0.png"
            tile.parent.mkdir(parents=True)
            old_tile = self._valid_tile(b"old")
            new_tile = self._valid_tile(b"new")
            tile.write_bytes(old_tile)
            config = {"url": "https://example.invalid/{z}/{x}/{y}.png"}

            with mock.patch.object(download_maps, "session") as session:
                self.assertTrue(
                    download_maps.download_tile((0, 0, 1, config, str(root), True))[3]
                )
                session.get.assert_not_called()

            response = SimpleNamespace(status_code=200, content=new_tile)
            with mock.patch.object(
                download_maps, "session", SimpleNamespace(get=mock.Mock(return_value=response))
            ):
                self.assertTrue(
                    download_maps.download_tile((0, 0, 1, config, str(root), False))[3]
                )
            self.assertEqual(tile.read_bytes(), new_tile)

    def test_tile_index_records_only_valid_nonempty_tiles(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for relative in ("8/100/80.png", "8/100/82.png", "8/103/81.png"):
                tile = root / relative
                tile.parent.mkdir(parents=True, exist_ok=True)
                tile.write_bytes(b"png")
            (root / "8/100/invalid.png").write_bytes(b"ignored")
            (root / "8/100/83.png").write_bytes(b"")
            out_of_world = root / "8" / "256" / "1.png"
            out_of_world.parent.mkdir(parents=True)
            out_of_world.write_bytes(b"ignored")

            index = download_maps.write_tile_index(str(root))

            self.assertEqual(index["version"], 1)
            self.assertEqual(index["tile_size"], 256)
            self.assertEqual(index["zooms"], [{
                "z": 8,
                "min_x": 100,
                "max_x": 103,
                "min_y": 80,
                "max_y": 82,
                "sample_x": 100,
                "sample_y": 80,
                "count": 3,
            }])
            self.assertEqual(
                json.loads((root / "index.json").read_text()), index)
            self.assertFalse((root / "index.json.tmp").exists())


if __name__ == "__main__":
    unittest.main()
