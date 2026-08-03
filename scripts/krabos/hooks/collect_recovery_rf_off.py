#!/usr/bin/env python3
"""Collect target-local recovery RF-policy diagnostics for 60 seconds.

The target's RF-blocked marker is structural evidence only.  Without an
independent RF observer it does not prove that no packet was transmitted.
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import select
import stat
import subprocess
import sys
import tempfile
import time
from typing import Any, Mapping, Sequence

try:
    import termios
except ImportError:  # pragma: no cover - permits contract tests on Windows
    termios = None  # type: ignore[assignment]

KRABOS_DIRECTORY = Path(__file__).resolve().parents[1]
if str(KRABOS_DIRECTORY) not in sys.path:
    sys.path.insert(0, str(KRABOS_DIRECTORY))

from fixture_config import (  # noqa: E402 - fixed repository module
    FixtureConfigError,
    load_fixture_config,
)

fixture_config = load_fixture_config

COMMIT_RE = re.compile(r"^[0-9a-f]{40}$")
HASH_RE = re.compile(r"^[0-9a-f]{64}$")
CHALLENGE_RE = re.compile(r"^[0-9a-f]{32}$")
RF_BLOCKED_LINE = "@krabos|event=rf_policy|tx=blocked|role=recovery"
BOOT_ADVERT_PREFIX = "@krabos|event=boot_advert|"
PUBLIC_CHAT_PREFIX = "@krabos|event=public_chat|"
REQUEST_KEYS = frozenset(
    {
        "schema_version",
        "operation",
        "commit",
        "manifest_sha256",
        "challenge",
        "target_by_id",
        "output_path",
        "required_soak_seconds",
        "expected_boot_advert_queued_markers",
        "expected_public_chat_queued_markers",
        "expected_structural_rf_policy",
        "expected_role",
    }
)


class CollectorError(RuntimeError):
    """The exact-target recovery diagnostic contract was not observed."""


def _load_request(path: Path) -> dict[str, Any]:
    config = fixture_config()
    if path.is_symlink() or not path.is_file():
        raise CollectorError("request must be a regular non-symlink file")
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise CollectorError("request is not valid JSON") from error
    if not isinstance(value, dict) or set(value) != REQUEST_KEYS:
        raise CollectorError("request fields are incomplete or unexpected")
    expected = {
        "schema_version": 1,
        "operation": "recovery_rf_off",
        "target_by_id": str(config.target_by_id),
        "required_soak_seconds": 60,
        "expected_boot_advert_queued_markers": 0,
        "expected_public_chat_queued_markers": 0,
        "expected_structural_rf_policy": "blocked",
        "expected_role": "recovery",
    }
    for key, expected_value in expected.items():
        actual = value[key]
        if type(expected_value) is int and type(actual) is not int:
            raise CollectorError(f"request mismatch: {key}")
        if actual != expected_value:
            raise CollectorError(f"request mismatch: {key}")
    if not isinstance(value["commit"], str) or not COMMIT_RE.fullmatch(value["commit"]):
        raise CollectorError("request commit is invalid")
    if not isinstance(value["manifest_sha256"], str) or not HASH_RE.fullmatch(
        value["manifest_sha256"]
    ):
        raise CollectorError("request manifest digest is invalid")
    if not isinstance(value["challenge"], str) or not CHALLENGE_RE.fullmatch(
        value["challenge"]
    ):
        raise CollectorError("request challenge is invalid")
    output = value["output_path"]
    expected_output = path.with_name("recovery-rf-off-evidence.json").resolve()
    if not isinstance(output, str) or Path(output).resolve() != expected_output:
        raise CollectorError("request output path is invalid")
    return value


def _udev_properties() -> dict[str, str]:
    config = fixture_config()
    if not config.target_by_id.is_symlink():
        raise CollectorError("exact T-Deck by-id symlink is absent")
    resolved = config.target_by_id.resolve(strict=True)
    if not re.fullmatch(r"/dev/ttyACM\d+", str(resolved)):
        raise CollectorError("exact T-Deck did not resolve to USB ACM")
    if not stat.S_ISCHR(resolved.stat().st_mode):
        raise CollectorError("exact T-Deck target is not a character device")
    completed = subprocess.run(
        ["udevadm", "info", "--query=property", f"--name={config.target_by_id}"],
        check=False,
        capture_output=True,
        text=True,
        timeout=10,
    )
    if completed.returncode:
        raise CollectorError("udev identity query failed")
    properties: dict[str, str] = {}
    for line in completed.stdout.splitlines():
        if "=" in line:
            key, item = line.split("=", 1)
            properties[key] = item
    for key, expected in config.expected_properties.items():
        if properties.get(key) != expected:
            raise CollectorError(f"exact-device property mismatch: {key}")
    return properties


def observe_line(line: str, state: dict[str, Any], short_sha: str) -> None:
    stripped = line.strip()
    if stripped.startswith(BOOT_ADVERT_PREFIX):
        state["boot_advert_queued_markers"] += 1
    elif stripped.startswith(PUBLIC_CHAT_PREFIX):
        state["public_chat_queued_markers"] += 1
    elif stripped == RF_BLOCKED_LINE:
        state["rf_blocked_marker"] = True
    elif stripped == (
        "@krabos|event=boot|status=ready|env=KrabOS_TDeckPlus_recovery|sha="
        + short_sha
    ):
        state["boot_ready_marker"] = True


def _configure_serial(fd: int) -> None:
    if termios is None:
        raise CollectorError("serial collection is Linux-only")
    attributes = termios.tcgetattr(fd)
    attributes[0] = termios.IGNPAR
    attributes[1] = 0
    attributes[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    attributes[2] &= ~termios.HUPCL
    attributes[3] = 0
    attributes[4] = termios.B115200
    attributes[5] = termios.B115200
    attributes[6][termios.VMIN] = 0
    attributes[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, attributes)


def collect(request: Mapping[str, Any]) -> dict[str, Any]:
    config = fixture_config()
    _udev_properties()
    fd = os.open(
        config.target_by_id,
        os.O_RDONLY
        | getattr(os, "O_NOCTTY", 0)
        | getattr(os, "O_NONBLOCK", 0),
    )
    state: dict[str, Any] = {
        "boot_ready_marker": False,
        "rf_blocked_marker": False,
        "boot_advert_queued_markers": 0,
        "public_chat_queued_markers": 0,
    }
    buffer = b""
    required = int(request["required_soak_seconds"])
    deadline = time.monotonic() + required
    try:
        _configure_serial(fd)
        while time.monotonic() < deadline:
            timeout = max(0.0, min(1.0, deadline - time.monotonic()))
            ready, _, _ = select.select([fd], [], [], timeout)
            if not ready:
                continue
            chunk = os.read(fd, 4096)
            if not chunk:
                raise CollectorError("exact T-Deck serial stream disconnected")
            buffer += chunk
            if len(buffer) > 65536:
                raise CollectorError("exact T-Deck emitted an unterminated serial record")
            while b"\n" in buffer:
                raw, buffer = buffer.split(b"\n", 1)
                observe_line(
                    raw.decode("utf-8", errors="replace"),
                    state,
                    str(request["commit"])[:12],
                )
                if (
                    state["boot_advert_queued_markers"]
                    or state["public_chat_queued_markers"]
                ):
                    raise CollectorError("recovery emitted a queued transmit marker")
    finally:
        os.close(fd)

    if not state["boot_ready_marker"] or not state["rf_blocked_marker"]:
        raise CollectorError("exact recovery boot and RF-blocked markers were not observed")
    return {
        "schema_version": 1,
        "operation": "recovery_rf_off",
        "commit": request["commit"],
        "manifest_sha256": request["manifest_sha256"],
        "challenge": request["challenge"],
        "outcome": "diagnostic_pass",
        "boot_ready_marker": True,
        "usb_reconnected": True,
        "structural_rf_policy": "blocked",
        "rf_blocked_marker": True,
        "boot_advert_queued_markers": 0,
        "public_chat_queued_markers": 0,
        "physical_rf_observer": "unavailable",
        "physical_rf_blocked_verified": False,
        "soak_duration_seconds": required,
    }


def _atomic_json(path: Path, value: Mapping[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        os.fchmod(fd, 0o600)
        with os.fdopen(fd, "w", encoding="utf-8", newline="\n") as output:
            json.dump(value, output, indent=2, sort_keys=True)
            output.write("\n")
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, path)
        os.chmod(path, 0o600)
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


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--request", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        request = _load_request(args.request)
        if Path(str(request["output_path"])).resolve() != args.output.resolve():
            raise CollectorError("CLI output does not match the private request")
        _atomic_json(args.output, collect(request))
    except (
        OSError,
        CollectorError,
        FixtureConfigError,
        subprocess.SubprocessError,
    ) as error:
        print(f"recovery collector failed: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
