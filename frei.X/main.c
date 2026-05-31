
/*
 * COMMIT 1 - OLED
 * Cardio.X | PIC18F4550 @ 8 MHz
 * Objetivo: dejar funcionando la pantalla OLED por I2C.
 * PORTB: RB0=SCL, RB1=SDA
 */

#include <xc.h>
#include "config.h"
#include "i2c.h"
#include "oled.h"

static void setup(void) {
    OSCCON = 0x72;
    while (!OSCCONbits.IOFS);
    __delay_ms(100);

    TRISD = 0x00;
    LATD  = 0x00;

    TRISB = 0xF8;    /* RB0,RB1,RB2 salidas | RB3-RB7 entradas */
    LATB  = 0x00;

    INTCON2bits.RBPU = 0;
    CMCON  = 0x07;
    ADCON1 = 0x0F;
}

void main(void) {
    setup();
    I2C_Init();
    OLED_Init();

    OLED_Clear();
    OLED_SetCursor(0, 0);
    OLED_WriteString("CARDIO.X");
    OLED_SetCursor(0, 2);
    OLED_WriteString("OLED OK");
    OLED_SetCursor(0, 4);
    OLED_WriteString("PIC18F4550");

    LED1 = 1;

    while (1) {
        LED3 = 1;
        __delay_ms(300);
        LED3 = 0;
        __delay_ms(300);
    }
}