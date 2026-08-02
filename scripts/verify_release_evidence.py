#!/usr/bin/env python3
"""Validate human release evidence and bind it to the produced artifact bytes."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from datetime import date, datetime, timezone
from pathlib import Path
from urllib.parse import urlparse

from release_artifact_contract import (
    REQUIRED_RELEASE_ARTIFACTS,
    ReleaseArtifactError,
    validate_release_directory,
)


ROOT = Path(__file__).resolve().parents[1]
REQUIREMENTS = ROOT / "ci/release_evidence_requirements.json"
TEMPLATE = ROOT / ".github/PULL_REQUEST_TEMPLATE/release.md"
DOC = ROOT / "docs/RELEASE_EVIDENCE.md"
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
ARTIFACT_DIGEST_RE = re.compile(r"^sha256:[0-9a-f]{64}$")
COMMIT_RE = re.compile(r"^[0-9a-f]{40}$")
REPOSITORY_RE = re.compile(r"^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$")
SAFE_VERSION_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._+-]{0,79}$")
FIELD_NAME_RE = re.compile(r"^[a-z][a-z0-9_]{0,63}$")
EXACT_EVIDENCE_SCHEMA = 3
EXACT_EVIDENCE_KIND = "krabos-exact-release-evidence-input"
EXACT_ATTESTATION_KIND = "sigurdos-exact-release-attestation"
EXACT_EVIDENCE_KEYS = frozenset(
    {
        "schema_version",
        "kind",
        "candidate_commit",
        "tag",
        "generated_at",
        "production_image_sha256",
        "artifact_sha256s",
        "requirements",
    }
)
EXACT_ATTESTATION_KEYS = frozenset(
    {
        "schema_version",
        "kind",
        "commit",
        "tag",
        "generated_at",
        "production_image_sha256",
        "artifact_sha256s",
        "source_evidence_sha256",
        "source_artifact",
        "artifacts",
        "requirements",
    }
)
PRODUCTION_IMAGE = "firmware-merged.bin"
ARTIFACT_ROLE_FILES = {
    "production": PRODUCTION_IMAGE,
    "recovery": "krabos-recovery-rf-off.bin",
    "debug": "firmware-debug.bin",
    "ota": "firmware.bin",
}
EVIDENCE_CLASSES = frozenset({"automated", "manual-review", "independent-observer"})
TRUSTED_EVIDENCE_WORKFLOW = ".github/workflows/krabos-evidence.yml"
TRUSTED_EVIDENCE_BRANCH = "main"


def load_requirements(path: Path = REQUIREMENTS) -> list[dict]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict) or set(data) != {"schema_version", "requirements"}:
        raise ValueError("requirements inventory fields are invalid")
    if type(data.get("schema_version")) is not int or data["schema_version"] not in (1, 2):
        raise ValueError("unsupported requirements schema")
    requirements = data.get("requirements")
    if not isinstance(requirements, list) or not requirements:
        raise ValueError("release requirements are empty")
    normalized: list[dict] = []
    allowed = {
        "id",
        "category",
        "description",
        "hardware",
        "evidence_classes",
        "artifact_roles",
        "required_claims",
        "metric_bounds",
        "peer_required",
    }
    for raw in requirements:
        required = {"id", "category", "description", "hardware"}
        if (
            not isinstance(raw, dict)
            or not required <= set(raw)
            or not set(raw) <= allowed
        ):
            raise ValueError("release requirement fields are invalid")
        item = dict(raw)
        if (
            not isinstance(item["id"], str)
            or not re.fullmatch(r"[A-Z0-9]+(?:-[A-Z0-9]+)*", item["id"])
            or not isinstance(item["category"], str)
            or not re.fullmatch(r"[a-z0-9]+(?:-[a-z0-9]+)*", item["category"])
            or not isinstance(item["description"], str)
            or not item["description"].strip()
        ):
            raise ValueError("release requirement identity is invalid")
        hardware = item.get("hardware")
        if type(hardware) is not bool:
            raise ValueError("release requirement hardware field is invalid")
        classes = item.get(
            "evidence_classes", ["manual-review" if hardware else "automated"]
        )
        if (
            not isinstance(classes, list)
            or not classes
            or any(not isinstance(value, str) for value in classes)
            or len(classes) != len(set(classes))
            or any(value not in EVIDENCE_CLASSES for value in classes)
        ):
            raise ValueError("release requirement evidence classes are invalid")
        roles = item.get("artifact_roles", ["production"])
        if (
            not isinstance(roles, list)
            or not roles
            or any(not isinstance(value, str) for value in roles)
            or roles != sorted(roles)
            or len(roles) != len(set(roles))
            or any(value not in ARTIFACT_ROLE_FILES for value in roles)
        ):
            raise ValueError("release requirement artifact roles are invalid")
        claims = item.get("required_claims", {})
        if not isinstance(claims, dict) or any(
            not FIELD_NAME_RE.fullmatch(str(key)) or type(value) is not bool
            for key, value in claims.items()
        ):
            raise ValueError("release requirement claims are invalid")
        bounds = item.get("metric_bounds", {})
        if not isinstance(bounds, dict):
            raise ValueError("release requirement metric bounds are invalid")
        for name, limit in bounds.items():
            if not FIELD_NAME_RE.fullmatch(str(name)) or not isinstance(limit, dict):
                raise ValueError("release requirement metric bounds are invalid")
            if not set(limit) or not set(limit) <= {"min", "max"}:
                raise ValueError("release requirement metric bounds are invalid")
            if any(type(value) is not int for value in limit.values()):
                raise ValueError("release requirement metric bounds must be integers")
            if "min" in limit and "max" in limit and limit["min"] > limit["max"]:
                raise ValueError("release requirement metric bounds are inverted")
        peer_required = item.get("peer_required", hardware)
        if type(peer_required) is not bool:
            raise ValueError("release requirement peer_required is invalid")
        item.update(
            {
                "evidence_classes": list(classes),
                "artifact_roles": list(roles),
                "required_claims": dict(claims),
                "metric_bounds": {key: dict(value) for key, value in bounds.items()},
                "peer_required": peer_required,
            }
        )
        normalized.append(item)
    return normalized


def verify_contract() -> int:
    requirements = load_requirements()
    template = TEMPLATE.read_text(encoding="utf-8")
    documentation = DOC.read_text(encoding="utf-8")
    seen: set[str] = set()
    categories: set[str] = set()
    for item in requirements:
        requirement_id = item.get("id")
        if not isinstance(requirement_id, str) or not requirement_id or requirement_id in seen:
            raise ValueError("requirement IDs must be non-empty and unique")
        seen.add(requirement_id)
        category = item.get("category")
        if not isinstance(category, str) or not category:
            raise ValueError(f"{requirement_id}: missing category")
        categories.add(category)
        if not isinstance(item.get("description"), str) or not isinstance(
            item.get("hardware"), bool
        ):
            raise ValueError(f"{requirement_id}: invalid description/hardware fields")
        token = f"`{requirement_id}`"
        if template.count(token) != 1:
            raise ValueError(f"release template must contain {token} exactly once")
    for category in categories:
        if category not in documentation.lower():
            raise ValueError(f"release documentation does not cover category {category}")
    return len(requirements)


def _https_url(value: object, field: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{field} must be a non-empty HTTPS URL")
    parsed = urlparse(value)
    if parsed.scheme != "https" or not parsed.netloc:
        raise ValueError(f"{field} must be a non-empty HTTPS URL")
    return value


def _immutable_https_url(value: object, field: str) -> str:
    result = _https_url(value, field)
    parsed = urlparse(result)
    if (
        any(character.isspace() for character in result)
        or parsed.username
        or parsed.password
        or parsed.query
        or parsed.fragment
    ):
        raise ValueError(
            f"{field} must not contain credentials, a query, or a fragment"
        )
    if not parsed.path or parsed.path == "/" or len(result) > 2048:
        raise ValueError(f"{field} must identify an immutable bundle path")
    return result


def _iso_date(value: object, field: str) -> date:
    if not isinstance(value, str):
        raise ValueError(f"{field} must use YYYY-MM-DD format")
    try:
        parsed = date.fromisoformat(value)
    except ValueError as exc:
        raise ValueError(f"{field} must use YYYY-MM-DD format") from exc
    if parsed.isoformat() != value:
        raise ValueError(f"{field} must use YYYY-MM-DD format")
    return parsed


def _utc_datetime(value: object, field: str) -> datetime:
    if not isinstance(value, str):
        raise ValueError(f"{field} must be an ISO-8601 UTC timestamp")
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError as exc:
        raise ValueError(f"{field} must be an ISO-8601 UTC timestamp") from exc
    if parsed.tzinfo is None or parsed.utcoffset() != timezone.utc.utcoffset(parsed):
        raise ValueError(f"{field} must include the UTC timezone")
    return parsed


def _utc_timestamp(value: object, field: str) -> str:
    _utc_datetime(value, field)
    return value


def _artifact_role_sha256s(value: object, field: str) -> dict[str, str]:
    if not isinstance(value, dict) or set(value) != set(ARTIFACT_ROLE_FILES):
        raise ValueError(f"{field} must contain the exact artifact role set")
    if any(
        not isinstance(digest, str) or not SHA256_RE.fullmatch(digest)
        for digest in value.values()
    ):
        raise ValueError(f"{field} values must be lowercase SHA-256 digests")
    return dict(value)


def _verify_exact_record(
    record: dict,
    requirement: dict,
    expected_tag: str,
    expected_commit: str,
    production_image_sha256: str,
    artifact_sha256s: dict[str, str],
) -> None:
    required_keys = {
        "id",
        "outcome",
        "evidence_class",
        "evidence_bundle_url",
        "evidence_bundle_sha256",
        "tested_at",
        "firmware_version",
        "candidate_commit",
        "production_image_sha256",
        "artifacts",
        "claims",
        "metrics",
    }
    if requirement["peer_required"]:
        required_keys.add("peer_version")
    if set(record) != required_keys:
        raise ValueError(f"{requirement['id']}: evidence fields are incomplete or unexpected")
    if record.get("outcome") != "pass":
        raise ValueError(f"{requirement['id']}: outcome must be pass")
    if record.get("evidence_class") not in requirement["evidence_classes"]:
        raise ValueError(f"{requirement['id']}: evidence_class is not allowed")
    _immutable_https_url(
        record.get("evidence_bundle_url"),
        f"{requirement['id']}.evidence_bundle_url",
    )
    if not isinstance(record.get("evidence_bundle_sha256"), str) or not SHA256_RE.fullmatch(
        record["evidence_bundle_sha256"]
    ):
        raise ValueError(f"{requirement['id']}: evidence_bundle_sha256 is invalid")
    if record.get("firmware_version") != expected_tag:
        raise ValueError(f"{requirement['id']}: firmware_version must match the release tag")
    if record.get("candidate_commit") != expected_commit:
        raise ValueError(f"{requirement['id']}: candidate_commit must match the exact candidate")
    if record.get("production_image_sha256") != production_image_sha256:
        raise ValueError(
            f"{requirement['id']}: production_image_sha256 must match the tested image"
        )
    artifacts = record.get("artifacts")
    expected_roles = requirement["artifact_roles"]
    if not isinstance(artifacts, list) or len(artifacts) != len(expected_roles):
        raise ValueError(f"{requirement['id']}: artifact role evidence is incomplete")
    for index, role in enumerate(expected_roles):
        artifact = artifacts[index]
        if not isinstance(artifact, dict) or set(artifact) != {"role", "sha256"}:
            raise ValueError(f"{requirement['id']}: artifact role evidence is invalid")
        if artifact.get("role") != role or artifact.get("sha256") != artifact_sha256s[role]:
            raise ValueError(f"{requirement['id']}: artifact role digest is not exact")
    claims = record.get("claims")
    if (
        not isinstance(claims, dict)
        or any(type(value) is not bool for value in claims.values())
        or claims != requirement["required_claims"]
    ):
        raise ValueError(f"{requirement['id']}: claims are incomplete or unexpected")
    metrics = record.get("metrics")
    bounds = requirement["metric_bounds"]
    if not isinstance(metrics, dict) or set(metrics) != set(bounds):
        raise ValueError(f"{requirement['id']}: metrics are incomplete or unexpected")
    for name, limits in bounds.items():
        measured = metrics[name]
        if type(measured) is not int:
            raise ValueError(f"{requirement['id']}: metric {name} must be an integer")
        if "min" in limits and measured < limits["min"]:
            raise ValueError(f"{requirement['id']}: metric {name} is below its minimum")
        if "max" in limits and measured > limits["max"]:
            raise ValueError(f"{requirement['id']}: metric {name} exceeds its maximum")
    if requirement["peer_required"]:
        peer_version = record.get("peer_version")
        if not isinstance(peer_version, str) or not SAFE_VERSION_RE.fullmatch(peer_version):
            raise ValueError(f"{requirement['id']}: peer_version is invalid")


def _verify_requirements(
    document: dict,
    expected_tag: str,
    max_age_days: int,
    *,
    expected_commit: str | None = None,
    production_image_sha256: str | None = None,
    artifact_sha256s: dict[str, str] | None = None,
) -> int:
    generated_at = _utc_timestamp(document.get("generated_at"), "generated_at")
    del generated_at
    records = document.get("requirements")
    if not isinstance(records, list):
        raise ValueError("requirements evidence must be a list")
    by_id: dict[str, dict] = {}
    for record in records:
        if not isinstance(record, dict):
            raise ValueError("each evidence record must be an object")
        requirement_id = record.get("id")
        if not isinstance(requirement_id, str) or not requirement_id or requirement_id in by_id:
            raise ValueError("evidence requirement IDs must be non-empty and unique")
        by_id[requirement_id] = record

    requirements = load_requirements()
    required_by_id = {item["id"]: item for item in requirements}
    missing = required_by_id.keys() - by_id.keys()
    extra = by_id.keys() - required_by_id.keys()
    if missing:
        raise ValueError(f"missing requirement evidence: {', '.join(sorted(missing))}")
    if extra:
        raise ValueError(f"unknown requirement evidence: {', '.join(sorted(extra))}")

    today = datetime.now(timezone.utc).date()
    for requirement_id, requirement in required_by_id.items():
        record = by_id[requirement_id]
        if expected_commit is not None:
            if production_image_sha256 is None or artifact_sha256s is None:
                raise ValueError("exact evidence validation needs artifact role digests")
            _verify_exact_record(
                record,
                requirement,
                expected_tag,
                expected_commit,
                production_image_sha256,
                artifact_sha256s,
            )
        else:
            if record.get("outcome") != "pass":
                raise ValueError(f"{requirement_id}: outcome must be pass")
            _https_url(record.get("evidence_url"), f"{requirement_id}.evidence_url")
            if record.get("firmware_version") != expected_tag:
                raise ValueError(f"{requirement_id}: firmware_version must match the release tag")
            if requirement["peer_required"]:
                peer_version = record.get("peer_version")
                if not isinstance(peer_version, str) or not peer_version.strip():
                    raise ValueError(f"{requirement_id}: hardware evidence needs a peer_version")
        tested_at = _iso_date(record.get("tested_at"), f"{requirement_id}.tested_at")
        age = (today - tested_at).days
        if age < 0 or age > max_age_days:
            raise ValueError(
                f"{requirement_id}: evidence is not within the allowed {max_age_days}-day window"
            )
    return len(records)


def _declared_hashes(document: dict) -> dict[str, str] | None:
    artifacts = document.get("artifacts")
    if artifacts is None:
        return None
    if not isinstance(artifacts, dict):
        raise ValueError("artifacts must map release filenames to SHA-256 digests")
    for filename, digest in artifacts.items():
        if (
            not isinstance(filename, str)
            or not isinstance(digest, str)
            or not SHA256_RE.fullmatch(digest)
        ):
            raise ValueError(f"{filename!r}: artifact digest must be lowercase SHA-256")
    return artifacts


def _load_object(path: Path, field: str) -> dict:
    if path.is_symlink() or not path.is_file():
        raise ValueError(f"{field} must be a regular non-symlink file")
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError(f"{field} must contain a JSON object")
    return value


def _positive_id(value: object, field: str) -> int:
    if isinstance(value, bool):
        raise ValueError(f"{field} must be a positive integer")
    if isinstance(value, int):
        parsed = value
    elif isinstance(value, str) and re.fullmatch(r"[1-9][0-9]*", value):
        parsed = int(value)
    else:
        raise ValueError(f"{field} must be a positive integer")
    if parsed <= 0:
        raise ValueError(f"{field} must be a positive integer")
    return parsed


def actions_artifact_reference(
    repository: str,
    run_id: object,
    artifact_id: object,
    artifact_digest: str,
    artifact_name: str,
) -> dict[str, object]:
    if not REPOSITORY_RE.fullmatch(repository):
        raise ValueError("source repository must use owner/name format")
    if not ARTIFACT_DIGEST_RE.fullmatch(artifact_digest):
        raise ValueError("source artifact digest must use sha256:<lowercase-hex>")
    if (
        not isinstance(artifact_name, str)
        or not artifact_name
        or len(artifact_name) > 255
        or "/" in artifact_name
        or "\\" in artifact_name
    ):
        raise ValueError("source artifact name is invalid")
    return {
        "repository": repository,
        "run_id": _positive_id(run_id, "source run ID"),
        "artifact_id": _positive_id(artifact_id, "source artifact ID"),
        "artifact_name": artifact_name,
        "artifact_digest": artifact_digest,
    }


def evidence_source_reference(
    repository: str,
    run_id: object,
    artifact_id: object,
    artifact_digest: str,
    commit: str,
) -> dict[str, object]:
    if not COMMIT_RE.fullmatch(commit):
        raise ValueError("source candidate commit must be a full lowercase SHA")
    reference = actions_artifact_reference(
        repository,
        run_id,
        artifact_id,
        artifact_digest,
        f"krabos-v1-evidence-{commit}",
    )
    reference.update(
        {
            "workflow_path": TRUSTED_EVIDENCE_WORKFLOW,
            "head_branch": TRUSTED_EVIDENCE_BRANCH,
            "event": "workflow_dispatch",
        }
    )
    return reference


def verify_github_artifact_metadata(
    artifact_metadata_path: Path,
    run_metadata_path: Path,
    reference: dict[str, object],
    expected_commit: str,
    *,
    trusted_workflow: str,
    expected_branch: str,
    max_age_days: int = 30,
) -> None:
    if not COMMIT_RE.fullmatch(expected_commit):
        raise ValueError("source candidate commit must be a full lowercase SHA")
    if max_age_days < 0:
        raise ValueError("source artifact freshness window cannot be negative")
    artifact = _load_object(artifact_metadata_path, "source artifact metadata")
    run = _load_object(run_metadata_path, "source workflow-run metadata")
    expected_run_id = reference["run_id"]
    expected_artifact_id = reference["artifact_id"]
    expected_repository = reference["repository"]

    if artifact.get("id") != expected_artifact_id:
        raise ValueError("source artifact metadata has a different artifact ID")
    if artifact.get("name") != reference["artifact_name"]:
        raise ValueError("source artifact name is not bound to the candidate commit")
    if artifact.get("digest") != reference["artifact_digest"]:
        raise ValueError("source artifact digest does not match the admitted digest")
    if artifact.get("expired") is not False:
        raise ValueError("source evidence artifact is expired")
    created_at = _utc_datetime(artifact.get("created_at"), "source artifact created_at")
    updated_at = _utc_datetime(artifact.get("updated_at"), "source artifact updated_at")
    expires_at = _utc_datetime(artifact.get("expires_at"), "source artifact expires_at")
    now = datetime.now(timezone.utc)
    age_seconds = (now - created_at).total_seconds()
    if age_seconds < -300 or age_seconds > max_age_days * 86400:
        raise ValueError("source evidence artifact is outside the allowed freshness window")
    if updated_at < created_at or expires_at <= now:
        raise ValueError("source evidence artifact timestamps are inconsistent or expired")
    artifact_run = artifact.get("workflow_run")
    if not isinstance(artifact_run, dict):
        raise ValueError("source artifact metadata has no workflow-run identity")
    if (
        artifact_run.get("id") != expected_run_id
        or artifact_run.get("head_sha") != expected_commit
        or artifact_run.get("head_branch") != expected_branch
    ):
        raise ValueError("source artifact is not from the admitted candidate run")

    repository = run.get("repository")
    if not isinstance(repository, dict) or repository.get("full_name") != expected_repository:
        raise ValueError("source workflow run belongs to a different repository")
    if run.get("id") != expected_run_id or run.get("head_sha") != expected_commit:
        raise ValueError("source workflow run is not bound to the exact candidate")
    head_repository = run.get("head_repository")
    if (
        not isinstance(head_repository, dict)
        or head_repository.get("full_name") != expected_repository
    ):
        raise ValueError("source workflow run is not internal to the trusted repository")
    if run.get("head_branch") != expected_branch:
        raise ValueError("source evidence workflow run is not from the trusted branch")
    if run.get("path") != trusted_workflow:
        raise ValueError("source evidence came from an untrusted workflow")
    if run.get("event") != "workflow_dispatch":
        raise ValueError("source evidence must come from a manually dispatched run")
    if run.get("pull_requests") != []:
        raise ValueError("source evidence workflow run must not be associated with a pull request")
    if run.get("status") != "completed" or run.get("conclusion") != "success":
        raise ValueError("source evidence workflow run did not complete successfully")
    run_created_at = _utc_datetime(run.get("created_at"), "source run created_at")
    run_updated_at = _utc_datetime(run.get("updated_at"), "source run updated_at")
    if not (run_created_at <= created_at <= run_updated_at):
        raise ValueError("source artifact creation is outside the source workflow run")


def verify_github_source_metadata(
    artifact_metadata_path: Path,
    run_metadata_path: Path,
    reference: dict[str, object],
    expected_commit: str,
    max_age_days: int = 30,
) -> None:
    if (
        reference.get("workflow_path") != TRUSTED_EVIDENCE_WORKFLOW
        or reference.get("head_branch") != TRUSTED_EVIDENCE_BRANCH
        or reference.get("event") != "workflow_dispatch"
    ):
        raise ValueError("source evidence reference does not name the trusted workflow")
    verify_github_artifact_metadata(
        artifact_metadata_path,
        run_metadata_path,
        reference,
        expected_commit,
        trusted_workflow=TRUSTED_EVIDENCE_WORKFLOW,
        expected_branch=TRUSTED_EVIDENCE_BRANCH,
        max_age_days=max_age_days,
    )


def _production_hashes(
    artifacts_dir: Path, expected_commit: str, expected_tag: str
) -> tuple[dict[str, str], str]:
    hashes = validate_release_directory(
        artifacts_dir,
        expected_commit=expected_commit,
        expected_version=expected_tag,
    )
    production_digest = hashes.get(PRODUCTION_IMAGE)
    if not SHA256_RE.fullmatch(str(production_digest)):
        raise ValueError("artifact contract did not return the production image SHA-256")
    return hashes, str(production_digest)


def _role_hashes_from_artifacts(hashes: dict[str, str]) -> dict[str, str]:
    roles: dict[str, str] = {}
    for role, filename in ARTIFACT_ROLE_FILES.items():
        digest = hashes.get(filename)
        if not isinstance(digest, str) or not SHA256_RE.fullmatch(digest):
            raise ValueError(f"artifact contract did not return the {role} image SHA-256")
        roles[role] = digest
    return roles


def _validate_expected_role_hashes(
    declared: dict[str, str], expected: dict[str, str] | None
) -> None:
    if expected is None:
        return
    if not isinstance(expected, dict) or not expected or any(
        role not in ARTIFACT_ROLE_FILES
        or not isinstance(digest, str)
        or not SHA256_RE.fullmatch(digest)
        for role, digest in expected.items()
    ):
        raise ValueError("expected artifact role digests are invalid")
    for role, digest in expected.items():
        if declared[role] != digest:
            raise ValueError(f"evidence {role} image digest does not match admitted bytes")


def verify_evidence(
    evidence_path: Path,
    expected_tag: str,
    *,
    expected_commit: str | None = None,
    artifacts_dir: Path | None = None,
    expected_production_image_sha256: str | None = None,
    expected_artifact_sha256s: dict[str, str] | None = None,
    max_age_days: int = 30,
    require_exact: bool = False,
) -> tuple[int, dict]:
    if not expected_tag.strip():
        raise ValueError("expected tag must be non-empty")
    if expected_commit is not None and not COMMIT_RE.fullmatch(expected_commit):
        raise ValueError("expected commit must be a full lowercase 40-character SHA")
    if (
        expected_production_image_sha256 is not None
        and not SHA256_RE.fullmatch(expected_production_image_sha256)
    ):
        raise ValueError("expected production image digest must be lowercase SHA-256")
    if max_age_days < 0:
        raise ValueError("maximum evidence age cannot be negative")

    evidence = _load_object(evidence_path, "release evidence")
    schema = evidence.get("schema_version")
    if require_exact and schema != EXACT_EVIDENCE_SCHEMA:
        raise ValueError("stable v1.0.0 requires schema 3 exact evidence")
    if type(schema) is not int or schema not in (1, EXACT_EVIDENCE_SCHEMA):
        if schema == 2:
            raise ValueError(
                "schema 2 version-only evidence cannot prove an exact tested candidate"
            )
        raise ValueError("unsupported evidence schema")
    if evidence.get("tag") != expected_tag:
        raise ValueError("evidence tag does not match the release tag")
    if schema == 1:
        commit = evidence.get("commit")
        if not COMMIT_RE.fullmatch(str(commit)):
            raise ValueError("legacy evidence commit must be a full lowercase SHA")
        if expected_commit is not None and commit != expected_commit:
            raise ValueError("evidence commit does not match the tagged commit")
        count = _verify_requirements(evidence, expected_tag, max_age_days)
        declared = _declared_hashes(evidence)
        if declared is None:
            raise ValueError("legacy evidence must declare artifact hashes")
        missing = REQUIRED_RELEASE_ARTIFACTS - declared.keys()
        if missing:
            raise ValueError(f"missing artifact hashes: {', '.join(sorted(missing))}")
        if artifacts_dir is not None:
            actual = validate_release_directory(
                artifacts_dir,
                expected_commit=expected_commit,
                expected_version=expected_tag,
            )
            for filename, digest in actual.items():
                if declared.get(filename) != digest:
                    raise ValueError(f"artifact hash does not match produced bytes: {filename}")
        return count, evidence

    if evidence.get("kind") != EXACT_EVIDENCE_KIND:
        raise ValueError("exact evidence kind is invalid")
    if expected_commit is None or (
        artifacts_dir is None and expected_production_image_sha256 is None
    ):
        raise ValueError(
            "exact evidence requires --commit and exact production image bytes or SHA-256"
        )
    if set(evidence) != EXACT_EVIDENCE_KEYS:
        raise ValueError("exact evidence has unknown or missing top-level fields")
    declared_roles = _artifact_role_sha256s(
        evidence.get("artifact_sha256s"), "artifact_sha256s"
    )
    if artifacts_dir is not None:
        hashes, production_digest = _production_hashes(
            artifacts_dir, expected_commit, expected_tag
        )
        actual_roles = _role_hashes_from_artifacts(hashes)
        if declared_roles != actual_roles:
            raise ValueError("evidence artifact role digests do not match produced bytes")
        if (
            expected_production_image_sha256 is not None
            and production_digest != expected_production_image_sha256
        ):
            raise ValueError("expected production image digest does not match produced bytes")
    else:
        production_digest = expected_production_image_sha256
    _validate_expected_role_hashes(declared_roles, expected_artifact_sha256s)
    if evidence.get("candidate_commit") != expected_commit:
        raise ValueError("evidence candidate_commit does not match the exact candidate")
    if evidence.get("production_image_sha256") != production_digest:
        raise ValueError("evidence production image digest does not match produced bytes")
    count = _verify_requirements(
        evidence,
        expected_tag,
        max_age_days,
        expected_commit=expected_commit,
        production_image_sha256=production_digest,
        artifact_sha256s=declared_roles,
    )
    return count, evidence


def write_attestation(
    output_path: Path,
    evidence_path: Path,
    evidence: dict,
    commit: str,
    tag: str,
    artifacts_dir: Path,
    source_reference: dict[str, object] | None = None,
) -> dict:
    if _load_object(evidence_path, "release evidence") != evidence:
        raise ValueError("release evidence bytes changed before attestation")
    hashes, production_digest = _production_hashes(artifacts_dir, commit, tag)
    artifact_sha256s = _role_hashes_from_artifacts(hashes)
    if evidence.get("schema_version") == EXACT_EVIDENCE_SCHEMA:
        if set(evidence) != EXACT_EVIDENCE_KEYS:
            raise ValueError("exact evidence has unknown or missing top-level fields")
        if evidence.get("kind") != EXACT_EVIDENCE_KIND:
            raise ValueError("exact evidence kind is invalid")
        if source_reference is None:
            raise ValueError(
                "exact evidence attestation requires immutable source artifact identity"
            )
        if (
            evidence.get("candidate_commit") != commit
            or evidence.get("production_image_sha256") != production_digest
            or evidence.get("artifact_sha256s") != artifact_sha256s
        ):
            raise ValueError("exact evidence identity changed before attestation")
        _verify_requirements(
            evidence,
            tag,
            30,
            expected_commit=commit,
            production_image_sha256=production_digest,
            artifact_sha256s=artifact_sha256s,
        )
        normalized_source = evidence_source_reference(
            str(source_reference.get("repository", "")),
            source_reference.get("run_id"),
            source_reference.get("artifact_id"),
            str(source_reference.get("artifact_digest", "")),
            commit,
        )
        if normalized_source != source_reference:
            raise ValueError("exact evidence source artifact identity is not canonical")
        attestation = {
            "schema_version": EXACT_EVIDENCE_SCHEMA,
            "kind": EXACT_ATTESTATION_KIND,
            "commit": commit,
            "tag": tag,
            "generated_at": evidence["generated_at"],
            "production_image_sha256": production_digest,
            "artifact_sha256s": artifact_sha256s,
            "source_evidence_sha256": hashlib.sha256(
                evidence_path.read_bytes()
            ).hexdigest(),
            "source_artifact": normalized_source,
            "artifacts": hashes,
            "requirements": evidence["requirements"],
        }
    else:
        attestation = {
            "schema_version": 2,
            "kind": "sigurdos-release-attestation",
            "commit": commit,
            "tag": tag,
            "generated_at": evidence["generated_at"],
            "source_evidence_sha256": hashlib.sha256(
                evidence_path.read_bytes()
            ).hexdigest(),
            "artifacts": hashes,
            "requirements": evidence["requirements"],
        }
    output_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = output_path.with_suffix(output_path.suffix + ".tmp")
    temporary.write_text(
        json.dumps(attestation, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    temporary.replace(output_path)
    return attestation


def verify_attestation(
    path: Path,
    commit: str,
    tag: str,
    artifacts_dir: Path,
    max_age_days: int = 30,
    expected_source_reference: dict[str, object] | None = None,
    require_exact: bool = False,
) -> int:
    if not COMMIT_RE.fullmatch(commit):
        raise ValueError("expected commit must be a full lowercase 40-character SHA")
    document = _load_object(path, "release attestation")
    schema = document.get("schema_version")
    kind = document.get("kind")
    if require_exact and schema != EXACT_EVIDENCE_SCHEMA:
        raise ValueError("stable v1.0.0 requires schema 3 exact attestation")
    if (schema, kind) not in {
        (2, "sigurdos-release-attestation"),
        (EXACT_EVIDENCE_SCHEMA, EXACT_ATTESTATION_KIND),
    }:
        raise ValueError("unsupported release attestation schema")
    if document.get("commit") != commit or document.get("tag") != tag:
        raise ValueError("release attestation does not match the tagged commit")
    source_digest = document.get("source_evidence_sha256")
    if not SHA256_RE.fullmatch(str(source_digest)):
        raise ValueError("release attestation has an invalid source evidence digest")
    declared = _declared_hashes(document)
    if declared is None or set(declared) != set(REQUIRED_RELEASE_ARTIFACTS):
        raise ValueError("release attestation must cover the complete artifact contract")
    actual, production_digest = _production_hashes(artifacts_dir, commit, tag)
    for filename, digest in actual.items():
        if declared.get(filename) != digest:
            raise ValueError(f"attested digest does not match produced bytes: {filename}")
    if schema == EXACT_EVIDENCE_SCHEMA:
        if set(document) != EXACT_ATTESTATION_KEYS:
            raise ValueError("exact attestation has unknown or missing top-level fields")
        if document.get("production_image_sha256") != production_digest:
            raise ValueError("release attestation production image digest is stale")
        artifact_sha256s = _artifact_role_sha256s(
            document.get("artifact_sha256s"), "artifact_sha256s"
        )
        if artifact_sha256s != _role_hashes_from_artifacts(actual):
            raise ValueError("release attestation artifact role digests are stale")
        source_reference = document.get("source_artifact")
        if not isinstance(source_reference, dict):
            raise ValueError("release attestation has no immutable source artifact")
        normalized_source = evidence_source_reference(
            str(source_reference.get("repository", "")),
            source_reference.get("run_id"),
            source_reference.get("artifact_id"),
            str(source_reference.get("artifact_digest", "")),
            commit,
        )
        if normalized_source != source_reference:
            raise ValueError("release attestation source artifact identity is not canonical")
        if (
            expected_source_reference is not None
            and normalized_source != expected_source_reference
        ):
            raise ValueError("release attestation source artifact was not the admitted input")
        return _verify_requirements(
            document,
            tag,
            max_age_days,
            expected_commit=commit,
            production_image_sha256=production_digest,
            artifact_sha256s=artifact_sha256s,
        )
    if expected_source_reference is not None:
        raise ValueError("legacy attestation cannot satisfy exact source artifact binding")
    return _verify_requirements(document, tag, max_age_days)


def verify_attested_requirement(
    path: Path,
    requirement_id: str,
    *,
    expected_tag: str,
    expected_commit: str,
    expected_artifact_sha256s: dict[str, str],
    expected_source_reference: dict[str, object],
    max_age_days: int = 30,
) -> tuple[dict, str]:
    """Validate one record from an admitted exact attestation without hardware I/O."""
    if not COMMIT_RE.fullmatch(expected_commit):
        raise ValueError("expected commit must be a full lowercase 40-character SHA")
    document = _load_object(path, "release attestation")
    if (
        document.get("schema_version") != EXACT_EVIDENCE_SCHEMA
        or document.get("kind") != EXACT_ATTESTATION_KIND
        or set(document) != EXACT_ATTESTATION_KEYS
    ):
        raise ValueError("independent evidence must be an exact release attestation")
    if document.get("commit") != expected_commit or document.get("tag") != expected_tag:
        raise ValueError("independent evidence does not match the exact candidate")
    source_digest = document.get("source_evidence_sha256")
    if not isinstance(source_digest, str) or not SHA256_RE.fullmatch(source_digest):
        raise ValueError("independent evidence source digest is invalid")
    declared = _declared_hashes(document)
    if declared is None or set(declared) != set(REQUIRED_RELEASE_ARTIFACTS):
        raise ValueError("independent evidence does not cover the artifact contract")
    artifact_sha256s = _artifact_role_sha256s(
        document.get("artifact_sha256s"), "artifact_sha256s"
    )
    _validate_expected_role_hashes(artifact_sha256s, expected_artifact_sha256s)
    if document.get("production_image_sha256") != artifact_sha256s["production"]:
        raise ValueError("independent evidence production image binding is invalid")
    source_reference = document.get("source_artifact")
    if not isinstance(source_reference, dict):
        raise ValueError("independent evidence has no immutable source artifact")
    normalized_source = evidence_source_reference(
        str(source_reference.get("repository", "")),
        source_reference.get("run_id"),
        source_reference.get("artifact_id"),
        str(source_reference.get("artifact_digest", "")),
        expected_commit,
    )
    if normalized_source != source_reference or normalized_source != expected_source_reference:
        raise ValueError("independent evidence source artifact identity was not admitted")
    _verify_requirements(
        document,
        expected_tag,
        max_age_days,
        expected_commit=expected_commit,
        production_image_sha256=artifact_sha256s["production"],
        artifact_sha256s=artifact_sha256s,
    )
    records = document["requirements"]
    record = next((item for item in records if item["id"] == requirement_id), None)
    if record is None:
        raise ValueError(f"missing requirement evidence: {requirement_id}")
    return dict(record), source_digest


def main() -> int:
    parser = argparse.ArgumentParser()
    source = parser.add_mutually_exclusive_group()
    source.add_argument("--evidence", type=Path)
    source.add_argument("--attestation", type=Path)
    parser.add_argument("--commit")
    parser.add_argument("--tag")
    parser.add_argument("--artifacts-dir", type=Path)
    parser.add_argument("--production-image-sha256")
    parser.add_argument("--write-attestation", type=Path)
    parser.add_argument("--source-repository")
    parser.add_argument("--source-run-id")
    parser.add_argument("--source-artifact-id")
    parser.add_argument("--source-artifact-digest")
    parser.add_argument("--source-artifact-metadata", type=Path)
    parser.add_argument("--source-run-metadata", type=Path)
    parser.add_argument("--max-age-days", type=int, default=30)
    parser.add_argument("--require-exact", action="store_true")
    args = parser.parse_args()
    try:
        contract_count = verify_contract()
        source_values = (
            args.source_repository,
            args.source_run_id,
            args.source_artifact_id,
            args.source_artifact_digest,
        )
        if any(value is not None for value in source_values) and not all(
            value is not None for value in source_values
        ):
            raise ValueError("source artifact identity requires all four source fields")
        source_reference = None
        if all(value is not None for value in source_values):
            if not args.commit:
                raise ValueError("source artifact identity requires --commit")
            source_reference = evidence_source_reference(
                args.source_repository,
                args.source_run_id,
                args.source_artifact_id,
                args.source_artifact_digest,
                args.commit,
            )
        metadata_values = (
            args.source_artifact_metadata,
            args.source_run_metadata,
        )
        if any(value is not None for value in metadata_values) and not all(
            value is not None for value in metadata_values
        ):
            raise ValueError("source verification requires both GitHub metadata documents")
        if any(value is not None for value in metadata_values) and source_reference is None:
            raise ValueError("source metadata requires the admitted source artifact identity")
        if args.evidence:
            if not args.tag:
                raise ValueError("--evidence requires --tag")
            if args.write_attestation and (not args.commit or not args.artifacts_dir):
                raise ValueError(
                    "--write-attestation requires --commit and --artifacts-dir"
                )
            evidence_count, evidence = verify_evidence(
                args.evidence,
                args.tag,
                expected_commit=args.commit,
                artifacts_dir=args.artifacts_dir,
                expected_production_image_sha256=args.production_image_sha256,
                max_age_days=args.max_age_days,
                require_exact=args.require_exact,
            )
            if all(value is not None for value in metadata_values):
                verify_github_source_metadata(
                    args.source_artifact_metadata,
                    args.source_run_metadata,
                    source_reference,
                    args.commit,
                )
            if args.write_attestation:
                if (
                    evidence.get("schema_version") == EXACT_EVIDENCE_SCHEMA
                    and (
                        source_reference is None
                        or not all(value is not None for value in metadata_values)
                    )
                ):
                    raise ValueError(
                        "exact attestation needs verified immutable source artifact metadata"
                    )
                write_attestation(
                    args.write_attestation,
                    args.evidence,
                    evidence,
                    args.commit,
                    args.tag,
                    args.artifacts_dir,
                    source_reference,
                )
        elif args.attestation:
            if not args.commit or not args.tag or not args.artifacts_dir:
                raise ValueError(
                    "--attestation requires --commit, --tag, and --artifacts-dir"
                )
            if args.write_attestation:
                raise ValueError("--write-attestation is only valid with --evidence")
            if any(value is not None for value in metadata_values):
                raise ValueError("GitHub source metadata is only valid with --evidence")
            evidence_count = verify_attestation(
                args.attestation,
                args.commit,
                args.tag,
                args.artifacts_dir,
                args.max_age_days,
                source_reference,
                args.require_exact,
            )
        elif (
            args.commit
            or args.tag
            or args.artifacts_dir
            or args.production_image_sha256
            or args.write_attestation
            or args.require_exact
            or source_reference is not None
            or any(value is not None for value in metadata_values)
        ):
            raise ValueError("release arguments require --evidence or --attestation")
        else:
            evidence_count = None
    except (OSError, ValueError, json.JSONDecodeError, ReleaseArtifactError) as exc:
        print(f"release evidence verification failed: {exc}", file=sys.stderr)
        return 1
    print(f"release evidence contract OK: {contract_count} requirements")
    if evidence_count is not None:
        print(f"completed release evidence OK: {evidence_count} requirements")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
