import importlib.util
import unittest
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "platformio_lock", ROOT / "scripts" / "platformio_lock.py"
)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class PlatformIOLockTests(unittest.TestCase):
    def test_requested_environment_reaches_both_package_queries(self) -> None:
        with mock.patch.object(
            MODULE,
            "run",
            side_effect=["PlatformIO Core, 6.1.0", "a" * 40, "platform", "libraries"],
        ) as run:
            MODULE.resolved_graph("KrabOS_TDeckPlus")

        self.assertEqual(
            run.call_args_list[2:],
            [
                mock.call(
                    "pio",
                    "pkg",
                    "list",
                    "-e",
                    "KrabOS_TDeckPlus",
                    "--only-platforms",
                ),
                mock.call(
                    "pio",
                    "pkg",
                    "list",
                    "-e",
                    "KrabOS_TDeckPlus",
                    "--only-libraries",
                ),
            ],
        )

    def test_environment_is_selectable_from_cli(self) -> None:
        args = MODULE.build_parser().parse_args(
            ["--check", "--environment", "KrabOS_TDeckPlus_recovery"]
        )
        self.assertTrue(args.check)
        self.assertEqual(args.environment, "KrabOS_TDeckPlus_recovery")


if __name__ == "__main__":
    unittest.main()
