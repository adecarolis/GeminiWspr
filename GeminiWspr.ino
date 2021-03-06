// GeminiWspr.ino - Gemini WSPR Beacon for pico-Balloon payloads using Arduino
//
// * Heavily based on OrionWspr by Michael Babineau, VE3WMB - https://github.com/ve3wmb/OrionWspr
// * Copyright 2019 Alain De Carolis, K1FM <alain@alain.it>
//
// Hardware Requirements
// ---------------------
// This firmware must be run on an Arduino or an AVR microcontroller with the Arduino Bootloader installed.
// Pay special attention, the Timer1 Interrupt code is highly dependant on processor clock speed.
//
// I have tried to keep the AVR/Arduino port usage in the implementation configurable via #defines in GeminiBoardConfig.h so that this
// code can more easily be setup to run on different beacon boards such as those designed by DL6OW and N2NXZ and in future
// the QRP Labs U3B. This was also the motivation for moving to a software I2C solution using SoftWire.h. The SoftWire interface
// mimics the Wire library so switching back to using hardware-enable I2C is simple.
//
// Required Libraries
// ------------------
// Etherkit JTEncode (Library Manager)  https://github.com/etherkit/JTEncode
// Time (Library Manager)   https://github.com/PaulStoffregen/Time - This provides a Unix-like System Time capability
// SoftI2CMaster (Software I2C with SoftWire wrapper) https://github.com/felias-fogg/SoftI2CMaster/blob/master/SoftI2CMaster.h - (assumes that you are using Software I2C otherwise Wire.h)
// NeoGps (https://github.com/SlashDevin/NeoGPS) - NMEA and uBlox GPS parser using Nominal Configuration : date, time, lat/lon, altitude, speed, heading,
//  number of satellites, HDOP, GPRMC and GPGGA messages.
// NeoSWSerial (https://github.com/SlashDevin/NeoSWSerial)
// LightChrono (https://github.com/SofaPirate/Chrono) - Simple Chronometer Library
//
// License
// -------
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject
// to the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
// ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
// CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//

#include <JTEncode.h>
#include <int.h>
#include <NMEAGPS.h>  // NeoGps
#include <TimeLib.h>
#include <LightChrono.h>
#include "GeminiXConfig.h"
#include "GeminiBoardConfig.h"
#include "GeminiSi5351.h"
#include "GeminiStateMachine.h"
#include "GeminiSerialMonitor.h"
#include "GeminiCalibration.h"
#include "GeminiTelemetry.h"
#include "GeminiCW.h"

// NOTE THAT ALL #DEFINES THAT ARE INTENDED TO BE USER CONFIGURABLE ARE LOCATED IN GeminiXConfig.h and GeminiBoardConfig.h
// DON'T TOUCH ANYTHING DEFINED IN THIS FILE WITHOUT SOME VERY CAREFUL CONSIDERATION.

// Port Definitions for NeoGps
// We support hardware-based Serial, or software serial communications with the GPS using NeoSWSerial using conditional compilation
// based on the #define GPS_USES_HW_SERIAL. For software serial comment out this #define in GeminiBoardConfig.h
#if defined (GPS_USES_HW_SERIAL)
#define gpsPort Serial
#define GPS_PORT_NAME "Serial"
#else
#include <NeoSWSerial.h>
#include <Streamers.h>
#define GPS_PORT_NAME "NeoSWSerial"
#endif

#define GPS_STATUS_STD 4  //This needs to match the definition in NeoGPS for STATUS_STD 


// WSPR specific defines. DO NOT CHANGE THESE VALUES, EVER!
#define TONE_SPACING            146                 // ~1.46 Hz
#define SYMBOL_COUNT            WSPR_SYMBOL_COUNT

// Globals
JTEncode jtencode;

// GPS related
static NMEAGPS gps;
static gps_fix fix;

//Time related
LightChrono g_chrono, h_chrono;

// If we are using software serial to talk to the GPS then we need to create an instance of NeoSWSerial and
// provide the RX and TX Pin numbers.
#if !defined (GPS_USES_HW_SERIAL)
NeoSWSerial gpsPort(SOFT_SERIAL_RX_PIN, SOFT_SERIAL_TX_PIN);  // RX, TX
#endif

// Allow software reset
void(* resetSoftware)(void) = 0;

// We initialize this to 61 so the first time through the scheduler we can't possibly match the current second
// This forces the scheduler to run on its very first call.
byte g_last_second = 61;

unsigned long g_beacon_freq_hz = FIXED_BEACON_FREQ_HZ;      // The Beacon Frequency in Hz

// Raw Telemetry data types define in GeminiTelemetry.h
// This contains the last validated Raw telemetry for use during GPS LOS (i.e. we use the last valid data if current data is missing)
struct GeminiTelemetryData g_last_valid_telemetry = {0, 0, 0, 0, 0, 0, 0, 0, 0};
struct GeminiTelemetryData g_gemini_current_telemetry {
  0, 0, 0, 0, 0, 0, 0, 0, 0
};

// This differs from GeminiTelemetryData mostly in that the values are reformatted with the correct units (i.e altitude in metres vs cm etc.)
// It is used to populate the global values below prior to encoding the WSPR message for TX
struct GeminiTxData g_tx_data  = {'0', 0, 0, 0, 0, 0, 0, 0};

// The following values are used in the encoding and transmission of the WSPR Type 1 messages.
// They are populated from g_tx_data according to the implemented Telemetry encoding rules.
char g_beacon_callsign[7] = BEACON_CALLSIGN_6CHAR;
char g_grid_loc[5] = BEACON_GRID_SQ_4CHAR; // Grid Square defaults to hardcoded value it is over-written with a value derived from GPS Coordinates
uint8_t g_tx_pwr_dbm = BEACON_TX_PWR_DBM;  // This value is overwritten to encode telemetry data.
uint8_t g_tx_buffer[SYMBOL_COUNT];

// Globals used by the Gemini Scheduler
GeminiAction g_current_action = NO_ACTION;

// Global variables used in ISRs
volatile bool g_proceed = false;

// Timer interrupt vector.  This toggles the variable g_proceed which we use to gate
// each column of output to ensure accurate timing.  This ISR is called whenever
// Timer1 hits the WSPR_CTC value used below in setup().
ISR(TIMER1_COMPA_vect)
{
  g_proceed = true;
}

// ------- Functions ------------------

unsigned long get_tx_frequency() {
  /********************************************************************************
    Get the frequency for the next transmission cycle
    This function also implements the QRM Avoidance feature
    which applies a pseudo-random offset of 0 to 100 hz to the base TX frequency
  ********************************************************************************/
  if (is_qrm_avoidance_on() == false)
    return FIXED_BEACON_FREQ_HZ;
  else {
    // QRM Avoidance - add a random number in range of 0 to 180 to the base TX frequency to help avoid QRM
    return (BEACON_FREQ_HZ + random(BEACON_RANDOM_OFFSET + 1)); // base freq + random offset
  }

}


// -- Telemetry -------

void calculate_gridsquare_6char(float lat, float lon) {
  /***************************************************************************************
    Calculate the 6 Character Maidenhead Gridsquare from the Lat and Long Coordinates
    We follow the convention that West and South are negative Lat/Long.
  ***************************************************************************************/
  // This puts the calculated 6 character Maidenhead Grid square into the field grid_sq_6char[]
  // of g_tx_data

  // Temporary variables for calculation
  int o1, o2, o3;
  int a1, a2, a3;
  float remainder;

  // longitude
  remainder = lon + 180.0;
  o1 = (int)(remainder / 20.0);
  remainder = remainder - (float)o1 * 20.0;
  o2 = (int)(remainder / 2.0);
  remainder = remainder - 2.0 * (float)o2;
  o3 = (int)(12.0 * remainder);

  // latitude
  remainder = lat + 90.0;
  a1 = (int)(remainder / 10.0);
  remainder = remainder - (float)a1 * 10.0;
  a2 = (int)(remainder);
  remainder = remainder - (float)a2;
  a3 = (int)(24.0 * remainder);

  // Generate the 6 character Grid Square
  g_tx_data.grid_sq_6char[0] = (char)o1 + 'A';
  g_tx_data.grid_sq_6char[1] = (char)a1 + 'A';
  g_tx_data.grid_sq_6char[2] = (char)o2 + '0';
  g_tx_data.grid_sq_6char[3] = (char)a2 + '0';
  g_tx_data.grid_sq_6char[4] = (char)o3 + 'A';
  g_tx_data.grid_sq_6char[5] = (char)a3 + 'A';
  g_tx_data.grid_sq_6char[6] = (char)0;
} // calculate_gridsquare_6char


void get_telemetry_data() {

  // GPS Location
  if (fix.valid.location) {
    // We have a valid location so save the lat/long to current_telemetry
    g_gemini_current_telemetry.latitude = fix.latitude();
    g_gemini_current_telemetry.longitude = fix.longitude();

    // Copy the lat/long to last_valid_telemetry so we can use it if our fix is invalid (GPS LOS?) next time.
    g_last_valid_telemetry.latitude = g_gemini_current_telemetry.latitude;
    g_last_valid_telemetry.longitude = g_gemini_current_telemetry.longitude;
  }
  else {
    // Our position fix isn't valid so use the position information from the last_valid_telemetry, not ideal but better than nothing
    g_gemini_current_telemetry.latitude = g_last_valid_telemetry.latitude;
    g_gemini_current_telemetry.longitude =  g_last_valid_telemetry.longitude;
  }

  // GPS Altitude
  if (fix.valid.altitude) {
    g_gemini_current_telemetry.altitude_cm = fix.altitude_cm();
    g_last_valid_telemetry.altitude_cm = g_gemini_current_telemetry.altitude_cm;
  }
  else {
    // Our altitude fix isn't valid so use the last valid data
    g_gemini_current_telemetry.altitude_cm  = g_last_valid_telemetry.altitude_cm;
  }

  // GpS Speed
  if (fix.valid.speed) {
    g_gemini_current_telemetry.speed_mkn = fix.speed_mkn();
    g_last_valid_telemetry.speed_mkn = g_gemini_current_telemetry.speed_mkn;
  }
  else {
    // Our speed fix isn't valid so use the last valid data
    g_gemini_current_telemetry.speed_mkn = g_last_valid_telemetry.speed_mkn;
  }

  // GPS number of sats
  if (fix.valid.satellites) {
    g_gemini_current_telemetry.number_of_sats = fix.satellites;
    g_last_valid_telemetry.number_of_sats = g_gemini_current_telemetry.number_of_sats;
  }
  else {
    g_gemini_current_telemetry.number_of_sats = g_last_valid_telemetry.number_of_sats;
  }

  // Overall GPS status.
  if (fix.valid.status) {
    g_gemini_current_telemetry.gps_status = fix.status;
  }
  else {
    // Assume that if we don't have a valid Status fix that the GPS status is not OK
    g_gemini_current_telemetry.gps_status = 0;

  }

  // Get the remaining non-GPS derived telemetry values.
  // Since these are not reliant on the GPS we assume that we will always be able to get valid values for these.
#if defined (DS1820_TEMP_SENSOR_PRESENT)
  g_gemini_current_telemetry.temperature_c = read_DS1820_temperature();
#elif defined (TMP36_TEMP_SENSOR_PRESENT)
  g_gemini_current_telemetry.temperature_c = read_TEMP36_temperature();
#else
  g_gemini_current_telemetry.temperature_c = read_processor_temperature();
#endif
  g_gemini_current_telemetry.battery_voltage_v_x10 = read_voltage_v_x10();

}

// This function populates g_tx_data from the current_telemetry data, calculates the 6 char grid square and does unit conversions for most of the GPS data
void set_tx_data(uint8_t msg_type) {
  
  byte i;
  // Set the transmit frequency
  g_beacon_freq_hz = get_tx_frequency();

  g_tx_data.altitude_m = g_gemini_current_telemetry.altitude_cm / 100; // convert from cm to metres;
  g_tx_data.speed_kn = g_gemini_current_telemetry.speed_mkn / 1000;   // covert from thousandths of a knot to knots
  g_tx_data.number_of_sats = g_gemini_current_telemetry.number_of_sats;
  g_tx_data.gps_status = g_gemini_current_telemetry.gps_status;
  //g_tx_data.gps_3d_fix_ok_bool = (bool)g_gemini_current_telemetry.gps_status_ok;
  g_tx_data.temperature_c = g_gemini_current_telemetry.temperature_c;
  g_tx_data.processor_temperature_c = g_gemini_current_telemetry.processor_temperature_c;
  g_tx_data.battery_voltage_v_x10 = g_gemini_current_telemetry.battery_voltage_v_x10;

  // Calculate the 6 character grid square and put it into g_tx_data.grid_sq_6char[]
  calculate_gridsquare_6char(g_gemini_current_telemetry.latitude, g_gemini_current_telemetry.longitude);

  // Copy the first four characters of the Grid Square to g_grid_loc[] (valid for all messages)
  for (i = 0; i < 4; i++ ) g_grid_loc[i] = g_tx_data.grid_sq_6char[i];
  g_grid_loc[i] = (char)0; // g_grid_loc[4]
  
  switch (msg_type) {
    case 0 :
     for (i = 0; i < 5; i++ ) {
       g_beacon_callsign[i] = BEACON_CALLSIGN_6CHAR[i];
       if (BEACON_CALLSIGN_6CHAR[i] == '\0') break;
     }
      g_tx_pwr_dbm = encode_altitude(g_tx_data.altitude_m);
      break;
    
    case 1:
      g_beacon_callsign[0] = BEACON_CHANNEL_ID_1;
      g_beacon_callsign[1] = encode_battery_voltage(g_tx_data.battery_voltage_v_x10);
      g_beacon_callsign[2] = BEACON_CHANNEL_ID_2;
      g_beacon_callsign[3] = g_tx_data.grid_sq_6char[4];
      g_beacon_callsign[4] = g_tx_data.grid_sq_6char[5];
      g_beacon_callsign[5] = encode_temperature(g_tx_data.temperature_c);

      g_tx_pwr_dbm = encode_solar_voltage_sats(0, g_tx_data.number_of_sats); // TDB: This can be utilized better
      break;

  }

  gemini_log_telemetry (&g_tx_data);  // Pass a pointer to the g_tx_data structure
}

void prepare_telemetry(uint8_t minute) {
  get_telemetry_data();

  if (minute % 8 == 0) {
    set_tx_data(0);
  } else {
    set_tx_data(1);
  }

} //end prepare_telemetry

void encode_and_tx_cw_msg(uint8_t times) {
  uint8_t i;
  char str[8];
  for (i=0; i<times; i++) {
    send_cw("VVV", 2);
    send_cw("CQ", 1);
    send_cw("DE", 1);

    sprintf(str, "%s/B", BEACON_CALLSIGN_6CHAR);
    send_cw(str, 3);

    send_cw("QTH", 1);
    send_cw(g_tx_data.grid_sq_6char, 2);

    send_cw("QAH", 1);
    sprintf(str, "%dM", g_tx_data.altitude_m);
    send_cw(str, 2);

    send_cw("QMX",1);
    sprintf(str, "%dC", g_tx_data.temperature_c);
    send_cw(str, 2);

    send_cw("BAT",1);
    sprintf(str, "%hdV", g_tx_data.battery_voltage_v_x10) ;
    send_cw(str, 2);

    send_cw("DE", 1);
    sprintf(str, "%s/B", BEACON_CALLSIGN_6CHAR);
    send_cw(str, 3);
    
    send_cw("K  ", 1);
  }
}

void encode_and_tx_wspr_msg() {
  /**************************************************************************
    Transmit the Primary WSPR Message
    Loop through the transmit buffer, transmitting one character at a time.
  * ************************************************************************/
  uint8_t i;

  // Reset the Timer1 interrupt for WSPR transmission
  wspr_tx_interrupt_setup();

  // Encode the primary message paramters into the TX Buffer
  jtencode.wspr_encode(g_beacon_callsign, g_grid_loc, g_tx_pwr_dbm, g_tx_buffer);

  // Reset the tone to 0 and turn on the TX output
  si5351bx_setfreq(SI5351A_WSPRTX_CLK_NUM, (g_beacon_freq_hz * 100ULL));

  // Turn off the PARK clock
  si5351bx_enable_clk(SI5351A_PARK_CLK_NUM, SI5351_CLK_OFF);

  // If we are using the TX LED turn it on
#if defined(TX_LED_PRESENT)
  digitalWrite(TX_LED_PIN, HIGH);
#endif

  // We need to synchronize the 1.46 second Timer/Counter-1 interrupt to the start of WSPR transmission as it is free-running.
  // We reset the counts to zero so we ensure that the first symbol is not truncated (i.e we get a full 1.46 seconds before the interrupt handler sets
  // the g_proceed flag).
  noInterrupts();
  TCNT1 = 0; // Clear the count for Timer/Counter-1
  GTCCR |= (1 << PSRSYNC); // Do a reset on the pre-scaler. Note that we are not using Timer 0, it shares a prescaler so it would also be impacted.
  interrupts();
  // Now send the rest of the message
  for (i = 0; i < SYMBOL_COUNT; i++)
  {
    si5351bx_setfreq(SI5351A_WSPRTX_CLK_NUM, (g_beacon_freq_hz * 100ULL) + (g_tx_buffer[i] * TONE_SPACING));
    g_proceed = false;

    // We spin our wheels in TX here, waiting until the Timer1 Interrupt sets the g_proceed flag
    // Then we can go back to the top of the for loop to start sending the next symbol
    while (!g_proceed);
  }

  // Turn off the WSPR TX clock output, we are done sending the message
  si5351bx_enable_clk(SI5351A_WSPRTX_CLK_NUM, SI5351_CLK_OFF);

  // Re-enable the Park Clock
  si5351bx_setfreq(SI5351A_PARK_CLK_NUM, (PARK_FREQ_HZ * 100ULL)); // Turn on Park Clock


  // If we are using the TX LED turn it off
#if defined (TX_LED_PRESENT)
  digitalWrite(TX_LED_PIN, LOW);
#endif

  delay(1000); // Delay one second
} // end of encode_and_tx_wspr_msg()

uint8_t gps_fix() {
  unsigned long start, partial = h_chrono.elapsed();
  // Status,UTC Date/Time,Lat,Lon,Hdg,Spd,Alt,Sats,Rx ok,Rx err,Rx chars,
  while (1) {
    while (gps.available( gpsPort )) {
      fix = gps.read();
      if (fix.valid.location) {
        setTime(fix.dateTime.hours,
                fix.dateTime.minutes,
                fix.dateTime.seconds,
                fix.dateTime.date,
                fix.dateTime.month, 
                fix.dateTime.year);
          return 0;
          break;
      }
    }
    if (h_chrono.elapsed() - partial > 5000) { // do something every 5 seconds
      trace_all( Serial, gps, fix );
      partial = h_chrono.elapsed();
    }
    if (h_chrono.elapsed() - start > 1200000) { // no fix in 20 minutes
      h_chrono.restart();
      return 1;
      break;
    }
  }
}

GeminiAction process_gemini_sm_action (GeminiAction action) {
  /****************************************************************************************************
    This is where all of the work gets triggered by processing Actions returned by the state machine.
    We don't need to worry about state we just do what we are told.
  ****************************************************************************************************/
  GeminiAction returned_action = NO_ACTION;
  uint8_t result;

  switch (action) {


    case NO_ACTION :
      returned_action = NO_ACTION;
      break;

    case DO_GPS_FIX :
      result = gps_fix();
      if (result == 0) {
        returned_action = gemini_state_machine(GPS_READY);
      } else {
        returned_action = gemini_state_machine(GPS_FAIL);
      }
      break;

    case DO_CALIBRATION :

      // Initialize Interrupts for Initial calibration
      setup_calibration();

      //TODO This should be modified with a boolean return code so we can handle calibration fail.
      do_calibration(COARSE_CORRECTION_STEP); // Initial calibration with 1 Hz correction step

      returned_action = gemini_state_machine(CALIBRATION_DONE);
      break;
    
    case DO_CW_TX :
      prepare_telemetry(0);
      gemini_log_wspr_tx(g_beacon_callsign, g_grid_loc, g_beacon_freq_hz, g_tx_pwr_dbm);
      encode_and_tx_cw_msg(2);
      returned_action = gemini_state_machine(TX_DONE);
      break;

    case DO_WSPR_TX :

      // Encode the 5th and 6th Maidenhead Grid characters into the pwr/dBm field
      // g_tx_pwr_dbm = encode_gridloc_char5_char6();
      // Note that this is actually done as part of the Telemetry gathering so no need to redo it.
      // It is the same story for :
      //  g_beacon_freq_hz = get_tx_frequency(); .. this is already covered for the Primary Msg in the Telemetry Phase. 

      // Encode and transmit the Primary WSPR Message
      prepare_telemetry(minute());
      // g_tx_pwr_dbm = encode_altitude(g_tx_data.altitude_m);
      // g_tx_pwr_dbm = encode_voltage(g_tx_data.battery_voltage_v_x10); 
// #if defined (DS1820_TEMP_SENSOR_PRESENT) | defined (TMP36_TEMP_SENSOR_PRESENT )
//       g_tx_pwr_dbm = encode_temperature(g_tx_data.temperature_c); // Use Sensor data
// #else
//       g_tx_pwr_dbm = encode_temperature(g_tx_data.processor_temperature_c); // Use internal processor temperature
// #endif
      gemini_log_wspr_tx(g_beacon_callsign, g_grid_loc, g_beacon_freq_hz, g_tx_pwr_dbm); // If TX Logging is enabled then ouput a log
      encode_and_tx_wspr_msg();

      // Tell the Gemini state machine that we are done tranmitting the Primary WSPR message and update the current_action
      returned_action = gemini_state_machine(TX_DONE);
      break;

    default :
      returned_action = NO_ACTION;
      break;
  } // end switch (action)
  return returned_action;
} //  process_gemini_sm_action


GeminiAction gemini_scheduler() {
  /*********************************************************************
    This is the scheduler code that determines the Gemini Beacon schedule
  **********************************************************************/
  byte Second; // The current second
  byte Minute; // The current minute
  byte i;
  GeminiAction returned_action = NO_ACTION;

  if (timeStatus() == timeSet) { // We have valid time from the GPS otherwise do nothing

    // The scheduler will get called many times per second but we only want it to run once per second,
    // otherwise we might generate multiples of the same time event on these extra passes through the scheduler.
    Second = second();

    // If we are on the same second as the last time we went through here then we bail-out with NO_ACTION returned.
    // This prevents us from sending time events more than once as our time resolution is only one second but we might
    // make it back here in less than 1 second.
    if (Second == g_last_second) return returned_action;

    // If the current second is different from the last time, then it has been updated so we run.

    g_last_second = Second; // Remember what second we are currently on for the next time the scheduler is called

    Minute = minute();

    switch (Minute) {
      case 0 :
      case 30 :
        if (Second == 0) {
          return (gemini_state_machine(CW_TX_TIME));
        }
        break;

      default :
        if (Minute % 2 == 0 && Second == 1) {
          return (gemini_state_machine(WSPR_TX_TIME));
        }
        break;

    } // end switch (Minute)

  } // end if timestatus == timeset

  return returned_action;
} // end gemini_scheduler()

void wspr_tx_interrupt_setup() {

  // Set up Timer1 for interrupts every symbol period (i.e 1.46 Hz)
  // The formula to calculate this is CPU_CLOCK_SPEED_HZ / (PRESCALE_VALUE) x (WSPR_CTC + 1)
  // In this case we are using a prescale of 1024.
  noInterrupts();          // Turn off interrupts.
  TCCR1A = 0;              // Set entire TCCR1A register to 0; disconnects
  //   interrupt output pins, sets normal waveform
  //   mode.  We're just using Timer1 as a counter.
  TCNT1  = 0;              // Initialize counter value to 0.
  TCCR1B = (1 << CS12) |   // Set CS12 and CS10 bit to set prescale
           (1 << CS10) |   //   to /1024
           (1 << WGM12);   //   turn on CTC
  //   which gives, 64 us ticks
  TIMSK1 = (1 << OCIE1A);  // Enable timer compare interrupt.

  // Note the the OCR1A value is processor clock speed dependant.
  // WSPR_CTC must be defined as 5336 for an 8Mhz processor clock or 10672 for a 16 Mhz Clock
  OCR1A = WSPR_CTC;       // Set up interrupt trigger count.

  interrupts();            // Re-enable interrupts.
}

void setup() {

  // Ensure that GPS is powered up if power disable feature supported
#if defined(GPS_POWER_DISABLE_SUPPORTED)
  pinMode(GPS_POWER_DISABLE_PIN, OUTPUT);
  digitalWrite(GPS_POWER_DISABLE_PIN, LOW);
#endif

  // Ensure that Si5351a is powered up if power disable feature supported
#if defined(SI5351_POWER_DISABLE_SUPPORTED)
  pinMode(TX_POWER_DISABLE_PIN, OUTPUT);
  digitalWrite(TX_POWER_DISABLE_PIN, LOW);
#endif

  // Use the TX_LED_PIN as a transmit indicator if it is present
#if defined (TX_LED_PRESENT)
  pinMode(TX_LED_PIN, OUTPUT);
  digitalWrite(TX_LED_PIN, LOW);
#endif

  // Use the SYNC_LED_PIN to indicate the beacon has valid time from the GPS, but only if it present
#if defined (SYNC_LED_PRESENT)
  pinMode(SYNC_LED_PIN, OUTPUT);
  digitalWrite(SYNC_LED_PIN, LOW);
#endif

  pinMode(CAL_FREQ_IN_PIN, INPUT); // This is the frequency input must be D5 to use Timer1 as a counter
  pinMode(GPS_PPS_PIN, INPUT);

  // Start Hardware serial communications with the GPS
  gpsPort.begin(GPS_SERIAL_BAUD);

  // Initialize the Si5351
  si5351bx_init();

  // Setup WSPR TX output
  si5351bx_setfreq(SI5351A_WSPRTX_CLK_NUM, (g_beacon_freq_hz * 100ULL));
  si5351bx_enable_clk(SI5351A_WSPRTX_CLK_NUM, SI5351_CLK_OFF); // Disable the TX clock initially

  // Set PARK CLK Output - Note that we leave SI5351A_PARK_CLK_NUM running at 108 Mhz to keep the SI5351 temperature more constant
  // This minimizes thermal induced drift during WSPR transmissions. The idea is borrowed from G0UPL's PARK feature on the QRP Labs U3S
  si5351bx_setfreq(SI5351A_PARK_CLK_NUM, (PARK_FREQ_HZ * 100ULL)); // Turn on Park Clock

  // Setup the software serial port for the serial monitor interface
  serial_monitor_begin();

#if defined (GPS_BALLOON_MODE_COMMAND)
  gps.send_P( &gpsPort, (const __FlashStringHelper *) GPS_BALLOON_MODE_COMMAND );
  delay( 250 );
#endif

  // Set the intial state for the Gemini Beacon State Machine
  gemini_sm_begin();

  // Read unused analog pin (not connected) to generate a random seed for QRM avoidance feature
  randomSeed(analogRead(ANALOG_PIN_FOR_RNG_SEED));

  // Start the Chronos
  g_chrono.start();
  h_chrono.start();

  // Tell the state machine that we are done SETUP
  char str[8];
  sprintf(str, "%s/B", BEACON_CALLSIGN_6CHAR);
  send_cw(str, 2);

  g_current_action = gemini_state_machine(SETUP_DONE);

} // end setup()


void loop() {
  
  if (h_chrono.hasPassed(TIME_SET_INTERVAL_MS, true)) {
    gps_fix();
    // For logging only:
    get_telemetry_data();
    set_tx_data(0);
  }
  
  // This triggers actual work when the state machine returns an GeminiAction
  while (g_current_action != NO_ACTION) {
    g_current_action = process_gemini_sm_action(g_current_action);
  }
  
  if (g_chrono.hasPassed(CALIBRATION_INTERVAL, true)) { // When the time set interval has passed, restart the Chronometer set system time again from GPS
    g_current_action = gemini_state_machine(TIMER_EXPIRED);
#if defined (SYNC_LED_PRESENT)
if (timeStatus() == timeSet)
  digitalWrite(SYNC_LED_PIN, HIGH); // Turn LED on if the time is synced
else
  digitalWrite(SYNC_LED_PIN, LOW); // Turn LED off
#endif
  } else {
    // Call the scheduler to determine if it is time for any action
    g_current_action = gemini_scheduler();
  }
} // end loop ()
