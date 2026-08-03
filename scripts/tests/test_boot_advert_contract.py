"""Static contract for the production boot advert handoff."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SOURCE = (ROOT / "src/main.cpp").read_text(encoding="utf-8")


def test_production_boot_emits_one_accepted_boot_advert_event() -> None:
    call = '''if (sigurdos::mesh::sendAdvert()) {
        Serial.println("@krabos|event=boot_advert");
    }'''

    assert SOURCE.count("sigurdos::mesh::sendAdvert()") == 1
    assert SOURCE.count('"@krabos|event=boot_advert"') == 1
    assert call in SOURCE
    assert "#if !defined(SIGURDOS_REMOTE_TEST) || !SIGURDOS_REMOTE_TEST" in SOURCE
    assert SOURCE.index("sigurdos::hal::boot_watchdog_enter_runtime();") < SOURCE.index(call)
