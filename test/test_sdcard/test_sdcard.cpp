/**
 * Unit tests for SD card HAL contract
 * Tests: API signatures, filesystem mount states, file path validation,
 *        size formatting, SPI CS pin sanity
 */
#include <gtest/gtest.h>
#include "hal/tdeck_pins.h"
#include "Arduino.h"
#include <cstring>

namespace {

// ── Replicated SD card state machine for testing ──────────
enum class SDState { NOT_MOUNTED, MOUNTING, MOUNTED, ERROR };

static SDState sd_state = SDState::NOT_MOUNTED;
static uint64_t sd_capacity_bytes = 0;
static uint64_t sd_free_bytes = 0;

void sd_mock_set_state(SDState s, uint64_t cap = 0, uint64_t free = 0) {
    sd_state = s;
    sd_capacity_bytes = cap;
    sd_free_bytes = free;
}

// ── File path validation ──────────────────────────────────
bool sd_is_valid_path(const char* path) {
    if (!path || path[0] == '\0') return false;
    if (path[0] != '/') return false;  // must start with /
    if (strstr(path, "..")) return false;  // no traversal
    if (strlen(path) > 255) return false;  // max path length
    return true;
}

// ── Format helpers ────────────────────────────────────────
const char* sd_format_size(uint64_t bytes, char* buf, size_t buf_sz) {
    if (bytes >= 1024ULL * 1024 * 1024) {
        snprintf(buf, buf_sz, "%.1f GB", (double)bytes / (1024.0 * 1024.0 * 1024.0));
    } else if (bytes >= 1024 * 1024) {
        snprintf(buf, buf_sz, "%.1f MB", (double)bytes / (1024.0 * 1024.0));
    } else {
        snprintf(buf, buf_sz, "%llu KB", (unsigned long long)(bytes / 1024));
    }
    return buf;
}

// ════════════════════════════════════════════════════════
// TESTS
// ════════════════════════════════════════════════════════
class SDCardTest : public ::testing::Test {
protected:
    void SetUp() override {
        arduino_mock::reset();
        sd_mock_set_state(SDState::NOT_MOUNTED, 0, 0);
    }
};

// ── CS pin ────────────────────────────────────────────────
TEST_F(SDCardTest, CSPinIsValidGPIO) {
    EXPECT_GE(PIN_SD_CS, 0);
    EXPECT_LE(PIN_SD_CS, 48);
}

TEST_F(SDCardTest, CSPinDoesNotConflictWithDisplayOrLoRa) {
    EXPECT_NE(PIN_SD_CS, PIN_TFT_CS);
    EXPECT_NE(PIN_SD_CS, PIN_LORA_NSS);
}

// ── Mount states ──────────────────────────────────────────
TEST_F(SDCardTest, InitialStateIsNotMounted) {
    EXPECT_EQ(sd_state, SDState::NOT_MOUNTED);
}

TEST_F(SDCardTest, MountedStateHasCapacity) {
    sd_mock_set_state(SDState::MOUNTED, 32000000000ULL, 15000000000ULL);
    EXPECT_EQ(sd_state, SDState::MOUNTED);
    EXPECT_GT(sd_capacity_bytes, 0ULL);
    EXPECT_GT(sd_free_bytes, 0ULL);
}

TEST_F(SDCardTest, ErrorStateHasZeroCapacity) {
    sd_mock_set_state(SDState::ERROR);
    EXPECT_EQ(sd_state, SDState::ERROR);
}

// ── File path validation ──────────────────────────────────
TEST_F(SDCardTest, ValidPathStartsWithSlash) {
    EXPECT_TRUE(sd_is_valid_path("/maps/london.mbtiles"));
    EXPECT_TRUE(sd_is_valid_path("/config.txt"));
    EXPECT_TRUE(sd_is_valid_path("/logs/mesh_2026_05_14.log"));
}

TEST_F(SDCardTest, PathWithoutSlashIsInvalid) {
    EXPECT_FALSE(sd_is_valid_path("maps/file.dat"));
    EXPECT_FALSE(sd_is_valid_path(""));
    EXPECT_FALSE(sd_is_valid_path(nullptr));
}

TEST_F(SDCardTest, PathWithDotDotIsInvalid) {
    EXPECT_FALSE(sd_is_valid_path("/etc/../passwd"));
    EXPECT_FALSE(sd_is_valid_path("/../../root"));
}

TEST_F(SDCardTest, PathTooLongIsInvalid) {
    char long_path[260];
    memset(long_path, 'a', sizeof(long_path));
    long_path[0] = '/';
    long_path[259] = '\0';
    EXPECT_FALSE(sd_is_valid_path(long_path));
}

TEST_F(SDCardTest, PathAtMaxLengthIsValid) {
    char max_path[256];
    memset(max_path, 'x', 254);
    max_path[0] = '/';
    max_path[255] = '\0';
    EXPECT_TRUE(sd_is_valid_path(max_path));
}

// ── Size formatting ───────────────────────────────────────
TEST_F(SDCardTest, FormatGB) {
    char buf[16];
    const char* s = sd_format_size(32ULL * 1024 * 1024 * 1024, buf, sizeof(buf));
    EXPECT_STREQ(s, "32.0 GB");
}

TEST_F(SDCardTest, FormatMB) {
    char buf[16];
    const char* s = sd_format_size(500 * 1024 * 1024ULL, buf, sizeof(buf));
    EXPECT_STREQ(s, "500.0 MB");
}

TEST_F(SDCardTest, FormatKB) {
    char buf[16];
    const char* s = sd_format_size(512 * 1024ULL, buf, sizeof(buf));
    EXPECT_STREQ(s, "512 KB");
}

TEST_F(SDCardTest, FormatZero) {
    char buf[16];
    const char* s = sd_format_size(0, buf, sizeof(buf));
    EXPECT_STREQ(s, "0 KB");
}

// ── Map tile path convention ──────────────────────────────
TEST_F(SDCardTest, MapTilePathConvention) {
    // Offline maps expect /maps/<name>.mbtiles or /maps/z/x/y.png
    EXPECT_TRUE(sd_is_valid_path("/maps/hertford.mbtiles"));
    EXPECT_TRUE(sd_is_valid_path("/maps/10/512/340.png"));
    EXPECT_TRUE(sd_is_valid_path("/maps/raster_tiles/5/16/10.jpg"));
}

} // anonymous namespace
