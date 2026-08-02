#!/usr/bin/env python3
"""Stamp, seal, and verify the deterministic public KrabOS edge bundle."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import tempfile
from typing import Any, Mapping, Sequence


SCRIPT_DIRECTORY = Path(__file__).resolve().parent
if str(SCRIPT_DIRECTORY) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIRECTORY))
SCRIPTS_DIRECTORY = SCRIPT_DIRECTORY.parent
if str(SCRIPTS_DIRECTORY) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIRECTORY))

from exact_device_release import (  # noqa: E402 - fixed local companion module
    SafetyError as ReceiptError,
    load_manifest,
    manifest_public_artifacts,
    revalidate_manifest_bytes,
    validate_public_release_receipt,
)
from release_artifact_contract import (  # noqa: E402 - fixed repository module
    ReleaseArtifactError,
    validate_firmware_roles,
    validate_release_directory,
)


PRODUCT = "KrabOS"
BOARD = "lilygo-t-deck-plus"
BUILD_ENVIRONMENT = "KrabOS_TDeckPlus"
COMMIT_RE = re.compile(r"^[0-9a-f]{40}$")
EDGE_VERSION_RE = re.compile(
    r"^edge-[0-9]{4}-[0-9]{2}-[0-9]{2}-[0-9a-f]{12}$"
)
STABLE_VERSION = "v1.0.0"
HASH_RE = re.compile(r"^[0-9a-f]{64}$")

REQUIRED_ARTIFACTS = frozenset(
    {
        "firmware.bin",
        "firmware-merged.bin",
        "firmware.elf",
        "firmware-debug.bin",
        "KrabOS-tdeck-plus-launcher.bin",
        "krabos-candidate.bin",
        "krabos-recovery-rf-off.bin",
        "krabos-recovery-rf-off.elf",
        "manifest.json",
        "build-metadata.json",
        "krabos-tdeck-plus-bootloader.bin",
        "krabos-tdeck-plus-partitions.bin",
        "krabos-tdeck-plus-boot_app0.bin",
        "krabos-tdeck-plus-firmware.bin",
        "krabos-tdeck-plus-full.bin",
        "krabos-tdeck-plus-launcher.bin",
        "candidate-flash-manifest.json",
        "recovery-flash-manifest.json",
        "krabos-public-receipt.json",
        "firmware-sbom.cdx.json",
        "krabos-licenses.tar.gz",
    }
)
GENERATED = {"krabos-bundle-manifest.json", "SHA256SUMS.txt"}
POST_SEAL = {"SHA256SUMS.sigstore.json"}


class BundleError(ValueError):
    """The public release bundle does not satisfy its exact-byte contract."""


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _atomic_json(path: Path, value: Mapping[str, Any]) -> None:
    fd, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        os.fchmod(fd, 0o644)
        with os.fdopen(fd, "w", encoding="utf-8", newline="\n") as output:
            json.dump(value, output, indent=2, sort_keys=True)
            output.write("\n")
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, path)
        os.chmod(path, 0o644)
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


def _atomic_text(path: Path, value: str) -> None:
    fd, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        os.fchmod(fd, 0o644)
        with os.fdopen(fd, "w", encoding="utf-8", newline="\n") as output:
            output.write(value)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, path)
        os.chmod(path, 0o644)
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


def _load_object(path: Path) -> dict[str, Any]:
    if path.is_symlink() or not path.is_file():
        raise BundleError(f"{path.name} must be a regular non-symlink file")
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise BundleError(f"{path.name} is not valid JSON") from error
    if not isinstance(value, dict):
        raise BundleError(f"{path.name} must contain an object")
    return value


def _validate_inputs(directory: Path, commit: str, version: str) -> Path:
    if not COMMIT_RE.fullmatch(commit):
        raise BundleError("commit must be a full lowercase Git SHA")
    edge_version = EDGE_VERSION_RE.fullmatch(version)
    if edge_version:
        if not version.endswith(commit[:12]):
            raise BundleError("edge version is not bound to the exact short SHA")
    elif version != STABLE_VERSION:
        raise BundleError("version must be exact-SHA edge or v1.0.0")
    if directory.is_symlink() or not directory.is_dir():
        raise BundleError("artifact directory must be a regular directory")
    return directory.resolve(strict=True)


def stamp(directory: Path, commit: str, version: str) -> None:
    root = _validate_inputs(directory, commit, version)
    manifest_path = root / "manifest.json"
    metadata_path = root / "build-metadata.json"
    manifest = _load_object(manifest_path)
    metadata = _load_object(metadata_path)
    if manifest.get("name") != "KrabOS T-Deck Plus":
        raise BundleError("web-flasher manifest is not for KrabOS T-Deck Plus")
    if metadata.get("git_sha") != commit:
        raise BundleError("build metadata is not bound to the exact commit")
    if metadata.get("build_environment") != BUILD_ENVIRONMENT:
        raise BundleError("build metadata is not from the production environment")
    if metadata.get("git_dirty") is not False:
        raise BundleError("build metadata came from a dirty source tree")
    manifest["version"] = version
    metadata["version"] = version
    _atomic_json(manifest_path, manifest)
    _atomic_json(metadata_path, metadata)


def _regular_files(root: Path, excluded: set[str]) -> dict[str, Path]:
    files: dict[str, Path] = {}
    for path in root.iterdir():
        if path.name in excluded:
            continue
        if path.is_symlink() or not path.is_file():
            raise BundleError(f"unexpected non-regular artifact: {path.name}")
        files[path.name] = path
    return files


def _validate_release_evidence(
    root: Path, files: Mapping[str, Path], commit: str
) -> None:
    try:
        candidate = load_manifest(root / "candidate-flash-manifest.json", "candidate")
        recovery = load_manifest(root / "recovery-flash-manifest.json", "recovery")
        if candidate.commit != commit or recovery.commit != commit:
            raise ReceiptError("flash manifests are not bound to the release commit")

        for manifest in (candidate, recovery):
            for segment in manifest.segments:
                sealed_path = files.get(segment.path.name)
                if sealed_path is None or sealed_path.resolve(strict=True) != segment.path:
                    raise ReceiptError(
                        f"{manifest.role} manifest references an unsealed artifact"
                    )

        receipt = _load_object(root / "krabos-public-receipt.json")
        validate_public_release_receipt(
            receipt,
            expected_commit=commit,
            expected_artifacts={
                "candidate": manifest_public_artifacts(candidate),
                "recovery": manifest_public_artifacts(recovery),
            },
        )
        revalidate_manifest_bytes(candidate)
        revalidate_manifest_bytes(recovery)
    except ReceiptError as error:
        raise BundleError(f"release evidence is not eligible: {error}") from error


def _validate_artifact_contract(
    root: Path, commit: str, version: str
) -> dict[str, dict[str, str | int]]:
    try:
        validate_release_directory(
            root, expected_commit=commit, expected_version=version
        )
        return validate_firmware_roles(root)
    except ReleaseArtifactError as error:
        raise BundleError(f"release artifact contract failed: {error}") from error


def seal(directory: Path, commit: str, version: str) -> None:
    root = _validate_inputs(directory, commit, version)
    stamp(root, commit, version)
    files = _regular_files(root, GENERATED | POST_SEAL)
    missing = sorted(REQUIRED_ARTIFACTS - set(files))
    if missing:
        raise BundleError(f"release bundle is missing: {', '.join(missing)}")
    firmware_roles = _validate_artifact_contract(root, commit, version)
    _validate_release_evidence(root, files, commit)

    entries = {
        name: {"size": path.stat().st_size, "sha256": sha256_file(path)}
        for name, path in sorted(files.items())
    }
    bundle = {
        "schema_version": 1,
        "product": PRODUCT,
        "board": BOARD,
        "version": version,
        "commit": commit,
        "firmware_roles": firmware_roles,
        "artifacts": entries,
    }
    bundle_path = root / "krabos-bundle-manifest.json"
    _atomic_json(bundle_path, bundle)

    checksum_files = {**files, bundle_path.name: bundle_path}
    checksum_text = "".join(
        f"{sha256_file(path)}  {name}\n"
        for name, path in sorted(checksum_files.items())
    )
    _atomic_text(root / "SHA256SUMS.txt", checksum_text)


def verify(directory: Path, commit: str, version: str) -> None:
    root = _validate_inputs(directory, commit, version)
    bundle = _load_object(root / "krabos-bundle-manifest.json")
    if (
        bundle.get("schema_version") != 1
        or bundle.get("product") != PRODUCT
        or bundle.get("board") != BOARD
        or bundle.get("version") != version
        or bundle.get("commit") != commit
    ):
        raise BundleError("bundle manifest identity does not match the release")
    entries = bundle.get("artifacts")
    if not isinstance(entries, dict) or not entries:
        raise BundleError("bundle manifest has no artifacts")

    files = _regular_files(root, GENERATED | POST_SEAL)
    missing = sorted(REQUIRED_ARTIFACTS - set(files))
    if missing:
        raise BundleError(f"release bundle is missing: {', '.join(missing)}")
    if set(entries) != set(files):
        raise BundleError("bundle manifest file set does not match the directory")
    firmware_roles = _validate_artifact_contract(root, commit, version)
    if bundle.get("firmware_roles") != firmware_roles:
        raise BundleError("bundle firmware role records do not match exact image bytes")
    _validate_release_evidence(root, files, commit)
    for name, path in files.items():
        record = entries.get(name)
        if not isinstance(record, dict) or set(record) != {"size", "sha256"}:
            raise BundleError(f"invalid bundle record: {name}")
        if record["size"] != path.stat().st_size or record["sha256"] != sha256_file(path):
            raise BundleError(f"bundle digest mismatch: {name}")

    expected_paths = {**files, "krabos-bundle-manifest.json": root / "krabos-bundle-manifest.json"}
    checksum_path = root / "SHA256SUMS.txt"
    if checksum_path.is_symlink() or not checksum_path.is_file():
        raise BundleError("SHA256SUMS.txt is missing")
    expected_lines = [
        f"{sha256_file(path)}  {name}"
        for name, path in sorted(expected_paths.items())
    ]
    actual_lines = checksum_path.read_text(encoding="utf-8").splitlines()
    if actual_lines != expected_lines or any(
        not HASH_RE.fullmatch(line.split("  ", 1)[0]) for line in actual_lines
    ):
        raise BundleError("SHA256SUMS.txt is incomplete, unordered, or stale")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("stamp", "seal", "verify"))
    parser.add_argument("--artifacts", type=Path, required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--version", required=True)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        {"stamp": stamp, "seal": seal, "verify": verify}[args.operation](
            args.artifacts, args.commit, args.version
        )
    except (OSError, BundleError) as error:
        print(f"bundle release failed: {error}", file=os.sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
