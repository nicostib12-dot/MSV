/**
 * @file    alarmas.c
 * @brief   Logica de umbrales clinicos y control del buzzer de alarma.
 *
 * El buzzer se controla directamente sobre el pin RD4 (alias BUZZER en config.h).
 * TRISD ya fue configurado como salida por LEDS_Init(), por lo que este
 * modulo solo manipula el valor del latch sin reconfigurar el TRIS.
 *
 * El pulso del buzzer de 200 ms se genera con __delay_ms(200). Durante
 * este tiempo el PIC esta ocupado, pero ocurre solo una vez por segundo
 * y no interfiere con las lecturas I2C que se realizan antes del beep.
 */

#include "alarmas.h"

/* =========================================================
 *  ALARMAS_Init
 * ========================================================= */
void ALARMAS_Init(void) {
    /* TRISD ya configurado como salida por LEDS_Init().
     * Solo asegurar que el buzzer arranca apagado. */
    BUZZER = 0;
}

/* =========================================================
 *  ALARMAS_Evaluar
 * ========================================================= */
unsigned char ALARMAS_Evaluar(unsigned int bpm, unsigned int temp_decimas) {
    unsigned char hay_alarma = 0U;

    /* --- Evaluacion de frecuencia cardiaca ---
     * Solo evaluar si bpm > 0: el valor 0 indica que el MAX30102
     * aun no tiene suficientes datos (primeros 2 segundos), no que
     * el corazon se haya detenido. Evitar falsas alarmas al inicio. */
    if (bpm > 0U) {
        if (bpm < (unsigned int)BPM_MIN || bpm > (unsigned int)BPM_MAX) {
            hay_alarma = 1U;
        }
    }

    /* --- Evaluacion de temperatura ---
     * temp_decimas = 0 tambien podria indicar fallo del sensor.
     * Solo evaluar si el valor es mayor que cero. */
    if (temp_decimas > 0U) {
        if (temp_decimas < (unsigned int)TEMP_MIN_DECIMAS ||
            temp_decimas > (unsigned int)TEMP_MAX_DECIMAS) {
            hay_alarma = 1U;
        }
    }

    /* --- Control del buzzer ---
     * Alarma activa  : pulso de 200 ms ? audible y claro.
     * Sin alarma     : buzzer apagado. */
    if (hay_alarma) {
        BUZZER = 1;
        __delay_ms(200);
        BUZZER = 0;
    } else {
        BUZZER = 0;
    }

    return hay_alarma;
}

/* =========================================================
 *  ALARMAS_Silenciar
 * ========================================================= */
void ALARMAS_Silenciar(void) {
    BUZZER = 0;
}
