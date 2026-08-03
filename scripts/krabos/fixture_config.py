"""Load the runner-private exact-device configuration."""

from __future__ import annotations

from dataclasses import dataclass
from functools import lru_cache
import json
import os
from pathlib import Path
import re
import stat
from typing import Mapping


CONFIG_ENVIRONMENT = "KRABOS_FIXTURE_CONFIG"
CONFIG_MODE = 0o600
BY_ID_DIRECTORY = Path("/dev/serial/by-id")
MAC_RE = re.compile(r"(?i)^[0-9a-f]{2}(?::[0-9a-f]{2}){5}$")
PROPERTY_KEYS = frozenset(
    {
        "ID_BUS",
        "ID_VENDOR_ID",
        "ID_MODEL_ID",
        "ID_SERIAL_SHORT",
        "ID_USB_INTERFACE_NUM",
        "ID_USB_DRIVER",
        "ID_PATH",
    }
)
PUBLIC_PROPERTIES = {
    "ID_BUS": "usb",
    "ID_VENDOR_ID": "303a",
    "ID_MODEL_ID": "1001",
    "ID_USB_INTERFACE_NUM": "00",
    "ID_USB_DRIVER": "cdc_acm",
}
CONFIG_KEYS = frozenset(
    {
        "target_by_id",
        "expected_usb_serial",
        "expected_efuse_mac",
        "expected_properties",
        "forbidden_devices",
    }
)
FORBIDDEN_DEVICE_KEYS = frozenset({"by_id", "serial", "vid_pid"})


class FixtureConfigError(RuntimeError):
    """The runner-private fixture configuration is missing or unsafe."""


@dataclass(frozen=True)
class FixtureConfig:
    target_by_id: Path
    expected_usb_serial: str
    expected_efuse_mac: str
    expected_properties: Mapping[str, str]
    forbidden_by_id: tuple[Path, ...]
    forbidden_serials: frozenset[str]
    forbidden_vid_pids: frozenset[tuple[str, str]]

    @classmethod
    def from_mapping(cls, value: object) -> "FixtureConfig":
        if not isinstance(value, dict) or set(value) != CONFIG_KEYS:
            raise FixtureConfigError(
                "fixture config fields are incomplete or unexpected"
            )

        target_by_id = _by_id_path(value["target_by_id"], "target_by_id")
        expected_usb_serial = _mac(value["expected_usb_serial"], "expected_usb_serial")
        expected_efuse_mac = _mac(value["expected_efuse_mac"], "expected_efuse_mac")

        properties = value["expected_properties"]
        if not isinstance(properties, dict) or set(properties) != PROPERTY_KEYS:
            raise FixtureConfigError(
                "fixture config properties are incomplete or unexpected"
            )
        if any(not isinstance(item, str) or not item for item in properties.values()):
            raise FixtureConfigError(
                "fixture config properties must be non-empty strings"
            )
        expected_properties = dict(properties)
        for key, expected in PUBLIC_PROPERTIES.items():
            if expected_properties[key].lower() != expected:
                raise FixtureConfigError(
                    f"fixture config public property mismatch: {key}"
                )
        if expected_properties["ID_SERIAL_SHORT"].upper() != expected_usb_serial:
            raise FixtureConfigError(
                "fixture config serial property does not match target serial"
            )
        if expected_usb_serial not in target_by_id.name.upper():
            raise FixtureConfigError("target by-id path does not contain the target serial")

        forbidden = value["forbidden_devices"]
        if not isinstance(forbidden, list) or not forbidden:
            raise FixtureConfigError("fixture config must forbid at least one device")
        forbidden_by_id: list[Path] = []
        forbidden_serials: set[str] = set()
        forbidden_vid_pids: set[tuple[str, str]] = set()
        for item in forbidden:
            if not isinstance(item, dict) or set(item) != FORBIDDEN_DEVICE_KEYS:
                raise FixtureConfigError(
                    "forbidden fixture device fields are incomplete or unexpected"
                )
            forbidden_by_id.append(_by_id_path(item["by_id"], "forbidden by_id"))
            forbidden_serials.add(_mac(item["serial"], "forbidden serial"))
            vid_pid = item["vid_pid"]
            if (
                not isinstance(vid_pid, list)
                or len(vid_pid) != 2
                or any(not isinstance(part, str) or not part for part in vid_pid)
            ):
                raise FixtureConfigError("forbidden vid_pid must contain two strings")
            forbidden_vid_pids.add((vid_pid[0].lower(), vid_pid[1].lower()))

        if target_by_id in forbidden_by_id or expected_usb_serial in forbidden_serials:
            raise FixtureConfigError("target fixture is also listed as forbidden")
        return cls(
            target_by_id=target_by_id,
            expected_usb_serial=expected_usb_serial,
            expected_efuse_mac=expected_efuse_mac,
            expected_properties=expected_properties,
            forbidden_by_id=tuple(forbidden_by_id),
            forbidden_serials=frozenset(forbidden_serials),
            forbidden_vid_pids=frozenset(forbidden_vid_pids),
        )


def _by_id_path(value: object, field: str) -> Path:
    if not isinstance(value, str) or not value.startswith(str(BY_ID_DIRECTORY) + "/"):
        raise FixtureConfigError(f"{field} must be under {BY_ID_DIRECTORY}")
    path = Path(value)
    if path.parent != BY_ID_DIRECTORY or path.name in ("", ".", ".."):
        raise FixtureConfigError(f"{field} must name one by-id entry")
    return path


def _mac(value: object, field: str) -> str:
    if not isinstance(value, str) or not MAC_RE.fullmatch(value):
        raise FixtureConfigError(f"{field} must be a MAC-shaped value")
    return value.upper()


@lru_cache(maxsize=1)
def load_fixture_config() -> FixtureConfig:
    raw_path = os.environ.get(CONFIG_ENVIRONMENT, "").strip()
    if not raw_path:
        raise FixtureConfigError(
            f"{CONFIG_ENVIRONMENT} must point to the runner-private fixture config"
        )
    path = Path(raw_path)
    if not path.is_absolute() or path.is_symlink():
        raise FixtureConfigError("fixture config must be an absolute non-symlink path")
    try:
        metadata = path.stat()
        if (
            not stat.S_ISREG(metadata.st_mode)
            or stat.S_IMODE(metadata.st_mode) != CONFIG_MODE
        ):
            raise FixtureConfigError("fixture config must be a regular mode-0600 file")
        if metadata.st_uid != os.getuid():
            raise FixtureConfigError("fixture config must be owned by the runner user")
        value = json.loads(path.read_text(encoding="utf-8"))
    except FixtureConfigError:
        raise
    except (OSError, json.JSONDecodeError) as error:
        raise FixtureConfigError("fixture config could not be read as JSON") from error
    return FixtureConfig.from_mapping(value)
