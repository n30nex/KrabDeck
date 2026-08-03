"""Static contracts for first-party warning-free platform variants."""

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PREFS_SOURCE = (ROOT / "src/hal/prefs.cpp").read_text(encoding="utf-8")
MESH_SOURCE = (ROOT / "src/mesh/mesh_wrapper.cpp").read_text(encoding="utf-8")
PRODUCTION_GUARD = "#if defined(KRABOS_PRODUCTION) && KRABOS_PRODUCTION"
RADIO_GUARD = "#if (!defined(SIGURDOS_REMOTE_TEST)"


def is_inside_guard(source: str, symbol: str, guard: str) -> bool:
    symbol_pos = source.index(symbol)
    guard_pos = source.rfind(guard, 0, symbol_pos)
    return guard_pos >= 0 and source.find("#endif", guard_pos, symbol_pos) < 0


class FirstPartyWarningContractTests(unittest.TestCase):
    def test_production_only_helpers_are_guarded(self) -> None:
        self.assertTrue(is_inside_guard(
            PREFS_SOURCE, "PrefsNamespaceState prefsNamespaceState()", PRODUCTION_GUARD))
        self.assertTrue(is_inside_guard(
            MESH_SOURCE, "boot_advert_attempted = false", PRODUCTION_GUARD))

    def test_radio_only_helper_is_guarded(self) -> None:
        self.assertTrue(is_inside_guard(
            MESH_SOURCE, "static void disableRadioPrefs", RADIO_GUARD))


if __name__ == "__main__":
    unittest.main()
