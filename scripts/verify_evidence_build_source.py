#!/usr/bin/env python3
"""Verify the immutable Pi validation artifact admitted by the evidence workflow."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from release_artifact_contract import ReleaseArtifactError
from verify_release_evidence import (
    actions_artifact_reference,
    verify_github_artifact_metadata,
)


TRUSTED_BUILD_WORKFLOW = ".github/workflows/krabos-edge.yml"
TRUSTED_BRANCH = "main"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact-metadata", type=Path, required=True)
    parser.add_argument("--run-metadata", type=Path, required=True)
    parser.add_argument("--repository", required=True)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--artifact-id", required=True)
    parser.add_argument("--artifact-digest", required=True)
    parser.add_argument("--commit", required=True)
    args = parser.parse_args()
    try:
        reference = actions_artifact_reference(
            args.repository,
            args.run_id,
            args.artifact_id,
            args.artifact_digest,
            f"krabos-validation-{args.commit}",
        )
        verify_github_artifact_metadata(
            args.artifact_metadata,
            args.run_metadata,
            reference,
            args.commit,
            trusted_workflow=TRUSTED_BUILD_WORKFLOW,
            expected_branch=TRUSTED_BRANCH,
        )
    except (OSError, ValueError, json.JSONDecodeError, ReleaseArtifactError) as exc:
        print(f"evidence build source verification failed: {exc}", file=sys.stderr)
        return 1
    print("evidence build source OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
