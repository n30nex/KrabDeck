# SlopOS T-Deck Test Suite

## Running Tests

```bash
# Run all 91 tests on native platform (no hardware needed)
pio test -e native_test -v

# Run specific test file
pio test -e native_test -f test_battery -v

# Run with verbose output
pio test -e native_test -vv
```

## Test Structure

```
test/
├── mocks/                  # Mock headers for native testing (no hardware needed)
│   ├── Arduino.h           # Mock Arduino core (millis, GPIO, Serial, SPI, Wire)
│   ├── lvgl.h              # Mock LVGL v9 API (all UI objects stubbed)
│   ├── RadioLib.h          # Mock SX1262 radio driver
│   ├── LovyanGFX.hpp       # Mock LovyanGFX display driver
│   ├── MeshCore.h          # Mock MeshCore base classes
│   └── mesh_helpers.h      # Mock AutoDiscoverRTCClock, StdRNG
├── unit/
│   ├── test_battery.cpp    # Battery mV→% conversion, ADC math, edge cases
│   ├── test_theme.cpp      # Color constants validation, distinctness, brightness
│   ├── test_pins.cpp       # Pin conflicts, GPIO ranges, bus consistency
│   ├── test_navigation.cpp # Screen routing state machine, back nav, edge cases
│   └── test_mesh_wrapper.cpp # Mesh API contract, return value ranges
└── integration/
    └── test_build.cpp      # All headers include together, cross-module consistency
```

## What's Tested

| Module | Tests | Coverage |
|--------|-------|----------|
| Battery HAL | 15 | mV→%, clamping, monotonicity, ADC math |
| Theme constants | 9 | darkness, vibrancy, distinctness, readability |
| Pin definitions | 13 | GPIO range, SPI/I2C conflicts, duplicates, LoRa params |
| Navigation | 9 | forward/back, noop guard, deep nav, all pairs, enumeration |
| Mesh wrapper | 11 | API signatures, return ranges, unread count init |
| Build integration | 7 | header inclusion, API existence, cross-module consistency |

## Adding New Tests

1. Create `test/unit/test_newthing.cpp`
2. Include the header(s) under test + `gtest/gtest.h`
3. Use `arduino_mock::reset()` in SetUp for clean state
4. Follow AAA pattern: Arrange, Act, Assert
5. Use `EXPECT_*` for non-fatal assertions, `ASSERT_*` for fatal

### Mock Guidelines
- `arduino_mock::current_millis` — control time
- `arduino_mock::analog_values[pin]` — set ADC values
- `arduino_mock::pin_states[pin]` — set digital pin states
- All LVGL object creation returns dummy objects (no rendering)
