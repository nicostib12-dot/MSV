/*
 * COMMIT 2 - OLED + SENSOR MAX30102
 * Cardio.X | PIC18F4550 @ 8 MHz
 * Objetivo: mostrar BPM usando el MAX30102 y la OLED.
 * PORTB: RB0=SCL, RB1=SDA
 */

#include <xc.h>
#include "config.h"
#include "i2c.h"
#include "oled.h"
#include "max30102.h"
#include "card.h"

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

static void mostrar_bpm(unsigned char bpm, unsigned char hay_dedo) {
    OLED_Clear();

    OLED_SetCursor(0, 0);
    OLED_WriteString("CARDIO.X");

    OLED_SetCursor(0, 2);
    if (hay_dedo)
        OLED_WriteString("DEDO: SI");
    else
        OLED_WriteString("DEDO: NO");

    OLED_SetCursor(0, 4);
    OLED_WriteString("BPM:");

    if (bpm >= 100)
        OLED_WriteChar('0' + (bpm / 100));

    OLED_WriteChar('0' + ((bpm / 10) % 10));
    OLED_WriteChar('0' + (bpm % 10));
}

void main(void) {
    unsigned long red, ir;
    unsigned char bpm = 0;
    unsigned char hay_dedo = 0;
    unsigned char i;

    setup();
    I2C_Init();
    OLED_Init();

    OLED_Clear();
    OLED_SetCursor(0, 0);
    OLED_WriteString("INICIANDO MAX");
    __delay_ms(1000);

    if (!MAX30102_Init()) {
        OLED_Error("MAX ERR");
        while (1);
    }

    OLED_Clear();
    OLED_SetCursor(0, 0);
    OLED_WriteString("MAX30102 OK");
    __delay_ms(1000);

    while (1) {
        for (i = 0; i < 20; i++) {
            if (MAX30102_ReadSample(&red, &ir)) {
                bpm = CAR_ProcesarMuestra(red, ir);
                hay_dedo = CAR_HayDedo(ir);
            }

            __delay_ms(15);
        }

        LED1 = 1;
        LED2 = hay_dedo ? 1 : 0;

        mostrar_bpm(bpm, hay_dedo);
    }
}