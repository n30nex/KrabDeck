from __future__ import annotations

import contextlib
import hashlib
import io
import importlib.util
import json
import os
from pathlib import Path
import stat
import struct
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[3]
MODULE_PATH = ROOT / "scripts" / "krabos" / "exact_device_release.py"
SPEC = importlib.util.spec_from_file_location(
    "krabos_exact_device_release", MODULE_PATH
)
assert SPEC and SPEC.loader
release = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = release
SPEC.loader.exec_module(release)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def canonical_partition_table() -> bytes:
    entries = b"".join(
        struct.pack(
            "<HBBII16sI",
            release.PARTITION_ENTRY_MAGIC,
            partition_type,
            subtype,
            address,
            size,
            label.encode("ascii").ljust(16, b"\0"),
            flags,
        )
        for label, partition_type, subtype, address, size, flags in release.CANONICAL_PARTITIONS
    )
    md5_record = (
        struct.pack("<H", release.PARTITION_MD5_MAGIC)
        + b"\xff" * 14
        + hashlib.md5(entries).digest()  # nosec: ESP-IDF partition format mandates MD5
    )
    return (entries + md5_record).ljust(release.PARTITION_TABLE_SIZE, b"\xff")


def state_bytes(label: str, size: int) -> bytes:
    seed = hashlib.sha256(("preserved-" + label).encode("ascii")).digest()
    return (seed * ((size + len(seed) - 1) // len(seed)))[:size]


def write_manifest(
    directory: Path,
    role: str,
    image_name: str,
    *,
    address: int = 0,
    partition_table: bytes | None = None,
) -> Path:
    image = directory / image_name
    image_bytes = bytearray(
        b"\xff" * (release.PARTITION_TABLE_OFFSET + release.PARTITION_TABLE_SIZE)
    )
    marker = (role.encode("ascii") + b"-firmware") * 8
    image_bytes[: len(marker)] = marker
    table = canonical_partition_table() if partition_table is None else partition_table
    image_bytes[
        release.PARTITION_TABLE_OFFSET : release.PARTITION_TABLE_OFFSET + len(table)
    ] = table
    image.write_bytes(image_bytes)
    manifest = directory / f"{role}.json"
    manifest.write_text(
        json.dumps(
            {
                "schema_version": 1,
                "product": release.PRODUCT,
                "board": release.BOARD,
                "target_chip": release.TARGET_CHIP,
                "flash_size": release.FLASH_SIZE,
                "role": role,
                "commit": "a" * 40,
                "build_environment": release.BUILD_ENVIRONMENTS[role],
                **release.FLASH_POLICIES[role],
                "segments": [
                    {
                        "address": address,
                        "file": image.name,
                        "size": image.stat().st_size,
                        "sha256": digest(image),
                    }
                ],
            }
        ),
        encoding="utf-8",
    )
    return manifest


def eligible_receipt(
    candidate: release.FlashManifest, recovery: release.FlashManifest
) -> dict:
    return {
        "schema_version": 1,
        "product": release.PRODUCT,
        "board": release.BOARD,
        "run_id": "20260801T120000Z-0123456789ab",
        "commit": candidate.commit,
        "generated_at": "2026-08-01T12:00:00+00:00",
        "outcome": "pass",
        "release_eligible": True,
        "artifacts": {
            "candidate": release.manifest_public_artifacts(candidate),
            "recovery": release.manifest_public_artifacts(recovery),
        },
        "gates": {name: True for name in release.REQUIRED_RELEASE_GATES},
        "recovery": {"used": True, "ok": True},
        "external_evidence": {
            "requirement_id": release.RF_OBSERVER_REQUIREMENT,
            "evidence_class": "independent-observer",
            "source_evidence_sha256": "f" * 64,
            "evidence_bundle_sha256": "e" * 64,
            "production_image_sha256": candidate.segments[0].sha256,
            "recovery_image_sha256": recovery.segments[0].sha256,
        },
    }


class FakeGuard:
    def __init__(self) -> None:
        self.snapshot = release.DeviceSnapshot(
            "/dev/ttyACM2", dict(release.EXPECTED_PROPERTIES), release.utc_now()
        )

    def inspect(self, **_: object) -> release.DeviceSnapshot:
        return self.snapshot

    def validate_efuse(self, _: object) -> str:
        return release.EXPECTED_EFUSE_MAC

    def wait_reconnect(self, _: float = 45.0) -> release.DeviceSnapshot:
        return self.snapshot


class FakeEspTool:
    def __init__(
        self,
        candidate_failures: int = 0,
        *,
        candidate_failure_calls: set[int] | None = None,
        recovery_failure_calls: set[int] | None = None,
        recovery_failure: Exception | None = None,
        events: list[str] | None = None,
        backup_partition_table: bytes | None = None,
        corrupt_readback_labels: set[str] | None = None,
        corrupt_readback_calls: set[int] | None = None,
    ) -> None:
        self.candidate_failures = candidate_failures
        self.candidate_failure_calls = candidate_failure_calls or set()
        self.recovery_failure_calls = recovery_failure_calls or set()
        self.recovery_failure = recovery_failure
        self.events = events
        self.candidate_writes = 0
        self.recovery_writes = 0
        self.erase_calls = 0
        self.write_roles: list[str] = []
        self.state_restore_calls = 0
        self.backup_partition_table = (
            backup_partition_table or canonical_partition_table()
        )
        self.corrupt_readback_labels = corrupt_readback_labels or set()
        self.corrupt_readback_calls = corrupt_readback_calls or set()
        self.read_calls = 0
        self.flash_regions: dict[tuple[int, int], bytes] = {}
        self.last_role: str | None = None

    def read_mac(self, *, before: str) -> str:
        return f"MAC: {release.EXPECTED_EFUSE_MAC}"

    def flash_id(self) -> str:
        return "Detected flash size: 16MB"

    def security_info(self) -> str:
        return "Secure Boot: Disabled\nFlash Encryption: Disabled\n"

    def backup(self, path: Path, size: int) -> None:
        with path.open("wb") as handle:
            handle.seek(size - 1)
            handle.write(b"\0")
            handle.seek(release.PARTITION_TABLE_OFFSET)
            handle.write(self.backup_partition_table)
            for label, _, _, address, partition_size, _ in release.CANONICAL_PARTITIONS:
                if label in release.PRESERVED_PARTITION_LABELS:
                    handle.seek(address)
                    handle.write(state_bytes(label, partition_size))

    def erase(self) -> None:
        self.erase_calls += 1
        self.flash_regions.clear()

    def write(self, segments: object) -> None:
        admitted = tuple(segments)
        segment = admitted[0]
        if "candidate" in segment.path.name:
            self.candidate_writes += 1
            self.write_roles.append("candidate")
            self.last_role = "candidate"
            if self.events is not None:
                self.events.append("flash:candidate")
            if (
                self.candidate_writes <= self.candidate_failures
                or self.candidate_writes in self.candidate_failure_calls
            ):
                raise release.CommandError("simulated candidate failure")
        elif "recovery" in segment.path.name:
            self.recovery_writes += 1
            self.write_roles.append("recovery")
            self.last_role = "recovery"
            if self.events is not None:
                self.events.append("flash:recovery")
            if self.recovery_writes in self.recovery_failure_calls:
                raise self.recovery_failure or release.CommandError(
                    "simulated recovery failure"
                )
        else:
            self.state_restore_calls += 1
            if self.events is not None:
                self.events.append(f"restore:{self.last_role}")
        for item in admitted:
            self.flash_regions[(item.address, item.size)] = item.path.read_bytes()

    def verify(self, _: object) -> None:
        return None

    def read(self, address: int, size: int, path: Path) -> None:
        self.read_calls += 1
        content = self.flash_regions.get((address, size))
        if content is None:
            raise release.CommandError("simulated missing flash region")
        label = next(
            (
                partition_label
                for partition_label, _, _, partition_address, partition_size, _ in release.CANONICAL_PARTITIONS
                if partition_address == address and partition_size == size
            ),
            "unknown",
        )
        if (
            label in self.corrupt_readback_labels
            or self.read_calls in self.corrupt_readback_calls
        ):
            content = bytes([content[0] ^ 1]) + content[1:]
        path.write_bytes(content)
        if self.events is not None:
            self.events.append(f"readback:{label}")

    def reset(self) -> None:
        if self.events is not None:
            self.events.append("reset")
        return None


class FakeHooks:
    def __init__(
        self,
        smoke_ok: bool = True,
        *,
        recovery_ok: bool = True,
        events: list[str] | None = None,
    ) -> None:
        self.smoke_ok = smoke_ok
        self.recovery_ok = recovery_ok
        self.events = events

    def collect_smoke(
        self,
        _: Path,
        output: Path,
        commit: str,
        manifest_sha256: str,
    ) -> dict[str, object]:
        if self.events is not None:
            self.events.append("candidate_smoke")
        if not self.smoke_ok:
            raise release.SafetyError("simulated smoke collector failure")
        challenge = "a" * 32
        output.write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "operation": "postflash_smoke",
                    "commit": commit,
                    "manifest_sha256": manifest_sha256,
                    "challenge": challenge,
                    "outcome": "diagnostic_pass",
                    "boot_ready_marker": True,
                    "usb_reconnected": True,
                    "structural_rf_policy": "one_boot_advert",
                    "boot_advert_queued_markers": 1,
                    "public_chat_queued_markers": 0,
                    "physical_rf_observer": "unavailable",
                    "physical_one_boot_advert_verified": False,
                    "soak_duration_seconds": 900,
                }
            ),
            encoding="utf-8",
        )
        release.validate_smoke_evidence(output, commit, manifest_sha256, challenge)
        return {
            "path": str(output),
            "size": output.stat().st_size,
            "sha256": digest(output),
            "local_only": True,
            "physical_rf_observer": "unavailable",
        }

    def collect_recovery(
        self,
        run_directory: Path,
        commit: str,
        manifest_sha256: str,
    ) -> dict[str, object]:
        if self.events is not None:
            self.events.append("recovery_evidence")
        if not self.recovery_ok:
            raise release.SafetyError("simulated recovery collector failure")
        challenge = "b" * 32
        output = run_directory / "recovery-rf-off-evidence.json"
        output.write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "operation": "recovery_rf_off",
                    "commit": commit,
                    "manifest_sha256": manifest_sha256,
                    "challenge": challenge,
                    "outcome": "diagnostic_pass",
                    "boot_ready_marker": True,
                    "usb_reconnected": True,
                    "structural_rf_policy": "blocked",
                    "rf_blocked_marker": True,
                    "boot_advert_queued_markers": 0,
                    "public_chat_queued_markers": 0,
                    "physical_rf_observer": "unavailable",
                    "physical_rf_blocked_verified": False,
                    "soak_duration_seconds": 60,
                }
            ),
            encoding="utf-8",
        )
        release.validate_recovery_evidence(output, commit, manifest_sha256, challenge)
        return {
            "path": str(output),
            "size": output.stat().st_size,
            "sha256": digest(output),
            "local_only": True,
        }


class RecordingRunner:
    def __init__(self, result: release.CommandResult | None = None) -> None:
        self.calls: list[list[str]] = []
        self.result = result or release.CommandResult(0, "", "")

    def run(self, args: object, **_: object) -> release.CommandResult:
        self.calls.append(list(args))
        return self.result


class IdentityGuardTests(unittest.TestCase):
    def test_exact_properties_pass(self) -> None:
        release.DeviceGuard.validate_properties(dict(release.EXPECTED_PROPERTIES))

    def test_d1l_and_peer_are_explicitly_rejected(self) -> None:
        d1l = dict(release.EXPECTED_PROPERTIES)
        d1l.update(ID_VENDOR_ID="1a86", ID_MODEL_ID="7523")
        with self.assertRaisesRegex(release.SafetyError, "D1L"):
            release.DeviceGuard.validate_properties(d1l)

        peer = dict(release.EXPECTED_PROPERTIES)
        peer["ID_SERIAL_SHORT"] = "8C:BF:EA:8F:8C:4C"
        with self.assertRaisesRegex(release.SafetyError, "peer"):
            release.DeviceGuard.validate_properties(peer)

    def test_id_path_is_part_of_the_identity(self) -> None:
        moved = dict(release.EXPECTED_PROPERTIES)
        moved["ID_PATH"] = "platform-xhci-hcd.0-usb-0:1.3:1.0"
        with self.assertRaisesRegex(release.SafetyError, "ID_PATH"):
            release.DeviceGuard.validate_properties(moved)

    def test_esptool_always_uses_the_pinned_by_id_path(self) -> None:
        runner = RecordingRunner(release.CommandResult(0, "MAC: CC:8D:A2:0D:14:28", ""))
        tool = release.EspTool(runner, "/venv/bin/python")
        tool.read_mac(before="no-reset")
        command = runner.calls[0]
        self.assertEqual(
            command[command.index("--port") + 1], str(release.TARGET_BY_ID)
        )
        self.assertNotIn(str(release.D1L_BY_ID), command)
        self.assertNotIn(str(release.PEER_ESP_BY_ID), command)

    def test_cli_has_no_port_override(self) -> None:
        parser = release.build_parser()
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            parser.parse_args(
                [
                    "release",
                    "--manifest",
                    "candidate.json",
                    "--recovery-manifest",
                    "recovery.json",
                    "--state-directory",
                    "state",
                    "--public-receipt",
                    "public.json",
                    "--smoke-evidence",
                    "smoke.json",
                    "--port",
                    "/dev/ttyUSB2",
                ]
            )

    def test_external_command_timeout_becomes_fail_closed_error(self) -> None:
        with mock.patch.object(
            release.subprocess,
            "run",
            side_effect=release.subprocess.TimeoutExpired(["collector"], 90),
        ):
            with self.assertRaisesRegex(release.CommandError, "collector timed out"):
                release.CommandRunner().run(["collector"], timeout=90)


class ManifestTests(unittest.TestCase):
    def test_valid_manifest_binds_local_bytes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = write_manifest(Path(temporary), "candidate", "candidate.bin")
            manifest = release.load_manifest(path, "candidate")
        self.assertEqual(manifest.commit, "a" * 40)
        self.assertEqual(manifest.segments[0].address, 0)

    def test_wrong_hash_and_nonzero_first_address_fail(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            path = write_manifest(directory, "candidate", "candidate.bin")
            raw = json.loads(path.read_text(encoding="utf-8"))
            raw["segments"][0]["sha256"] = "0" * 64
            path.write_text(json.dumps(raw), encoding="utf-8")
            with self.assertRaisesRegex(release.SafetyError, "bytes"):
                release.load_manifest(path, "candidate")

            path = write_manifest(
                directory, "candidate", "candidate.bin", address=0x10000
            )
            with self.assertRaisesRegex(release.SafetyError, "0x0"):
                release.load_manifest(path, "candidate")

    def test_boolean_manifest_address_and_size_fail_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            path = write_manifest(directory, "candidate", "candidate.bin")
            raw = json.loads(path.read_text(encoding="utf-8"))
            raw["segments"][0]["address"] = False
            path.write_text(json.dumps(raw), encoding="utf-8")
            with self.assertRaisesRegex(release.SafetyError, "address"):
                release.load_manifest(path, "candidate")

            path = write_manifest(directory, "candidate", "candidate.bin")
            raw = json.loads(path.read_text(encoding="utf-8"))
            raw["segments"][0]["size"] = True
            path.write_text(json.dumps(raw), encoding="utf-8")
            with self.assertRaisesRegex(release.SafetyError, "size"):
                release.load_manifest(path, "candidate")

    def test_role_specific_rf_policy_is_mandatory(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            candidate = write_manifest(directory, "candidate", "candidate.bin")
            raw = json.loads(candidate.read_text(encoding="utf-8"))
            raw.update(rf_policy="blocked", mesh_tx_enabled=False)
            candidate.write_text(json.dumps(raw), encoding="utf-8")
            with self.assertRaisesRegex(release.SafetyError, "rf_policy"):
                release.load_manifest(candidate, "candidate")

            recovery = write_manifest(directory, "recovery", "recovery.bin")
            raw = json.loads(recovery.read_text(encoding="utf-8"))
            raw.update(rf_policy="one_boot_advert", mesh_tx_enabled=True)
            recovery.write_text(json.dumps(raw), encoding="utf-8")
            with self.assertRaisesRegex(release.SafetyError, "rf_policy"):
                release.load_manifest(recovery, "recovery")

    def test_path_escape_and_overlap_fail(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            artifacts = root / "artifacts"
            artifacts.mkdir()
            outside = root / "outside.bin"
            outside.write_bytes(b"outside")
            manifest = write_manifest(artifacts, "candidate", "candidate.bin")
            raw = json.loads(manifest.read_text(encoding="utf-8"))
            raw["segments"][0].update(
                file="../outside.bin",
                size=outside.stat().st_size,
                sha256=digest(outside),
            )
            manifest.write_text(json.dumps(raw), encoding="utf-8")
            with self.assertRaisesRegex(release.SafetyError, "stay beside"):
                release.load_manifest(manifest, "candidate")

            first = artifacts / "first.bin"
            second = artifacts / "second.bin"
            first.write_bytes(b"a" * 20)
            second.write_bytes(b"b" * 20)
            raw["segments"] = [
                {"address": 0, "file": first.name, "size": 20, "sha256": digest(first)},
                {
                    "address": 10,
                    "file": second.name,
                    "size": 20,
                    "sha256": digest(second),
                },
            ]
            manifest.write_text(json.dumps(raw), encoding="utf-8")
            with self.assertRaisesRegex(release.SafetyError, "overlap"):
                release.load_manifest(manifest, "candidate")

    def test_bytes_are_rechecked_after_manifest_admission(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            path = write_manifest(directory, "candidate", "candidate.bin")
            manifest = release.load_manifest(path, "candidate")
            manifest.segments[0].path.write_bytes(b"changed")
            with self.assertRaisesRegex(release.SafetyError, "changed"):
                release.revalidate_manifest_bytes(manifest)


class RawPartitionStateTests(unittest.TestCase):
    def make_archive(
        self, root: Path
    ) -> tuple[release.RawStateArchive, FakeEspTool, release.FlashManifest]:
        candidate = release.load_manifest(
            write_manifest(root, "candidate", "candidate.bin"), "candidate"
        )
        run_directory = root / "private-run"
        release._safe_private_directory(run_directory)
        tool = FakeEspTool()
        backup = release._create_backup(tool, run_directory)
        archive = release._extract_raw_state_archive(run_directory, backup, candidate)
        return archive, tool, candidate

    def test_canonical_partition_table_is_exact_and_md5_bound(self) -> None:
        parsed = release.parse_partition_table(canonical_partition_table())
        self.assertEqual(
            tuple(entry.contract_tuple() for entry in parsed),
            release.CANONICAL_PARTITIONS,
        )

        corrupt_md5 = bytearray(canonical_partition_table())
        corrupt_md5[
            len(release.CANONICAL_PARTITIONS) * release.PARTITION_ENTRY_SIZE + 16
        ] ^= 1
        with self.assertRaisesRegex(release.SafetyError, "MD5"):
            release.parse_partition_table(bytes(corrupt_md5))

        noncanonical = bytearray(canonical_partition_table())
        flags_offset = 28
        struct.pack_into("<I", noncanonical, flags_offset, 1)
        md5_offset = len(release.CANONICAL_PARTITIONS) * release.PARTITION_ENTRY_SIZE
        noncanonical[md5_offset + 16 : md5_offset + 32] = hashlib.md5(
            noncanonical[:md5_offset]
        ).digest()
        with self.assertRaisesRegex(release.SafetyError, "canonical"):
            release.parse_partition_table(bytes(noncanonical))

    def test_archive_preserves_only_nvs_nvs_keys_and_spiffs(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            archive, _, candidate = self.make_archive(Path(temporary))
            manifest = json.loads(archive.manifest_path.read_text(encoding="utf-8"))
            self.assertEqual(
                [record["label"] for record in manifest["partitions"]],
                ["nvs", "nvs_keys", "spiffs"],
            )
            self.assertNotIn("private_state", manifest)
            self.assertEqual(manifest["commit"], candidate.commit)
            self.assertEqual(manifest["backup_sha256"], archive.backup_sha256)
            self.assertEqual(manifest["binding_sha256"], archive.binding_sha256)
            self.assertRegex(manifest["challenge"], r"^[0-9a-f]{32}$")
            for blob in archive.blobs:
                self.assertEqual(
                    blob.path.read_bytes(), state_bytes(blob.label, blob.size)
                )
                self.assertEqual(digest(blob.path), blob.sha256)
                if os.name == "posix":
                    self.assertEqual(stat.S_IMODE(blob.path.stat().st_mode), 0o600)
            if os.name == "posix":
                self.assertEqual(
                    stat.S_IMODE(archive.manifest_path.stat().st_mode), 0o600
                )

    def test_preflash_backup_and_parent_directories_are_synced(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            run_directory = root / "private-run"
            release._safe_private_directory(run_directory)
            with (
                mock.patch.object(
                    release, "_fsync_file", wraps=release._fsync_file
                ) as sync_file,
                mock.patch.object(
                    release, "_fsync_directory", wraps=release._fsync_directory
                ) as sync_directory,
            ):
                backup = release._create_backup(FakeEspTool(), run_directory)

            backup_path = Path(backup["path"])
            sync_file.assert_any_call(backup_path)
            synced_directories = {
                call.args[0] for call in sync_directory.call_args_list
            }
            self.assertIn(run_directory, synced_directories)
            self.assertIn(root, synced_directories)

    def test_blob_or_full_backup_tamper_invalidates_archive(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive, _, _ = self.make_archive(root)
            blob = archive.blobs[0]
            content = bytearray(blob.path.read_bytes())
            content[0] ^= 1
            blob.path.write_bytes(content)
            if os.name == "posix":
                os.chmod(blob.path, 0o600)
            with self.assertRaisesRegex(release.SafetyError, "blob changed"):
                release.revalidate_raw_state_archive(archive)

            second = root / "second"
            second.mkdir()
            archive, _, _ = self.make_archive(second)
            with archive.backup_path.open("r+b") as handle:
                handle.seek(0)
                handle.write(b"X")
            with self.assertRaisesRegex(release.SafetyError, "backup changed"):
                release.revalidate_raw_state_archive(archive)

    def test_candidate_manifest_must_explicitly_carry_full_partition_sector(
        self,
    ) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            path = write_manifest(root, "candidate", "candidate.bin")
            raw = json.loads(path.read_text(encoding="utf-8"))
            image = root / raw["segments"][0]["file"]
            image.write_bytes(image.read_bytes()[:-1])
            raw["segments"][0]["size"] = image.stat().st_size
            raw["segments"][0]["sha256"] = digest(image)
            path.write_text(json.dumps(raw), encoding="utf-8")
            manifest = release.load_manifest(path, "candidate")
            with self.assertRaisesRegex(release.SafetyError, "explicitly contain"):
                release.validate_manifest_partition_layout(manifest)


@unittest.skipUnless(os.name == "posix", "flock contract is Linux-only")
class HardwareLockTests(unittest.TestCase):
    def test_preprovisioned_group_lock_is_accepted(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            lock_path = Path(temporary) / "meshcore-hardware.lock"
            lock_path.touch(mode=0o660)
            os.chmod(lock_path, 0o660)
            with release.HardwareLock(lock_path, timeout=0.1):
                self.assertTrue(lock_path.is_file())

    def test_missing_or_world_accessible_lock_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            lock_path = Path(temporary) / "meshcore-hardware.lock"
            with self.assertRaisesRegex(release.SafetyError, "unavailable"):
                with release.HardwareLock(lock_path, timeout=0.1):
                    pass

            lock_path.touch(mode=0o666)
            os.chmod(lock_path, 0o666)
            with self.assertRaisesRegex(release.SafetyError, "unsafe"):
                with release.HardwareLock(lock_path, timeout=0.1):
                    pass


class ReceiptTests(unittest.TestCase):
    def test_sensitive_public_fields_and_values_fail(self) -> None:
        with self.assertRaisesRegex(release.SafetyError, "sensitive field"):
            release.assert_public_receipt_safe({"wifi_ssid": "private"})
        with self.assertRaisesRegex(release.SafetyError, "device identity"):
            release.assert_public_receipt_safe({"message": "/dev/ttyACM2"})
        with self.assertRaisesRegex(release.SafetyError, "supplied secret"):
            release.assert_public_receipt_safe(
                {"message": "joined hidden network"}, ["hidden network"]
            )

    def test_atomic_private_output_is_mode_0600(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "receipt.json"
            release.atomic_write_json(output, {"ok": True}, 0o600)
            if os.name == "posix":
                self.assertEqual(stat.S_IMODE(output.stat().st_mode), 0o600)

    def test_public_gate_rejects_boolean_only_receipt(self) -> None:
        self.assertFalse(
            release.public_receipt_release_eligible({"release_eligible": True})
        )

    def test_recovery_target_evidence_is_diagnostic_only_and_exact(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "recovery.json"
            commit = "a" * 40
            manifest_sha256 = "b" * 64
            challenge = "c" * 32
            evidence = {
                "schema_version": 1,
                "operation": "recovery_rf_off",
                "commit": commit,
                "manifest_sha256": manifest_sha256,
                "challenge": challenge,
                "outcome": "diagnostic_pass",
                "boot_ready_marker": True,
                "usb_reconnected": True,
                "structural_rf_policy": "blocked",
                "rf_blocked_marker": True,
                "boot_advert_queued_markers": 0,
                "public_chat_queued_markers": 0,
                "physical_rf_observer": "unavailable",
                "physical_rf_blocked_verified": False,
                "soak_duration_seconds": 60,
            }
            output.write_text(json.dumps(evidence), encoding="utf-8")
            release.validate_recovery_evidence(
                output, commit, manifest_sha256, challenge
            )

            self.assertNotIn("tx_blocked", evidence)
            self.assertNotIn("outbound_markers", evidence)

            evidence["physical_rf_blocked_verified"] = True
            output.write_text(json.dumps(evidence), encoding="utf-8")
            with self.assertRaisesRegex(
                release.SafetyError, "physical_rf_blocked_verified"
            ):
                release.validate_recovery_evidence(
                    output, commit, manifest_sha256, challenge
                )

            evidence["boot_advert_queued_markers"] = 0
            evidence["extra"] = True
            output.write_text(json.dumps(evidence), encoding="utf-8")
            with self.assertRaisesRegex(release.SafetyError, "schema"):
                release.validate_recovery_evidence(
                    output, commit, manifest_sha256, challenge
                )

            evidence.pop("extra")
            evidence["physical_rf_blocked_verified"] = False
            evidence["boot_advert_queued_markers"] = 1
            output.write_text(json.dumps(evidence), encoding="utf-8")
            with self.assertRaisesRegex(
                release.SafetyError, "boot_advert_queued_markers"
            ):
                release.validate_recovery_evidence(
                    output, commit, manifest_sha256, challenge
                )

    def test_candidate_target_evidence_cannot_claim_observed_rf(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "candidate.json"
            commit = "a" * 40
            manifest_sha256 = "b" * 64
            challenge = "c" * 32
            evidence = {
                "schema_version": 1,
                "operation": "postflash_smoke",
                "commit": commit,
                "manifest_sha256": manifest_sha256,
                "challenge": challenge,
                "outcome": "diagnostic_pass",
                "boot_ready_marker": True,
                "usb_reconnected": True,
                "structural_rf_policy": "one_boot_advert",
                "boot_advert_queued_markers": 1,
                "public_chat_queued_markers": 0,
                "physical_rf_observer": "unavailable",
                "physical_one_boot_advert_verified": False,
                "soak_duration_seconds": 900,
            }
            output.write_text(json.dumps(evidence), encoding="utf-8")
            release.validate_smoke_evidence(output, commit, manifest_sha256, challenge)

            evidence["physical_one_boot_advert_verified"] = True
            output.write_text(json.dumps(evidence), encoding="utf-8")
            with self.assertRaisesRegex(
                release.SafetyError, "physical_one_boot_advert_verified"
            ):
                release.validate_smoke_evidence(
                    output, commit, manifest_sha256, challenge
                )

            evidence["physical_one_boot_advert_verified"] = False
            evidence["outbound_packets"] = 1
            output.write_text(json.dumps(evidence), encoding="utf-8")
            with self.assertRaisesRegex(release.SafetyError, "schema"):
                release.validate_smoke_evidence(
                    output, commit, manifest_sha256, challenge
                )

    def test_canonical_claim_requires_the_admitted_external_evidence_digest(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            candidate = release.load_manifest(
                write_manifest(root, "candidate", "candidate.bin"), "candidate"
            )
            recovery = release.load_manifest(
                write_manifest(root, "recovery", "recovery.bin"), "recovery"
            )
            receipt = eligible_receipt(candidate, recovery)
            release._validate_public_release_receipt_schema(
                receipt,
                expected_commit=candidate.commit,
                expected_artifacts={
                    "candidate": release.manifest_public_artifacts(candidate),
                    "recovery": release.manifest_public_artifacts(recovery),
                },
            )
            with self.assertRaisesRegex(release.SafetyError, "evidence digest"):
                release.validate_public_release_receipt(receipt)
            self.assertFalse(release.public_receipt_release_eligible(receipt))
            release.validate_public_release_receipt(
                receipt,
                expected_source_evidence_sha256="f" * 64,
                expected_evidence_bundle_sha256="e" * 64,
            )
            self.assertTrue(
                release.public_receipt_release_eligible(
                    receipt,
                    expected_source_evidence_sha256="f" * 64,
                    expected_evidence_bundle_sha256="e" * 64,
                )
            )
            with self.assertRaisesRegex(release.SafetyError, "admitted evidence"):
                release.validate_public_release_receipt(
                    receipt,
                    expected_source_evidence_sha256="0" * 64,
                    expected_evidence_bundle_sha256="e" * 64,
                )
            with self.assertRaisesRegex(release.SafetyError, "evidence bundle"):
                release.validate_public_release_receipt(
                    receipt,
                    expected_source_evidence_sha256="f" * 64,
                    expected_evidence_bundle_sha256="0" * 64,
                )
            stale = json.loads(json.dumps(receipt))
            stale["external_evidence"]["recovery_image_sha256"] = "0" * 64
            with self.assertRaisesRegex(release.SafetyError, "stale image"):
                release.validate_public_release_receipt(
                    stale,
                    expected_source_evidence_sha256="f" * 64,
                    expected_evidence_bundle_sha256="e" * 64,
                )

    def test_observer_admission_hashes_the_supporting_bundle_bytes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            candidate = release.load_manifest(
                write_manifest(root, "candidate", "candidate.bin"), "candidate"
            )
            recovery = release.load_manifest(
                write_manifest(root, "recovery", "recovery.bin"), "recovery"
            )
            bundle_path = root / "observer.json"
            bundle_path.write_bytes(b"different bytes")
            source = release.evidence_source_reference(
                "n30nex/KrabDeck",
                123,
                456,
                "sha256:" + "d" * 64,
                candidate.commit,
            )
            with (
                mock.patch.object(release, "verify_github_source_metadata"),
                mock.patch.object(
                    release,
                    "verify_attested_requirement",
                    return_value=(
                        {"evidence_bundle_sha256": "e" * 64},
                        "f" * 64,
                    ),
                ),
            ):
                with self.assertRaisesRegex(release.SafetyError, "observer evidence"):
                    release.validate_independent_rf_evidence(
                        root / "release-evidence.json",
                        bundle_path,
                        source,
                        root / "artifact-metadata.json",
                        root / "run-metadata.json",
                        candidate,
                        recovery,
                    )

    def test_missing_extra_or_false_gate_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            candidate = release.load_manifest(
                write_manifest(root, "candidate", "candidate.bin"), "candidate"
            )
            recovery = release.load_manifest(
                write_manifest(root, "recovery", "recovery.bin"), "recovery"
            )
            original = eligible_receipt(candidate, recovery)
            variants = []
            missing = json.loads(json.dumps(original))
            del missing["gates"]["smoke_passed"]
            variants.append(missing)
            extra = json.loads(json.dumps(original))
            extra["gates"]["invented_gate"] = True
            variants.append(extra)
            unsatisfied = json.loads(json.dumps(original))
            unsatisfied["gates"]["smoke_passed"] = False
            variants.append(unsatisfied)
            extra_field = json.loads(json.dumps(original))
            extra_field["note"] = "not canonical"
            variants.append(extra_field)
            for receipt in variants:
                with self.subTest(receipt=receipt):
                    self.assertFalse(release.public_receipt_release_eligible(receipt))

    def test_canonical_scalar_types_fail_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            candidate = release.load_manifest(
                write_manifest(root, "candidate", "candidate.bin"), "candidate"
            )
            recovery = release.load_manifest(
                write_manifest(root, "recovery", "recovery.bin"), "recovery"
            )
            original = eligible_receipt(candidate, recovery)
            for field, value in (
                ("schema_version", True),
                ("release_eligible", 1),
                ("generated_at", "2026-99-99T12:00:00+00:00"),
            ):
                receipt = json.loads(json.dumps(original))
                receipt[field] = value
                with self.subTest(field=field):
                    self.assertFalse(release.public_receipt_release_eligible(receipt))

    def test_receipt_rejects_artifact_role_and_manifest_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            candidate = release.load_manifest(
                write_manifest(root, "candidate", "candidate.bin"), "candidate"
            )
            recovery = release.load_manifest(
                write_manifest(root, "recovery", "recovery.bin"), "recovery"
            )
            original = eligible_receipt(candidate, recovery)

            missing_role = json.loads(json.dumps(original))
            del missing_role["artifacts"]["recovery"]
            self.assertFalse(release.public_receipt_release_eligible(missing_role))

            mismatched = json.loads(json.dumps(original))
            mismatched["artifacts"]["recovery"][0]["sha256"] = "0" * 64
            with self.assertRaisesRegex(release.SafetyError, "do not match"):
                release.validate_public_release_receipt(
                    mismatched,
                    expected_commit=candidate.commit,
                    expected_artifacts={
                        "candidate": release.manifest_public_artifacts(candidate),
                        "recovery": release.manifest_public_artifacts(recovery),
                    },
                    expected_source_evidence_sha256="f" * 64,
                    expected_evidence_bundle_sha256="e" * 64,
                )


class OrchestratorTests(unittest.TestCase):
    def run_post_smoke_fault(
        self,
        root: Path,
        fault: Exception,
        *,
        recovery_failure_calls: set[int] | None = None,
        recovery_failure: Exception | None = None,
    ) -> tuple[bool, FakeEspTool, dict[str, object]]:
        armed = {"value": False}
        original_revalidate = release.revalidate_manifest_bytes

        class ArmingHooks(FakeHooks):
            def collect_smoke(
                self,
                run_directory: Path,
                output: Path,
                commit: str,
                manifest_sha256: str,
            ) -> dict[str, object]:
                result = super().collect_smoke(
                    run_directory, output, commit, manifest_sha256
                )
                armed["value"] = True
                return result

        def fault_after_smoke(manifest: release.FlashManifest) -> None:
            if armed["value"] and manifest.role == "candidate":
                armed["value"] = False
                raise fault
            original_revalidate(manifest)

        tool = FakeEspTool(
            recovery_failure_calls=recovery_failure_calls,
            recovery_failure=recovery_failure,
        )
        with mock.patch.object(
            release, "revalidate_manifest_bytes", side_effect=fault_after_smoke
        ):
            eligible = release.execute_release(
                write_manifest(root, "candidate", "candidate.bin"),
                write_manifest(root, "recovery", "recovery.bin"),
                root / "state",
                root / "public.json",
                root / "smoke.json",
                "/venv/bin/python",
                runner=RecordingRunner(),
                lock_factory=contextlib.nullcontext,
                guard_factory=lambda _: FakeGuard(),
                esptool_factory=lambda _runner, _python: tool,
                hooks_factory=lambda _runner, _python: ArmingHooks(),
            )
        private_path = next((root / "state").glob("*/receipt.json"))
        private = json.loads(private_path.read_text(encoding="utf-8"))
        return eligible, tool, private

    def test_noncanonical_captured_layout_stops_after_backup_before_erase(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            candidate = write_manifest(root, "candidate", "candidate.bin")
            recovery = write_manifest(root, "recovery", "recovery.bin")
            invalid_table = bytearray(canonical_partition_table())
            invalid_table[0] ^= 1
            tool = FakeEspTool(backup_partition_table=bytes(invalid_table))
            eligible = release.execute_release(
                candidate,
                recovery,
                root / "state",
                root / "public.json",
                root / "smoke.json",
                "/venv/bin/python",
                runner=RecordingRunner(),
                lock_factory=contextlib.nullcontext,
                guard_factory=lambda _: FakeGuard(),
                esptool_factory=lambda _runner, _python: tool,
                hooks_factory=lambda _runner, _python: FakeHooks(),
            )
            self.assertFalse(eligible)
            self.assertEqual(tool.erase_calls, 0)
            self.assertEqual(tool.candidate_writes, 0)
            backup = next((root / "state").glob("*/preflash.bin"))
            self.assertEqual(backup.stat().st_size, release.FLASH_SIZE)

    def test_late_safety_error_restores_recovery_before_unlock(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            eligible, tool, private = self.run_post_smoke_fault(
                Path(temporary), release.SafetyError("injected late safety fault")
            )

            self.assertFalse(eligible)
            self.assertEqual(
                tool.write_roles, ["recovery", "candidate", "recovery"]
            )
            self.assertEqual(private["error"], "injected late safety fault")
            self.assertTrue(private["gates"]["recovery_ready"])
            self.assertTrue(private["recovery"]["fallback_ok"])
            self.assertTrue(private["recovery"]["installed_final"])

    def test_late_ordinary_exception_restores_recovery_and_keeps_type(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            eligible, tool, private = self.run_post_smoke_fault(
                Path(temporary), OSError("injected ordinary fault")
            )

            self.assertFalse(eligible)
            self.assertEqual(
                tool.write_roles, ["recovery", "candidate", "recovery"]
            )
            self.assertEqual(
                private["error"], "OSError: injected ordinary fault"
            )
            self.assertTrue(private["gates"]["recovery_ready"])
            self.assertTrue(private["recovery"]["installed_final"])

    def test_recovery_failure_is_appended_to_original_failure(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            eligible, tool, private = self.run_post_smoke_fault(
                Path(temporary),
                release.SafetyError("injected original transaction fault"),
                recovery_failure_calls={2, 3},
                recovery_failure=OSError("injected recovery I/O fault"),
            )

            self.assertFalse(eligible)
            self.assertEqual(
                tool.write_roles,
                ["recovery", "candidate", "recovery", "recovery"],
            )
            self.assertIn("injected original transaction fault", private["error"])
            self.assertIn(
                "RF-off transaction recovery also failed", private["error"]
            )
            self.assertIn("OSError: injected recovery I/O fault", private["error"])
            self.assertFalse(private["gates"]["recovery_ready"])
            self.assertFalse(private["recovery"]["installed_final"])
            self.assertEqual(len(private["recovery"]["attempt_errors"]), 2)

    def test_target_only_diagnostics_fail_closed_and_restore_recovery(
        self,
    ) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            candidate_path = write_manifest(root, "candidate", "candidate.bin")
            recovery_path = write_manifest(root, "recovery", "recovery.bin")
            smoke = root / "smoke.json"
            public = root / "public.json"
            events: list[str] = []
            tool = FakeEspTool(events=events)
            eligible = release.execute_release(
                candidate_path,
                recovery_path,
                root / "state",
                public,
                smoke,
                "/venv/bin/python",
                runner=RecordingRunner(),
                lock_factory=contextlib.nullcontext,
                guard_factory=lambda _: FakeGuard(),
                esptool_factory=lambda _runner, _python: tool,
                hooks_factory=lambda _runner, _python: FakeHooks(events=events),
            )
            receipt = json.loads(public.read_text(encoding="utf-8"))
            private_path = next((root / "state").glob("*/receipt.json"))

            private = json.loads(private_path.read_text(encoding="utf-8"))
            self.assertFalse(eligible)
            self.assertFalse(receipt["release_eligible"])
            self.assertFalse(receipt["gates"]["one_boot_advert_verified"])
            self.assertFalse(receipt["gates"]["recovery_rf_blocked"])
            self.assertTrue(receipt["gates"]["recovery_ready"])
            self.assertFalse(release.public_receipt_release_eligible(receipt))
            self.assertTrue(private["recovery"]["used"])
            self.assertFalse(private["recovery"]["ok"])
            self.assertTrue(private["recovery"]["diagnostics_ok"])
            self.assertTrue(private["recovery"]["installed_final"])
            self.assertTrue(private["recovery"]["evidence"]["local_only"])
            self.assertTrue(private["candidate_diagnostics"]["local_only"])
            self.assertIn("independent RF observer unavailable", private["error"])
            self.assertEqual(tool.candidate_writes, 1)
            self.assertEqual(tool.recovery_writes, 2)
            self.assertEqual(tool.state_restore_calls, 3)
            self.assertEqual(tool.last_role, "recovery")
            self.assertEqual(
                events,
                [
                    "flash:recovery",
                    "restore:recovery",
                    "readback:nvs",
                    "readback:nvs_keys",
                    "readback:spiffs",
                    "reset",
                    "recovery_evidence",
                    "flash:candidate",
                    "restore:candidate",
                    "readback:nvs",
                    "readback:nvs_keys",
                    "readback:spiffs",
                    "reset",
                    "candidate_smoke",
                    "flash:recovery",
                    "restore:recovery",
                    "readback:nvs",
                    "readback:nvs_keys",
                    "readback:spiffs",
                    "reset",
                ],
            )
            for index, event in enumerate(events):
                if event == "flash:candidate":
                    self.assertEqual(events[index + 1], "restore:candidate")
                    self.assertEqual(
                        events[index + 2 : index + 5],
                        ["readback:nvs", "readback:nvs_keys", "readback:spiffs"],
                    )
                    self.assertEqual(events[index + 5], "reset")
            self.assertEqual(
                [attempt["phase"] for attempt in private["attempts"]],
                ["recovery_drill", "final_candidate", "final_recovery_posture"],
            )
            self.assertEqual(set(receipt["artifacts"]), {"candidate", "recovery"})
            self.assertNotIn(
                release.EXPECTED_USB_SERIAL, public.read_text(encoding="utf-8")
            )
            if os.name == "posix":
                self.assertEqual(stat.S_IMODE(private_path.stat().st_mode), 0o600)
                self.assertEqual(
                    stat.S_IMODE(
                        next((root / "state").glob("*/preflash.bin")).stat().st_mode
                    ),
                    0o600,
                )
                state_root = next((root / "state").glob("*/raw-partition-state"))
                self.assertEqual(stat.S_IMODE(state_root.stat().st_mode), 0o700)
                for path in state_root.iterdir():
                    self.assertEqual(stat.S_IMODE(path.stat().st_mode), 0o600)
                restoration = next((root / "state").glob("*/state-restoration.json"))
                self.assertEqual(stat.S_IMODE(restoration.stat().st_mode), 0o600)

    def test_independent_exact_hash_evidence_owns_only_rf_gates(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            candidate_path = write_manifest(root, "candidate", "candidate.bin")
            recovery_path = write_manifest(root, "recovery", "recovery.bin")
            public = root / "public.json"
            tool = FakeEspTool()
            bundle_path = root / "observer-bundle.json"
            bundle_path.write_bytes(b"sanitized independent observer evidence")
            bundle_digest = digest(bundle_path)
            source = release.evidence_source_reference(
                "n30nex/KrabDeck",
                123,
                456,
                "sha256:" + "d" * 64,
                "a" * 40,
            )
            with (
                mock.patch.object(
                    release,
                    "verify_attested_requirement",
                    return_value=(
                        {
                            "id": release.RF_OBSERVER_REQUIREMENT,
                            "evidence_class": "independent-observer",
                            "evidence_bundle_sha256": bundle_digest,
                        },
                        "f" * 64,
                    ),
                ) as verify_observer,
                mock.patch.object(
                    release, "verify_github_source_metadata"
                ) as verify_source,
            ):
                eligible = release.execute_release(
                    candidate_path,
                    recovery_path,
                    root / "state",
                    public,
                    root / "smoke.json",
                    "/venv/bin/python",
                    observer_evidence_path=root / "release-evidence.json",
                    observer_bundle_path=bundle_path,
                    observer_source_reference=source,
                    observer_artifact_metadata_path=root / "artifact-metadata.json",
                    observer_run_metadata_path=root / "run-metadata.json",
                    runner=RecordingRunner(),
                    lock_factory=contextlib.nullcontext,
                    guard_factory=lambda _: FakeGuard(),
                    esptool_factory=lambda _runner, _python: tool,
                    hooks_factory=lambda _runner, _python: FakeHooks(),
                )
            receipt = json.loads(public.read_text(encoding="utf-8"))
            private_path = next((root / "state").glob("*/receipt.json"))
            private = json.loads(private_path.read_text(encoding="utf-8"))
            self.assertTrue(eligible)
            self.assertTrue(receipt["release_eligible"])
            self.assertTrue(all(receipt["gates"].values()))
            self.assertTrue(receipt["recovery"]["ok"])
            self.assertEqual(receipt["external_evidence"]["source_evidence_sha256"], "f" * 64)
            self.assertEqual(
                private["candidate_diagnostics"]["physical_rf_observer"],
                "unavailable",
            )
            self.assertTrue(private["recovery"]["evidence"]["local_only"])
            self.assertEqual(tool.write_roles, ["recovery", "candidate"])
            self.assertEqual(tool.last_role, "candidate")
            verify_observer.assert_called_once()
            verify_source.assert_called_once_with(
                root / "artifact-metadata.json",
                root / "run-metadata.json",
                source,
                "a" * 40,
            )

    def test_invalid_external_evidence_stops_before_device_access(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            guard_called = False

            def guard_factory(_: object) -> FakeGuard:
                nonlocal guard_called
                guard_called = True
                return FakeGuard()

            with mock.patch.object(
                release,
                "verify_attested_requirement",
                side_effect=ValueError("recovery digest mismatch"),
            ), mock.patch.object(release, "verify_github_source_metadata"):
                bundle_path = root / "observer-bundle.json"
                bundle_path.write_bytes(b"observer")
                source = release.evidence_source_reference(
                    "n30nex/KrabDeck",
                    123,
                    456,
                    "sha256:" + "d" * 64,
                    "a" * 40,
                )
                with self.assertRaisesRegex(release.SafetyError, "observer evidence"):
                    release.execute_release(
                        write_manifest(root, "candidate", "candidate.bin"),
                        write_manifest(root, "recovery", "recovery.bin"),
                        root / "state",
                        root / "public.json",
                        root / "smoke.json",
                        "/venv/bin/python",
                        observer_evidence_path=root / "release-evidence.json",
                        observer_bundle_path=bundle_path,
                        observer_source_reference=source,
                        observer_artifact_metadata_path=root / "artifact-metadata.json",
                        observer_run_metadata_path=root / "run-metadata.json",
                        guard_factory=guard_factory,
                    )
            self.assertFalse(guard_called)
            self.assertFalse((root / "state").exists())

    def test_readback_hash_mismatch_retries_without_losing_state(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            tool = FakeEspTool(corrupt_readback_calls={4})
            eligible = release.execute_release(
                write_manifest(root, "candidate", "candidate.bin"),
                write_manifest(root, "recovery", "recovery.bin"),
                root / "state",
                root / "public.json",
                root / "smoke.json",
                "/venv/bin/python",
                runner=RecordingRunner(),
                lock_factory=contextlib.nullcontext,
                guard_factory=lambda _: FakeGuard(),
                esptool_factory=lambda _runner, _python: tool,
                hooks_factory=lambda _runner, _python: FakeHooks(),
            )
            self.assertFalse(eligible)
            self.assertEqual(tool.candidate_writes, 2)
            self.assertEqual(tool.recovery_writes, 2)
            self.assertEqual(tool.last_role, "recovery")
            restoration_path = next((root / "state").glob("*/state-restoration.json"))
            restoration = json.loads(restoration_path.read_text(encoding="utf-8"))
            self.assertEqual(
                [attempt["outcome"] for attempt in restoration["attempts"]],
                ["pass", "fail", "pass", "pass"],
            )
            for attempt in (
                restoration["attempts"][0],
                restoration["attempts"][2],
                restoration["attempts"][3],
            ):
                self.assertTrue(attempt["bytes_verified"])
                self.assertTrue(attempt["reset_reconnected"])
                self.assertEqual(
                    [item["label"] for item in attempt["partitions"]],
                    ["nvs", "nvs_keys", "spiffs"],
                )
                self.assertTrue(
                    all(
                        item["source_sha256"] == item["readback_sha256"]
                        for item in attempt["partitions"]
                    )
                )

    def test_persistent_restore_failure_still_boots_clean_rf_off_recovery(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            tool = FakeEspTool(corrupt_readback_labels={"nvs"})
            eligible = release.execute_release(
                write_manifest(root, "candidate", "candidate.bin"),
                write_manifest(root, "recovery", "recovery.bin"),
                root / "state",
                root / "public.json",
                root / "smoke.json",
                "/venv/bin/python",
                runner=RecordingRunner(),
                lock_factory=contextlib.nullcontext,
                guard_factory=lambda _: FakeGuard(),
                esptool_factory=lambda _runner, _python: tool,
                hooks_factory=lambda _runner, _python: FakeHooks(),
            )
            self.assertFalse(eligible)
            self.assertEqual(tool.candidate_writes, 0)
            self.assertEqual(tool.recovery_writes, 3)
            self.assertEqual(tool.last_role, "recovery")
            private_path = next((root / "state").glob("*/receipt.json"))
            private = json.loads(private_path.read_text(encoding="utf-8"))
            self.assertTrue(private["recovery"]["fallback_ok"])
            self.assertFalse(private["recovery"]["fallback_state_restored"])

    def test_two_candidate_failures_trigger_rf_off_recovery(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            candidate = write_manifest(root, "candidate", "candidate.bin")
            recovery = write_manifest(root, "recovery", "recovery.bin")
            tool = FakeEspTool(candidate_failures=2)
            eligible = release.execute_release(
                candidate,
                recovery,
                root / "state",
                root / "public.json",
                root / "missing-smoke.json",
                "/venv/bin/python",
                runner=RecordingRunner(),
                lock_factory=contextlib.nullcontext,
                guard_factory=lambda _: FakeGuard(),
                esptool_factory=lambda _runner, _python: tool,
                hooks_factory=lambda _runner, _python: FakeHooks(),
            )
            private_path = next((root / "state").glob("*/receipt.json"))
            private = json.loads(private_path.read_text(encoding="utf-8"))

            self.assertFalse(eligible)
            self.assertEqual(tool.candidate_writes, 2)
            self.assertEqual(tool.recovery_writes, 2)
            self.assertTrue(private["recovery"]["used"])
            self.assertFalse(private["recovery"]["ok"])
            self.assertTrue(private["recovery"]["fallback_used"])
            self.assertTrue(private["recovery"]["fallback_ok"])
            self.assertFalse(private["gates"]["recovery_rf_blocked"])
            self.assertTrue(private["gates"]["recovery_ready"])
            self.assertFalse(private["release_eligible"])

    def test_failed_post_flash_smoke_restores_rf_off_recovery(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            candidate = write_manifest(root, "candidate", "candidate.bin")
            recovery = write_manifest(root, "recovery", "recovery.bin")
            tool = FakeEspTool()
            eligible = release.execute_release(
                candidate,
                recovery,
                root / "state",
                root / "public.json",
                root / "missing-smoke.json",
                "/venv/bin/python",
                runner=RecordingRunner(),
                lock_factory=contextlib.nullcontext,
                guard_factory=lambda _: FakeGuard(),
                esptool_factory=lambda _runner, _python: tool,
                hooks_factory=lambda _runner, _python: FakeHooks(smoke_ok=False),
            )
            private_path = next((root / "state").glob("*/receipt.json"))
            private = json.loads(private_path.read_text(encoding="utf-8"))

            self.assertFalse(eligible)
            self.assertEqual(tool.candidate_writes, 1)
            self.assertEqual(tool.recovery_writes, 2)
            self.assertFalse(private["recovery"]["ok"])
            self.assertTrue(private["recovery"]["fallback_ok"])
            self.assertFalse(private["gates"]["smoke_passed"])
            self.assertTrue(private["gates"]["recovery_ready"])
            self.assertEqual(tool.write_roles[-1], "recovery")

    def test_recovery_evidence_failure_stops_before_final_candidate(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            candidate = write_manifest(root, "candidate", "candidate.bin")
            recovery = write_manifest(root, "recovery", "recovery.bin")
            tool = FakeEspTool()
            eligible = release.execute_release(
                candidate,
                recovery,
                root / "state",
                root / "public.json",
                root / "smoke.json",
                "/venv/bin/python",
                runner=RecordingRunner(),
                lock_factory=contextlib.nullcontext,
                guard_factory=lambda _: FakeGuard(),
                esptool_factory=lambda _runner, _python: tool,
                hooks_factory=lambda _runner, _python: FakeHooks(recovery_ok=False),
            )
            private_path = next((root / "state").glob("*/receipt.json"))
            private = json.loads(private_path.read_text(encoding="utf-8"))

            self.assertFalse(eligible)
            self.assertEqual(tool.write_roles, ["recovery"])
            self.assertFalse(private["recovery"]["ok"])
            self.assertFalse(private["gates"]["recovery_rf_blocked"])
            self.assertTrue(private["gates"]["recovery_ready"])
            self.assertIn("device remains on recovery", private["error"])

    def test_final_candidate_retry_still_finishes_on_recovery_ineligible(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            candidate = write_manifest(root, "candidate", "candidate.bin")
            recovery = write_manifest(root, "recovery", "recovery.bin")
            tool = FakeEspTool(candidate_failure_calls={1})
            eligible = release.execute_release(
                candidate,
                recovery,
                root / "state",
                root / "public.json",
                root / "smoke.json",
                "/venv/bin/python",
                runner=RecordingRunner(),
                lock_factory=contextlib.nullcontext,
                guard_factory=lambda _: FakeGuard(),
                esptool_factory=lambda _runner, _python: tool,
                hooks_factory=lambda _runner, _python: FakeHooks(),
            )
            private_path = next((root / "state").glob("*/receipt.json"))
            private = json.loads(private_path.read_text(encoding="utf-8"))

            self.assertFalse(eligible)
            self.assertEqual(
                tool.write_roles, ["recovery", "candidate", "candidate", "recovery"]
            )
            self.assertEqual(tool.last_role, "recovery")
            self.assertFalse(private["gates"]["one_boot_advert_verified"])
            self.assertFalse(private["gates"]["recovery_rf_blocked"])
            restore_attempts = [
                attempt
                for attempt in private["attempts"]
                if attempt["phase"] == "final_candidate"
            ]
            self.assertEqual(
                [attempt["outcome"] for attempt in restore_attempts], ["fail", "pass"]
            )

    def test_two_final_candidate_failures_leave_rf_off_fallback(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            candidate = write_manifest(root, "candidate", "candidate.bin")
            recovery = write_manifest(root, "recovery", "recovery.bin")
            tool = FakeEspTool(candidate_failure_calls={1, 2})
            eligible = release.execute_release(
                candidate,
                recovery,
                root / "state",
                root / "public.json",
                root / "smoke.json",
                "/venv/bin/python",
                runner=RecordingRunner(),
                lock_factory=contextlib.nullcontext,
                guard_factory=lambda _: FakeGuard(),
                esptool_factory=lambda _runner, _python: tool,
                hooks_factory=lambda _runner, _python: FakeHooks(),
            )
            private_path = next((root / "state").glob("*/receipt.json"))
            private = json.loads(private_path.read_text(encoding="utf-8"))

            self.assertFalse(eligible)
            self.assertEqual(
                tool.write_roles,
                ["recovery", "candidate", "candidate", "recovery"],
            )
            self.assertTrue(private["recovery"]["fallback_ok"])
            self.assertFalse(private["recovery"]["ok"])
            self.assertTrue(private["gates"]["recovery_ready"])

    def test_recovery_flash_failure_retries_rf_off_then_fails_release(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            candidate = write_manifest(root, "candidate", "candidate.bin")
            recovery = write_manifest(root, "recovery", "recovery.bin")
            tool = FakeEspTool(recovery_failure_calls={1})
            eligible = release.execute_release(
                candidate,
                recovery,
                root / "state",
                root / "public.json",
                root / "smoke.json",
                "/venv/bin/python",
                runner=RecordingRunner(),
                lock_factory=contextlib.nullcontext,
                guard_factory=lambda _: FakeGuard(),
                esptool_factory=lambda _runner, _python: tool,
                hooks_factory=lambda _runner, _python: FakeHooks(),
            )
            private_path = next((root / "state").glob("*/receipt.json"))
            private = json.loads(private_path.read_text(encoding="utf-8"))

            self.assertFalse(eligible)
            self.assertEqual(tool.write_roles, ["recovery", "recovery"])
            self.assertTrue(private["recovery"]["fallback_ok"])
            self.assertFalse(private["gates"]["recovery_rf_blocked"])


class WorkflowContractTests(unittest.TestCase):
    def test_pi5_workflow_is_exact_and_fail_closed(self) -> None:
        workflow_path = ROOT / ".github" / "workflows" / "krabos-edge.yml"
        workflow = workflow_path.read_text(encoding="utf-8")
        self.assertIn("krabdeck-pi5", workflow)
        self.assertIn("[self-hosted, Linux, ARM64, krabdeck-pi5]", workflow)
        self.assertIn("meshcore-hardware", workflow)
        self.assertIn("pio run -j 2 -e KrabOS_TDeckPlus", workflow)
        self.assertIn("pio run -j 2 -e KrabOS_TDeckPlus_recovery", workflow)
        self.assertIn("pio run -j 2 -e KrabOS_TDeckPlus_debug", workflow)
        self.assertNotIn("pio run -j 2 -e SigurdOS_TDeck\n", workflow)
        self.assertNotIn("SigurdOS_TDeck_debug", workflow)
        self.assertIn("scripts/check_roadmap.py", workflow)
        self.assertIn(
            'KrabOS_TDeckPlus_debug/firmware-merged.bin "$KRABOS_ARTIFACTS/firmware-debug.bin"',
            workflow,
        )
        self.assertNotIn(
            'KrabOS_TDeckPlus_recovery/firmware-merged.bin "$KRABOS_ARTIFACTS/firmware-debug.bin"',
            workflow,
        )
        self.assertIn('tag="edge-$(date -u +%Y-%m-%d)-${GITHUB_SHA:0:12}"', workflow)
        self.assertIn("workflow_dispatch:", workflow)
        self.assertIn("validation", workflow)
        self.assertNotIn("  push:\n", workflow)
        self.assertIn("Reserve or validate immutable release tag", workflow)
        self.assertNotIn("--clobber", workflow)
        self.assertIn("check-public", workflow)
        self.assertIn("release_eligible == 'true'", workflow)
        self.assertIn("github.ref == 'refs/heads/main'", workflow)
        self.assertIn("steps.request.outputs.release_mode == 'true'", workflow)
        self.assertIn(".github/actions/cache-platformio", workflow)
        self.assertIn("git diff --quiet refs/remotes/origin/main", workflow)
        self.assertNotIn("\n  pull_request:\n", workflow)
        self.assertNotIn("--port", workflow)
        self.assertNotIn("/dev/ttyACM", workflow)
        self.assertNotIn("/dev/ttyUSB", workflow)


if __name__ == "__main__":
    unittest.main()
