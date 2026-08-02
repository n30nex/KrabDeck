from __future__ import annotations

import contextlib
import importlib.util
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[3]
MODULE_PATH = ROOT / "scripts" / "krabos" / "bundle_release.py"
SPEC = importlib.util.spec_from_file_location("krabos_bundle_release", MODULE_PATH)
assert SPEC and SPEC.loader
bundle = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = bundle
SPEC.loader.exec_module(bundle)
release_contract = sys.modules["exact_device_release"]

COMMIT = "a" * 40
VERSION = "edge-2026-08-01-aaaaaaaaaaaa"


def digest(path: Path) -> str:
    return bundle.sha256_file(path)


def write_flash_manifest(root: Path, role: str, image_name: str) -> dict:
    image = root / image_name
    record = {
        "address": 0,
        "file": image.name,
        "size": image.stat().st_size,
        "sha256": digest(image),
    }
    value = {
        "schema_version": 1,
        "product": bundle.PRODUCT,
        "board": bundle.BOARD,
        "target_chip": "esp32s3",
        "flash_size": 16 * 1024 * 1024,
        "role": role,
        "commit": COMMIT,
        "build_environment": (
            "KrabOS_TDeckPlus" if role == "candidate" else "KrabOS_TDeckPlus_recovery"
        ),
        "rf_policy": "one_boot_advert" if role == "candidate" else "blocked",
        "mesh_tx_enabled": role == "candidate",
        "segments": [record],
    }
    (root / f"{role}-flash-manifest.json").write_text(
        json.dumps(value), encoding="utf-8"
    )
    return {key: record[key] for key in ("address", "size", "sha256")}


def write_fixture(root: Path) -> None:
    for name in bundle.REQUIRED_ARTIFACTS:
        (root / name).write_bytes(f"fixture:{name}".encode("utf-8"))
    (root / "manifest.json").write_text(
        json.dumps({"name": "KrabOS T-Deck Plus", "version": "old"}),
        encoding="utf-8",
    )
    (root / "build-metadata.json").write_text(
        json.dumps(
            {
                "git_sha": COMMIT,
                "git_dirty": False,
                "build_environment": bundle.BUILD_ENVIRONMENT,
                "version": "old",
            }
        ),
        encoding="utf-8",
    )
    candidate = write_flash_manifest(root, "candidate", "krabos-candidate.bin")
    recovery = write_flash_manifest(root, "recovery", "krabos-recovery-rf-off.bin")
    (root / "krabos-public-receipt.json").write_text(
        json.dumps(
            {
                "schema_version": 1,
                "product": bundle.PRODUCT,
                "board": bundle.BOARD,
                "run_id": "20260801T120000Z-0123456789ab",
                "commit": COMMIT,
                "generated_at": "2026-08-01T12:00:00+00:00",
                "outcome": "pass",
                "release_eligible": True,
                "artifacts": {
                    "candidate": [candidate],
                    "recovery": [recovery],
                },
                "gates": {
                    name: True for name in release_contract.REQUIRED_RELEASE_GATES
                },
                "recovery": {"used": True, "ok": True},
            }
        ),
        encoding="utf-8",
    )


def fixture_role_records(root: Path) -> dict[str, dict[str, str | int]]:
    identities = {
        "production": ("firmware-merged.bin", "KrabOS_TDeckPlus"),
        "recovery": (
            "krabos-recovery-rf-off.bin",
            "KrabOS_TDeckPlus_recovery",
        ),
        "debug": ("firmware-debug.bin", "KrabOS_TDeckPlus_debug"),
    }
    return {
        role: {
            "file": name,
            "build_environment": environment,
            "size": (root / name).stat().st_size,
            "sha256": digest(root / name),
        }
        for role, (name, environment) in identities.items()
    }


@contextlib.contextmanager
def fixture_artifact_contract() -> object:
    """Replace the binary parser only for deliberately tiny bundle fixtures."""
    with (
        mock.patch.object(bundle, "validate_release_directory", return_value={}),
        mock.patch.object(
            bundle, "validate_firmware_roles", side_effect=fixture_role_records
        ),
    ):
        yield


@contextlib.contextmanager
def schema_only_bundle_contract() -> object:
    """Exercise bundle mechanics without weakening the production RF gate."""
    with (
        fixture_artifact_contract(),
        mock.patch.object(
            bundle,
            "validate_public_release_receipt",
            release_contract._validate_public_release_receipt_schema,
        ),
    ):
        yield


class BundleReleaseTests(unittest.TestCase):
    def test_stable_v1_version_is_accepted_but_other_semver_is_not(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.assertEqual(
                bundle._validate_inputs(directory, COMMIT, "v1.0.0"),
                directory.resolve(),
            )
            with self.assertRaisesRegex(bundle.BundleError, "v1.0.0"):
                bundle._validate_inputs(directory, COMMIT, "v1.0.1")

    def setUp(self) -> None:
        # The actual bundle is produced on case-sensitive Linux. Collapse the
        # two required Launcher aliases only for this Windows unit-test FS.
        seen: set[str] = set()
        portable: set[str] = set()
        for name in sorted(bundle.REQUIRED_ARTIFACTS):
            key = os.path.normcase(name)
            if key not in seen:
                seen.add(key)
                portable.add(name)
        self.required_patch = mock.patch.object(
            bundle, "REQUIRED_ARTIFACTS", frozenset(portable)
        )
        self.required_patch.start()

    def tearDown(self) -> None:
        self.required_patch.stop()

    def test_stamp_seal_and_verify_exact_file_set(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            write_fixture(root)
            bundle.stamp(root, COMMIT, VERSION)
            self.assertEqual(
                json.loads((root / "manifest.json").read_text(encoding="utf-8"))[
                    "version"
                ],
                VERSION,
            )
            with schema_only_bundle_contract():
                bundle.seal(root, COMMIT, VERSION)
                bundle.verify(root, COMMIT, VERSION)
            self.assertTrue((root / "krabos-bundle-manifest.json").is_file())
            self.assertTrue((root / "SHA256SUMS.txt").is_file())
            manifest = json.loads(
                (root / "krabos-bundle-manifest.json").read_text(encoding="utf-8")
            )
            self.assertEqual(
                manifest["firmware_roles"], fixture_role_records(root)
            )

    def test_bundle_rejects_stale_firmware_role_binding(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            write_fixture(root)
            with schema_only_bundle_contract():
                bundle.seal(root, COMMIT, VERSION)
                manifest_path = root / "krabos-bundle-manifest.json"
                manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
                manifest["firmware_roles"]["debug"]["build_environment"] = (
                    "KrabOS_TDeckPlus_recovery"
                )
                manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
                with self.assertRaisesRegex(bundle.BundleError, "role records"):
                    bundle.verify(root, COMMIT, VERSION)

    def test_mutated_bytes_and_arbitrary_tag_fail(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            write_fixture(root)
            with self.assertRaisesRegex(bundle.BundleError, "edge"):
                bundle.stamp(root, COMMIT, "chosen-by-user")

            with schema_only_bundle_contract():
                bundle.seal(root, COMMIT, VERSION)
                (root / "firmware.bin").write_bytes(b"mutated")
                with self.assertRaisesRegex(bundle.BundleError, "digest mismatch"):
                    bundle.verify(root, COMMIT, VERSION)

    def test_production_bundle_rejects_claim_without_independent_rf_observer(
        self,
    ) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            write_fixture(root)
            with fixture_artifact_contract():
                with self.assertRaisesRegex(
                    bundle.BundleError, "independent RF observer"
                ):
                    bundle.seal(root, COMMIT, VERSION)

    def test_missing_or_extra_receipt_gate_fails_closed(self) -> None:
        mutations = (
            lambda gates: gates.pop("smoke_passed"),
            lambda gates: gates.update(invented_gate=True),
        )
        for mutate in mutations:
            with self.subTest(mutate=mutate), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                write_fixture(root)
                receipt_path = root / "krabos-public-receipt.json"
                receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
                mutate(receipt["gates"])
                receipt_path.write_text(json.dumps(receipt), encoding="utf-8")
                with fixture_artifact_contract():
                    with self.assertRaisesRegex(bundle.BundleError, "gate set"):
                        bundle.seal(root, COMMIT, VERSION)

    def test_candidate_and_recovery_receipt_hashes_bind_to_sealed_files(self) -> None:
        for role in ("candidate", "recovery"):
            with self.subTest(role=role), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                write_fixture(root)
                receipt_path = root / "krabos-public-receipt.json"
                receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
                receipt["artifacts"][role][0]["sha256"] = "0" * 64
                receipt_path.write_text(json.dumps(receipt), encoding="utf-8")
                with fixture_artifact_contract():
                    with self.assertRaisesRegex(bundle.BundleError, "do not match"):
                        bundle.seal(root, COMMIT, VERSION)

    def test_manifest_hash_must_match_actual_sealed_bytes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            write_fixture(root)
            (root / "krabos-recovery-rf-off.bin").write_bytes(b"mutated recovery")
            with fixture_artifact_contract():
                with self.assertRaisesRegex(bundle.BundleError, "bytes do not match"):
                    bundle.seal(root, COMMIT, VERSION)


if __name__ == "__main__":
    unittest.main()
