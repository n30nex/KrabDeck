// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben

#include <gtest/gtest.h>
#include "Arduino.h"
#include "hal/tdeck_pins.h"
#include "hal/trackball.h"

namespace {

class TrackballTest : public ::testing::Test {
protected:
    void SetUp() override {
        arduino_mock::reset();
        slopos_trackball_init();
        set_pin(PIN_TRACKBALL_UP, false);
        set_pin(PIN_TRACKBALL_DOWN, false);
        set_pin(PIN_TRACKBALL_LEFT, false);
        set_pin(PIN_TRACKBALL_RIGHT, false);
        set_pin(PIN_TRACKBALL_BTN, false);
        slopos_trackball_reset_scan_state();
        advance(300);
    }

    void set_pin(uint8_t pin, bool active) {
        arduino_mock::pin_states[pin] = active ? LOW : HIGH;
    }

    bool next(SlopOSTrackballEvent* event) {
        return slopos_trackball_next_event(event);
    }

    void advance(uint32_t ms) {
        arduino_mock::current_millis += ms;
    }

    void reset_scan_state_after_settle() {
        slopos_trackball_reset_scan_state();
        advance(300);
    }
};

TEST_F(TrackballTest, NoInputProducesNoEvent) {
    slopos_trackball_scan();

    SlopOSTrackballEvent event = SlopOSTrackballEvent::None;
    EXPECT_FALSE(next(&event));
}

TEST_F(TrackballTest, DirectionPulseProducesEventImmediately) {
    set_pin(PIN_TRACKBALL_UP, true);
    slopos_trackball_scan();

    SlopOSTrackballEvent event = SlopOSTrackballEvent::None;
    EXPECT_TRUE(next(&event));
    EXPECT_EQ(event, SlopOSTrackballEvent::Up);
}

TEST_F(TrackballTest, DirectionsMapToExpectedEvents) {
    struct Case {
        uint8_t pin;
        SlopOSTrackballEvent event;
    };

    const Case cases[] = {
        {PIN_TRACKBALL_UP, SlopOSTrackballEvent::Up},
        {PIN_TRACKBALL_DOWN, SlopOSTrackballEvent::Down},
        {PIN_TRACKBALL_LEFT, SlopOSTrackballEvent::Left},
        {PIN_TRACKBALL_RIGHT, SlopOSTrackballEvent::Right},
    };

    for (const Case& c : cases) {
        reset_scan_state_after_settle();
        set_pin(c.pin, true);
        slopos_trackball_scan();

        SlopOSTrackballEvent event = SlopOSTrackballEvent::None;
        ASSERT_TRUE(next(&event));
        EXPECT_EQ(event, c.event);
        EXPECT_FALSE(next(&event));

        set_pin(c.pin, false);
        slopos_trackball_scan();
        advance(20);
        slopos_trackball_scan();
    }
}

TEST_F(TrackballTest, PulsesAfterDeadtimeProduceSeparateEvents) {
    set_pin(PIN_TRACKBALL_DOWN, true);
    slopos_trackball_scan();

    SlopOSTrackballEvent event = SlopOSTrackballEvent::None;
    ASSERT_TRUE(next(&event));
    EXPECT_EQ(event, SlopOSTrackballEvent::Down);

    // Return to idle and pulse again after deadtime expires
    set_pin(PIN_TRACKBALL_DOWN, false);
    advance(61);
    slopos_trackball_scan();

    set_pin(PIN_TRACKBALL_DOWN, true);
    advance(1);
    slopos_trackball_scan();

    ASSERT_TRUE(next(&event));
    EXPECT_EQ(event, SlopOSTrackballEvent::Down);
}

TEST_F(TrackballTest, DirectionBounceInsideDeadtimeIsSuppressed) {
    set_pin(PIN_TRACKBALL_LEFT, true);
    slopos_trackball_scan();

    SlopOSTrackballEvent event = SlopOSTrackballEvent::None;
    ASSERT_TRUE(next(&event));

    set_pin(PIN_TRACKBALL_LEFT, false);
    advance(20);
    slopos_trackball_scan();

    set_pin(PIN_TRACKBALL_LEFT, true);
    advance(20);
    slopos_trackball_scan();
    EXPECT_FALSE(next(&event));
}

TEST_F(TrackballTest, HeldDirectionDoesNotRepeat) {
    set_pin(PIN_TRACKBALL_RIGHT, true);
    slopos_trackball_scan();

    SlopOSTrackballEvent event = SlopOSTrackballEvent::None;
    ASSERT_TRUE(next(&event));
    EXPECT_EQ(event, SlopOSTrackballEvent::Right);

    advance(1000);
    slopos_trackball_scan();
    EXPECT_FALSE(next(&event));
}

TEST_F(TrackballTest, DirectionIdleLevelIsCalibratedAtInit) {
    set_pin(PIN_TRACKBALL_LEFT, true);
    reset_scan_state_after_settle();

    slopos_trackball_scan();
    SlopOSTrackballEvent event = SlopOSTrackballEvent::None;
    EXPECT_FALSE(next(&event));

    set_pin(PIN_TRACKBALL_LEFT, false);
    slopos_trackball_scan();
    ASSERT_TRUE(next(&event));
    EXPECT_EQ(event, SlopOSTrackballEvent::Left);
}

TEST_F(TrackballTest, ClickProducesOneSelectEventWithoutRepeat) {
    set_pin(PIN_TRACKBALL_BTN, true);
    slopos_trackball_scan();
    advance(20);
    slopos_trackball_scan();

    SlopOSTrackballEvent event = SlopOSTrackballEvent::None;
    ASSERT_TRUE(next(&event));
    EXPECT_EQ(event, SlopOSTrackballEvent::Click);

    advance(1000);
    slopos_trackball_scan();
    EXPECT_FALSE(next(&event));
}

TEST_F(TrackballTest, NullEventPointerIsRejected) {
    EXPECT_FALSE(slopos_trackball_next_event(nullptr));
}

} // namespace
