/*
   OrionStateMachine.cpp - Control Logic for the Orion WSPR Beacon
   This file implements a state machine.

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
#include "OrionSerialMonitor.h"
#include "OrionStateMachine.h"
#include "OrionBoardConfig.h"


OrionState g_current_orion_state = POWER_UP;
OrionState g_previous_orion_state = POWER_UP;
OrionEvent g_current_orion_event = NO_EVENT;
OrionEvent g_previous_orion_event = NO_EVENT;


void orion_sm_no_op () {
  // Placeholder
}

// State Machine Initializationto be called once in setup()
void orion_sm_begin() {
  // Start out in the initial state
  g_current_orion_state = POWER_UP;
  g_previous_orion_state = POWER_UP;
}

OrionState orion_sm_get_current_state(){
  return g_current_orion_state;
}
void orion_sm_change_state(OrionState new_state) {
  g_previous_orion_state = g_current_orion_state;
  g_current_orion_state = new_state;
}


// This is the event processor that implements the core of the Orion State Machine
// It returns an Action of type OrionAction to trigger work.
OrionAction orion_state_machine(OrionEvent event) {

  OrionAction next_action = NO_ACTION; // Always default to NO_ACTION
  g_current_orion_event = event;

  // Pre_sm trace logging
  orion_sm_trace_pre(g_current_orion_state, event);

  switch (g_current_orion_state) {

    case  POWER_UP :
      if (event == SETUP_DONE) {
        orion_sm_change_state(WAIT_GPS_READY);
        next_action = DO_GPS_FIX;
      }
      else
        swerr(0, event); // This event is not supported in this state       
      break;

    case  WAIT_GPS_READY : //executing setup()
      if (event == GPS_READY) {
        if ((SI5351_SELF_CALIBRATION_SUPPORTED == true) && (is_selfcalibration_on())) {
          // Done setup now do intial calibration
          orion_sm_change_state(CALIBRATE);
          next_action = DO_CALIBRATION;
        }
        else {
          // If we don't support self Calibration then skip to telemetry
          orion_sm_change_state(WAIT_TX);
        }
      }
      else if (event == TIMER_EXPIRED) {
        orion_sm_change_state(WAIT_GPS_READY);
        next_action = DO_GPS_FIX;
      }
      else
        swerr(1, event); // This event is not supported in this state       
      break;

    case  CALIBRATE :  // calibrating Si5351a clock
      if (event == CALIBRATION_DONE) {
        // Done setup now do intial calibration
        orion_sm_change_state(WAIT_TX);
      }
      else
          swerr(2, event); // This event is not supported in this state
      break;

    case WAIT_TX :  // Waiting for the next WSPR transmission slot
      if (event == WSPR_TX_TIME) {
        // Time to transmit
        orion_sm_change_state(WSPR_TX);
        next_action = DO_WSPR_TX;
      } else if (event == TIMER_EXPIRED) {
        orion_sm_change_state(WAIT_GPS_READY);
        next_action = DO_GPS_FIX;
      }
      else
        swerr(3, event); // This event is not supported in this state
      break;

    case WSPR_TX :  // WSPR Transmission

      if (event == WSPR_TX_DONE) { // Time to send Primary WSPR MSG
        orion_sm_change_state(WAIT_TX);
      } else if (event == TIMER_EXPIRED) {
        orion_sm_change_state(WAIT_GPS_READY);
        next_action = DO_GPS_FIX;
      }
      else
        swerr(4, event); // This event is not supported in this state
      break;

    default : 
        swerr(5, g_current_orion_state); // If we end up here it is an error as we have and unimplemented state.
        break;
    

  } // end switch

  g_previous_orion_event = event;
  g_current_orion_event = NO_EVENT;

  // Post_sm trace logging
  orion_sm_trace_post(byte(g_current_orion_state), event, next_action);

  return next_action;

}
