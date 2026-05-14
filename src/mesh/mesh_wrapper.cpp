#include "mesh_wrapper.h"
#include "hal/tdeck_board.h"
#include "hal/tdeck_pins.h"

// MeshCore core headers (from lib/meshcore)
#include <Mesh.h>
#include <helpers/SimpleMeshTables.h>
#include <helpers/radiolib/RadioLibWrappers.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include <helpers/AutoDiscoverRTCClock.h>

// ── Global objects ──────────────────────────────────────
static slopos::TDeckBoard    board;
static SPIClass              lora_spi;
static CustomSX1262          radio_module(PIN_LORA_NSS, PIN_LORA_DIO1,
                                          PIN_LORA_RESET, PIN_LORA_BUSY, lora_spi);
static CustomSX1262Wrapper   radio_driver(radio_module, board);
static ESP32RTCClock         fallback_clock;
static AutoDiscoverRTCClock  rtc_clock(fallback_clock);
static StdRNG                fast_rng;
static SimpleMeshTables      tables;

// Placeholder — full mesh app integration to come
static bool initialized = false;
static int  unread_count = 0;

namespace slopos {
namespace mesh {

bool init()
{
    // Initialize RTC and I2C
    fallback_clock.begin();
    rtc_clock.begin(Wire);

    // Initialize radio
    if (!radio_module.std_init(&lora_spi)) {
        return false;
    }

    // Seed RNG from radio entropy
    fast_rng.begin(radio_driver.getRngSeed());

    initialized = true;
    return true;
}

void loop()
{
    if (!initialized) return;

    // Radio polling — check for incoming packets
    radio_driver.loop();

    // RTC tick
    rtc_clock.tick();
}

bool send_direct(const char* dest_name, const char* message)
{
    // TODO: Full mesh integration
    (void)dest_name;
    (void)message;
    return false;
}

bool send_channel(uint8_t channel_hash[1], const char* message)
{
    (void)channel_hash;
    (void)message;
    return false;
}

int get_noise_floor()
{
    return radio_driver.getNoiseFloor();
}

int get_last_rssi()
{
    return (int)radio_driver.getLastRSSI();
}

float get_last_snr()
{
    return radio_driver.getLastSNR();
}

int get_unread_count()
{
    return unread_count;
}

int get_recent_nodes(char names[][32], int max_count)
{
    // TODO: return recently heard nodes from mesh tables
    (void)names;
    (void)max_count;
    return 0;
}

} // namespace mesh
} // namespace slopos
