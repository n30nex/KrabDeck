// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// This file is part of SlopOS-TDeck.
//
// SlopOS-TDeck is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// SlopOS-TDeck is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with SlopOS-TDeck.  If not, see <https://www.gnu.org/licenses/>.


#include "gps.h"
#include "tdeck_pins.h"
#include <Arduino.h>
#include <cstring>
#include <cstdlib>

// ── GPS state ────────────────────────────────────────────
static struct GPSData {
    float    latitude;
    float    longitude;
    float    altitude_m;
    float    speed_kn;
    float    heading;
    uint8_t  satellites;
    uint8_t  fix_quality;
    uint8_t  hour, minute, second;
    bool     has_fix;
    bool     initialized;
} gps;

static char nmea_buf[128];
static int  nmea_pos = 0;

// ── Helpers ───────────────────────────────────────────────
static float nmea_to_decimal(const char* coord, char dir) {
    if (!coord || coord[0] == '\0') return 0.0f;
    float val = strtof(coord, nullptr);
    int degrees = (int)(val / 100.0f);
    float minutes = val - (degrees * 100.0f);
    float decimal = degrees + minutes / 60.0f;
    if (dir == 'S' || dir == 'W') decimal = -decimal;
    return decimal;
}

static void parse_gga(const char* sentence) {
    // $GPGGA,hhmmss.ss,lat,N,lon,E,q,sat,hdop,alt,M,...
    char buf[128];
    strncpy(buf, sentence, sizeof(buf) - 1);
    char* token = strtok(buf, ",");

    // Skip header
    token = strtok(nullptr, ",");

    // Time
    token = strtok(nullptr, ",");
    if (token && strlen(token) >= 6) {
        char h[3] = {token[0], token[1], 0};
        char m[3] = {token[2], token[3], 0};
        char s[3] = {token[4], token[5], 0};
        gps.hour   = atoi(h);
        gps.minute = atoi(m);
        gps.second = atoi(s);
    }

    // Latitude + N/S
    char* lat_str = strtok(nullptr, ",");
    char* ns = strtok(nullptr, ",");
    if (lat_str && ns) gps.latitude = nmea_to_decimal(lat_str, ns[0]);

    // Longitude + E/W
    char* lon_str = strtok(nullptr, ",");
    char* ew = strtok(nullptr, ",");
    if (lon_str && ew) gps.longitude = nmea_to_decimal(lon_str, ew[0]);

    // Fix quality
    token = strtok(nullptr, ",");
    if (token) gps.fix_quality = atoi(token);

    // Satellites
    token = strtok(nullptr, ",");
    if (token) gps.satellites = atoi(token);

    // Skip HDOP
    token = strtok(nullptr, ",");

    // Altitude
    token = strtok(nullptr, ",");
    if (token) gps.altitude_m = strtof(token, nullptr);

    gps.has_fix = (gps.fix_quality > 0);
}

static void parse_rmc(const char* sentence) {
    // $GPRMC,...speed,heading...
    char buf[128];
    strncpy(buf, sentence, sizeof(buf) - 1);
    char* token = strtok(buf, ",");

    // Fields 1-6: skip (time, status, lat, NS, lon, EW)
    // Field 7: speed (knots) — 7 skips from header (field 0)
    for (int i = 0; i < 7; i++) token = strtok(nullptr, ",");

    if (token) gps.speed_kn = strtof(token, nullptr);

    // Field 8: heading (degrees)
    token = strtok(nullptr, ",");
    if (token) gps.heading = strtof(token, nullptr);
}

static void process_nmea(const char* sentence) {
    if (strncmp(sentence, "$GPGGA,", 7) == 0) {
        parse_gga(sentence);
    } else if (strncmp(sentence, "$GPRMC,", 7) == 0) {
        parse_rmc(sentence);
    }
}

// ════════════════════════════════════════════════════════
// PUBLIC API
// ════════════════════════════════════════════════════════

void slopos_gps_init() {
    Serial1.begin(GPS_BAUD_RATE);
    memset(&gps, 0, sizeof(gps));
    nmea_pos = 0;
    gps.initialized = true;
}

void slopos_gps_loop() {
    if (!gps.initialized) return;

    while (Serial1.available()) {
        char c = Serial1.read();

        if (c == '\n') {
            // End of NMEA sentence
            if (nmea_pos > 0) {
                nmea_buf[nmea_pos] = '\0';
                process_nmea(nmea_buf);
                nmea_pos = 0;
            }
        } else if (c == '\r') {
            // Ignore carriage return
        } else if (c == '$' && nmea_pos > 0) {
            // New sentence starting before previous ended — flush
            nmea_pos = 0;
            nmea_buf[nmea_pos++] = c;
        } else if (nmea_pos < (int)(sizeof(nmea_buf) - 1)) {
            nmea_buf[nmea_pos++] = c;
        }
    }
}

float    slopos_gps_latitude()     { return gps.latitude; }
float    slopos_gps_longitude()    { return gps.longitude; }
float    slopos_gps_altitude_m()   { return gps.altitude_m; }
float    slopos_gps_speed_kn()     { return gps.speed_kn; }
float    slopos_gps_heading()      { return gps.heading; }
uint8_t  slopos_gps_satellites()   { return gps.satellites; }
uint8_t  slopos_gps_fix_quality()  { return gps.fix_quality; }
bool     slopos_gps_has_fix()      { return gps.has_fix; }
uint8_t  slopos_gps_hour()         { return gps.hour; }
uint8_t  slopos_gps_minute()       { return gps.minute; }
uint8_t  slopos_gps_second()       { return gps.second; }
