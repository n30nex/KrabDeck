#!/usr/bin/env python3
"""Fail a stable release while an open P0, P1, or gate-labelled issue exists."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path


PRIORITY_RE = re.compile(r"(^|[^a-z0-9])p[01]([^a-z0-9]|$)", re.IGNORECASE)


def blocking_issues(document: object) -> list[str]:
    if not isinstance(document, list):
        raise ValueError("GitHub issue response must be a list of pages")
    issues: list[object] = []
    for page in document:
        if not isinstance(page, list):
            raise ValueError("GitHub issue response page must be a list")
        issues.extend(page)

    blocked: list[str] = []
    for issue in issues:
        if not isinstance(issue, dict):
            raise ValueError("GitHub issue response contains a non-object")
        if "pull_request" in issue:
            continue
        number = issue.get("number")
        title = issue.get("title")
        labels = issue.get("labels")
        if isinstance(number, bool) or not isinstance(number, int):
            raise ValueError("GitHub issue is missing a numeric issue number")
        if not isinstance(title, str) or not isinstance(labels, list):
            raise ValueError(f"GitHub issue #{number} has malformed title or labels")
        label_names: list[str] = []
        for label in labels:
            if not isinstance(label, dict) or not isinstance(label.get("name"), str):
                raise ValueError(f"GitHub issue #{number} has malformed labels")
            label_names.append(label["name"])
        searchable = " ".join([title, *label_names])
        gate_labelled = any(name.casefold() == "gate" for name in label_names)
        if gate_labelled or PRIORITY_RE.search(searchable):
            blocked.append(f"#{number} {title}")
    return blocked


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("issues", type=Path)
    args = parser.parse_args()
    try:
        document = json.loads(args.issues.read_text(encoding="utf-8"))
        blocked = blocking_issues(document)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"stable issue gate failed closed: {exc}", file=sys.stderr)
        return 2
    if blocked:
        print("Stable release blocked by open gate/P0/P1 issues:", file=sys.stderr)
        for issue in blocked:
            print(f"- {issue}", file=sys.stderr)
        return 1
    print("stable release issue gate OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
