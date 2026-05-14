#pragma once
// SlopOS T-Deck Hardware Pin Definitions
// LilyGo T-Deck: ESP32-S3 + ST7789 320x240 + SX1262 LoRa + GT911 Touch

// ════════════════════════════════════════════════════════
// LoRa SX1262 (SPI)
// ════════════════════════════════════════════════════════
#define PIN_LORA_NSS      9
#define PIN_LORA_DIO1    45
#define PIN_LORA_RESET   17
#define PIN_LORA_BUSY    13
#define PIN_LORA_SCLK    40
#define PIN_LORA_MISO    38
#define PIN_LORA_MOSI    41

// ════════════════════════════════════════════════════════
// Display ST7789 320x240 (shares SPI with LoRa)
// ════════════════════════════════════════════════════════
#define PIN_TFT_CS       12
#define PIN_TFT_DC       11
#define PIN_TFT_RST      -1
#define PIN_TFT_BL       42
#define PIN_TFT_SCL      PIN_LORA_SCLK   // shared SPI clock
#define PIN_TFT_SDA      PIN_LORA_MOSI   // shared SPI data
#define TFT_WIDTH       320
#define TFT_HEIGHT      240

// ════════════════════════════════════════════════════════
// Touch GT911 (I2C)
// ════════════════════════════════════════════════════════
#define PIN_TOUCH_SDA    18
#define PIN_TOUCH_SCL     8
#define PIN_TOUCH_INT    16
#define PIN_TOUCH_RST    -1
#define TOUCH_I2C_ADDR  0x5D
#define TOUCH_MAX_X     TFT_WIDTH
#define TOUCH_MAX_Y     TFT_HEIGHT
#define TOUCH_SWAP_XY    true
#define TOUCH_MIRROR_X   false
#define TOUCH_MIRROR_Y   false

// ── I2C bus aliases (shared with touch) ──────────────────
#define PIN_I2C_SDA      PIN_TOUCH_SDA
#define PIN_I2C_SCL      PIN_TOUCH_SCL

// ════════════════════════════════════════════════════════
// Matrix Keyboard (T-Deck QWERTY)
// ════════════════════════════════════════════════════════
#define PIN_KB_ROW0      4
#define PIN_KB_ROW1      5
#define PIN_KB_ROW2      6
#define PIN_KB_ROW3      7
#define PIN_KB_COL0     14
#define PIN_KB_COL1     15
#define PIN_KB_COL2     21
#define PIN_KB_COL3     47
#define PIN_KB_COL4     48
#define KB_ROWS           4
#define KB_COLS           5

// ════════════════════════════════════════════════════════
// Trackball / User Button
// ════════════════════════════════════════════════════════
#define PIN_TRACKBALL    0

// ════════════════════════════════════════════════════════
// Battery & Power
// ════════════════════════════════════════════════════════
#define PIN_BAT_ADC       4
#define PIN_PERIPH_PWR   10
#define BAT_ADC_MULT     (2.0f * 3.3f * 1000.0f)
#define BAT_MIN_MV      3000
#define BAT_MAX_MV      4200

// ════════════════════════════════════════════════════════
// GPS (Serial1)
// ════════════════════════════════════════════════════════
#define PIN_GPS_RX       43
#define PIN_GPS_TX       44
#define GPS_BAUD_RATE 38400

// ════════════════════════════════════════════════════════
// SD Card (SPI, shared bus)
// ════════════════════════════════════════════════════════
#define PIN_SD_CS        13

// ════════════════════════════════════════════════════════
// Audio Buzzer
// ════════════════════════════════════════════════════════
#define PIN_BUZZER       46

// ════════════════════════════════════════════════════════
// LoRa Radio Defaults
// ════════════════════════════════════════════════════════
#define LORA_FREQ    869.618f
#define LORA_BW        62.5f
#define LORA_SF           8
#define LORA_CR           5
#define LORA_TX_PWR      22
