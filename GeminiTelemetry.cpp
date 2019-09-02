/*
   GeminiTelemetry.cpp - Telemetry Data gathering, formatting and encoding

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
#include <int.h>
#include "GeminiXConfig.h"
#include "GeminiBoardConfig.h"

#if defined (DS1820_TEMP_SENSOR_PRESENT)
  #include <OneWire.h>
  #include <DallasTemperature.h>
#endif

#if defined (DS1820_TEMP_SENSOR_PRESENT)
OneWire oneWire(ONE_WIRE_BUS);               // Setup a oneWire instance to communicate with any OneWire devices
DallasTemperature sensors(&oneWire);        // Pass our oneWire reference to Dallas Temperature.

// Read temperature in C from Dallas DS1820 temperature sensor
int read_DS1820_temperature() {

  sensors.requestTemperatures();

  // After we have the temperatures, we use the function ByIndex, and in this case only the temperature from the first sensor
  // The temperature is in degrees C, returned as a signed int.
  return ((int)(sensors.getTempCByIndex(0)));
}
#endif // DS1820_TEMP_SENSOR_PRESENT

#if defined (TMP36_TEMP_SENSOR_PRESENT)
int read_TEMP36_temperature() {
  return (double)analogRead(TMP36_PIN) / 1024 * 330 - 50;
}
#endif

int read_voltage_v_x10() {

  int AdcCount, i, voltage_v_x10;
  float Vpower, sum;

  sum = 0;

  // Read the voltage 10 times so we can calculate an average
  for (i = 0; i < 10; i++) {

    // Arduino 10bit ADC; 3.3v external AREF each count of 1 = 0.00322265625V
    AdcCount = analogRead(Vpwerbus); // read Vpwerbus
    Vpower = AdcCount * 0.00322 * VpwerDivider;
    sum = sum + Vpower ;
  }

  Vpower = sum / 10.0; // Calculate the average of the 10 voltage samples

  // Shift the voltage one decimal place to the left and convert to an int
  voltage_v_x10 = (int) (Vpower * 10); // i.e. This converts 3.3333 volt reading to 33 representing 3.3 v

  return voltage_v_x10;
}

#if !defined (DS1820_TEMP_SENSOR_PRESENT) && !defined (TMP36_TEMP_SENSOR_PRESENT)
int read_processor_temperature() {

  unsigned int wADC;
  double temp_c;

  // The internal temperature has to be used
  // with the internal reference of 1.1V.
  // Channel 8 can not be selected with
  // the analogRead function yet.

  // Set the internal reference and mux.
  ADMUX = (_BV(REFS1) | _BV(REFS0) | _BV(MUX3));
  ADCSRA |= _BV(ADEN);  // enable the ADC

  delay(20);            // wait for voltages to become stable.

  ADCSRA |= _BV(ADSC);  // Start the ADC

  // Detect end-of-conversion
  while (bit_is_set(ADCSRA, ADSC));

  // Reading register "ADCW" takes care of how to read ADCL and ADCH.
  wADC = ADCW;

  // The offset of 324.31 could be wrong. It is just an indication.
  temp_c = (wADC - 324.31) / 1.22;

  // The returned temperature is in degrees Celsius.
  return (((int)temp_c));
}
#endif

uint8_t encode_altitude (int altitude_m) {
  uint8_t ret_value = 0;
 
  switch (altitude_m) { 
    
    case 0 ... 999 :
      ret_value = 0;
      break;

    case 1000 ... 1999 :
      ret_value = 3;
      break;

    case 2000 ... 2999 :
      ret_value = 7;
      break;

    case 3000 ... 3999 :
      ret_value = 10;
      break;

    case 4000 ... 4999 :
      ret_value = 13;
      break;

    case 5000 ... 5999 :
      ret_value = 17;
      break;

    case 6000 ... 6999 :
      ret_value = 20;
      break;

    case 7000 ... 7999 :
      ret_value = 23;
      break;

    case 8000 ... 8999 :
      ret_value = 27;
      break;

    case 9000 ... 9999 :
      ret_value = 30;
      break;

    case 10000 ... 10999 :
      ret_value = 33;
      break;

    case 11000 ... 11999 :
      ret_value = 37;
      break;

    case 12000 ... 12999 :
      ret_value = 40;
      break;

    case 13000 ... 13999 :
      ret_value = 43;
      break;

    case 14000 ... 14999 :
      ret_value = 47;
      break;

    case 15000 ... 15999 :
      ret_value = 50;
      break;

    case 16000 ... 16999 :
      ret_value = 53;
      break;

    case 17000 ... 17999 :
      ret_value = 57;
      break;

    default : //  >= 18000 metres
      ret_value = 60;
      break;
  }
  return ret_value;
}

int encode_solar_voltage_sats(uint8_t solar_voltage, uint8_t number_of_sats) {
  // No solar voltage, for now
  switch (number_of_sats) {
    case 0 ... 3 :
      return 0;
      break;
    case 4 ... 7 :
      return 1;
      break;
    case 8 ... INT8_MAX :
      return 2;
      break;
  }
}

char encode_battery_voltage(uint8_t battery_voltage) {
  uint8_t encoded_voltage, encoded_altitude_fine;

  switch (battery_voltage) {
    case 0 ... 30 :
      encoded_voltage = 0;
      break;
    case 31 ... 32 :
      encoded_voltage = 1;
      break;
    case 33 ... 34 :
      encoded_voltage = 2;
      break;
    case 35 ... 36 :
      encoded_voltage = 3;
      break;
    case 37 ... 38 :
      encoded_voltage = 4;
      break;
    case 39 ... 40 :
      encoded_voltage = 5;
      break;
    case 41 ... 42 :
      encoded_voltage = 6;
      break;
    case 43 ... 44 :
      encoded_voltage = 7;
      break;
    case 45 ... 46 :
      encoded_voltage = 8;
      break;
    case 47 ... 48 :
      encoded_voltage = 9;
      break;
    case 49 ... 50 :
      encoded_voltage = 10;
      break;
    case 51 ... INT8_MAX :
      encoded_voltage = 11;
      break;
  }
  return encoded_voltage + 'A';
}

char encode_temperature (int8_t temperature_c) {
  char ret_value;

  switch ( temperature_c ){
  
    case INT8_MIN ... -35:
      ret_value = (char)0;
      break;

    case -34 ... -30:
      ret_value = (char)1;
      break;

    case -29 ... -25:
      ret_value = (char)2;
      break;

    case -24 ... -20:
      ret_value = (char)3;
      break;

    case -19 ... -15:
      ret_value = (char)4;
      break;

    case -14 ... -10:
      ret_value = (char)5;
      break;

    case -9 ... -5:
      ret_value = (char)6;
      break;

    case -4 ... 0:
      ret_value = (char)7;
      break;

    case 1 ... 5:
      ret_value = (char)8;
      break;
    
    default:
      ret_value = (char)9;
    break;
  }
  return ret_value + 'A';
}