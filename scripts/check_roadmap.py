#!/usr/bin/env python3
"""Fail when the public and detailed KrabOS roadmaps disagree."""

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
ALLOWED = ("✓ Complete", "▶ Active", "○ Planned", "! Blocked")
ICONS = {status[0]: status for status in ALLOWED}


def table_status(path: Path) -> list[tuple[str, str]]:
    text = path.read_text(encoding="utf-8")
    try:
        block = text.split("<!-- roadmap-status:start -->", 1)[1].split(
            "<!-- roadmap-status:end -->", 1
        )[0]
    except IndexError as exc:
        raise ValueError(f"{path.name}: missing roadmap status markers") from exc
    rows = re.findall(r"^\| (M[0-5]) \| ([^|]+?) \|", block, re.MULTILINE)
    if [phase for phase, _ in rows] != [f"M{i}" for i in range(6)]:
        raise ValueError(f"{path.name}: expected ordered phases M0 through M5")
    for phase, status in rows:
        if status not in ALLOWED:
            raise ValueError(f"{path.name}: {phase} has invalid status {status!r}")
    return rows


def validate_order(rows: list[tuple[str, str]]) -> None:
    statuses = [status for _, status in rows]
    current = [status for status in statuses if status in ("▶ Active", "! Blocked")]
    if statuses != ["✓ Complete"] * 6 and len(current) != 1:
        raise ValueError("roadmap must have exactly one active or blocked phase")
    rank = {"✓ Complete": 0, "▶ Active": 1, "! Blocked": 1, "○ Planned": 2}
    if [rank[s] for s in statuses] != sorted(rank[s] for s in statuses):
        raise ValueError("roadmap phases cannot be skipped or completed out of order")


def main() -> int:
    readme = table_status(ROOT / "README.md")
    roadmap = table_status(ROOT / "ROADMAP.md")
    if readme != roadmap:
        raise ValueError("README.md and ROADMAP.md status tables differ")
    validate_order(readme)

    diagram = (ROOT / "README.md").read_text(encoding="utf-8")
    nodes = re.findall(r'M([0-5])\["([✓▶○!]) M\1', diagram)
    diagram_status = [(f"M{phase}", ICONS[icon]) for phase, icon in nodes]
    if diagram_status != readme:
        raise ValueError("README Mermaid symbols do not match its status table")

    print("roadmap contract: ok")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except ValueError as error:
        print(f"roadmap contract: {error}", file=sys.stderr)
        sys.exit(1)
