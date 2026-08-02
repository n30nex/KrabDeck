from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[3]
MODULE_PATH = ROOT / "scripts" / "krabos" / "hooks" / "collect_recovery_rf_off.py"
SPEC = importlib.util.spec_from_file_location("krabos_recovery_collector", MODULE_PATH)
assert SPEC and SPEC.loader
collector = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = collector
SPEC.loader.exec_module(collector)


def request_value(root: Path) -> dict[str, object]:
    return {
        "schema_version": 1,
        "operation": "recovery_rf_off",
        "commit": "a" * 40,
        "manifest_sha256": "b" * 64,
        "challenge": "c" * 32,
        "target_by_id": str(collector.TARGET_BY_ID),
        "output_path": str((root / "recovery-rf-off-evidence.json").resolve()),
        "required_soak_seconds": 60,
        "expected_boot_advert_queued_markers": 0,
        "expected_public_chat_queued_markers": 0,
        "expected_structural_rf_policy": "blocked",
        "expected_role": "recovery",
    }


class RecoveryCollectorContractTests(unittest.TestCase):
    def test_only_exact_recovery_markers_count(self) -> None:
        state = {
            "boot_ready_marker": False,
            "rf_blocked_marker": False,
            "boot_advert_queued_markers": 0,
            "public_chat_queued_markers": 0,
        }
        short_sha = "a" * 12
        collector.observe_line(
            "@krabos|event=boot|status=ready|env=KrabOS_TDeckPlus|sha="
            + short_sha,
            state,
            short_sha,
        )
        collector.observe_line(
            "@krabos|event=rf_policy|tx=blocked|role=debug", state, short_sha
        )
        self.assertFalse(state["boot_ready_marker"])
        self.assertFalse(state["rf_blocked_marker"])

        collector.observe_line(collector.RF_BLOCKED_LINE, state, short_sha)
        collector.observe_line(
            "@krabos|event=boot|status=ready|env=KrabOS_TDeckPlus_recovery|sha="
            + short_sha,
            state,
            short_sha,
        )
        self.assertTrue(state["boot_ready_marker"])
        self.assertTrue(state["rf_blocked_marker"])
        self.assertEqual(state["boot_advert_queued_markers"], 0)
        self.assertEqual(state["public_chat_queued_markers"], 0)

        collector.observe_line(
            "@krabos|event=boot_advert|status=queued|scope=wildcard",
            state,
            short_sha,
        )
        collector.observe_line(
            "@krabos|event=public_chat|status=queued|channel=Public",
            state,
            short_sha,
        )
        self.assertEqual(state["boot_advert_queued_markers"], 1)
        self.assertEqual(state["public_chat_queued_markers"], 1)

    def test_request_cannot_select_another_serial_or_output(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            path = root / "recovery-rf-off-request.json"
            value = request_value(root)
            value["target_by_id"] = "/dev/ttyUSB2"
            path.write_text(json.dumps(value), encoding="utf-8")
            with self.assertRaisesRegex(collector.CollectorError, "target_by_id"):
                collector._load_request(path)

            value = request_value(root)
            value["output_path"] = str((root / "other.json").resolve())
            path.write_text(json.dumps(value), encoding="utf-8")
            with self.assertRaisesRegex(collector.CollectorError, "output path"):
                collector._load_request(path)

            value = request_value(root)
            value["port"] = "/dev/ttyACM9"
            path.write_text(json.dumps(value), encoding="utf-8")
            with self.assertRaisesRegex(collector.CollectorError, "unexpected"):
                collector._load_request(path)

    def test_collect_returns_diagnostic_markers_not_physical_rf_claims(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            request = request_value(root)
            serial = (
                collector.RF_BLOCKED_LINE
                + "\n@krabos|event=boot|status=ready|env="
                "KrabOS_TDeckPlus_recovery|sha="
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
                    collector.time, "monotonic", side_effect=[0.0, 0.0, 0.0, 60.0]
                ),
            ):
                evidence = collector.collect(request)
            self.assertEqual(evidence["soak_duration_seconds"], 60)
            self.assertTrue(evidence["rf_blocked_marker"])
            self.assertFalse(evidence["physical_rf_blocked_verified"])
            self.assertEqual(evidence["boot_advert_queued_markers"], 0)
            self.assertNotIn("tx_blocked", evidence)
            self.assertNotIn("outbound_markers", evidence)

    def test_collect_rejects_any_advert_marker(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            request = request_value(Path(temporary))
            serial = (
                collector.RF_BLOCKED_LINE
                + "\n@krabos|event=boot_advert|status=queued|scope=wildcard\n"
            ).encode("utf-8")
            with (
                mock.patch.object(collector, "_udev_properties", return_value={}),
                mock.patch.object(collector, "_configure_serial"),
                mock.patch.object(collector.os, "open", return_value=3),
                mock.patch.object(collector.os, "close"),
                mock.patch.object(collector.os, "read", return_value=serial),
                mock.patch.object(collector.select, "select", return_value=([3], [], [])),
                mock.patch.object(collector.time, "monotonic", side_effect=[0.0, 0.0, 0.0]),
                self.assertRaisesRegex(collector.CollectorError, "queued transmit"),
            ):
                collector.collect(request)


if __name__ == "__main__":
    unittest.main()
