
/*
 * ds18b20.c  -  Driver OneWire para DS18B20 en PIC18F4550
 *
 * Pin RD6 con pull-up externo 4.7k a VDD (alimentacion externa):
 *   OW_DIR = 0  -> salida  (PIC domina el bus)
 *   OW_DIR = 1  -> entrada (pull-up sube el bus a VDD)
 *   OW_LOW = 0  -> nivel bajo
 *   OW_LOW = 1  -> nivel alto en LAT (irrelevante cuando DIR=1)
 *
 * REGLA DE ORO: siempre setear OW_LOW ANTES de cambiar OW_DIR
 * para evitar glitches de salida al cambiar de direccion.
 */

#include "config.h"
#include "ds18b20.h"

/* ------------------------------------------------------------------ */
/* Liberar bus (dejar en reposo)                                        */
/* ------------------------------------------------------------------ */
static void OW_Release(void) {
    OW_LOW = 1;    /* LAT alto primero */
    OW_DIR = 1;    /* luego entrada -> pull-up sube el bus */
}

/* ------------------------------------------------------------------ */
/* Reset / Presencia                                                    */
/* Retorna 1 si el sensor respondio, 0 si no hay sensor               */
/* ------------------------------------------------------------------ */
unsigned char DS18B20_Reset(void) {
    unsigned char presence;

    /* Asegurar bus libre antes del reset */
    OW_Release();
    __delay_us(10);

    /* El bus debe estar en ALTO (pull-up). Si esta en bajo = problema */
    if (OW_PIN == 0) return 0;

    /* Pulso de reset: jalar a GND por 500 us */
    OW_LOW = 0;    /* preparar LAT bajo */
    OW_DIR = 0;    /* salida: fuerza GND */
    __delay_us(500);

    /* Liberar: el pull-up sube el bus */
    OW_Release();
    __delay_us(70);    /* esperar ventana de presencia (60-240 us) */

    presence = (OW_PIN == 0);    /* sensor jala a GND = presencia */

    __delay_us(430);   /* completar slot de reset (total ~1000 us) */

    return presence;
}

/* ------------------------------------------------------------------ */
/* Escribir un bit                                                      */
/* ------------------------------------------------------------------ */
static void OW_WriteBit(unsigned char bit) {
    if (bit) {
        /* Bit '1': jalar 2 us, soltar -> pull-up mantiene alto */
        OW_LOW = 0; OW_DIR = 0;
        __delay_us(2);
        OW_Release();            /* soltar: sensor lee '1' */
        __delay_us(62);
    } else {
        /* Bit '0': mantener bajo todo el slot */
        OW_LOW = 0; OW_DIR = 0;
        __delay_us(64);
        OW_Release();
        __delay_us(2);           /* recuperacion entre slots */
    }
}

/* ------------------------------------------------------------------ */
/* Leer un bit                                                          */
/* ------------------------------------------------------------------ */
static unsigned char OW_ReadBit(void) {
    unsigned char bit;
    OW_LOW = 0; OW_DIR = 0;    /* iniciar slot */
    __delay_us(2);
    OW_Release();               /* soltar: sensor coloca su dato */
    __delay_us(10);             /* muestrear antes de los 15 us */
    bit = OW_PIN;
    __delay_us(52);             /* completar slot de 64 us */
    return bit;
}

/* ------------------------------------------------------------------ */
/* Escribir/Leer un byte completo                                       */
/* ------------------------------------------------------------------ */
void DS18B20_WriteByte(unsigned char byte) {
    unsigned char i;
    for (i = 0; i < 8; i++) {
        OW_WriteBit(byte & 0x01);
        byte >>= 1;
    }
}

unsigned char DS18B20_ReadByte(void) {
    unsigned char i, data = 0;
    for (i = 0; i < 8; i++) {
        data >>= 1;
        if (OW_ReadBit()) data |= 0x80;
    }
    return data;
}

/* ------------------------------------------------------------------ */
/* Iniciar conversion de temperatura (no bloqueante)                   */
/* ------------------------------------------------------------------ */
unsigned char DS18B20_StartConversion(void) {
    if (!DS18B20_Reset()) return 0;
    DS18B20_WriteByte(0xCC);    /* Skip ROM  (un solo sensor) */
    DS18B20_WriteByte(0x44);    /* Convert T                  */
    return 1;
}

/* ------------------------------------------------------------------ */
/* Esperar conversion: delay fijo 800 ms                               */
/* Con alimentacion EXTERNA no se puede hacer polling del pin porque   */
/* el pull-up mantiene el bus en ALTO durante toda la conversion.      */
/* El DS18B20 en 12 bits tarda maximo 750 ms -> usamos 800 ms.        */
/* ------------------------------------------------------------------ */
void DS18B20_WaitConversion(void) {
    __delay_ms(100); __delay_ms(100);
    __delay_ms(100); __delay_ms(100);
    __delay_ms(100); __delay_ms(100);
    __delay_ms(100); __delay_ms(100);
}

/* ------------------------------------------------------------------ */
/* Leer temperatura en grados Celsius                                   */
/* Llamar DESPUES de WaitConversion() o >= 800 ms post StartConversion */
/*                                                                      */
/* Retorna:                                                             */
/*   Temperatura real  (-55.0 a +125.0 grados)                        */
/*   -999.0  sin sensor (reset fallo)                                  */
/*   -998.0  bytes invalidos (0x0000 o 0xFFFF)                        */
/*   -997.0  valor de power-on reset (0x0550 = 85.0) -> muy rapido    */
/* ------------------------------------------------------------------ */
float DS18B20_ReadTemp(void) {
    unsigned char lsb, msb;
    int raw;

    if (!DS18B20_Reset()) return -999.0f;

    DS18B20_WriteByte(0xCC);    /* Skip ROM        */
    DS18B20_WriteByte(0xBE);    /* Read Scratchpad */

    lsb = DS18B20_ReadByte();
    msb = DS18B20_ReadByte();

    if (lsb == 0xFF && msb == 0xFF) return -998.0f;  /* bus flotante */
    if (lsb == 0x00 && msb == 0x00) return -998.0f;  /* sin dato     */
    if (lsb == 0x50 && msb == 0x05) return -997.0f;  /* POR = 85 C   */

    raw = (int)((unsigned int)((msb << 8) | lsb));
    return (float)raw / 16.0f;
}