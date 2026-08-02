import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class ReleaseIssueGateTests(unittest.TestCase):
    def run_gate(self, pages, *extra_args):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "issues.json"
            path.write_text(json.dumps(pages), encoding="utf-8")
            return subprocess.run(
                [
                    sys.executable,
                    str(ROOT / "scripts/check_release_issue_gate.py"),
                    path,
                    *extra_args,
                ],
                cwd=ROOT,
                capture_output=True,
                text=True,
            )

    def test_gate_label_blocks_even_without_priority_label(self):
        result = self.run_gate(
            [[{"number": 6, "title": "M5 release", "labels": [{"name": "gate"}]}]]
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("#6 M5 release", result.stderr)

    def test_p0_and_p1_still_block(self):
        result = self.run_gate(
            [[
                {"number": 7, "title": "P0 data loss", "labels": []},
                {"number": 8, "title": "Regression", "labels": [{"name": "P1"}]},
            ]]
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("#7 P0 data loss", result.stderr)
        self.assertIn("#8 Regression", result.stderr)

    def test_publication_issue_may_remain_open_until_release_is_verified(self):
        result = self.run_gate(
            [[
                {
                    "number": 6,
                    "title": "M5 v1.0 publication",
                    "labels": [
                        {"name": "gate"},
                        {"name": "phase:m5"},
                        {"name": "release"},
                    ],
                }
            ]],
            "--allow-open-release-tracker",
            "6",
        )
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_publication_issue_cannot_exempt_a_priority_blocker(self):
        result = self.run_gate(
            [[
                {
                    "number": 6,
                    "title": "M5 v1.0 publication P1 blocker",
                    "labels": [
                        {"name": "gate"},
                        {"name": "phase:m5"},
                        {"name": "release"},
                    ],
                }
            ]],
            "--allow-open-release-tracker",
            "6",
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("#6 M5 v1.0 publication P1 blocker", result.stderr)

    def test_publication_exception_does_not_exempt_other_gate_issues(self):
        result = self.run_gate(
            [[
                {"number": 2, "title": "M1 hardware", "labels": [{"name": "gate"}]},
                {
                    "number": 6,
                    "title": "M5 publication",
                    "labels": [
                        {"name": "gate"},
                        {"name": "phase:m5"},
                        {"name": "release"},
                    ],
                },
            ]],
            "--allow-open-release-tracker",
            "6",
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("#2 M1 hardware", result.stderr)
        self.assertNotIn("#6 M5 publication", result.stderr)

    def test_release_tracker_with_missing_identity_label_fails_closed(self):
        result = self.run_gate(
            [[
                {
                    "number": 6,
                    "title": "M5 publication",
                    "labels": [{"name": "gate"}, {"name": "release"}],
                }
            ]],
            "--allow-open-release-tracker",
            "6",
        )
        self.assertEqual(result.returncode, 2)
        self.assertIn("missing required labels: phase:m5", result.stderr)

    def test_pull_requests_and_unlabelled_lower_priority_issues_do_not_block(self):
        result = self.run_gate(
            [[
                {
                    "number": 9,
                    "title": "P0 in a pull request",
                    "labels": [{"name": "gate"}],
                    "pull_request": {"url": "https://example.invalid/pr/9"},
                },
                {"number": 10, "title": "P2 cleanup", "labels": []},
            ]]
        )
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_malformed_response_fails_closed(self):
        result = self.run_gate({"items": []})
        self.assertEqual(result.returncode, 2)
        self.assertIn("failed closed", result.stderr)


if __name__ == "__main__":
    unittest.main()
