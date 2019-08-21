/*
 * OrionSerialMonitor.cpp - Simple Software Serial system monitor for Orion WSPR Beacon
 *
 * Copyright 2019 Michael Babineau, VE3WMB <mbabineau.ve3wmb@gmail.com>
 *                          
 * This sketch  is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * Foobar is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License.
 * If not, see <http://www.gnu.org/licenses/>.
*/
#include <Arduino.h>
#include "OrionSerialMonitor.h"
#include "OrionXConfig.h"
#include "OrionBoardConfig.h"
#include <TimeLib.h>
#define OFF false
#define ON true
#if defined (DEBUG_USES_SW_SERIAL)
  #include <NeoSWSerial.h>
#endif

// This is needed if an application used both PinChangeInterrupts and NeoSWSerial which we do.
// Note that you must modify this to define an ISR for the correct PCINTx_vect 
// according to what pins you are using for SOFT_SERIAL_RX_PIN and SOFT_SERIAL_TX_PIN
//
// Port B (digital pin 8 to 13)
// Port C (all analog input pins)
// Port D (digital pins 0 to 7) 
//
// ISR (PCINT0_vect) handles pin change interrupt for D8 to D13, (Port B, PCINT0 - PCINT7)
// ISR (PCINT1_vect) handles pin change interrupt for A0 to A5, (Port C, PCINT8 - PCINT14)
// ISR (PCINT2_vect) handles pin change interrupt for D0 to D7, (Port D, PCINT16 - PCINT23)

// ISR(PCINT0_vect){}    // Port B, PCINT0 - PCINT7
// ISR(PCINT1_vect){}    // Port C, PCINT8 - PCINT14
// ISR(PCINT2_vect){}    // Port D, PCINT16 - PCINT23
//  PCINT0_vect must call NeoSWSerial::rxISR(PINB);
//  PCINT1_vect must call NeoSWSerial::rxISR(PINC);
//  PCINT2_vect must call NeoSWSerial::rxISR(PIND);
// Also note that you must uncomment the third last line in NeoSWSerial.h
// which is #define NEOSWSERIAL_EXTERNAL_PCINT, othewise you will have linking errors

#if defined (NEOSWSERIAL_EXTERNAL_PCINT)
  ISR (PCINT0_vect)
  {
    // handle pin change interrupt for D8 to D13 here. 
    // Note SOFT_SERIAL_TX_PIN (MOSI) = 11 and SOFT_SERIAL_RX_PIN =  = 12  (MISO - PB4/PCINT4)for U3S Clones
    // 
    NeoSWSerial::rxISR(PINB);
  }  // end of PCINT0_vect

  ISR (PCINT2_vect)
  {
    NeoSWSerial::rxISR(PIND);
  }  // end of PCINT2_vect
#endif

static bool g_debug_on_off = ON; 
static bool g_txlog_on_off = ON;
static bool g_info_log_on_off = ON;
static bool g_qrm_avoidance_on_off = ON;
static bool g_selfcalibration_on_off = ON;
 
#if defined (DEBUG_USES_SW_SERIAL)  
  NeoSWSerial debugSerial(SOFT_SERIAL_RX_PIN, SOFT_SERIAL_TX_PIN);  // RX, TX
#else
  #define debugSerial Serial
#endif

bool is_selfcalibration_on(){
  if (g_selfcalibration_on_off == OFF)
    return false;
  else
    return true;
}

bool is_qrm_avoidance_on(){
  if (g_qrm_avoidance_on_off == OFF)
    return false;
  else
    return true;
}

void print_date_time() {
  debugSerial.print(year());
  debugSerial.print(F("-"));
  debugSerial.print(month());
  debugSerial.print(F("-"));
  debugSerial.print(day());
  debugSerial.print(F(" "));
  debugSerial.print(hour());
  debugSerial.print(F(":"));
  debugSerial.print(minute());
  debugSerial.print(F(":"));
  debugSerial.print(second());
  debugSerial.print(F(" "));

}

bool toggle_on_off(bool flag){
  bool return_flag = ON;
  
  if (flag == ON){
    return_flag = OFF;
    debugSerial.println(F(" OFF"));
  }
  else{
    debugSerial.println(F(" ON"));
  }
  return return_flag;
}

// Log a software error. 
// swerr_num is unique number 1-255 assigned sequentially (should be unique for each call to swerr). 
void swerr(byte swerr_num, int data){
  print_date_time();
  debugSerial.print(F("***SWERR: "));
  debugSerial.print(swerr_num);
  debugSerial.print(F(" data dump in hex: "));
  debugSerial.println(data, HEX);     
}

char *StateNames[] =
{
    "POWER_UP",
    "WAIT_GPS_READY",
    "CALIBRATE",
    "WAIT_TX",
    "WSPR_TX"
};

char *EventNames[] =
{
  "NO_EVENT",
  "GPS_READY",
  "GPS_FAIL",
  "SETUP_DONE",
  "CALIBRATION_DONE",
  "WSPR_TX_TIME",
  "WSPR_TX_DONE",
  "TIMER_EXPIRED"
};

char *ActionNames[] =
{
  "NO_ACTION",
  "DO_GPS_FIX",
  "DO_CALIBRATION",
  "DO_WSPR_TX"
};



void orion_sm_trace_pre(byte state, byte event){
  
  if (g_debug_on_off == OFF) return;
  
  print_date_time();
  debugSerial.print(F(">> orion PRE sm trace: "));
  debugSerial.print(F("curr_state: "));
  debugSerial.print(StateNames[state]);
  debugSerial.print(F(" curr_event: "));
  debugSerial.println(EventNames[event]);
}

void orion_sm_trace_post(byte state, byte processed_event,  byte resulting_action){
  
  if (g_debug_on_off == OFF) return;
  
  print_date_time();
  debugSerial.print(F("<< orion POST sm trace: "));
  debugSerial.print(F("curr_state: "));
  debugSerial.print(StateNames[state]);
  debugSerial.print(F(" event_just_processed: "));
  debugSerial.print(EventNames[processed_event]);
  debugSerial.print(F(" action: "));
  debugSerial.println(ActionNames[resulting_action]);
}

void orion_log_wspr_tx(char call[], char grid[], unsigned long freq_hz, uint8_t pwr_dbm){

  // If either txlog is turned on or info logs are turned on then log the TX
  if ((g_txlog_on_off == OFF) && (g_info_log_on_off == OFF)) return; 
  
  print_date_time();
  debugSerial.print(F("TX:"));
  debugSerial.print(freq_hz);
  debugSerial.print(F(" Call:"));
  debugSerial.print(call);
  debugSerial.print(F(" Locator:"));
  debugSerial.print(grid);
  debugSerial.print(F(" dbm:"));
  debugSerial.println(pwr_dbm);  
}

void orion_log_telemetry (struct OrionTxData *data) {
  
  // If either txlog is turned on or info logs are turned on then log the TX
  if ((g_txlog_on_off == OFF) && (g_info_log_on_off == OFF)) return; 
  
  print_date_time();
  debugSerial.print(F("Telem Grid:"));
  debugSerial.print( (*data).grid_sq_6char );
  debugSerial.print(F(", alt_m:"));
  debugSerial.print( (*data).altitude_m);
  debugSerial.print(F(", spd_kn:"));
  debugSerial.print( (*data).speed_kn);
  debugSerial.print(F(", num_sats:"));
  debugSerial.print( (*data).number_of_sats);
  debugSerial.print(F(", gps_stat:"));
  debugSerial.print( (*data).gps_status);
  //debugSerial.print(F(", gps_fix: "));
  //debugSerial.print( (*data).gps_3d_fix_ok_bool);
  debugSerial.print(F(", batt_v_x10:"));
  debugSerial.print( (*data).battery_voltage_v_x10);
  debugSerial.print(F(", ptemp_c:"));
  debugSerial.print( (*data).processor_temperature_c);
  debugSerial.print(F(", temp_c:"));
  debugSerial.println( (*data).temperature_c);
}

void orion_log(char msg[])
{
  if (g_info_log_on_off == OFF) return;
  print_date_time();
  debugSerial.println(msg);
}

/**********************
/* Serial Monitor code 
/**********************/

void serial_monitor_begin(){
  
  // Start software serial port 
  debugSerial.begin(MONITOR_SERIAL_BAUD);
  while (!debugSerial)
    ;
  debugSerial.flush();
}