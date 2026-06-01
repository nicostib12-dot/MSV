/*
 * main.c  -  Cardio.X  |  PIC18F4550 @ 8 MHz
 *
 * PORTD:  RD0=LED verde  RD1=LED azul  RD2=LED amarillo  RD3=LED rojo
 *         RD6=DS18B20 OneWire
 * PORTB:  RB0=SCL  RB1=SDA  RB2=BUZZER  RB3=BTN perfil
 * PORTC:  RC6=TX UART (automatico)  RC7=RX UART (automatico)
 */

#include <xc.h>
#include "config.h"
#include "i2c.h"
#include "oled.h"
#include "ds18b20.h"
#include "max30102.h"
#include "card.h"
#include "uart.h"

#define BUZZER              LATBbits.LATB2
#define BTN_PERFIL          PORTBbits.RB3

#define TEMP_UMBRAL_CUERPO  30.0f

#define BPM_MIN_NORMAL       50
#define BPM_MAX_NORMAL      120
#define TEMP_MAX_NORMAL      38.5f

#define BPM_MIN_DEPORTISTA   40
#define BPM_MAX_DEPORTISTA  180
#define TEMP_MAX_DEPORTISTA  39.0f

#define BUZZER_TOGGLE_CICLOS  7

/* Enviar UART cada N ciclos (~4.5 s con 15 ciclos de 300ms) */
#define UART_CADA_CICLOS     15

/* ------------------------------------------------------------------ */
static void setup(void) {
    OSCCON = 0x72;
    while (!OSCCONbits.IOFS);
    __delay_ms(100);

    TRISD = 0x40;    /* RD6 entrada (DS18B20), resto salidas */
    LATD  = 0x00;

    TRISB = 0xF8;    /* RB0,RB1,RB2 salidas | RB3-RB7 entradas */
    LATB  = 0x00;

    /* RC6 y RC7 los configura UART_Init() */

    INTCON2bits.RBPU = 0;
    CMCON  = 0x07;
    ADCON1 = 0x0F;
}

static void blink_amarillo(unsigned char n) {
    unsigned char k;
    for (k = 0; k < n; k++) {
        LED3 = 1; __delay_ms(200);
        LED3 = 0; __delay_ms(200);
    }
}

static void mostrar_perfil(unsigned char deportista) {
    OLED_Clear();
    OLED_SetCursor(10, 2);
    if (deportista)
        OLED_WriteString("PERFIL: DEPORTISTA");
    else
        OLED_WriteString("PERFIL: NORMAL");
    __delay_ms(1000);
    OLED_Clear();
}

/* ------------------------------------------------------------------ */
void main(void) {
    unsigned long red, ir;
    unsigned char bpm           = 0;
    unsigned char hay_dedo      = 0;
    unsigned char hay_temp      = 0;
    unsigned char activo        = 0;
    unsigned char alarma        = 0;
    unsigned char i;

    unsigned char ciclo_oled    = 0;
    unsigned char ciclo_temp    = 0;
    unsigned char ciclo_buzzer  = 0;
    unsigned char ciclo_uart    = 0;
    unsigned char buzzer_estado = 0;

    unsigned char perfil        = 0;
    unsigned char btn_ant       = 1;

    float         temp_f        = 0.0f;
    unsigned char temp_int      = 0;
    unsigned char temp_dec      = 0;
    unsigned char temp_valida   = 0;

    setup();
    I2C_Init();
    UART_Init();      /* <-- inicializar UART antes del OLED */
    OLED_Init();

    UART_WriteString("=== CARDIO.X INICIANDO ===\r\n");

    blink_amarillo(3);
    OLED_Splash();
    __delay_ms(2000);

    /* ---- DS18B20 ---- */
    LED3 = 1;
    if (!DS18B20_Reset()) {
        OLED_Error("DS18 ERR");
        UART_WriteString("ERR: DS18B20 no encontrado\r\n");
        __delay_ms(2000);
    } else {
        DS18B20_StartConversion();
        DS18B20_WaitConversion();
        temp_f = DS18B20_ReadTemp();

        if (temp_f > -55.0f && temp_f < 125.0f) {
            temp_valida = 1;
            temp_int = (unsigned char)temp_f;
            temp_dec = (unsigned char)((temp_f - (float)temp_int) * 10.0f);
            hay_temp = (temp_f >= TEMP_UMBRAL_CUERPO) ? 1 : 0;
            UART_WriteString("DS18B20 OK\r\n");
        } else {
            if      (temp_f == -997.0f) { OLED_Error("POR 85C"); UART_WriteString("ERR: POR 85C\r\n"); }
            else if (temp_f == -998.0f) { OLED_Error("RAW ERR"); UART_WriteString("ERR: RAW\r\n"); }
            else                        { OLED_Error("NO SENS"); UART_WriteString("ERR: SIN SENSOR\r\n"); }
            __delay_ms(2000);
        }
        DS18B20_StartConversion();
    }
    LED3 = 0;

    /* ---- MAX30102 ---- */
    if (!MAX30102_Init()) {
        OLED_Error("MAX ERR");
        UART_WriteString("ERR: MAX30102 no encontrado\r\n");
        while (1);
    }
    UART_WriteString("MAX30102 OK\r\n");
    UART_WriteString("=== SISTEMA LISTO ===\r\n");

    LED1 = 1;
    OLED_Clear();

    /* ==== Loop principal ==== */
    while (1) {

        /* Boton perfil */
        {
            unsigned char btn_ahora = BTN_PERFIL;
            if (btn_ant == 1 && btn_ahora == 0) {
                perfil ^= 1;
                mostrar_perfil(perfil);

                if (perfil == 0)
                    UART_WriteString("PERFIL: NORMAL\r\n");
                else
                    UART_WriteString("PERFIL: DEPORTISTA\r\n");
            }
            btn_ant = btn_ahora;
        }

        /* 20 muestras MAX30102 (~300 ms) */
        for (i = 0; i < 20; i++) {
            if (MAX30102_ReadSample(&red, &ir)) {
                bpm      = CAR_ProcesarMuestra(red, ir);
                hay_dedo = CAR_HayDedo(ir);
            }
            __delay_ms(15);
        }

        /* Temperatura cada ~25 ciclos */
        ciclo_temp++;
        if (ciclo_temp >= 25) {
            ciclo_temp = 0;
            temp_f = DS18B20_ReadTemp();
            if (temp_f > -55.0f && temp_f < 125.0f) {
                temp_valida = 1;
                temp_int = (unsigned char)temp_f;
                temp_dec = (unsigned char)((temp_f - (float)temp_int) * 10.0f);
                hay_temp = (temp_f >= TEMP_UMBRAL_CUERPO) ? 1 : 0;
            }
            DS18B20_StartConversion();
        }

        activo = (hay_dedo || hay_temp) ? 1 : 0;

        /* Evaluar alarma */
        alarma = 0;
        if (activo && bpm > 0) {
            if (perfil == 0) {
                if (bpm < BPM_MIN_NORMAL || bpm > BPM_MAX_NORMAL) alarma = 1;
                if (temp_valida && temp_f > TEMP_MAX_NORMAL)       alarma = 1;
            } else {
                if (bpm < BPM_MIN_DEPORTISTA || bpm > BPM_MAX_DEPORTISTA) alarma = 1;
                if (temp_valida && temp_f > TEMP_MAX_DEPORTISTA)           alarma = 1;
            }
        }

        /* LEDs */
        LED1 = 1;
        LED2 = activo  ? 1 : 0;
        LED3 = 0;
        LED4 = alarma  ? 1 : 0;

        /* Buzzer */
        if (alarma) {
            ciclo_buzzer++;
            if (ciclo_buzzer >= BUZZER_TOGGLE_CICLOS) {
                ciclo_buzzer  = 0;
                buzzer_estado ^= 1;
                BUZZER = buzzer_estado;
            }
        } else {
            BUZZER = 0;
            buzzer_estado = 0;
            ciclo_buzzer = 0;
        }

        /* OLED cada 2 ciclos */
        ciclo_oled++;
        if (ciclo_oled >= 2) {
            ciclo_oled = 0;
            if (!activo)
                OLED_Paused();
            else
                OLED_Vitals(temp_int, temp_dec, bpm, alarma, perfil);
        }

        /* UART cada ~15 ciclos */
        ciclo_uart++;
        if (ciclo_uart >= UART_CADA_CICLOS) {
            ciclo_uart = 0;
            if (activo)
                UART_SendVitals(temp_int, temp_dec, bpm, perfil, alarma);
            else
                UART_WriteString("ESTADO: EN PAUSA\r\n");
        }
    }
}