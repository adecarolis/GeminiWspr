#ifndef GEMINICALIBRATION_H
#define GEMINICALIBRATION_H
/*
   GeminiCalibration.h - Definitions for Gemini SI5351 Self Calibration

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
#include "GeminiXConfig.h"

#define FINE_CORRECTION_STEP   10     // 0.1 HZ step
#define COARSE_CORRECTION_STEP 100    // 10 Hz step

void setup_calibration();
void reset_for_calibration();
void do_calibration(unsigned long calibration_step);
#endif
