"""Static contract for the production boot advert handoff."""

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MAIN_SOURCE = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
MESH_SOURCE = (ROOT / "src/mesh/mesh_wrapper.cpp").read_text(encoding="utf-8")


class BootAdvertContractTests(unittest.TestCase):
    def test_production_boot_emits_one_accepted_boot_advert_event(self) -> None:
        self.assertNotIn("sendAdvert", MAIN_SOURCE)
        self.assertEqual(MESH_SOURCE.count("sigurdos::mesh::sendAdvert(false)"), 1)
        self.assertEqual(
            MESH_SOURCE.count(
                '"@krabos|event=boot_advert|status=queued|scope=wildcard"'),
            1,
        )
        service = MESH_SOURCE.index("static void serviceProductionBootAdvert()")
        advert = MESH_SOURCE.index("sigurdos::mesh::sendAdvert(false)", service)
        marker = MESH_SOURCE.index(
            '"@krabos|event=boot_advert|status=queued|scope=wildcard"', advert)
        self.assertLess(service, advert)
        self.assertLess(advert, marker)
        self.assertIn("boot_advert_queued", MESH_SOURCE)
