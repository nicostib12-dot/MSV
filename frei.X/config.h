#ifndef CONFIG_H
#define CONFIG_H

#include <xc.h>

#define _XTAL_FREQ 8000000

#pragma config FOSC   = INTOSCIO_EC
#pragma config WDT    = OFF
#pragma config LVP    = OFF
#pragma config PBADEN = OFF
#pragma config MCLRE  = ON

#define CALENTADOR  LATDbits.LATD0
#define LED_TIRA    LATDbits.LATD2
#define ALARMA      LATDbits.LATD5
#define LED1        LATDbits.LATD0
#define LED2        LATDbits.LATD1
#define LED3        LATDbits.LATD2
#define LED4        LATDbits.LATD3
#define LED5        LATDbits.LATD4

#endif /* CONFIG_H */