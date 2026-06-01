/*
 * COMMIT 3 - OLED + MAX30102 + TEMPERATURA DS18B20
 * Cardio.X | PIC18F4550 @ 8 MHz
 * Objetivo: mostrar BPM y temperatura en OLED.
 * PORTD: RD6=DS18B20 OneWire
 * PORTB: RB0=SCL, RB1=SDA
 */

#include <xc.h>
#include "config.h"
#include "i2c.h"
#include "oled.h"
#include "ds18b20.h"
#include "max30102.h"
#include "card.h"

#define TEMP_UMBRAL_CUERPO  30.0f

static void setup(void) {
    OSCCON = 0x72;
    while (!OSCCONbits.IOFS);
    __delay_ms(100);

    TRISD = 0x40;    /* RD6 entrada para DS18B20 */
    LATD  = 0x00;

    TRISB = 0xF8;    /* RB0,RB1,RB2 salidas | RB3-RB7 entradas */
    LATB  = 0x00;

    INTCON2bits.RBPU = 0;
    CMCON  = 0x07;
    ADCON1 = 0x0F;
}

void main(void) {
    unsigned long red, ir;
    unsigned char bpm = 0;
    unsigned char hay_dedo = 0;
    unsigned char hay_temp = 0;
    unsigned char activo = 0;
    unsigned char alarma = 0;
    unsigned char i;
    unsigned char ciclo_temp = 0;

    float temp_f = 0.0f;
    unsigned char temp_int = 0;
    unsigned char temp_dec = 0;

    setup();
    I2C_Init();
    OLED_Init();

    OLED_Splash();
    __delay_ms(1500);

    LED3 = 1;
    if (!DS18B20_Reset()) {
        OLED_Error("DS18 ERR");
        __delay_ms(2000);
    } else {
        DS18B20_StartConversion();
        DS18B20_WaitConversion();
        temp_f = DS18B20_ReadTemp();

        if (temp_f > -55.0f && temp_f < 125.0f) {
            temp_int = (unsigned char)temp_f;
            temp_dec = (unsigned char)((temp_f - (float)temp_int) * 10.0f);
            hay_temp = (temp_f >= TEMP_UMBRAL_CUERPO) ? 1 : 0;
        }

        DS18B20_StartConversion();
    }
    LED3 = 0;

    if (!MAX30102_Init()) {
        OLED_Error("MAX ERR");
        while (1);
    }

    OLED_Clear();
    LED1 = 1;

    while (1) {
        for (i = 0; i < 20; i++) {
            if (MAX30102_ReadSample(&red, &ir)) {
                bpm = CAR_ProcesarMuestra(red, ir);
                hay_dedo = CAR_HayDedo(ir);
            }

            __delay_ms(15);
        }

        ciclo_temp++;

        if (ciclo_temp >= 25) {
            ciclo_temp = 0;

            temp_f = DS18B20_ReadTemp();

            if (temp_f > -55.0f && temp_f < 125.0f) {
                temp_int = (unsigned char)temp_f;
                temp_dec = (unsigned char)((temp_f - (float)temp_int) * 10.0f);
                hay_temp = (temp_f >= TEMP_UMBRAL_CUERPO) ? 1 : 0;
            }

            DS18B20_StartConversion();
        }

        activo = (hay_dedo || hay_temp) ? 1 : 0;
        alarma = 0;

        LED1 = 1;
        LED2 = activo ? 1 : 0;
        LED3 = 0;
        LED4 = alarma ? 1 : 0;

        if (!activo)
            OLED_Paused();
        else
            OLED_Vitals(temp_int, temp_dec, bpm, alarma, 0);
    }
}