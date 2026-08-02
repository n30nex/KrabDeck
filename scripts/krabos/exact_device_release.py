#!/usr/bin/env python3
"""Fail-closed KrabOS build-artifact flash and release gate for one T-Deck Plus.

The command line intentionally has no port option. Hardware commands can only
address the single stable USB identity pinned below. Importing this module and
running its unit tests never touches a serial device.
"""

from __future__ import annotations

import argparse
import contextlib
import dataclasses
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import struct
import subprocess
import sys
import tempfile
import time
import uuid
from typing import Any, Callable, Iterable, Mapping, Sequence


SCRIPT_DIRECTORY = Path(__file__).resolve().parent
SCRIPTS_DIRECTORY = SCRIPT_DIRECTORY.parent
if str(SCRIPTS_DIRECTORY) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIRECTORY))

from verify_release_evidence import (  # noqa: E402 - fixed repository module
    evidence_source_reference,
    verify_attested_requirement,
    verify_github_source_metadata,
)

try:  # Linux-only hardware execution; imports must still work in Windows CI.
    import fcntl
except ImportError:  # pragma: no cover - exercised by Windows import itself
    fcntl = None  # type: ignore[assignment]


PRODUCT = "KrabOS"
BOARD = "lilygo-t-deck-plus"
TARGET_CHIP = "esp32s3"
FLASH_SIZE = 16 * 1024 * 1024
BUILD_ENVIRONMENTS = {
    "candidate": "KrabOS_TDeckPlus",
    "recovery": "KrabOS_TDeckPlus_recovery",
}
FLASH_POLICIES = {
    "candidate": {"rf_policy": "one_boot_advert", "mesh_tx_enabled": True},
    "recovery": {"rf_policy": "blocked", "mesh_tx_enabled": False},
}

STABLE_RELEASE_TAG = "v1.0.0"
RF_OBSERVER_REQUIREMENT = "RF-END-TO-END"

TARGET_BY_ID = Path(
    "/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_CC:8D:A2:0D:14:28-if00"
)
EXPECTED_USB_SERIAL = "CC:8D:A2:0D:14:28"
EXPECTED_EFUSE_MAC = EXPECTED_USB_SERIAL
EXPECTED_ID_PATH = "platform-xhci-hcd.0-usb-0:1.1.2:1.0"
EXPECTED_PROPERTIES = {
    "ID_BUS": "usb",
    "ID_VENDOR_ID": "303a",
    "ID_MODEL_ID": "1001",
    "ID_SERIAL_SHORT": EXPECTED_USB_SERIAL,
    "ID_USB_INTERFACE_NUM": "00",
    "ID_USB_DRIVER": "cdc_acm",
    "ID_PATH": EXPECTED_ID_PATH,
}

D1L_BY_ID = Path("/dev/serial/by-id/usb-1a86_USB_Serial-if00-port0")
PEER_ESP_BY_ID = Path(
    "/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_8C:BF:EA:8F:8C:4C-if00"
)
FORBIDDEN_SERIALS = {"8C:BF:EA:8F:8C:4C"}
FORBIDDEN_VID_PIDS = {("1a86", "7523")}
HARDWARE_LOCK = Path("/run/lock/meshcore-hardware.lock")
HOOKS_DIRECTORY = Path(__file__).resolve().parent / "hooks"
SMOKE_COLLECTOR = HOOKS_DIRECTORY / "collect_postflash_smoke.py"
RECOVERY_COLLECTOR = HOOKS_DIRECTORY / "collect_recovery_rf_off.py"
PORT_ENVIRONMENT = {
    "ESPPORT",
    "ESPTOOL_PORT",
    "PLATFORMIO_UPLOAD_PORT",
    "PIO_UPLOAD_PORT",
    "MESH_PEER_PORT",
    "D1L_PORT",
}

SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
COMMIT_RE = re.compile(r"^[0-9a-f]{40}$")
RUN_ID_RE = re.compile(r"^[0-9]{8}T[0-9]{6}Z-[0-9a-f]{12}$")
GENERATED_AT_RE = re.compile(
    r"^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}\+00:00$"
)
MAC_RE = re.compile(r"(?i)\b(?:[0-9a-f]{2}:){5}[0-9a-f]{2}\b")
SENSITIVE_KEY_RE = re.compile(
    r"(?i)(password|passphrase|psk|ssid|latitude|longitude|location|"
    r"credential|private|serial|mac|by[_-]?id|id[_-]?path|backup|cid)"
)

PARTITION_TABLE_OFFSET = 0x8000
PARTITION_TABLE_SIZE = 0x1000
PARTITION_ENTRY_SIZE = 0x20
PARTITION_ENTRY_MAGIC = 0x50AA
PARTITION_MD5_MAGIC = 0xEBEB
PARTITION_FLAG_ENCRYPTED = 0x01

# This is the sole admitted 16 MiB KrabOS layout. State is restored only when
# both build roles and the captured device already carry this exact table.
CANONICAL_PARTITIONS = (
    ("nvs", 0x01, 0x02, 0x9000, 0x4000, 0),
    ("nvs_keys", 0x01, 0x04, 0xD000, 0x1000, PARTITION_FLAG_ENCRYPTED),
    ("otadata", 0x01, 0x00, 0xE000, 0x2000, 0),
    ("app0", 0x00, 0x10, 0x10000, 0x640000, 0),
    ("app1", 0x00, 0x11, 0x650000, 0x640000, 0),
    ("spiffs", 0x01, 0x82, 0xC90000, 0x360000, PARTITION_FLAG_ENCRYPTED),
    ("coredump", 0x01, 0x03, 0xFF0000, 0x10000, PARTITION_FLAG_ENCRYPTED),
)
PRESERVED_PARTITION_LABELS = ("nvs", "nvs_keys", "spiffs")

PUBLIC_RECEIPT_KEYS = frozenset(
    {
        "schema_version",
        "product",
        "board",
        "run_id",
        "commit",
        "generated_at",
        "outcome",
        "release_eligible",
        "artifacts",
        "gates",
        "recovery",
        "external_evidence",
    }
)
PUBLIC_ARTIFACT_ROLES = frozenset({"candidate", "recovery"})
PUBLIC_ARTIFACT_KEYS = frozenset({"address", "size", "sha256"})
REQUIRED_RELEASE_GATES = frozenset(
    {
        "manifest_valid",
        "exact_device_bound",
        "preflash_capture_complete",
        "preflash_state_exported",
        "flash_verified",
        "usb_reconnected",
        "smoke_passed",
        "candidate_rf_policy_bound",
        "one_boot_advert_verified",
        "public_silence_verified",
        "correlated_dm_ack_verified",
        "correlated_channel_ack_verified",
        "forced_retry_rf_off_verified",
        "recovery_rf_blocked",
        "recovery_ready",
        "secrets_redacted",
    }
)
PUBLIC_RECOVERY_KEYS = frozenset({"used", "ok"})
PUBLIC_EXTERNAL_EVIDENCE_KEYS = frozenset(
    {
        "requirement_id",
        "evidence_class",
        "source_evidence_sha256",
        "evidence_bundle_sha256",
        "production_image_sha256",
        "recovery_image_sha256",
    }
)


class SafetyError(RuntimeError):
    """A fail-closed safety gate rejected the requested operation."""


class CommandError(SafetyError):
    """A bounded external command failed."""


def utc_now() -> str:
    return dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def normalize_mac(value: str) -> str:
    match = MAC_RE.search(value)
    if not match:
        raise SafetyError("esptool did not return a MAC address")
    return match.group(0).upper()


def parse_udev_properties(text: str) -> dict[str, str]:
    properties: dict[str, str] = {}
    for line in text.splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        properties[key.strip()] = value.strip()
    return properties


def parse_flash_size(text: str) -> int:
    match = re.search(r"(?i)Detected flash size:\s*(\d+)\s*(KB|MB)", text)
    if not match:
        raise SafetyError("esptool did not report the detected flash size")
    multiplier = 1024 if match.group(2).upper() == "KB" else 1024 * 1024
    return int(match.group(1)) * multiplier


def validate_security_info(text: str) -> dict[str, bool]:
    required = {
        "secure_boot_disabled": r"(?i)Secure Boot:\s*Disabled",
        "flash_encryption_disabled": r"(?i)Flash Encryption:\s*Disabled",
    }
    result = {
        name: bool(re.search(pattern, text)) for name, pattern in required.items()
    }
    if not all(result.values()):
        raise SafetyError("unexpected or unparseable ESP32 security state")
    if re.search(r"(?i)Secure download mode:\s*Enabled", text):
        raise SafetyError(
            "secure download mode is not supported by the recovery contract"
        )
    return result


def redact_text(text: str) -> str:
    redacted = text
    for value in (
        str(TARGET_BY_ID),
        str(D1L_BY_ID),
        str(PEER_ESP_BY_ID),
        EXPECTED_USB_SERIAL,
        *FORBIDDEN_SERIALS,
    ):
        redacted = redacted.replace(value, "<redacted-device>")
        redacted = redacted.replace(value.lower(), "<redacted-device>")
    return MAC_RE.sub("<redacted-device>", redacted)


def _failure_context(error: Exception) -> str:
    """Return a redacted failure description without hiding non-safety types."""
    message = redact_text(str(error)).strip()
    if isinstance(error, SafetyError):
        return message or type(error).__name__
    return f"{type(error).__name__}: {message or '<no message>'}"


@dataclasses.dataclass(frozen=True)
class CommandResult:
    returncode: int
    stdout: str = ""
    stderr: str = ""


class CommandRunner:
    """Runs argument-vector commands with port autodetection variables removed."""

    def run(
        self,
        args: Sequence[str],
        *,
        timeout: float = 120.0,
        check: bool = True,
    ) -> CommandResult:
        if not args or any(not isinstance(value, str) for value in args):
            raise SafetyError(
                "external commands require a non-empty string argument vector"
            )
        env = os.environ.copy()
        for name in PORT_ENVIRONMENT:
            env.pop(name, None)
        operation = Path(args[0]).name
        try:
            completed = subprocess.run(
                list(args),
                check=False,
                capture_output=True,
                text=True,
                timeout=timeout,
                env=env,
            )
        except subprocess.TimeoutExpired as error:
            raise CommandError(f"{operation} timed out") from error
        except OSError as error:
            raise CommandError(f"{operation} could not start") from error
        result = CommandResult(completed.returncode, completed.stdout, completed.stderr)
        if check and result.returncode:
            raise CommandError(
                f"{operation} failed with exit {result.returncode}: "
                f"{redact_text((result.stderr or result.stdout).strip())[:500]}"
            )
        return result


@dataclasses.dataclass(frozen=True)
class DeviceSnapshot:
    resolved_node: str
    properties: Mapping[str, str]
    captured_at: str

    def private_dict(self) -> dict[str, Any]:
        return {
            "by_id": str(TARGET_BY_ID),
            "resolved_node": self.resolved_node,
            "captured_at": self.captured_at,
            "properties": dict(self.properties),
        }


class DeviceGuard:
    def __init__(self, runner: CommandRunner):
        self.runner = runner

    @staticmethod
    def validate_properties(properties: Mapping[str, str]) -> None:
        vid_pid = (
            properties.get("ID_VENDOR_ID", "").lower(),
            properties.get("ID_MODEL_ID", "").lower(),
        )
        if vid_pid in FORBIDDEN_VID_PIDS:
            raise SafetyError("D1L USB identity is explicitly forbidden")
        if properties.get("ID_SERIAL_SHORT", "").upper() in FORBIDDEN_SERIALS:
            raise SafetyError("the peer ESP32 identity is explicitly forbidden")
        for key, expected in EXPECTED_PROPERTIES.items():
            actual = properties.get(key)
            if actual != expected:
                raise SafetyError(f"exact-device property mismatch: {key}")

    @staticmethod
    def _same_device(left: Path, right: Path) -> bool:
        try:
            return os.path.samefile(left, right)
        except (FileNotFoundError, OSError):
            return False

    def inspect(self, *, check_busy: bool = True) -> DeviceSnapshot:
        if os.name != "posix":
            raise SafetyError("hardware execution is Linux-only")
        if not TARGET_BY_ID.is_symlink():
            raise SafetyError("the exact T-Deck by-id symlink is absent")

        matches = [
            path
            for path in TARGET_BY_ID.parent.iterdir()
            if path.is_symlink() and EXPECTED_USB_SERIAL in path.name
        ]
        if matches != [TARGET_BY_ID]:
            raise SafetyError("the exact USB serial did not resolve uniquely")

        resolved = TARGET_BY_ID.resolve(strict=True)
        if not re.fullmatch(r"/dev/ttyACM\d+", str(resolved)):
            raise SafetyError("the exact by-id link did not resolve to a USB ACM node")
        if not stat.S_ISCHR(resolved.stat().st_mode):
            raise SafetyError("the exact by-id target is not a character device")
        for forbidden in (D1L_BY_ID, PEER_ESP_BY_ID):
            if self._same_device(TARGET_BY_ID, forbidden):
                raise SafetyError("the target aliases a forbidden device")

        result = self.runner.run(
            ["udevadm", "info", "--query=property", f"--name={TARGET_BY_ID}"],
            timeout=10,
        )
        properties = parse_udev_properties(result.stdout)
        self.validate_properties(properties)

        if check_busy:
            busy = self.runner.run(
                ["fuser", str(TARGET_BY_ID)], timeout=10, check=False
            )
            if busy.returncode == 0:
                raise SafetyError("the exact T-Deck serial node is already in use")
            if busy.returncode not in (1,):
                raise SafetyError("serial ownership could not be determined")

        return DeviceSnapshot(str(resolved), properties, utc_now())

    def validate_efuse(self, esptool: "EspTool") -> str:
        self.inspect()
        last_error: SafetyError | None = None
        for before in ("no-reset", "usb-reset"):
            try:
                mac = normalize_mac(esptool.read_mac(before=before))
            except CommandError as error:
                last_error = error
                continue
            if mac != EXPECTED_EFUSE_MAC:
                raise SafetyError("eFuse MAC does not match the pinned T-Deck identity")
            return mac
        raise SafetyError("unable to read the pinned T-Deck eFuse MAC") from last_error

    def wait_reconnect(self, timeout: float = 45.0) -> DeviceSnapshot:
        deadline = time.monotonic() + timeout
        last_error: SafetyError | None = None
        while time.monotonic() < deadline:
            try:
                return self.inspect(check_busy=False)
            except SafetyError as error:
                last_error = error
                time.sleep(0.5)
        raise SafetyError(
            "the exact T-Deck did not reconnect after reset"
        ) from last_error


class HardwareLock:
    def __init__(self, path: Path = HARDWARE_LOCK, timeout: float = 120.0):
        self.path = path
        self.timeout = timeout
        self.fd: int | None = None

    def __enter__(self) -> "HardwareLock":
        if fcntl is None or os.name != "posix":
            raise SafetyError("shared hardware locking is Linux-only")
        flags = os.O_RDWR | os.O_CLOEXEC | getattr(os, "O_NOFOLLOW", 0)
        try:
            self.fd = os.open(self.path, flags)
        except OSError as error:
            raise SafetyError(
                "the pre-provisioned shared hardware lock is unavailable"
            ) from error
        lock_stat = os.fstat(self.fd)
        lock_mode = stat.S_IMODE(lock_stat.st_mode)
        if (
            not stat.S_ISREG(lock_stat.st_mode)
            or lock_mode & 0o007
            or lock_mode & 0o060 != 0o060
        ):
            os.close(self.fd)
            self.fd = None
            raise SafetyError("the shared hardware lock has unsafe type or permissions")
        deadline = time.monotonic() + self.timeout
        while True:
            try:
                fcntl.flock(self.fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                return self
            except BlockingIOError:
                if time.monotonic() >= deadline:
                    os.close(self.fd)
                    self.fd = None
                    raise SafetyError(
                        "timed out waiting for the shared ESP hardware lock"
                    )
                time.sleep(0.25)

    def __exit__(self, *_: object) -> None:
        if self.fd is not None:
            assert fcntl is not None
            fcntl.flock(self.fd, fcntl.LOCK_UN)
            os.close(self.fd)
            self.fd = None


@dataclasses.dataclass(frozen=True)
class Segment:
    address: int
    path: Path
    size: int
    sha256: str

    def public_dict(self) -> dict[str, Any]:
        return {"address": self.address, "size": self.size, "sha256": self.sha256}


@dataclasses.dataclass(frozen=True)
class FlashManifest:
    path: Path
    role: str
    commit: str
    segments: tuple[Segment, ...]
    sha256: str


@dataclasses.dataclass(frozen=True)
class PartitionLayout:
    label: str
    partition_type: int
    subtype: int
    address: int
    size: int
    flags: int

    def contract_tuple(self) -> tuple[str, int, int, int, int, int]:
        return (
            self.label,
            self.partition_type,
            self.subtype,
            self.address,
            self.size,
            self.flags,
        )


@dataclasses.dataclass(frozen=True)
class StateBlob:
    label: str
    address: int
    path: Path
    size: int
    sha256: str

    def segment(self) -> Segment:
        return Segment(self.address, self.path, self.size, self.sha256)

    def record(self) -> dict[str, Any]:
        return {
            "label": self.label,
            "address": self.address,
            "size": self.size,
            "sha256": self.sha256,
            "file": self.path.name,
        }


@dataclasses.dataclass(frozen=True)
class RawStateArchive:
    manifest_path: Path
    manifest_sha256: str
    backup_path: Path
    backup_sha256: str
    candidate_manifest: FlashManifest
    candidate_partition_table_sha256: str
    commit: str
    challenge: str
    binding_sha256: str
    blobs: tuple[StateBlob, ...]


def _bounded_child(base: Path, value: str) -> Path:
    relative = Path(value)
    if relative.is_absolute() or ".." in relative.parts:
        raise SafetyError("manifest segment path must stay beside the manifest")
    unresolved = base / relative
    if unresolved.is_symlink():
        raise SafetyError("manifest segment must not be a symlink")
    candidate = unresolved.resolve(strict=True)
    if os.path.commonpath((str(base.resolve()), str(candidate))) != str(base.resolve()):
        raise SafetyError("manifest segment escaped its artifact directory")
    if candidate.is_symlink() or not candidate.is_file():
        raise SafetyError("manifest segment must be a regular non-symlink file")
    return candidate


def _parse_address(value: object) -> int:
    if type(value) is int:  # bool is not a flash address
        return value
    if isinstance(value, str):
        try:
            return int(value, 0)
        except ValueError as error:
            raise SafetyError("manifest address is not an integer") from error
    raise SafetyError("manifest address is not an integer")


def load_manifest(path: Path, expected_role: str) -> FlashManifest:
    if path.is_symlink() or not path.is_file():
        raise SafetyError("flash manifest must be a regular non-symlink file")
    try:
        raw = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise SafetyError("flash manifest is missing or invalid JSON") from error
    if not isinstance(raw, dict) or raw.get("schema_version") != 1:
        raise SafetyError("unsupported flash manifest schema")
    required = {
        "product": PRODUCT,
        "board": BOARD,
        "target_chip": TARGET_CHIP,
        "flash_size": FLASH_SIZE,
        "role": expected_role,
        "build_environment": BUILD_ENVIRONMENTS[expected_role],
        **FLASH_POLICIES[expected_role],
    }
    for key, expected in required.items():
        if raw.get(key) != expected:
            raise SafetyError(f"flash manifest mismatch: {key}")
    commit = raw.get("commit")
    if not isinstance(commit, str) or not COMMIT_RE.fullmatch(commit):
        raise SafetyError("manifest commit must be a full lowercase Git SHA")
    entries = raw.get("segments")
    if not isinstance(entries, list) or not entries or len(entries) > 32:
        raise SafetyError("manifest must contain between 1 and 32 segments")

    segments: list[Segment] = []
    for entry in entries:
        if not isinstance(entry, dict):
            raise SafetyError("manifest segment must be an object")
        address = _parse_address(entry.get("address"))
        declared_hash = entry.get("sha256")
        declared_size = entry.get("size")
        if not isinstance(declared_hash, str) or not SHA256_RE.fullmatch(declared_hash):
            raise SafetyError("manifest segment SHA-256 is invalid")
        if type(declared_size) is not int or declared_size <= 0:
            raise SafetyError("manifest segment size is invalid")
        file_value = entry.get("file")
        if not isinstance(file_value, str):
            raise SafetyError("manifest segment file is invalid")
        file_path = _bounded_child(path.parent, file_value)
        actual_size = file_path.stat().st_size
        if actual_size != declared_size or sha256_file(file_path) != declared_hash:
            raise SafetyError("manifest segment bytes do not match size and SHA-256")
        if address < 0 or address + actual_size > FLASH_SIZE:
            raise SafetyError("manifest segment exceeds T-Deck flash bounds")
        segments.append(Segment(address, file_path, actual_size, declared_hash))

    segments.sort(key=lambda item: item.address)
    if segments[0].address != 0:
        raise SafetyError("full recovery-safe images must include address 0x0")
    for previous, current in zip(segments, segments[1:]):
        if previous.address + previous.size > current.address:
            raise SafetyError("manifest segments overlap")
    return FlashManifest(
        path, expected_role, commit, tuple(segments), sha256_file(path)
    )


def revalidate_manifest_bytes(manifest: FlashManifest) -> None:
    if manifest.path.is_symlink() or sha256_file(manifest.path) != manifest.sha256:
        raise SafetyError("flash manifest changed after validation")
    for segment in manifest.segments:
        if segment.path.is_symlink() or not segment.path.is_file():
            raise SafetyError("flash segment changed after validation")
        if (
            segment.path.stat().st_size != segment.size
            or sha256_file(segment.path) != segment.sha256
        ):
            raise SafetyError("flash segment changed after validation")


def _manifest_flash_range(manifest: FlashManifest, address: int, size: int) -> bytes:
    """Return explicitly supplied flash bytes for a range.

    Partition admission must not infer bytes from erase gaps. Requiring the
    candidate/recovery artifact to carry the complete table makes its layout
    independently hashable and immutable before hardware access.
    """
    if address < 0 or size <= 0 or address + size > FLASH_SIZE:
        raise SafetyError("requested manifest flash range is invalid")
    result = bytearray(size)
    covered = bytearray(size)
    end = address + size
    for segment in manifest.segments:
        segment_end = segment.address + segment.size
        overlap_start = max(address, segment.address)
        overlap_end = min(end, segment_end)
        if overlap_start >= overlap_end:
            continue
        source_offset = overlap_start - segment.address
        target_offset = overlap_start - address
        length = overlap_end - overlap_start
        with segment.path.open("rb") as handle:
            handle.seek(source_offset)
            chunk = handle.read(length)
        if len(chunk) != length:
            raise SafetyError("flash manifest segment became truncated")
        result[target_offset : target_offset + length] = chunk
        covered[target_offset : target_offset + length] = b"\x01" * length
    if not all(covered):
        raise SafetyError(
            "flash manifest does not explicitly contain its partition table"
        )
    return bytes(result)


def parse_partition_table(table: bytes) -> tuple[PartitionLayout, ...]:
    if len(table) != PARTITION_TABLE_SIZE:
        raise SafetyError("partition table must be exactly one flash sector")
    entries: list[PartitionLayout] = []
    position = 0
    md5_end: int | None = None
    while position + PARTITION_ENTRY_SIZE <= len(table):
        raw = table[position : position + PARTITION_ENTRY_SIZE]
        magic = struct.unpack_from("<H", raw)[0]
        if magic == PARTITION_MD5_MAGIC:
            if raw[2:16] != b"\xff" * 14:
                raise SafetyError("partition-table MD5 record padding is invalid")
            expected = hashlib.md5(  # nosec: ESP-IDF partition format mandates MD5
                table[:position]
            ).digest()
            if raw[16:] != expected:
                raise SafetyError("partition-table MD5 does not match its entries")
            md5_end = position + PARTITION_ENTRY_SIZE
            break
        if magic in (0xFFFF, 0x0000):
            raise SafetyError("partition table ended before its MD5 record")
        if magic != PARTITION_ENTRY_MAGIC:
            raise SafetyError("partition table contains an invalid entry magic")
        (
            _,
            partition_type,
            subtype,
            partition_offset,
            partition_size,
            raw_label,
            flags,
        ) = struct.unpack("<HBBII16sI", raw)
        try:
            label = raw_label.split(b"\0", 1)[0].decode("ascii")
        except UnicodeDecodeError as error:
            raise SafetyError("partition table contains a non-ASCII label") from error
        if not label or partition_size <= 0:
            raise SafetyError("partition table contains an empty partition")
        if partition_offset % 0x1000 or partition_size % 0x1000:
            raise SafetyError("partition boundaries are not flash-sector aligned")
        if partition_offset + partition_size > FLASH_SIZE:
            raise SafetyError("partition exceeds the pinned flash size")
        entries.append(
            PartitionLayout(
                label,
                partition_type,
                subtype,
                partition_offset,
                partition_size,
                flags,
            )
        )
        position += PARTITION_ENTRY_SIZE
    if md5_end is None or not entries:
        raise SafetyError("partition table has no valid MD5 record")
    if table[md5_end:] != b"\xff" * (len(table) - md5_end):
        raise SafetyError("partition table has non-erased bytes after its MD5 record")
    if tuple(entry.contract_tuple() for entry in entries) != CANONICAL_PARTITIONS:
        raise SafetyError("partition table is not the canonical KrabOS 16 MiB layout")
    for previous, current in zip(entries, entries[1:]):
        if previous.address + previous.size > current.address:
            raise SafetyError("canonical partitions overlap")
    return tuple(entries)


def validate_manifest_partition_layout(
    manifest: FlashManifest,
) -> tuple[tuple[PartitionLayout, ...], bytes]:
    revalidate_manifest_bytes(manifest)
    table = _manifest_flash_range(
        manifest, PARTITION_TABLE_OFFSET, PARTITION_TABLE_SIZE
    )
    return parse_partition_table(table), table


def manifest_public_artifacts(manifest: FlashManifest) -> list[dict[str, Any]]:
    """Return the canonical public byte records for one admitted manifest."""
    return [segment.public_dict() for segment in manifest.segments]


def _fsync_file(path: Path) -> None:
    # Windows requires a writable descriptor for fsync; release outputs are
    # owner-writable on both platforms and O_RDWR does not alter their bytes.
    flags = (
        os.O_RDWR
        | getattr(os, "O_BINARY", 0)
        | getattr(os, "O_CLOEXEC", 0)
    )
    fd = os.open(path, flags)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def _fsync_directory(path: Path) -> None:
    # Directory handles cannot be opened this way on Windows. Hardware release
    # execution is Linux-only; Windows imports exist for static unit tests.
    if os.name != "posix":
        return
    flags = (
        os.O_RDONLY
        | getattr(os, "O_CLOEXEC", 0)
        | getattr(os, "O_DIRECTORY", 0)
    )
    fd = os.open(path, flags)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def _commit_atomic_file(temporary: str, path: Path, mode: int) -> None:
    os.replace(temporary, path)
    os.chmod(path, mode)
    # The temporary file's contents were synced before rename. Sync again after
    # chmod, then persist the directory entry containing the rename.
    _fsync_file(path)
    _fsync_directory(path.parent)


def atomic_write_json(path: Path, value: Mapping[str, Any], mode: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        os.fchmod(fd, mode)
        with os.fdopen(fd, "w", encoding="utf-8", newline="\n") as handle:
            json.dump(value, handle, indent=2, sort_keys=True)
            handle.write("\n")
            handle.flush()
            os.fsync(handle.fileno())
        _commit_atomic_file(temporary, path, mode)
    except BaseException:
        try:
            os.close(fd)
        except OSError:
            pass
        try:
            os.unlink(temporary)
        except OSError:
            pass
        raise


def atomic_write_text(path: Path, value: str, mode: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        os.fchmod(fd, mode)
        with os.fdopen(fd, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(value)
            handle.flush()
            os.fsync(handle.fileno())
        _commit_atomic_file(temporary, path, mode)
    except BaseException:
        try:
            os.close(fd)
        except OSError:
            pass
        try:
            os.unlink(temporary)
        except OSError:
            pass
        raise


def atomic_write_bytes(path: Path, value: bytes, mode: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        os.fchmod(fd, mode)
        with os.fdopen(fd, "wb") as handle:
            handle.write(value)
            handle.flush()
            os.fsync(handle.fileno())
        _commit_atomic_file(temporary, path, mode)
    except BaseException:
        try:
            os.close(fd)
        except OSError:
            pass
        try:
            os.unlink(temporary)
        except OSError:
            pass
        raise


def _private_regular_file(path: Path, description: str) -> None:
    if path.is_symlink() or not path.is_file():
        raise SafetyError(f"{description} must be a regular non-symlink file")
    if os.name == "posix" and stat.S_IMODE(path.stat().st_mode) != 0o600:
        raise SafetyError(f"{description} must have mode 0600")


def _state_binding(value: Mapping[str, Any]) -> str:
    canonical = json.dumps(value, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(
        ("krabos-raw-partition-state-v1\n" + canonical).encode("utf-8")
    ).hexdigest()


def _extract_raw_state_archive(
    run_directory: Path,
    backup: Mapping[str, Any],
    candidate: FlashManifest,
) -> RawStateArchive:
    backup_path_value = backup.get("path")
    backup_digest = backup.get("sha256")
    backup_size = backup.get("size")
    if (
        not isinstance(backup_path_value, str)
        or not isinstance(backup_digest, str)
        or not SHA256_RE.fullmatch(backup_digest)
        or type(backup_size) is not int
        or backup_size != FLASH_SIZE
    ):
        raise SafetyError("full backup metadata is invalid")
    backup_path = Path(backup_path_value)
    _private_regular_file(backup_path, "full pre-flash backup")
    if (
        backup_path.stat().st_size != FLASH_SIZE
        or sha256_file(backup_path) != backup_digest
    ):
        raise SafetyError("full pre-flash backup changed before state extraction")

    layout, candidate_table = validate_manifest_partition_layout(candidate)
    with backup_path.open("rb") as handle:
        handle.seek(PARTITION_TABLE_OFFSET)
        backup_table = handle.read(PARTITION_TABLE_SIZE)
    parse_partition_table(backup_table)
    if backup_table != candidate_table:
        raise SafetyError("captured-device and candidate partition tables differ")

    state_directory = run_directory / "raw-partition-state"
    if state_directory.exists():
        raise SafetyError("raw partition state directory already exists")
    _safe_private_directory(state_directory)
    challenge = uuid.uuid4().hex
    blobs: list[StateBlob] = []
    preserved = {
        entry.label: entry
        for entry in layout
        if entry.label in PRESERVED_PARTITION_LABELS
    }
    if tuple(preserved) != PRESERVED_PARTITION_LABELS:
        raise SafetyError("canonical state-bearing partitions are incomplete")
    with backup_path.open("rb") as handle:
        for label in PRESERVED_PARTITION_LABELS:
            partition = preserved[label]
            handle.seek(partition.address)
            content = handle.read(partition.size)
            if len(content) != partition.size:
                raise SafetyError("full backup is truncated inside a state partition")
            blob_path = state_directory / f"{label}.bin"
            atomic_write_bytes(blob_path, content, 0o600)
            blobs.append(
                StateBlob(
                    label,
                    partition.address,
                    blob_path,
                    partition.size,
                    hashlib.sha256(content).hexdigest(),
                )
            )

    partition_records = [blob.record() for blob in blobs]
    binding_value = {
        "commit": candidate.commit,
        "challenge": challenge,
        "backup_sha256": backup_digest,
        "backup_size": FLASH_SIZE,
        "candidate_manifest_sha256": candidate.sha256,
        "candidate_partition_table_sha256": hashlib.sha256(candidate_table).hexdigest(),
        "partitions": partition_records,
    }
    binding_digest = _state_binding(binding_value)
    manifest_value = {
        "schema_version": 1,
        "operation": "raw_partition_state_export",
        **binding_value,
        "flash_encryption_disabled": True,
        "binding_sha256": binding_digest,
        "local_only": True,
    }
    manifest_path = state_directory / "manifest.json"
    atomic_write_json(manifest_path, manifest_value, 0o600)
    archive = RawStateArchive(
        manifest_path,
        sha256_file(manifest_path),
        backup_path,
        backup_digest,
        candidate,
        binding_value["candidate_partition_table_sha256"],
        candidate.commit,
        challenge,
        binding_digest,
        tuple(blobs),
    )
    revalidate_raw_state_archive(archive)
    return archive


def revalidate_raw_state_archive(archive: RawStateArchive) -> None:
    _private_regular_file(archive.backup_path, "full pre-flash backup")
    if (
        archive.backup_path.stat().st_size != FLASH_SIZE
        or sha256_file(archive.backup_path) != archive.backup_sha256
    ):
        raise SafetyError("full pre-flash backup changed after state extraction")
    _private_regular_file(archive.manifest_path, "raw state manifest")
    if sha256_file(archive.manifest_path) != archive.manifest_sha256:
        raise SafetyError("raw state manifest changed after extraction")
    try:
        value = json.loads(archive.manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise SafetyError("raw state manifest is invalid") from error
    required_keys = {
        "schema_version",
        "operation",
        "commit",
        "challenge",
        "backup_sha256",
        "backup_size",
        "candidate_manifest_sha256",
        "candidate_partition_table_sha256",
        "partitions",
        "flash_encryption_disabled",
        "binding_sha256",
        "local_only",
    }
    if not isinstance(value, dict) or set(value) != required_keys:
        raise SafetyError("raw state manifest schema is invalid")
    expected_scalars = {
        "schema_version": 1,
        "operation": "raw_partition_state_export",
        "commit": archive.commit,
        "challenge": archive.challenge,
        "backup_sha256": archive.backup_sha256,
        "backup_size": FLASH_SIZE,
        "candidate_manifest_sha256": archive.candidate_manifest.sha256,
        "candidate_partition_table_sha256": archive.candidate_partition_table_sha256,
        "flash_encryption_disabled": True,
        "binding_sha256": archive.binding_sha256,
        "local_only": True,
    }
    for key, expected in expected_scalars.items():
        if type(value.get(key)) is not type(expected) or value.get(key) != expected:
            raise SafetyError(f"raw state manifest binding mismatch: {key}")
    if not re.fullmatch(r"[0-9a-f]{32}", archive.challenge):
        raise SafetyError("raw state manifest challenge is invalid")

    records = value.get("partitions")
    expected_records = [blob.record() for blob in archive.blobs]
    if records != expected_records:
        raise SafetyError("raw state manifest partition records changed")
    if tuple(blob.label for blob in archive.blobs) != PRESERVED_PARTITION_LABELS:
        raise SafetyError("raw state archive contains unexpected partitions")
    for blob in archive.blobs:
        _private_regular_file(blob.path, f"raw {blob.label} state blob")
        if (
            blob.path.parent.resolve() != archive.manifest_path.parent.resolve()
            or blob.path.name != f"{blob.label}.bin"
            or blob.path.stat().st_size != blob.size
            or sha256_file(blob.path) != blob.sha256
        ):
            raise SafetyError(f"raw {blob.label} state blob changed after extraction")

    binding_value = {
        "commit": value["commit"],
        "challenge": value["challenge"],
        "backup_sha256": value["backup_sha256"],
        "backup_size": value["backup_size"],
        "candidate_manifest_sha256": value["candidate_manifest_sha256"],
        "candidate_partition_table_sha256": value["candidate_partition_table_sha256"],
        "partitions": value["partitions"],
    }
    if _state_binding(binding_value) != archive.binding_sha256:
        raise SafetyError("raw state archive binding digest is invalid")
    layout, table = validate_manifest_partition_layout(archive.candidate_manifest)
    if hashlib.sha256(table).hexdigest() != archive.candidate_partition_table_sha256:
        raise SafetyError("candidate partition table changed after state extraction")
    expected_layout = {
        entry.label: (entry.address, entry.size)
        for entry in layout
        if entry.label in PRESERVED_PARTITION_LABELS
    }
    if any(
        expected_layout.get(blob.label) != (blob.address, blob.size)
        for blob in archive.blobs
    ):
        raise SafetyError("raw state blob no longer matches the candidate layout")


def assert_public_receipt_safe(
    receipt: Mapping[str, Any], secret_values: Iterable[str] = ()
) -> None:
    def walk(value: object, key: str = "") -> None:
        if key and SENSITIVE_KEY_RE.search(key):
            raise SafetyError(f"public receipt contains sensitive field: {key}")
        if isinstance(value, dict):
            for child_key, child_value in value.items():
                walk(child_value, str(child_key))
        elif isinstance(value, list):
            for child in value:
                walk(child, key)

    walk(receipt)
    serialized = json.dumps(receipt, sort_keys=True)
    if "/dev/" in serialized or MAC_RE.search(serialized):
        raise SafetyError("public receipt contains a device identity")
    for secret in secret_values:
        if secret and secret in serialized:
            raise SafetyError("public receipt contains a supplied secret")


def _validate_public_artifacts(artifacts: object) -> dict[str, list[dict[str, Any]]]:
    if not isinstance(artifacts, dict) or set(artifacts) != PUBLIC_ARTIFACT_ROLES:
        raise SafetyError("public receipt artifact roles are incomplete or unexpected")

    validated: dict[str, list[dict[str, Any]]] = {}
    for role in sorted(PUBLIC_ARTIFACT_ROLES):
        records = artifacts[role]
        if not isinstance(records, list) or not records or len(records) > 32:
            raise SafetyError(f"public receipt {role} artifacts are missing or invalid")
        previous_end = 0
        role_records: list[dict[str, Any]] = []
        for index, record in enumerate(records):
            if not isinstance(record, dict) or set(record) != PUBLIC_ARTIFACT_KEYS:
                raise SafetyError(f"public receipt {role} artifact schema is invalid")
            address = record["address"]
            size = record["size"]
            digest = record["sha256"]
            if type(address) is not int or address < 0:  # bool is not an address
                raise SafetyError(f"public receipt {role} artifact address is invalid")
            if type(size) is not int or size <= 0:  # bool is not a byte count
                raise SafetyError(f"public receipt {role} artifact size is invalid")
            if not isinstance(digest, str) or not SHA256_RE.fullmatch(digest):
                raise SafetyError(f"public receipt {role} artifact SHA-256 is invalid")
            if index == 0 and address != 0:
                raise SafetyError(f"public receipt {role} artifacts must start at 0x0")
            if address < previous_end or address + size > FLASH_SIZE:
                raise SafetyError(f"public receipt {role} artifact range is invalid")
            previous_end = address + size
            role_records.append(dict(record))
        validated[role] = role_records
    return validated


def _full_image_sha256(manifest: FlashManifest) -> str:
    if len(manifest.segments) != 1 or manifest.segments[0].address != 0:
        raise SafetyError(
            f"{manifest.role} observer binding requires one full image at address 0"
        )
    return manifest.segments[0].sha256


def validate_independent_rf_evidence(
    path: Path,
    bundle_path: Path,
    source_reference: Mapping[str, object],
    artifact_metadata_path: Path,
    run_metadata_path: Path,
    candidate: FlashManifest,
    recovery: FlashManifest,
) -> dict[str, str]:
    candidate_digest = _full_image_sha256(candidate)
    recovery_digest = _full_image_sha256(recovery)
    try:
        normalized_source = evidence_source_reference(
            str(source_reference.get("repository", "")),
            source_reference.get("run_id"),
            source_reference.get("artifact_id"),
            str(source_reference.get("artifact_digest", "")),
            candidate.commit,
        )
        if normalized_source != source_reference:
            raise ValueError("observer source reference is not canonical")
        verify_github_source_metadata(
            artifact_metadata_path,
            run_metadata_path,
            normalized_source,
            candidate.commit,
        )
        record, source_digest = verify_attested_requirement(
            path,
            RF_OBSERVER_REQUIREMENT,
            expected_tag=STABLE_RELEASE_TAG,
            expected_commit=candidate.commit,
            expected_artifact_sha256s={
                "production": candidate_digest,
                "recovery": recovery_digest,
            },
            expected_source_reference=normalized_source,
        )
        if bundle_path.is_symlink() or not bundle_path.is_file():
            raise ValueError("observer evidence bundle must be a regular file")
        if sha256_file(bundle_path) != record["evidence_bundle_sha256"]:
            raise ValueError("observer evidence bundle digest is invalid")
    except (OSError, ValueError) as error:
        raise SafetyError("independent RF observer evidence is invalid") from error
    binding = {
        "requirement_id": RF_OBSERVER_REQUIREMENT,
        "evidence_class": "independent-observer",
        "source_evidence_sha256": source_digest,
        "evidence_bundle_sha256": record["evidence_bundle_sha256"],
        "production_image_sha256": candidate_digest,
        "recovery_image_sha256": recovery_digest,
    }
    assert_public_receipt_safe(binding)
    return binding


def _validate_public_external_evidence(
    value: object,
    artifacts: Mapping[str, Sequence[Mapping[str, Any]]],
    *,
    expected_source_evidence_sha256: str | None,
    expected_evidence_bundle_sha256: str | None,
) -> None:
    if not isinstance(value, dict) or set(value) != PUBLIC_EXTERNAL_EVIDENCE_KEYS:
        raise SafetyError("public receipt external evidence is incomplete or unexpected")
    if value.get("requirement_id") != RF_OBSERVER_REQUIREMENT:
        raise SafetyError("public receipt external evidence requirement is invalid")
    if value.get("evidence_class") != "independent-observer":
        raise SafetyError("public receipt external evidence class is invalid")
    for field in (
        "source_evidence_sha256",
        "evidence_bundle_sha256",
        "production_image_sha256",
        "recovery_image_sha256",
    ):
        digest = value.get(field)
        if not isinstance(digest, str) or not SHA256_RE.fullmatch(digest):
            raise SafetyError(f"public receipt external evidence {field} is invalid")
    if (
        expected_source_evidence_sha256 is not None
        and value["source_evidence_sha256"] != expected_source_evidence_sha256
    ):
        raise SafetyError("public receipt is not bound to the admitted evidence packet")
    if (
        expected_evidence_bundle_sha256 is not None
        and value["evidence_bundle_sha256"] != expected_evidence_bundle_sha256
    ):
        raise SafetyError("public receipt is not bound to the admitted evidence bundle")
    if len(artifacts["candidate"]) != 1 or len(artifacts["recovery"]) != 1:
        raise SafetyError("public receipt external evidence requires full-image artifacts")
    if (
        value["production_image_sha256"] != artifacts["candidate"][0]["sha256"]
        or value["recovery_image_sha256"] != artifacts["recovery"][0]["sha256"]
    ):
        raise SafetyError("public receipt external evidence has stale image bindings")


def _validate_public_release_receipt_schema(
    receipt: Mapping[str, Any],
    *,
    expected_commit: str | None = None,
    expected_artifacts: Mapping[str, Sequence[Mapping[str, Any]]] | None = None,
    expected_source_evidence_sha256: str | None = None,
    expected_evidence_bundle_sha256: str | None = None,
) -> None:
    """Validate canonical receipt shape without admitting it for release."""
    assert_public_receipt_safe(receipt)
    if not isinstance(receipt, dict) or set(receipt) != PUBLIC_RECEIPT_KEYS:
        raise SafetyError("public receipt fields are incomplete or unexpected")
    if type(receipt["schema_version"]) is not int or receipt["schema_version"] != 1:
        raise SafetyError("public receipt release field is invalid: schema_version")
    required = {"product": PRODUCT, "board": BOARD, "outcome": "pass"}
    for key, value in required.items():
        if not isinstance(receipt[key], str) or receipt[key] != value:
            raise SafetyError(f"public receipt release field is invalid: {key}")
    if receipt["release_eligible"] is not True:
        raise SafetyError("public receipt release field is invalid: release_eligible")

    run_id = receipt["run_id"]
    if not isinstance(run_id, str) or not RUN_ID_RE.fullmatch(run_id):
        raise SafetyError("public receipt run ID is invalid")
    generated_at = receipt["generated_at"]
    if not isinstance(generated_at, str) or not GENERATED_AT_RE.fullmatch(generated_at):
        raise SafetyError("public receipt generation time is invalid")
    try:
        parsed_generated_at = dt.datetime.fromisoformat(generated_at)
    except ValueError as error:
        raise SafetyError("public receipt generation time is invalid") from error
    if parsed_generated_at.utcoffset() != dt.timedelta(0):
        raise SafetyError("public receipt generation time is invalid")
    commit = receipt["commit"]
    if not isinstance(commit, str) or not COMMIT_RE.fullmatch(commit):
        raise SafetyError("public receipt commit is invalid")
    if expected_commit is not None and commit != expected_commit:
        raise SafetyError("public receipt is not bound to the release commit")

    gates = receipt["gates"]
    if not isinstance(gates, dict) or set(gates) != REQUIRED_RELEASE_GATES:
        raise SafetyError("public receipt gate set is incomplete or unexpected")
    if not all(gates[name] is True for name in REQUIRED_RELEASE_GATES):
        raise SafetyError("public receipt contains an unsatisfied release gate")

    recovery = receipt["recovery"]
    if not isinstance(recovery, dict) or set(recovery) != PUBLIC_RECOVERY_KEYS:
        raise SafetyError(
            "public receipt recovery evidence is incomplete or unexpected"
        )
    if recovery["used"] is not True or recovery["ok"] is not True:
        raise SafetyError(
            "public receipt recovery drill was not exercised successfully"
        )

    artifacts = _validate_public_artifacts(receipt["artifacts"])
    if expected_artifacts is not None:
        if set(expected_artifacts) != PUBLIC_ARTIFACT_ROLES:
            raise SafetyError("expected public artifact roles are invalid")
        canonical_expected = {
            role: [dict(record) for record in expected_artifacts[role]]
            for role in PUBLIC_ARTIFACT_ROLES
        }
        if artifacts != canonical_expected:
            raise SafetyError(
                "public receipt artifacts do not match admitted manifests"
            )
    _validate_public_external_evidence(
        receipt["external_evidence"],
        artifacts,
        expected_source_evidence_sha256=expected_source_evidence_sha256,
        expected_evidence_bundle_sha256=expected_evidence_bundle_sha256,
    )


def validate_public_release_receipt(
    receipt: Mapping[str, Any],
    *,
    expected_commit: str | None = None,
    expected_artifacts: Mapping[str, Sequence[Mapping[str, Any]]] | None = None,
    expected_source_evidence_sha256: str | None = None,
    expected_evidence_bundle_sha256: str | None = None,
) -> None:
    """Admit only a canonical receipt bound to pre-admitted external RF evidence."""
    if (
        not isinstance(expected_source_evidence_sha256, str)
        or not SHA256_RE.fullmatch(expected_source_evidence_sha256)
    ):
        raise SafetyError("an admitted independent evidence digest is required")
    if (
        not isinstance(expected_evidence_bundle_sha256, str)
        or not SHA256_RE.fullmatch(expected_evidence_bundle_sha256)
    ):
        raise SafetyError("an admitted independent evidence bundle digest is required")
    _validate_public_release_receipt_schema(
        receipt,
        expected_commit=expected_commit,
        expected_artifacts=expected_artifacts,
        expected_source_evidence_sha256=expected_source_evidence_sha256,
        expected_evidence_bundle_sha256=expected_evidence_bundle_sha256,
    )


def public_receipt_release_eligible(
    receipt: Mapping[str, Any],
    *,
    expected_source_evidence_sha256: str | None = None,
    expected_evidence_bundle_sha256: str | None = None,
) -> bool:
    """Fail closed unless the complete canonical public contract is satisfied."""
    try:
        validate_public_release_receipt(
            receipt,
            expected_source_evidence_sha256=expected_source_evidence_sha256,
            expected_evidence_bundle_sha256=expected_evidence_bundle_sha256,
        )
    except (SafetyError, TypeError, ValueError):
        return False
    return True


def validate_smoke_evidence(
    path: Path, commit: str, manifest_sha256: str, challenge: str
) -> None:
    try:
        evidence = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise SafetyError("fresh smoke evidence is required for release") from error
    expected = {
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
    if not isinstance(evidence, dict) or set(evidence) != set(expected):
        raise SafetyError("smoke diagnostic evidence schema is invalid")
    for key, expected_value in expected.items():
        actual = evidence[key]
        if type(expected_value) in (bool, int) and type(actual) is not type(
            expected_value
        ):
            raise SafetyError(f"smoke diagnostic evidence failed: {key}")
        if actual != expected_value:
            raise SafetyError(f"smoke diagnostic evidence failed: {key}")
    assert_public_receipt_safe(evidence)


def validate_recovery_evidence(
    path: Path, commit: str, manifest_sha256: str, challenge: str
) -> None:
    try:
        evidence = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise SafetyError("fresh recovery serial diagnostics are required") from error
    expected = {
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
    if not isinstance(evidence, dict) or set(evidence) != set(expected):
        raise SafetyError("RF-off recovery evidence schema is invalid")
    for key, expected_value in expected.items():
        actual = evidence[key]
        if type(expected_value) in (bool, int) and type(actual) is not type(
            expected_value
        ):
            raise SafetyError(f"RF-off recovery evidence failed gate: {key}")
        if actual != expected_value:
            raise SafetyError(f"RF-off recovery evidence failed gate: {key}")
    assert_public_receipt_safe(evidence)


class ExternalReleaseHooks:
    """Invoke fixed, repository-owned collectors through private JSON contracts.

    The collectors are intentionally fixed paths with no CLI override. Missing
    or replaced collector files block the release before any candidate flash.
    """

    def __init__(self, runner: CommandRunner, python: str):
        self.runner = runner
        self.python = python
        self._trusted_script(SMOKE_COLLECTOR)
        self._trusted_script(RECOVERY_COLLECTOR)

    @staticmethod
    def _trusted_script(path: Path) -> Path:
        if path.is_symlink() or not path.is_file() or path.suffix != ".py":
            raise SafetyError(f"required exact-device hook is unavailable: {path.name}")
        resolved = path.resolve(strict=True)
        if resolved.parent != HOOKS_DIRECTORY.resolve():
            raise SafetyError("exact-device hook escaped its fixed directory")
        return resolved

    def _invoke(
        self,
        script: Path,
        request_path: Path,
        output_path: Path,
        request: Mapping[str, Any],
        *,
        timeout: float,
    ) -> None:
        trusted = self._trusted_script(script)
        try:
            output_path.unlink(missing_ok=True)
        except OSError as error:
            raise SafetyError("collector output could not be cleared") from error
        atomic_write_json(request_path, request, 0o600)
        self.runner.run(
            [
                self.python,
                str(trusted),
                "--request",
                str(request_path),
                "--output",
                str(output_path),
            ],
            timeout=timeout,
        )
        if output_path.is_symlink() or not output_path.is_file():
            raise SafetyError("collector did not create a regular output file")
        os.chmod(output_path, 0o600)

    def collect_smoke(
        self,
        run_directory: Path,
        output_path: Path,
        commit: str,
        manifest_sha256: str,
    ) -> dict[str, Any]:
        challenge = uuid.uuid4().hex
        request_path = run_directory / "postflash-smoke-request.json"
        request = {
            "schema_version": 1,
            "operation": "postflash_smoke",
            "commit": commit,
            "manifest_sha256": manifest_sha256,
            "challenge": challenge,
            "target_by_id": str(TARGET_BY_ID),
            "output_path": str(output_path),
            "required_soak_seconds": 900,
            "expected_boot_advert_queued_markers": 1,
            "expected_public_chat_queued_markers": 0,
            "expected_structural_rf_policy": "one_boot_advert",
        }
        self._invoke(SMOKE_COLLECTOR, request_path, output_path, request, timeout=960.0)
        validate_smoke_evidence(output_path, commit, manifest_sha256, challenge)
        return {
            "path": str(output_path),
            "size": output_path.stat().st_size,
            "sha256": sha256_file(output_path),
            "local_only": True,
            "physical_rf_observer": "unavailable",
        }

    def collect_recovery(
        self,
        run_directory: Path,
        commit: str,
        manifest_sha256: str,
    ) -> dict[str, Any]:
        challenge = uuid.uuid4().hex
        request_path = run_directory / "recovery-rf-off-request.json"
        output_path = run_directory / "recovery-rf-off-evidence.json"
        request = {
            "schema_version": 1,
            "operation": "recovery_rf_off",
            "commit": commit,
            "manifest_sha256": manifest_sha256,
            "challenge": challenge,
            "target_by_id": str(TARGET_BY_ID),
            "output_path": str(output_path),
            "required_soak_seconds": 60,
            "expected_boot_advert_queued_markers": 0,
            "expected_public_chat_queued_markers": 0,
            "expected_structural_rf_policy": "blocked",
            "expected_role": "recovery",
        }
        self._invoke(
            RECOVERY_COLLECTOR, request_path, output_path, request, timeout=90.0
        )
        validate_recovery_evidence(output_path, commit, manifest_sha256, challenge)
        return {
            "path": str(output_path),
            "size": output_path.stat().st_size,
            "sha256": sha256_file(output_path),
            "local_only": True,
        }


class EspTool:
    def __init__(self, runner: CommandRunner, python: str):
        self.runner = runner
        self.python = python

    def _run(
        self,
        command: str,
        arguments: Sequence[str] = (),
        *,
        before: str = "no-reset",
        after: str = "no-reset",
        no_stub: bool = False,
        timeout: float = 180.0,
    ) -> str:
        args = [
            self.python,
            "-m",
            "esptool",
            "--chip",
            TARGET_CHIP,
            "--port",
            str(TARGET_BY_ID),
            "--baud",
            "460800",
            "--before",
            before,
            "--after",
            after,
            "--connect-attempts",
            "1",
        ]
        if no_stub:
            args.append("--no-stub")
        args.extend((command, *arguments))
        result = self.runner.run(args, timeout=timeout)
        return f"{result.stdout}\n{result.stderr}"

    def read_mac(self, *, before: str) -> str:
        return self._run("read-mac", before=before, no_stub=True, timeout=30)

    def flash_id(self) -> str:
        return self._run("flash-id", no_stub=True, timeout=30)

    def security_info(self) -> str:
        return self._run("get-security-info", no_stub=True, timeout=30)

    def backup(self, path: Path, size: int) -> None:
        self._run("read-flash", ("0", str(size), str(path)), timeout=600)

    def read(self, address: int, size: int, path: Path) -> None:
        self._run("read-flash", (hex(address), str(size), str(path)), timeout=600)

    def erase(self) -> None:
        self._run("erase-flash", timeout=180)

    def write(self, segments: Sequence[Segment]) -> None:
        arguments: list[str] = []
        for segment in segments:
            arguments.extend((hex(segment.address), str(segment.path)))
        self._run("write-flash", arguments, timeout=600)

    def verify(self, segments: Sequence[Segment]) -> None:
        arguments: list[str] = []
        for segment in segments:
            arguments.extend((hex(segment.address), str(segment.path)))
        self._run("verify-flash", arguments, timeout=600)

    def reset(self) -> None:
        self._run("flash-id", after="watchdog-reset", no_stub=True, timeout=30)


def _safe_private_directory(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True, mode=0o700)
    os.chmod(path, 0o700)
    _fsync_directory(path)
    _fsync_directory(path.parent)


def _create_backup(esptool: EspTool, run_directory: Path) -> dict[str, Any]:
    backup = run_directory / "preflash.bin"
    if backup.exists():
        raise SafetyError("private backup path already exists")
    fd = os.open(backup, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    os.close(fd)
    esptool.backup(backup, FLASH_SIZE)
    if not backup.is_file() or backup.stat().st_size != FLASH_SIZE:
        raise SafetyError("full pre-flash backup is incomplete")
    os.chmod(backup, 0o600)
    _private_regular_file(backup, "full pre-flash backup")
    _fsync_file(backup)
    # Persist both the backup entry and the private run-directory entry before
    # any erase can be admitted.
    _fsync_directory(run_directory)
    _fsync_directory(run_directory.parent)
    digest = sha256_file(backup)
    sidecar = run_directory / "preflash.sha256"
    atomic_write_text(sidecar, f"{digest}  preflash.bin\n", 0o600)
    return {
        "path": str(backup),
        "sha256_path": str(sidecar),
        "size": FLASH_SIZE,
        "sha256": digest,
        "local_only": True,
    }


def _new_restoration_receipt(archive: RawStateArchive) -> dict[str, Any]:
    return {
        "schema_version": 1,
        "operation": "raw_partition_state_restoration",
        "commit": archive.commit,
        "challenge": archive.challenge,
        "backup_sha256": archive.backup_sha256,
        "state_manifest_sha256": archive.manifest_sha256,
        "binding_sha256": archive.binding_sha256,
        "local_only": True,
        "attempts": [],
    }


def _restore_raw_state(
    esptool: EspTool,
    archive: RawStateArchive,
    phase: str,
    role: str,
    attempt: int,
    restoration_receipt: dict[str, Any],
    restoration_receipt_path: Path,
) -> dict[str, Any]:
    revalidate_raw_state_archive(archive)
    record: dict[str, Any] = {
        "phase": phase,
        "role": role,
        "attempt": attempt,
        "outcome": "running",
        "error": None,
        "bytes_verified": False,
        "reset_reconnected": False,
        "partitions": [],
    }
    restoration_receipt["attempts"].append(record)
    atomic_write_json(restoration_receipt_path, restoration_receipt, 0o600)
    try:
        segments = tuple(blob.segment() for blob in archive.blobs)
        esptool.write(segments)
        for blob in archive.blobs:
            revalidate_raw_state_archive(archive)
            fd, readback_name = tempfile.mkstemp(
                prefix=f".{blob.label}-readback.",
                dir=archive.manifest_path.parent,
            )
            os.fchmod(fd, 0o600)
            os.close(fd)
            readback_path = Path(readback_name)
            try:
                esptool.read(blob.address, blob.size, readback_path)
                os.chmod(readback_path, 0o600)
                _private_regular_file(readback_path, "raw partition readback")
                readback_sha256 = sha256_file(readback_path)
                if (
                    readback_path.stat().st_size != blob.size
                    or readback_sha256 != blob.sha256
                ):
                    raise SafetyError(
                        f"raw {blob.label} state readback SHA-256 mismatch"
                    )
                record["partitions"].append(
                    {
                        "label": blob.label,
                        "address": blob.address,
                        "size": blob.size,
                        "source_sha256": blob.sha256,
                        "readback_sha256": readback_sha256,
                    }
                )
            finally:
                readback_path.unlink(missing_ok=True)
        revalidate_raw_state_archive(archive)
        record["bytes_verified"] = True
        atomic_write_json(restoration_receipt_path, restoration_receipt, 0o600)
        return record
    except Exception as error:
        record["outcome"] = "fail"
        record["error"] = _failure_context(error)
        atomic_write_json(restoration_receipt_path, restoration_receipt, 0o600)
        raise


def _flash_once(
    guard: DeviceGuard,
    esptool: EspTool,
    manifest: FlashManifest,
    *,
    phase: str,
    attempt: int,
    state_archive: RawStateArchive | None = None,
    restoration_receipt: dict[str, Any] | None = None,
    restoration_receipt_path: Path | None = None,
) -> DeviceSnapshot:
    revalidate_manifest_bytes(manifest)
    guard.validate_efuse(esptool)
    esptool.erase()
    revalidate_manifest_bytes(manifest)
    guard.validate_efuse(esptool)
    esptool.write(manifest.segments)
    revalidate_manifest_bytes(manifest)
    guard.validate_efuse(esptool)
    esptool.verify(manifest.segments)
    restoration_record: dict[str, Any] | None = None
    if state_archive is not None:
        if restoration_receipt is None or restoration_receipt_path is None:
            raise SafetyError("raw state restoration receipt is unavailable")
        restoration_record = _restore_raw_state(
            esptool,
            state_archive,
            phase,
            manifest.role,
            attempt,
            restoration_receipt,
            restoration_receipt_path,
        )
    try:
        esptool.reset()
        snapshot = guard.wait_reconnect()
    except Exception as error:
        if restoration_record is not None:
            restoration_record["outcome"] = "fail"
            restoration_record["error"] = _failure_context(error)
            atomic_write_json(
                restoration_receipt_path,
                restoration_receipt,
                0o600,  # type: ignore[arg-type]
            )
        raise
    if restoration_record is not None:
        restoration_record["reset_reconnected"] = True
        restoration_record["outcome"] = "pass"
        atomic_write_json(
            restoration_receipt_path,
            restoration_receipt,
            0o600,  # type: ignore[arg-type]
        )
    return snapshot


def _flash_with_retries(
    private: dict[str, Any],
    guard: DeviceGuard,
    esptool: EspTool,
    manifest: FlashManifest,
    phase: str,
    *,
    max_attempts: int = 2,
    checkpoint_path: Path | None = None,
    state_archive: RawStateArchive | None = None,
    restoration_receipt: dict[str, Any] | None = None,
    restoration_receipt_path: Path | None = None,
) -> DeviceSnapshot | None:
    if max_attempts not in (1, 2):
        raise SafetyError("flash retry count is outside the release contract")
    for attempt in range(1, max_attempts + 1):
        record = {
            "phase": phase,
            "role": manifest.role,
            "attempt": attempt,
            "outcome": "running",
            "error": None,
        }
        private["attempts"].append(record)
        if checkpoint_path is not None:
            atomic_write_json(checkpoint_path, private, 0o600)
        try:
            snapshot = _flash_once(
                guard,
                esptool,
                manifest,
                phase=phase,
                attempt=attempt,
                state_archive=state_archive,
                restoration_receipt=restoration_receipt,
                restoration_receipt_path=restoration_receipt_path,
            )
            record["outcome"] = "pass"
            if checkpoint_path is not None:
                atomic_write_json(checkpoint_path, private, 0o600)
            return snapshot
        except SafetyError as error:
            record["outcome"] = "fail"
            record["error"] = redact_text(str(error))
            if checkpoint_path is not None:
                atomic_write_json(checkpoint_path, private, 0o600)
    return None


def _run_recovery(
    private: dict[str, Any],
    guard: DeviceGuard,
    esptool: EspTool,
    recovery: FlashManifest,
    *,
    state_archive: RawStateArchive | None = None,
    restoration_receipt: dict[str, Any] | None = None,
    restoration_receipt_path: Path | None = None,
) -> bool:
    private["recovery"]["used"] = True
    private["recovery"]["fallback_used"] = True
    attempt_errors: list[str] = []

    def record_failure(attempt: int, error: Exception) -> None:
        attempt_errors.append(f"attempt {attempt}: {_failure_context(error)}")
        private["recovery"]["attempt_errors"] = list(attempt_errors)
        private["recovery"]["error"] = "; ".join(attempt_errors)

    try:
        recovery_snapshot = _flash_once(
            guard,
            esptool,
            recovery,
            phase="recovery_fallback",
            attempt=1,
            state_archive=state_archive,
            restoration_receipt=restoration_receipt,
            restoration_receipt_path=restoration_receipt_path,
        )
        private["recovery"].update(
            {
                "fallback_ok": True,
                "fallback_state_restored": state_archive is not None,
                "fallback_target": recovery_snapshot.private_dict(),
            }
        )
        return True
    except Exception as error:
        record_failure(1, error)
    # RF-off recovery is the final safety posture. If restoring preserved state
    # itself failed, boot a clean recovery image rather than leaving the board
    # erased or stuck in the ROM loader. The immutable local archive remains
    # available for a later supervised retry and the release remains failed.
    if state_archive is not None:
        try:
            recovery_snapshot = _flash_once(
                guard,
                esptool,
                recovery,
                phase="recovery_fallback_rf_off",
                attempt=2,
            )
            private["recovery"].update(
                {
                    "fallback_ok": True,
                    "fallback_state_restored": False,
                    "fallback_target": recovery_snapshot.private_dict(),
                }
            )
            return True
        except Exception as error:
            record_failure(2, error)
    return False


@contextlib.contextmanager
def _recover_before_unlock(
    lock: Any, on_failure: Callable[[Exception], None]
) -> Iterable[None]:
    """Run transaction recovery while the exclusive hardware lock is held."""
    with lock:
        try:
            yield
        except Exception as error:
            on_failure(error)
            raise


def _public_receipt(private: Mapping[str, Any]) -> dict[str, Any]:
    return {
        "schema_version": 1,
        "product": PRODUCT,
        "board": BOARD,
        "run_id": private["run_id"],
        "commit": private["commit"],
        "generated_at": utc_now(),
        "outcome": private["outcome"],
        "release_eligible": private["release_eligible"],
        "artifacts": private["public_artifacts"],
        "gates": private["gates"],
        "recovery": {
            "used": private["recovery"]["used"],
            "ok": private["recovery"]["ok"],
        },
        "external_evidence": private["external_evidence"],
    }


def execute_release(
    manifest_path: Path,
    recovery_manifest_path: Path,
    state_directory: Path,
    public_receipt_path: Path,
    smoke_evidence_path: Path,
    esptool_python: str,
    *,
    observer_evidence_path: Path | None = None,
    observer_bundle_path: Path | None = None,
    observer_source_reference: Mapping[str, object] | None = None,
    observer_artifact_metadata_path: Path | None = None,
    observer_run_metadata_path: Path | None = None,
    runner: CommandRunner | None = None,
    lock_factory: Callable[[], Any] = HardwareLock,
    guard_factory: Callable[[CommandRunner], DeviceGuard] = DeviceGuard,
    esptool_factory: Callable[[CommandRunner, str], EspTool] = EspTool,
    hooks_factory: Callable[[CommandRunner, str], Any] = ExternalReleaseHooks,
) -> bool:
    candidate = load_manifest(manifest_path, "candidate")
    recovery = load_manifest(recovery_manifest_path, "recovery")
    if candidate.commit != recovery.commit:
        raise SafetyError("candidate and recovery manifests identify different commits")
    _, candidate_table = validate_manifest_partition_layout(candidate)
    _, recovery_table = validate_manifest_partition_layout(recovery)
    if recovery_table != candidate_table:
        raise SafetyError("candidate and recovery partition tables differ")
    observer_inputs = (
        observer_evidence_path,
        observer_bundle_path,
        observer_source_reference,
        observer_artifact_metadata_path,
        observer_run_metadata_path,
    )
    if any(value is not None for value in observer_inputs) and not all(
        value is not None for value in observer_inputs
    ):
        raise SafetyError("independent RF observer admission inputs are incomplete")
    observer_binding = {}
    if observer_evidence_path is not None:
        assert observer_bundle_path is not None
        assert observer_source_reference is not None
        assert observer_artifact_metadata_path is not None
        assert observer_run_metadata_path is not None
        observer_binding = validate_independent_rf_evidence(
            observer_evidence_path,
            observer_bundle_path,
            observer_source_reference,
            observer_artifact_metadata_path,
            observer_run_metadata_path,
            candidate,
            recovery,
        )

    active_runner = runner or CommandRunner()
    guard = guard_factory(active_runner)
    esptool = esptool_factory(active_runner, esptool_python)
    # This validates the fixed diagnostic hooks before acquiring the lock or looking
    # at a device, so an unintegrated workflow cannot flash and then fail late.
    hooks = hooks_factory(active_runner, esptool_python)

    run_id = (
        f"{dt.datetime.now(dt.timezone.utc):%Y%m%dT%H%M%SZ}-{uuid.uuid4().hex[:12]}"
    )
    run_directory = state_directory / run_id
    _safe_private_directory(run_directory)
    private_receipt_path = run_directory / "receipt.json"
    restoration_receipt_path = run_directory / "state-restoration.json"
    private: dict[str, Any] = {
        "schema_version": 1,
        "run_id": run_id,
        "created_at": utc_now(),
        "commit": candidate.commit,
        "outcome": "running",
        "release_eligible": False,
        "target": {},
        "efuse_mac": None,
        "security": {},
        "backup": {},
        "preflash_export": {},
        "state_restoration": {},
        "candidate_diagnostics": {},
        "external_evidence": observer_binding,
        "attempts": [],
        "recovery": {
            "used": False,
            "ok": False,
            "diagnostics_ok": False,
            "installed_final": False,
            "error": None,
            "evidence": {},
            "fallback_used": False,
            "fallback_ok": False,
        },
        "gates": {
            "manifest_valid": True,
            "exact_device_bound": False,
            "preflash_capture_complete": False,
            "preflash_state_exported": False,
            "flash_verified": False,
            "usb_reconnected": False,
            "smoke_passed": False,
            "candidate_rf_policy_bound": True,
            "one_boot_advert_verified": False,
            "public_silence_verified": False,
            "correlated_dm_ack_verified": False,
            "correlated_channel_ack_verified": False,
            "forced_retry_rf_off_verified": False,
            "recovery_rf_blocked": False,
            "recovery_ready": False,
            "secrets_redacted": False,
        },
        "public_artifacts": {
            "candidate": manifest_public_artifacts(candidate),
            "recovery": manifest_public_artifacts(recovery),
        },
        "candidate_manifest_sha256": candidate.sha256,
        "recovery_manifest_sha256": recovery.sha256,
        "error": None,
    }
    atomic_write_json(private_receipt_path, private, 0o600)

    state_archive: RawStateArchive | None = None
    restoration_receipt: dict[str, Any] | None = None
    destructive_state_started = False
    device_posture = "untouched"
    transaction_recovery_error: str | None = None

    def recover_failed_transaction(_: Exception) -> None:
        nonlocal device_posture, transaction_recovery_error
        if not destructive_state_started:
            return
        if device_posture == "recovery":
            private["gates"]["recovery_ready"] = True
            private["recovery"]["installed_final"] = True
            return
        try:
            recovered = _run_recovery(
                private,
                guard,
                esptool,
                recovery,
                state_archive=state_archive,
                restoration_receipt=restoration_receipt,
                restoration_receipt_path=restoration_receipt_path,
            )
        except Exception as recovery_error:
            # Recovery must never replace the transaction's original exception.
            transaction_recovery_error = _failure_context(recovery_error)
            private["recovery"]["error"] = transaction_recovery_error
            recovered = False
        if recovered:
            device_posture = "recovery"
            private["gates"]["recovery_ready"] = True
            private["recovery"]["installed_final"] = True
            return
        private["gates"]["recovery_ready"] = False
        stored_error = private["recovery"].get("error")
        transaction_recovery_error = (
            stored_error
            if isinstance(stored_error, str) and stored_error
            else "RF-off recovery did not complete"
        )

    try:
        with _recover_before_unlock(lock_factory(), recover_failed_transaction):
            snapshot = guard.inspect()
            private["target"] = snapshot.private_dict()
            private["efuse_mac"] = guard.validate_efuse(esptool)
            private["gates"]["exact_device_bound"] = True

            detected_size = parse_flash_size(esptool.flash_id())
            if detected_size != FLASH_SIZE:
                raise SafetyError(
                    "detected flash size is not the pinned 16 MiB T-Deck size"
                )
            private["security"] = validate_security_info(esptool.security_info())
            private["backup"] = _create_backup(esptool, run_directory)
            private["gates"]["preflash_capture_complete"] = True
            state_archive = _extract_raw_state_archive(
                run_directory, private["backup"], candidate
            )
            private["preflash_export"] = {
                "operation": "raw_partition_state_export",
                "path": str(state_archive.manifest_path),
                "size": state_archive.manifest_path.stat().st_size,
                "sha256": state_archive.manifest_sha256,
                "binding_sha256": state_archive.binding_sha256,
                "partitions": [blob.record() for blob in state_archive.blobs],
                "local_only": True,
            }
            restoration_receipt = _new_restoration_receipt(state_archive)
            atomic_write_json(restoration_receipt_path, restoration_receipt, 0o600)
            private["state_restoration"] = {
                "path": str(restoration_receipt_path),
                "local_only": True,
            }
            private["gates"]["preflash_state_exported"] = True
            atomic_write_json(private_receipt_path, private, 0o600)

            # Install and byte-verify the immutable RF-off fallback before the
            # only candidate boot. This guarantees a known recovery path before
            # candidate bytes can execute; target markers do not prove RF state.
            private["recovery"]["used"] = True
            destructive_state_started = True
            device_posture = "unknown"
            recovery_snapshot = _flash_with_retries(
                private,
                guard,
                esptool,
                recovery,
                "recovery_drill",
                max_attempts=1,
                checkpoint_path=private_receipt_path,
                state_archive=state_archive,
                restoration_receipt=restoration_receipt,
                restoration_receipt_path=restoration_receipt_path,
            )
            if recovery_snapshot is None:
                private["gates"]["recovery_ready"] = _run_recovery(
                    private,
                    guard,
                    esptool,
                    recovery,
                    state_archive=state_archive,
                    restoration_receipt=restoration_receipt,
                    restoration_receipt_path=restoration_receipt_path,
                )
                if private["gates"]["recovery_ready"]:
                    device_posture = "recovery"
                raise SafetyError(
                    "recovery drill flash failed; RF-off recovery was retried"
                )
            device_posture = "recovery"
            private["recovery"]["drill_target"] = recovery_snapshot.private_dict()
            # The manifest, flash readback, restored-state readback, reset, and
            # reconnect are sufficient to call this recovery installation
            # ready.  They do not prove RF silence.
            private["gates"]["recovery_ready"] = True
            try:
                private["recovery"]["evidence"] = hooks.collect_recovery(
                    run_directory,
                    recovery.commit,
                    recovery.sha256,
                )
            except SafetyError as recovery_error:
                private["recovery"]["error"] = redact_text(str(recovery_error))
                raise SafetyError(
                    "recovery serial diagnostics failed; device remains on recovery"
                ) from recovery_error
            private["recovery"]["diagnostics_ok"] = True

            device_posture = "unknown"
            final_candidate = _flash_with_retries(
                private,
                guard,
                esptool,
                candidate,
                "final_candidate",
                checkpoint_path=private_receipt_path,
                state_archive=state_archive,
                restoration_receipt=restoration_receipt,
                restoration_receipt_path=restoration_receipt_path,
            )
            if final_candidate is None:
                private["gates"]["recovery_ready"] = _run_recovery(
                    private,
                    guard,
                    esptool,
                    recovery,
                    state_archive=state_archive,
                    restoration_receipt=restoration_receipt,
                    restoration_receipt_path=restoration_receipt_path,
                )
                if private["gates"]["recovery_ready"]:
                    device_posture = "recovery"
                raise SafetyError(
                    "final candidate failed twice; RF-off recovery was attempted"
                )
            device_posture = "candidate"
            private["post_reset_target"] = final_candidate.private_dict()

            private["gates"]["flash_verified"] = True
            private["gates"]["usb_reconnected"] = True
            try:
                private["candidate_diagnostics"] = hooks.collect_smoke(
                    run_directory,
                    smoke_evidence_path,
                    candidate.commit,
                    candidate.sha256,
                )
            except SafetyError as smoke_error:
                private["gates"]["recovery_ready"] = _run_recovery(
                    private,
                    guard,
                    esptool,
                    recovery,
                    state_archive=state_archive,
                    restoration_receipt=restoration_receipt,
                    restoration_receipt_path=restoration_receipt_path,
                )
                if private["gates"]["recovery_ready"]:
                    device_posture = "recovery"
                raise SafetyError(
                    "post-flash smoke gate failed; RF-off recovery was attempted"
                ) from smoke_error
            revalidate_manifest_bytes(candidate)
            revalidate_manifest_bytes(recovery)
            private["gates"]["smoke_passed"] = True

            if observer_binding:
                # These gates are owned only by the independently observed,
                # exact-image-bound RF record. Target-local diagnostics above
                # remain diagnostic and cannot set them.
                for gate in (
                    "one_boot_advert_verified",
                    "public_silence_verified",
                    "correlated_dm_ack_verified",
                    "correlated_channel_ack_verified",
                    "forced_retry_rf_off_verified",
                    "recovery_rf_blocked",
                ):
                    private["gates"][gate] = True
                private["recovery"]["ok"] = True
                private["outcome"] = "pass"
            else:
                # Without independent evidence, restore RF-off recovery before
                # reporting failure. Target serial markers never satisfy RF.
                device_posture = "unknown"
                final_recovery = _flash_with_retries(
                    private,
                    guard,
                    esptool,
                    recovery,
                    "final_recovery_posture",
                    max_attempts=1,
                    checkpoint_path=private_receipt_path,
                    state_archive=state_archive,
                    restoration_receipt=restoration_receipt,
                    restoration_receipt_path=restoration_receipt_path,
                )
                if final_recovery is None:
                    private["gates"]["recovery_ready"] = _run_recovery(
                        private,
                        guard,
                        esptool,
                        recovery,
                        state_archive=state_archive,
                        restoration_receipt=restoration_receipt,
                        restoration_receipt_path=restoration_receipt_path,
                    )
                    if not private["gates"]["recovery_ready"]:
                        raise SafetyError(
                            "final RF-off recovery posture could not be restored"
                        )
                    device_posture = "recovery"
                else:
                    private["recovery"]["final_target"] = final_recovery.private_dict()
                    private["gates"]["recovery_ready"] = True
                    device_posture = "recovery"
                private["recovery"]["installed_final"] = True
                raise SafetyError(
                    "independent RF observer unavailable; target serial markers are "
                    "diagnostic only; device remains on RF-off recovery"
                )
    except Exception as error:
        private["outcome"] = "fail"
        original_failure = _failure_context(error)
        private["error"] = (
            f"{original_failure}; RF-off transaction recovery also failed: "
            f"{transaction_recovery_error}"
            if transaction_recovery_error
            else original_failure
        )
    finally:
        if restoration_receipt_path.is_file():
            os.chmod(restoration_receipt_path, 0o600)
            private["state_restoration"].update(
                {
                    "size": restoration_receipt_path.stat().st_size,
                    "sha256": sha256_file(restoration_receipt_path),
                }
            )
        public = _public_receipt(private)
        assert_public_receipt_safe(public)
        private["gates"]["secrets_redacted"] = True
        gates = private["gates"]
        recovery_status = private["recovery"]
        private["release_eligible"] = bool(
            observer_binding
            and private["outcome"] == "pass"
            and isinstance(gates, dict)
            and set(gates) == REQUIRED_RELEASE_GATES
            and all(gates[name] is True for name in REQUIRED_RELEASE_GATES)
            and recovery_status["used"] is True
            and recovery_status["ok"] is True
        )
        public = _public_receipt(private)
        assert_public_receipt_safe(public)
        atomic_write_json(private_receipt_path, private, 0o600)
        atomic_write_json(public_receipt_path, public, 0o644)
    return public_receipt_release_eligible(
        public,
        expected_source_evidence_sha256=observer_binding.get(
            "source_evidence_sha256"
        ),
        expected_evidence_bundle_sha256=observer_binding.get(
            "evidence_bundle_sha256"
        ),
    )


def make_manifest(image: Path, output: Path, commit: str, role: str) -> None:
    if not COMMIT_RE.fullmatch(commit):
        raise SafetyError("manifest commit must be a full lowercase Git SHA")
    if role not in {"candidate", "recovery"}:
        raise SafetyError("manifest role must be candidate or recovery")
    if image.parent.resolve() != output.parent.resolve():
        raise SafetyError("image and manifest must share one artifact directory")
    if image.resolve() == output.resolve():
        raise SafetyError("image and manifest output must be different files")
    if image.is_symlink() or not image.is_file() or image.stat().st_size <= 0:
        raise SafetyError("release image must be a non-empty regular file")
    value = {
        "schema_version": 1,
        "product": PRODUCT,
        "board": BOARD,
        "target_chip": TARGET_CHIP,
        "flash_size": FLASH_SIZE,
        "role": role,
        "commit": commit,
        **FLASH_POLICIES[role],
        "build_environment": BUILD_ENVIRONMENTS[role],
        "segments": [
            {
                "address": 0,
                "file": image.name,
                "size": image.stat().st_size,
                "sha256": sha256_file(image),
            }
        ],
    }
    atomic_write_json(output, value, 0o644)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    manifest = subparsers.add_parser("make-manifest")
    manifest.add_argument("--image", type=Path, required=True)
    manifest.add_argument("--output", type=Path, required=True)
    manifest.add_argument("--commit", required=True)
    manifest.add_argument("--role", choices=("candidate", "recovery"), required=True)

    release = subparsers.add_parser("release")
    release.add_argument("--manifest", type=Path, required=True)
    release.add_argument("--recovery-manifest", type=Path, required=True)
    release.add_argument("--state-directory", type=Path, required=True)
    release.add_argument("--public-receipt", type=Path, required=True)
    release.add_argument("--smoke-evidence", type=Path, required=True)
    release.add_argument("--observer-evidence", type=Path)
    release.add_argument("--observer-bundle", type=Path)
    release.add_argument("--observer-source-repository")
    release.add_argument("--observer-source-run-id", type=int)
    release.add_argument("--observer-source-artifact-id", type=int)
    release.add_argument("--observer-source-artifact-digest")
    release.add_argument("--observer-artifact-metadata", type=Path)
    release.add_argument("--observer-run-metadata", type=Path)
    release.add_argument("--esptool-python", default=sys.executable)

    check = subparsers.add_parser("check-public")
    check.add_argument("--receipt", type=Path, required=True)
    check.add_argument("--source-evidence-sha256")
    check.add_argument("--evidence-bundle-sha256")
    check.add_argument("--github-output", type=Path)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        if args.command == "make-manifest":
            make_manifest(args.image, args.output, args.commit, args.role)
            return 0
        if args.command == "release":
            source_values = (
                args.observer_source_repository,
                args.observer_source_run_id,
                args.observer_source_artifact_id,
                args.observer_source_artifact_digest,
            )
            if any(value is not None for value in source_values) and not all(
                value is not None for value in source_values
            ):
                raise SafetyError("observer source identity inputs are incomplete")
            observer_source_reference = (
                evidence_source_reference(
                    args.observer_source_repository,
                    args.observer_source_run_id,
                    args.observer_source_artifact_id,
                    args.observer_source_artifact_digest,
                    load_manifest(args.manifest, "candidate").commit,
                )
                if all(value is not None for value in source_values)
                else None
            )
            eligible = execute_release(
                args.manifest,
                args.recovery_manifest,
                args.state_directory,
                args.public_receipt,
                args.smoke_evidence,
                args.esptool_python,
                observer_evidence_path=args.observer_evidence,
                observer_bundle_path=args.observer_bundle,
                observer_source_reference=observer_source_reference,
                observer_artifact_metadata_path=args.observer_artifact_metadata,
                observer_run_metadata_path=args.observer_run_metadata,
            )
            return 0 if eligible else 2
        receipt = json.loads(args.receipt.read_text(encoding="utf-8"))
        if not isinstance(receipt, dict):
            raise SafetyError("public receipt must be an object")
        eligible = public_receipt_release_eligible(
            receipt,
            expected_source_evidence_sha256=args.source_evidence_sha256,
            expected_evidence_bundle_sha256=args.evidence_bundle_sha256,
        )
        if args.github_output:
            with args.github_output.open("a", encoding="utf-8", newline="\n") as handle:
                handle.write(f"release_eligible={'true' if eligible else 'false'}\n")
        return 0 if eligible else 2
    except (OSError, json.JSONDecodeError, SafetyError, ValueError) as error:
        print(f"ERROR: {redact_text(str(error))}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
