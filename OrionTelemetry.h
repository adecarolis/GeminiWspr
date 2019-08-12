#ifndef ORIONTELEMETRY_H
#define ORIONTELEMETRY_H
/*
   OrionTelemetry.h - Orion WSPR Beacon for pico-Balloon payloads for Arduino

   Copyright (C) 2018-2019 Michael Babineau <mbabineau.ve3wmb@gmail.com>

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
int read_voltage_v_x10 ();

uint8_t encode_altitude (int altitude_m);
uint8_t encode_temperature (int temperature_c);

#if defined (TMP36_TEMP_SENSOR_PRESENT)
int read_TEMP36_temperature();
#endif

#if defined (DS1820_TEMP_SENSOR_PRESENT)
int read_DS1820_temperature();
#endif

#if !defined (DS1820_TEMP_SENSOR_PRESENT) && !defined TMP36_TEMP_SENSOR_PRESENT
int read_processor_temperature();
#endif

#endif