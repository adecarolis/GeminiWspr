#ifndef GEMINITELEMETRY_H
#define GEMINITELEMETRY_H
/*
   GeminiTelemetry.h - Gemini WSPR Beacon for pico-Balloon payloads for Arduino

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

#if defined (TMP36_TEMP_SENSOR_PRESENT)
int read_TEMP36_temperature();
#endif

#if defined (DS1820_TEMP_SENSOR_PRESENT)
int read_DS1820_temperature();
#endif

#if !defined (DS1820_TEMP_SENSOR_PRESENT) && !defined TMP36_TEMP_SENSOR_PRESENT
int read_processor_temperature();
#endif

int read_voltage_v_x10 ();
uint8_t encode_altitude (int altitude_m);
char encode_temperature (int8_t temperature_c);
int encode_solar_voltage_sats(uint8_t solar_voltage, uint8_t number_of_sats);
char encode_battery_voltage(uint8_t battery_voltage);
#endif