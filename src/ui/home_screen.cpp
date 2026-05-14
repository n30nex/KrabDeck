#include "home_screen.h"
#include "theme.h"
#include <lvgl.h>

namespace slopos::ui {

using namespace theme;

static lv_obj_t* scr = nullptr;
static lv_obj_t* top_bar = nullptr;
static lv_obj_t* bottom_bar = nullptr;
static lv_obj_t* grid = nullptr;
static lv_obj_t* time_label = nullptr;
static lv_obj_t* batt_label = nullptr;
static lv_obj_t* device_label = nullptr;

// ── Icon grid labels (3 columns × 4 rows = 12 items) ──
struct IconDef {
    const char* label;
    const char* emoji;
    bool badge;
};

static const IconDef icons[] = {
    {"Chat",       "\xF0\x9F\x92\xAC", true},   // 💬
    {"Contacts",   "\xF0\x9F\x91\xA5", false},  // 👥
    {"Repeaters",  "\xF0\x9F\x94\x84", false},  // 🔄
    {"Finder",     "\xF0\x9F\x94\x8D", false},  // 🔍
    {"Heard",      "\xF0\x9F\x91\x82", false},  // 👂
    {"Map",        "\xF0\x9F\x97\xBA", false},  // 🗺
    {"Advertise",  "\xF0\x9F\x93\xA2", false},  // 📢
    {"Settings",   "\xE2\x9A\x99",     false},  // ⚙
    {"Trace",      "\xF0\x9F\x93\x8D", false},  // 📍
    {"Terminal",   "\xF0\x9F\x92\xBB", false},  // 💻
    {"Noise",      "\xF0\x9F\x93\x8A", false},  // 📊
    {"Signal",     "\xF0\x9F\x93\xB6", false},  // 📶
};

static constexpr int GRID_COLS = 3;
static constexpr int GRID_ROWS = 4;
static constexpr int TOP_BAR_H  = 24;
static constexpr int BOT_BAR_H  = 20;
static constexpr int GRID_PAD   = 4;

// ── Top Status Bar ──────────────────────────────────────
static void create_top_bar()
{
    top_bar = lv_obj_create(scr);
    lv_obj_set_size(top_bar, LV_PCT(100), TOP_BAR_H);
    lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(BG_SECONDARY), 0);
    lv_obj_set_style_bg_opa(top_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(top_bar, 0, 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);

    // Menu icon (☰)
    lv_obj_t* menu_icon = lv_label_create(top_bar);
    lv_label_set_text(menu_icon, "\xe2\x98\xb0");  // ☰
    lv_obj_set_style_text_color(menu_icon, lv_color_hex(TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(menu_icon, &lv_font_montserrat_16, 0);
    lv_obj_align(menu_icon, LV_ALIGN_LEFT_MID, 4, 0);

    // Hashtag channels
    lv_obj_t* hashtags = lv_label_create(top_bar);
    lv_label_set_text(hashtags, "#hertford*  #london*  #Jokez");
    lv_obj_set_style_text_color(hashtags, lv_color_hex(CHANNEL_HASH), 0);
    lv_obj_set_style_text_font(hashtags, &lv_font_montserrat_12, 0);
    lv_obj_align(hashtags, LV_ALIGN_LEFT_MID, 26, 0);

    // Time (right-aligned)
    time_label = lv_label_create(top_bar);
    lv_label_set_text(time_label, "14:03");
    lv_obj_set_style_text_color(time_label, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_14, 0);
    lv_obj_align(time_label, LV_ALIGN_RIGHT_MID, -4, 0);
}

// ── Bottom Status Bar ───────────────────────────────────
static void create_bottom_bar()
{
    bottom_bar = lv_obj_create(scr);
    lv_obj_set_size(bottom_bar, LV_PCT(100), BOT_BAR_H);
    lv_obj_align(bottom_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(bottom_bar, lv_color_hex(BG_SECONDARY), 0);
    lv_obj_set_style_bg_opa(bottom_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(bottom_bar, 0, 0);
    lv_obj_set_style_border_width(bottom_bar, 0, 0);

    // Device name
    device_label = lv_label_create(bottom_bar);
    lv_label_set_text(device_label, "TDeck+");
    lv_obj_set_style_text_color(device_label, lv_color_hex(TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(device_label, &lv_font_montserrat_12, 0);
    lv_obj_align(device_label, LV_ALIGN_LEFT_MID, 4, 0);

    // Signal bars (simple text for now)
    lv_obj_t* signal = lv_label_create(bottom_bar);
    lv_label_set_text(signal, "▂▄▆█");
    lv_obj_set_style_text_color(signal, lv_color_hex(ACCENT_GREEN), 0);
    lv_obj_set_style_text_font(signal, &lv_font_montserrat_12, 0);
    lv_obj_align(signal, LV_ALIGN_CENTER, -20, 0);

    // Battery
    batt_label = lv_label_create(bottom_bar);
    lv_label_set_text(batt_label, "85%");
    lv_obj_set_style_text_color(batt_label, lv_color_hex(ACCENT_GREEN), 0);
    lv_obj_set_style_text_font(batt_label, &lv_font_montserrat_12, 0);
    lv_obj_align(batt_label, LV_ALIGN_RIGHT_MID, -4, 0);
}

// ── Create a single icon tile ───────────────────────────
static lv_obj_t* create_icon_tile(lv_obj_t* parent, const IconDef& icon, int idx)
{
    lv_obj_t* tile = lv_obj_create(parent);
    lv_obj_set_style_bg_color(tile, lv_color_hex(BG_TERTIARY), 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(tile, 10, 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_set_style_pad_all(tile, 4, 0);

    // Emoji icon
    lv_obj_t* emoji = lv_label_create(tile);
    lv_label_set_text(emoji, icon.emoji);
    lv_obj_set_style_text_font(emoji, &lv_font_montserrat_24, 0);
    lv_obj_align(emoji, LV_ALIGN_CENTER, 0, -8);

    // Label
    lv_obj_t* label = lv_label_create(tile);
    lv_label_set_text(label, icon.label);
    lv_obj_set_style_text_color(label, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -4);

    // Notification badge (if enabled)
    if (icon.badge) {
        lv_obj_t* badge = lv_obj_create(tile);
        lv_obj_set_size(badge, 10, 10);
        lv_obj_set_style_bg_color(badge, lv_color_hex(ACCENT_RED), 0);
        lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(badge, 0, 0);
        lv_obj_set_style_pad_all(badge, 0, 0);
        lv_obj_align(badge, LV_ALIGN_TOP_RIGHT, -4, 4);
    }

    return tile;
}

// ── 3×4 Icon Grid ───────────────────────────────────────
static void create_icon_grid()
{
    grid = lv_obj_create(scr);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, GRID_PAD, 0);
    lv_obj_set_style_pad_gap(grid, GRID_PAD, 0);
    lv_obj_set_size(grid, LV_PCT(100),
                    LV_VER_RES - TOP_BAR_H - BOT_BAR_H);
    lv_obj_align(grid, LV_ALIGN_TOP_MID, 0, TOP_BAR_H);

    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Calculate tile size: grid has GRID_COLS columns, GRID_ROWS rows
    int grid_w = TFT_WIDTH  - (GRID_PAD * (GRID_COLS + 1));
    int grid_h = (TFT_HEIGHT - TOP_BAR_H - BOT_BAR_H) - (GRID_PAD * (GRID_ROWS + 1));
    int tile_w = grid_w / GRID_COLS;
    int tile_h = grid_h / GRID_ROWS;

    for (int i = 0; i < GRID_COLS * GRID_ROWS; i++) {
        lv_obj_t* tile = create_icon_tile(grid, icons[i], i);
        lv_obj_set_size(tile, tile_w, tile_h);
    }
}

// ── Public API ───────────────────────────────────────────
void home_screen_create()
{
    scr = lv_scr_act();
    apply_dark_bg(scr);

    create_top_bar();
    create_bottom_bar();
    create_icon_grid();
}

} // namespace slopos::ui
