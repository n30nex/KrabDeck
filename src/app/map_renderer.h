#pragma once
#include <cstdint>

// Initialize the map renderer with LVGL parent object
// Call after LVGL is initialized and SD card is mounted
void slopos_map_init();

// Set the map viewport center (lat/lon) and zoom level
void slopos_map_set_view(double lat, double lon, int zoom);

// Get current view state
double slopos_map_get_lat();
double slopos_map_get_lon();
int    slopos_map_get_zoom();

// Pan the map by screen pixel deltas
void slopos_map_pan(int dx, int dy);

// Zoom in/out by one level
void slopos_map_zoom_in();
void slopos_map_zoom_out();

// Render the current map view to the LVGL canvas
// Call from display loop
void slopos_map_render();

// Check if map tiles are available on SD card
bool slopos_map_tiles_available();

// Convert screen pixel to lat/lon
void slopos_map_pixel_to_latlon(int px, int py, double* out_lat, double* out_lon);
