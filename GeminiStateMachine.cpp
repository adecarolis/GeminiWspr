/*
   GeminiStateMachine.cpp - Control Logic for the Gemini WSPR Beacon
   This file implements a state machine.

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
#include "GeminiSerialMonitor.h"
#include "GeminiStateMachine.h"
#include "GeminiBoardConfig.h"


GeminiState g_current_gemini_state = POWER_UP;
GeminiState g_previous_gemini_state = POWER_UP;
GeminiEvent g_current_gemini_event = NO_EVENT;
GeminiEvent g_previous_gemini_event = NO_EVENT;


void gemini_sm_no_op () {
  // Placeholder
}

// State Machine Initializationto be called once in setup()
void gemini_sm_begin() {
  // Start out in the initial state
  g_current_gemini_state = POWER_UP;
  g_previous_gemini_state = POWER_UP;
}

GeminiState gemini_sm_get_current_state(){
  return g_current_gemini_state;
}
void gemini_sm_change_state(GeminiState new_state) {
  g_previous_gemini_state = g_current_gemini_state;
  g_current_gemini_state = new_state;
}


// This is the event processor that implements the core of the Gemini State Machine
// It returns an Action of type GeminiAction to trigger work.
GeminiAction gemini_state_machine(GeminiEvent event) {

  GeminiAction next_action = NO_ACTION; // Always default to NO_ACTION
  g_current_gemini_event = event;

  // Pre_sm trace logging
  gemini_sm_trace_pre(g_current_gemini_state, event);

  switch (g_current_gemini_state) {

    case  POWER_UP :
      if (event == SETUP_DONE) {
        gemini_sm_change_state(WAIT_GPS_READY);
        next_action = DO_GPS_FIX;
      }
      else
        swerr(0, event); // This event is not supported in this state       
      break;

    case  WAIT_GPS_READY : //executing setup()
      if (event == GPS_READY) {
        if ((SI5351_SELF_CALIBRATION_SUPPORTED == true) && (is_selfcalibration_on())) {
          // Done setup now do intial calibration
          gemini_sm_change_state(CALIBRATE);
          next_action = DO_CALIBRATION;
        }
        else {
          // If calibration is not suppoerted wait for next TX cycle
          gemini_sm_change_state(WAIT_TX);
        }
      }
      else if (event == TIMER_EXPIRED) {
        gemini_sm_change_state(WAIT_GPS_READY);
        next_action = DO_GPS_FIX;
      }
      else if (event == GPS_FAIL) {
          // trying again getting a fix
          gemini_sm_change_state(WAIT_GPS_READY);
          next_action = DO_GPS_FIX;
      }
      else
        swerr(1, event); // This event is not supported in this state       
      break;

    case  CALIBRATE :  // calibrating Si5351a clock
      if (event == CALIBRATION_DONE) {
        // Done setup now do intial calibration
        gemini_sm_change_state(WAIT_TX);
      }
      else
          swerr(2, event); // This event is not supported in this state
      break;

    case WAIT_TX :  // Waiting for the next WSPR transmission slot
      if (event == WSPR_TX_TIME) {
        // Time to transmit
        gemini_sm_change_state(TX);
        next_action = DO_WSPR_TX;
      }
      else if (event == CW_TX_TIME) {
        gemini_sm_change_state(TX);
        next_action = DO_CW_TX;
      } 
      else if (event == TIMER_EXPIRED) {
        gemini_sm_change_state(WAIT_GPS_READY);
        next_action = DO_GPS_FIX;
      }
      else
        swerr(3, event); // This event is not supported in this state
      break;

    case TX :  // Transmission

      if (event == TX_DONE) {
        gemini_sm_change_state(WAIT_TX);
      } else if (event == TIMER_EXPIRED) {
        gemini_sm_change_state(WAIT_GPS_READY);
        next_action = DO_GPS_FIX;
      }
      else
        swerr(4, event); // This event is not supported in this state
      break;

    default : 
        swerr(5, g_current_gemini_state); // If we end up here it is an error as we have and unimplemented state.
        break;
    

  } // end switch

  g_previous_gemini_event = event;
  g_current_gemini_event = NO_EVENT;

  // Post_sm trace logging
  gemini_sm_trace_post(byte(g_current_gemini_state), event, next_action);

  return next_action;

}
