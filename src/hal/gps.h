#pragma once
#include <cstdint>

// Initialize GPS module on Serial1 at 38400 baud
void slopos_gps_init();

// Call each frame to read and parse incoming NMEA data
void slopos_gps_loop();

// Current GPS fix data
float    slopos_gps_latitude();
float    slopos_gps_longitude();
float    slopos_gps_altitude_m();
float    slopos_gps_speed_kn();
float    slopos_gps_heading();
uint8_t  slopos_gps_satellites();
uint8_t  slopos_gps_fix_quality(); // 0=none, 1=GPS, 2=DGPS, 4=RTK
bool     slopos_gps_has_fix();

// Time from GPS (UTC)
uint8_t slopos_gps_hour();
uint8_t slopos_gps_minute();
uint8_t slopos_gps_second();
