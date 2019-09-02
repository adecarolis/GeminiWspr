#ifndef GEMINICW_H
#define GEMINICW_H
#include <Arduino.h>

/*
    GeminiCW.h - Tiny CW add-on for Gemini WSPR

    Based on si5351_beacon - https://github.com/la3pna/si5351_beacon
    by LA3PNA
*/

void send(uint16_t len);                    // Send a CW note of length len (mS)
void send_char(char c);                     // Send a single character
void send_cw(char *str, uint8_t times);     // Send a string with an appended space at the end

 #endif