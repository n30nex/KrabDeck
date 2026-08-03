from __future__ import annotations

import sys
import unittest
from pathlib import Path

SCRIPTS = Path(__file__).resolve().parents[1]
if str(SCRIPTS) not in sys.path:
    sys.path.insert(0, str(SCRIPTS))

from hw_test.hw_report import HardwareReport, TestResult, TestStatus, utc_now


class HardwareReportGateTests(unittest.TestCase):
    def result(self, status: TestStatus) -> TestResult:
        now = utc_now()
        return TestResult(
            name="fixture.check",
            status=status,
            started_at=now,
            finished_at=now,
            duration_s=0.0,
        )

    def test_incomplete_reports_never_claim_pass(self) -> None:
        self.assertEqual(HardwareReport("smoke", "pi").exit_code, 1)
        for status in (TestStatus.WARN, TestStatus.SKIP):
            report = HardwareReport("smoke", "pi", results=[self.result(status)])
            self.assertEqual(report.exit_code, 1)
            self.assertEqual(report.outcome, "PARTIAL")

    def test_all_pass_report_remains_passing(self) -> None:
        report = HardwareReport("smoke", "pi", results=[self.result(TestStatus.PASS)])
        self.assertEqual(report.exit_code, 0)
        self.assertEqual(report.outcome, "PASS")


if __name__ == "__main__":
    unittest.main()
