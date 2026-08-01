"""Keep scheduled firmware compilation coverage in sync with PlatformIO."""

from __future__ import annotations

import re
import unittest
from pathlib import Path

import yaml


ROOT = Path(__file__).resolve().parents[2]
PLATFORMIO_ENV_RE = re.compile(r"^\[env:([^]]+)]$", re.MULTILINE)
MATRIX_ENV_RE = re.compile(r"^\s+- env:\s+(\S+)\s*$", re.MULTILINE)

# Remote-test radio profiles are exercised on hardware during device testing,
# not compiled in the nightly validation matrix (trimmed 2026-07-31).
NOT_SCHEDULED = {
    "SigurdOS_TDeck_remote_test_radio_testfreq",
    "SigurdOS_TDeck_remote_test_radio_roomtest",
    "SigurdOS_TDeck_remote_test_radio_usca",
    "SigurdOS_TDeck_remote_test_radio_usca_rxonly",
}
OPTIONAL_BOOTSTRAP_ENVIRONMENTS = {
    "KrabOS_TDeckPlus",
    "KrabOS_TDeckPlus_recovery",
    "KrabOS_TDeckPlus_debug",
}


class ValidationMatrixTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.platformio = (ROOT / "platformio.ini").read_text(encoding="utf-8")
        cls.workflow = (
            ROOT / ".github" / "workflows" / "build-validation-matrix.yml"
        ).read_text(encoding="utf-8")

    def test_every_firmware_environment_is_scheduled(self) -> None:
        configured = {
            environment
            for environment in PLATFORMIO_ENV_RE.findall(self.platformio)
            if not environment.startswith("native")
        }
        scheduled = set(MATRIX_ENV_RE.findall(self.workflow))
        required = configured - NOT_SCHEDULED
        self.assertTrue(required.issubset(scheduled))
        self.assertEqual(scheduled - required, OPTIONAL_BOOTSTRAP_ENVIRONMENTS - required)
        self.assertTrue(OPTIONAL_BOOTSTRAP_ENVIRONMENTS.issubset(scheduled))
        self.assertEqual(len(scheduled), len(MATRIX_ENV_RE.findall(self.workflow)))

    def test_every_cell_builds_and_requires_an_artifact(self) -> None:
        self.assertIn("schedule:", self.workflow)
        self.assertNotIn("requires_wifi", self.workflow)
        self.assertIn("SIGURDOS_GPS_VALIDATION_WIFI_SSID", self.workflow)
        self.assertIn("SIGURDOS_GPS_VALIDATION_WIFI_HOST", self.workflow)
        self.assertIn("SIGURDOS_GPS_VALIDATION_WIFI_TOKEN", self.workflow)
        self.assertIn("if-no-files-found: error", self.workflow)
        self.assertIn("check_first_party_warnings.py", self.workflow)
        self.assertIn("retention-days: 7", self.workflow)

    def test_full_matrix_gates_prs_dev_pushes_and_releases(self) -> None:
        workflow = yaml.safe_load(self.workflow)
        triggers = workflow["on"]
        self.assertIn("pull_request", triggers)
        self.assertIn("push", triggers)
        self.assertIn("workflow_call", triggers)
        self.assertIn("main", triggers["pull_request"]["branches"])
        self.assertIn("main", triggers["push"]["branches"])
        for environment in (
            "KrabOS_TDeckPlus",
            "KrabOS_TDeckPlus_recovery",
            "KrabOS_TDeckPlus_debug",
        ):
            self.assertIn(environment, MATRIX_ENV_RE.findall(self.workflow))
        release = yaml.safe_load(
            (ROOT / ".github" / "workflows" / "build-release.yml").read_text(encoding="utf-8")
        )
        self.assertEqual(
            release["jobs"]["full-matrix"]["uses"],
            "./.github/workflows/build-validation-matrix.yml",
        )
        self.assertIn("full-matrix", release["jobs"]["build"]["needs"])

    def test_artifact_retention_is_explicit(self) -> None:
        pr = (ROOT / ".github" / "workflows" / "pr-ci.yml").read_text(encoding="utf-8")
        release = (ROOT / ".github" / "workflows" / "build-release.yml").read_text(encoding="utf-8")
        self.assertIn("retention-days: 14", pr)
        self.assertIn("retention-days: 30", release)

    def test_main_pr_builds_all_krabos_release_roles(self) -> None:
        pr = yaml.safe_load(
            (ROOT / ".github" / "workflows" / "pr-ci.yml").read_text(
                encoding="utf-8"
            )
        )
        self.assertIn("main", pr["on"]["pull_request"]["branches"])
        build_steps = "\n".join(
            str(step.get("run", "")) for step in pr["jobs"]["build"]["steps"]
        )
        for environment in (
            "KrabOS_TDeckPlus",
            "KrabOS_TDeckPlus_recovery",
            "KrabOS_TDeckPlus_debug",
        ):
            self.assertIn(f"pio run -e {environment}", build_steps)


if __name__ == "__main__":
    unittest.main()
