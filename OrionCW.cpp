#include "OrionCW.h"
#include "OrionSi5351.h"
#include "OrionBoardConfig.h"
#include "OrionXConfig.h"
#include <int.h>

/*
    OrionCW.cpp - Tiny CW add-on for Orion WSPR

    Based on si5351_beacon - https://github.com/la3pna/si5351_beacon
    by LA3PNA
*/

#if defined (SI5351A_USES_SOFTWARE_I2C)
  #include <SoftWire.h>  // Needed for Software I2C otherwise include <Wire.h>
#else
  #include <Wire.h> 
#endif

long tonetime = 32000; // Here you set the continuous tone time.

struct t_mtab {
  char c, pat;
} ;

char morsetab[] = {
  63, // 0
  62, // 1
  60, // 2
  56, // 3
  48, // 4
  32, // 5
  33, // 6
  35, // 7
  39, // 8
  47, // 9
  6,  // A
  17, // B
  21, // C
  9,  // D
  2,  // E
  20, // F 
  11, // G
  16, // H 
  4,  // I
  30, // J
  13, // K
  18, // L 
  7,  // M
  5,  // N
  15, // O
  22, // P 
  27, // Q
  10, // R
  8,  // S
  3,  // T
  12, // U
  24, // V
  14, // W
  25, // X
  29, // Y
  19, // Z
  115,// ,
  106,// .
  76, // ?
  41  // '/'
};

#define N_MORSE  (sizeof(morsetab)/sizeof(morsetab[0]))

#define SPEED  (30)
#define DOTLEN  (1200/SPEED)
#define DASHLEN  (3*(1200/SPEED))

void send(uint16_t len)
{
  si5351bx_setfreq(SI5351A_WSPRTX_CLK_NUM, (CW_BEACON_FREQ_HZ * 100ULL));
  delay(len);
  si5351bx_setfreq(SI5351A_WSPRTX_CLK_NUM, 0);
  delay(DOTLEN) ;
}

void
send_char(char c)
{
  int i ;
  if (c == ' ') {
    Serial.print(' ') ;
    delay(7 * DOTLEN) ;
    return ;
  } else {
    i = int(c);
    if (i >= 48 and i <= 57) // 0-9
      i -= 48;
    if (i >=65 and i <= 90) // A-Z
      i -= 55;
    else if (i>= 97 and i <= 122) // a-z
      i -= 87;
    else if (i == 44) // ,
      i = 36;
    else if (i == 46) // .
      i = 37;
    else if (i == 63) // ?
      i = 38;
    else if (i == 47) // '/'
      i = 39;
    Serial.print(c) ;
    unsigned char p = morsetab[i] ;
    while (p != 1) {
      if (p & 1)
        send(DASHLEN) ;
      else
        send(DOTLEN) ;
      p = p / 2 ;
    }
    delay(2 * DOTLEN) ;
    return ;
  }
}

void send_cw(char *str, uint8_t times)
{
  uint8_t i;
  char *string;

  for (i=0; i<times; i++) {
    char *string = str;
    while (*string)
      send_char(*string++) ;
    send_char(' ');
  } 
}