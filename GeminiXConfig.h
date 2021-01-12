#ifndef GEMINIXCONFIG_H
#define GEMINIXCONFIG_H
/*
   GeminiXConfig.h - Gemini WSPR Beacon for pico-Balloon payloads for Arduino

   Heavily based on OrionWspr by Michael Babineau, VE3WMB - https://github.com/ve3wmb/OrionWspr
   Copyright 2019 Alain De Carolis, K1FM <alain@alain.it>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <Arduino.h>

// THIS FILE CONTAINS THE USER MODIFIABLE #DEFINES TO CONFIGURE THE GEMINI WSPR BEACON

#define GEMINI_FW_VERSION "v0.17x"  // Whole numbers are for released versions. (i.e. 1.0, 2.0 etc.)
                                  // Numbers to the right of the decimal are allocated consecutively, one per GITHUB submission.(i.e. 0.01, 0.02 etc)
                                  // a = alpha b=beta, r=release
                                  
/***********************************************************
   USER SPECIFIED PARAMETERS FOR WSPR
 ***********************************************************/
//#define BEACON_FREQ_HZ          10140110UL     // Base Beacon Frequency In Hz for 30m. 

#define BEACON_FREQ_HZ            14097010UL    //  Base Frequency In Hz for use when QRM Avoidance is enabled. Actual Tx frequency is BEACON_FREQ_HZ + a random offset.
#define BEACON_RANDOM_OFFSET      100           //  Beacon Frequency offset used when QRM Avoidance is enabled.
#define FIXED_BEACON_FREQ_HZ      14097070UL    //  Beacon Frequency In Hz for use when QRM Avoidance is disabled.
#define PARK_FREQ_HZ              108000000ULL  //  Use this on clk SI5351A_PARK_CLK_NUM to keep the SI5351a warm to avoid thermal drift during WSPR transmissions. Max 109 Mhz.
#define CW_BEACON_FREQ_HZ         14099000UL    //  CW Beacon Frequency

// Configuration parameters for Primary WSPR Message (i.e. Callsign, 4 character grid square and power out in dBm)
#define BEACON_CALLSIGN_6CHAR   "MYCALL"        // Your beacon Callsign, maximum of 6 characters
#define BEACON_GRID_SQ_4CHAR    "FN30"        // Your hardcoded 4 character Grid Square - this will be overwritten with GPS derived Grid
#define BEACON_TX_PWR_DBM          7          // Beacon Power Output in dBm (5mW = 7dBm)
#define BEACON_CHANNEL_ID_1     'Q'
#define BEACON_CHANNEL_ID_2     '9'  

// This defines how often we reset the Arduino Clock to the current GPS time 
#define TIME_SET_INTERVAL_MS   30000           // 30,000 ms   = 30 seconds
#define CALIBRATION_INTERVAL   1200000         // 1,200,000 ms  = 20 minutes

// Type Definitions

struct GeminiTelemetryData {
  float latitude;
  float longitude;
  int32_t altitude_cm;
  uint32_t speed_mkn;
  int temperature_c;
  int processor_temperature_c;
  uint8_t battery_voltage_v_x10;
  uint8_t number_of_sats;
  uint8_t gps_status; 
};

struct GeminiTxData {
  char grid_sq_6char[7]; // 6 Character Grid Square calculated from GPS Lat/Long values.
  int32_t altitude_m; 
  uint32_t speed_kn; 
  int temperature_c;
  int processor_temperature_c;
  uint8_t number_of_sats;
  uint8_t gps_status;
  uint8_t battery_voltage_v_x10;
 // bool gps_3d_fix_ok_bool;
};


#endif
