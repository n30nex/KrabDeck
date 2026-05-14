/**
 * Unit tests for mesh_wrapper API contract
 * Tests: function signatures, return value ranges, mock integration
 *
 * The actual MeshCore integration is tested on-hardware;
 * these tests validate the API surface and mock behavior.
 */
#include <gtest/gtest.h>
#include <cstdint>

// Include our mesh wrapper header (uses mocks for MeshCore)
#include "mesh/mesh_wrapper.h"

namespace {

class MeshWrapperTest : public ::testing::Test {
protected:
    void SetUp() override {
        arduino_mock::reset();
    }
};

// ── API function signatures compile and link ────────────
TEST_F(MeshWrapperTest, InitFunctionExists) {
    // We can't call init() without hardware, but the function symbol exists
    // Just verify return type is bool
    using init_fn = bool (*)();
    (void)static_cast<init_fn>(slopos::mesh::init);
    SUCCEED();
}

TEST_F(MeshWrapperTest, LoopFunctionExists) {
    using loop_fn = void (*)();
    (void)static_cast<loop_fn>(slopos::mesh::loop);
    SUCCEED();
}

TEST_F(MeshWrapperTest, SendDirectSignature) {
    using send_fn = bool (*)(const char*, const char*);
    (void)static_cast<send_fn>(slopos::mesh::send_direct);
    SUCCEED();
}

TEST_F(MeshWrapperTest, SendChannelSignature) {
    using send_fn = bool (*)(uint8_t*, const char*);
    (void)static_cast<send_fn>(slopos::mesh::send_channel);
    SUCCEED();
}

TEST_F(MeshWrapperTest, GetNoiseFloorReturnsInt) {
    using fn = int (*)();
    (void)static_cast<fn>(slopos::mesh::get_noise_floor);
    SUCCEED();
}

TEST_F(MeshWrapperTest, GetLastRSSIReturnsInt) {
    using fn = int (*)();
    (void)static_cast<fn>(slopos::mesh::get_last_rssi);
    SUCCEED();
}

TEST_F(MeshWrapperTest, GetLastSNRReturnsFloat) {
    using fn = float (*)();
    (void)static_cast<fn>(slopos::mesh::get_last_snr);
    SUCCEED();
}

TEST_F(MeshWrapperTest, GetUnreadCountReturnsInt) {
    using fn = int (*)();
    (void)static_cast<fn>(slopos::mesh::get_unread_count);
    SUCCEED();
}

// ── Initial unread count is zero ────────────────────────
TEST_F(MeshWrapperTest, UnreadCountStartsAtZero) {
    EXPECT_EQ(slopos::mesh::get_unread_count(), 0);
}

// ── Noise floor is within realistic range ───────────────
TEST_F(MeshWrapperTest, NoiseFloorInRealisticRange) {
    // Even with mocks, the return should be in dBm range
    int nf = slopos::mesh::get_noise_floor();
    EXPECT_GE(nf, -150);
    EXPECT_LE(nf, 0);
}

// ── Signal values are within physical limits ─────────────
TEST_F(MeshWrapperTest, RSSIInRealisticRange) {
    int rssi = slopos::mesh::get_last_rssi();
    EXPECT_GE(rssi, -160);
    EXPECT_LE(rssi, 0);
}

TEST_F(MeshWrapperTest, SNRInRealisticRange) {
    float snr = slopos::mesh::get_last_snr();
    EXPECT_GE(snr, -20.0f);
    EXPECT_LE(snr, 20.0f);
}

// ── Recent nodes returns valid count ────────────────────
TEST_F(MeshWrapperTest, GetRecentNodesReturnsNonNegative) {
    char names[4][32];
    int count = slopos::mesh::get_recent_nodes(names, 4);
    EXPECT_GE(count, 0);
    EXPECT_LE(count, 4);
}

} // anonymous namespace
