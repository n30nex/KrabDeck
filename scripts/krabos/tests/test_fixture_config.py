from __future__ import annotations

import json
import os
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
KRABOS_DIRECTORY = ROOT / "scripts" / "krabos"
if str(KRABOS_DIRECTORY) not in sys.path:
    sys.path.insert(0, str(KRABOS_DIRECTORY))

from fixture_config import (  # noqa: E402 - fixed repository module
    CONFIG_ENVIRONMENT,
    FixtureConfigError,
    load_fixture_config,
)


def valid_config() -> dict[str, object]:
    return {
        "target_by_id": "/dev/serial/by-id/test-target-00:11:22:33:44:55",
        "expected_usb_serial": "00:11:22:33:44:55",
        "expected_efuse_mac": "00:11:22:33:44:55",
        "expected_properties": {
            "ID_BUS": "usb",
            "ID_VENDOR_ID": "303a",
            "ID_MODEL_ID": "1001",
            "ID_SERIAL_SHORT": "00:11:22:33:44:55",
            "ID_USB_INTERFACE_NUM": "00",
            "ID_USB_DRIVER": "cdc_acm",
            "ID_PATH": "test-usb-path",
        },
        "forbidden_devices": [
            {
                "by_id": "/dev/serial/by-id/test-neighbour",
                "serial": "66:77:88:99:AA:BB",
                "vid_pid": ["1a86", "7523"],
            }
        ],
    }


class FixtureConfigTests(unittest.TestCase):
    def setUp(self) -> None:
        self.previous = os.environ.get(CONFIG_ENVIRONMENT)
        load_fixture_config.cache_clear()

    def tearDown(self) -> None:
        if self.previous is None:
            os.environ.pop(CONFIG_ENVIRONMENT, None)
        else:
            os.environ[CONFIG_ENVIRONMENT] = self.previous
        load_fixture_config.cache_clear()

    def test_missing_runner_config_fails_closed(self) -> None:
        os.environ.pop(CONFIG_ENVIRONMENT, None)
        with self.assertRaises(FixtureConfigError):
            load_fixture_config()

    def test_valid_mode_0600_config_loads(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "fixture.json"
            path.write_text(json.dumps(valid_config()), encoding="utf-8")
            os.chmod(path, 0o600)
            os.environ[CONFIG_ENVIRONMENT] = str(path)

            config = load_fixture_config()

        self.assertEqual(str(config.target_by_id), valid_config()["target_by_id"])
        self.assertEqual(config.expected_usb_serial, "00:11:22:33:44:55")

    def test_group_readable_config_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "fixture.json"
            path.write_text(json.dumps(valid_config()), encoding="utf-8")
            os.chmod(path, 0o640)
            os.environ[CONFIG_ENVIRONMENT] = str(path)

            with self.assertRaisesRegex(FixtureConfigError, "mode-0600"):
                load_fixture_config()

    def test_production_sources_have_no_hard_coded_usb_fixture_path(self) -> None:
        for relative in (
            "scripts/krabos/exact_device_release.py",
            "scripts/krabos/hooks/collect_postflash_smoke.py",
            "scripts/krabos/hooks/collect_recovery_rf_off.py",
        ):
            source = (ROOT / relative).read_text(encoding="utf-8")
            self.assertNotIn("usb-Espressif_USB_JTAG_" + "serial_debug_unit_", source)


if __name__ == "__main__":
    unittest.main()
