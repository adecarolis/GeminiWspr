#ifndef GEMINISTATEMACHINE_H
#define GEMINISTATEMACHINE_H
/*
   GeminiStateMachine.h - Definitions for control of Gemini WSPR Beacon

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
enum GeminiState {POWER_UP, WAIT_GPS_READY, CALIBRATE, WAIT_TX, TX};
                 
enum GeminiEvent {NO_EVENT, GPS_READY, GPS_FAIL, SETUP_DONE, CALIBRATION_DONE, WSPR_TX_TIME, CW_TX_TIME, TX_DONE, TIMER_EXPIRED};

enum GeminiAction {NO_ACTION, DO_GPS_FIX, DO_CALIBRATION, DO_WSPR_TX, DO_CW_TX}; 
void gemini_sm_begin();

GeminiState gemini_sm_get_current_state();
GeminiAction gemini_state_machine(GeminiEvent event);                                 
#endif
