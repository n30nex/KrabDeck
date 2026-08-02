from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[3]
MODULE_PATH = ROOT / "scripts" / "krabos" / "hooks" / "collect_postflash_smoke.py"
SPEC = importlib.util.spec_from_file_location("krabos_smoke_collector", MODULE_PATH)
assert SPEC and SPEC.loader
collector = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = collector
SPEC.loader.exec_module(collector)


class SmokeCollectorContractTests(unittest.TestCase):
    def test_only_exact_structured_markers_count(self) -> None:
        state = {
            "boot_ready_marker": False,
            "boot_advert_queued_markers": 0,
            "public_chat_queued_markers": 0,
        }
        short_sha = "a" * 12
        collector.observe_line(
            "@krabos|event=boot_advert|status=queued|scope=wildcard", state, short_sha
        )
        collector.observe_line(
            f"@krabos|event=boot|status=ready|env=KrabOS_TDeckPlus|sha={short_sha}",
            state,
            short_sha,
        )
        collector.observe_line("ordinary private diagnostic", state, short_sha)
        self.assertEqual(
            state,
            {
                "boot_ready_marker": True,
                "boot_advert_queued_markers": 1,
                "public_chat_queued_markers": 0,
            },
        )

        collector.observe_line(
            "@krabos|event=public_chat|status=queued|channel=Public", state, short_sha
        )
        self.assertEqual(state["public_chat_queued_markers"], 1)

    def test_request_cannot_select_another_serial_device(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            request = root / "request.json"
            request.write_text(
                json.dumps(
                    {
                        "schema_version": 1,
                        "operation": "postflash_smoke",
                        "commit": "a" * 40,
                        "manifest_sha256": "b" * 64,
                        "challenge": "c" * 32,
                        "target_by_id": "/dev/ttyUSB2",
                        "output_path": str((root / "output.json").resolve()),
                        "required_soak_seconds": 900,
                        "expected_boot_advert_queued_markers": 1,
                        "expected_public_chat_queued_markers": 0,
                        "expected_structural_rf_policy": "one_boot_advert",
                    }
                ),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(collector.CollectorError, "target_by_id"):
                collector._load_request(request)

    def test_target_markers_never_claim_an_observed_rf_packet(self) -> None:
        request = {
            "schema_version": 1,
            "operation": "postflash_smoke",
            "commit": "a" * 40,
            "manifest_sha256": "b" * 64,
            "challenge": "c" * 32,
            "target_by_id": str(collector.TARGET_BY_ID),
            "output_path": "C:/diagnostic.json",
            "required_soak_seconds": 900,
            "expected_boot_advert_queued_markers": 1,
            "expected_public_chat_queued_markers": 0,
            "expected_structural_rf_policy": "one_boot_advert",
        }
        serial = (
            collector.BOOT_ADVERT_LINE
            + "\n@krabos|event=boot|status=ready|env=KrabOS_TDeckPlus|sha="
            + "a" * 12
            + "\n"
        ).encode("utf-8")
        with (
            mock.patch.object(collector, "_udev_properties", return_value={}),
            mock.patch.object(collector, "_configure_serial"),
            mock.patch.object(collector.os, "open", return_value=3),
            mock.patch.object(collector.os, "close"),
            mock.patch.object(collector.os, "read", return_value=serial),
            mock.patch.object(collector.select, "select", return_value=([3], [], [])),
            mock.patch.object(
                collector.time, "monotonic", side_effect=[0.0, 0.0, 0.0, 900.0]
            ),
        ):
            evidence = collector.collect(request)

        self.assertEqual(evidence["boot_advert_queued_markers"], 1)
        self.assertEqual(evidence["structural_rf_policy"], "one_boot_advert")
        self.assertFalse(evidence["physical_one_boot_advert_verified"])
        for false_claim in ("outbound_packets", "tx_attempts", "boot_adverts"):
            self.assertNotIn(false_claim, evidence)


if __name__ == "__main__":
    unittest.main()
