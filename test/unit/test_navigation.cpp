/**
 * Unit tests for screen navigation state machine
 * Tests: routing correctness, back navigation, same-screen guard
 *
 * NOTE: These tests validate the navigation logic by replicating
 * the state machine behavior. Full LVGL integration is tested
 * on-hardware since the mock layer doesn't render screens.
 */
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace {

// ── Replicate the navigation state machine for pure testing ──
enum class Screen {
    Home, Chat, Contacts, Repeaters, Finder, Heard,
    Map, Advertise, Settings, Trace, Terminal, Noise, Signal, COUNT
};

static Screen current = Screen::Home;
static Screen previous = Screen::Home;
static std::vector<std::string> nav_log; // track navigation events

void navigate_to(Screen screen) {
    if (screen == current) return;
    previous = current;
    current = screen;
    nav_log.push_back("nav:" + std::to_string((int)screen));
}

void go_back() {
    if (previous == current) return; // no previous
    navigate_to(previous);
}

void reset_nav() {
    current = Screen::Home;
    previous = Screen::Home;
    nav_log.clear();
}

class NavigationTest : public ::testing::Test {
protected:
    void SetUp() override { reset_nav(); }
};

// ── Initial state ───────────────────────────────────────
TEST_F(NavigationTest, InitialStateIsHome) {
    EXPECT_EQ(current, Screen::Home);
    EXPECT_EQ(previous, Screen::Home);
    EXPECT_TRUE(nav_log.empty());
}

// ── Forward navigation ──────────────────────────────────
TEST_F(NavigationTest, NavigateToChatUpdatesState) {
    navigate_to(Screen::Chat);
    EXPECT_EQ(current, Screen::Chat);
    EXPECT_EQ(previous, Screen::Home);
    EXPECT_EQ(nav_log.size(), 1u);
}

TEST_F(NavigationTest, NavigateToSameScreenIsNoop) {
    navigate_to(Screen::Home);  // already home
    EXPECT_EQ(current, Screen::Home);
    EXPECT_EQ(previous, Screen::Home);
    EXPECT_TRUE(nav_log.empty()); // no event logged
}

TEST_F(NavigationTest, NavigateToAllScreens) {
    std::vector<Screen> screens = {
        Screen::Chat, Screen::Contacts, Screen::Repeaters, Screen::Finder,
        Screen::Heard, Screen::Map, Screen::Advertise, Screen::Settings,
        Screen::Trace, Screen::Terminal, Screen::Noise, Screen::Signal
    };

    for (auto s : screens) {
        reset_nav();
        navigate_to(s);
        EXPECT_EQ(current, s);
        EXPECT_EQ(previous, Screen::Home);
    }
}

// ── Back navigation ─────────────────────────────────────
TEST_F(NavigationTest, GoBackReturnsToPrevious) {
    navigate_to(Screen::Chat);
    go_back();
    EXPECT_EQ(current, Screen::Home);
    EXPECT_EQ(previous, Screen::Chat);
}

TEST_F(NavigationTest, GoBackFromHomeIsNoop) {
    go_back(); // already home, no previous
    EXPECT_EQ(current, Screen::Home);
    EXPECT_EQ(previous, Screen::Home);
}

TEST_F(NavigationTest, DeepNavigationAndBack) {
    // Home → Chat → Settings → Home (via back, back)
    navigate_to(Screen::Chat);
    navigate_to(Screen::Settings);
    EXPECT_EQ(current, Screen::Settings);
    EXPECT_EQ(previous, Screen::Chat);

    go_back();
    EXPECT_EQ(current, Screen::Chat);
    EXPECT_EQ(previous, Screen::Settings);

    go_back();
    EXPECT_EQ(current, Screen::Home);
    EXPECT_EQ(previous, Screen::Chat);
}

// ── Rapid navigation ────────────────────────────────────
TEST_F(NavigationTest, RapidNavigationDoesNotLoseState) {
    for (int i = 0; i < 100; i++) {
        Screen s = (Screen)((i % 12) + 1); // cycle through all screens
        navigate_to(s);
    }
    // Should still have a valid state
    EXPECT_NE(current, Screen::COUNT);
}

// ── Navigation to every screen from every screen ────────
TEST_F(NavigationTest, AllScreenPairsWork) {
    for (int from = 0; from < (int)Screen::COUNT; from++) {
        for (int to = 0; to < (int)Screen::COUNT; to++) {
            reset_nav();
            navigate_to((Screen)from);
            navigate_to((Screen)to);
            if (from == to) {
                // Should have stayed at 'from' since same-screen is noop
                EXPECT_EQ(current, (Screen)from);
            } else {
                EXPECT_EQ(current, (Screen)to);
                EXPECT_EQ(previous, (Screen)from);
            }
        }
    }
}

// ── Screen count matches expected ───────────────────────
TEST_F(NavigationTest, ScreenCountIs13) {
    // Home + 12 app screens = 13
    EXPECT_EQ((int)Screen::COUNT, 13);
}

// ── Screen enum values are contiguous ───────────────────
TEST_F(NavigationTest, ScreenEnumValuesAreContiguous) {
    EXPECT_EQ((int)Screen::Home, 0);
    EXPECT_EQ((int)Screen::Chat, 1);
    EXPECT_EQ((int)Screen::Signal, 12);
    EXPECT_EQ((int)Screen::COUNT, 13);
}

} // anonymous namespace
