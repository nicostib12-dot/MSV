/**
 * @file    alarmas.h
 * @brief   Logica de umbrales clinicos y control del buzzer de alarma.
 *
 * Compara las mediciones actuales contra los umbrales definidos en config.h
 * y activa o desactiva el buzzer (RD4) segun corresponda.
 *
 * Umbrales (definidos en config.h):
 *  - BPM_MIN = 60  / BPM_MAX = 100    (bradicardia / taquicardia)
 *  - TEMP_MIN_DECIMAS = 360 (36.0 C)  / TEMP_MAX_DECIMAS = 375 (37.5 C)
 *
 * Patron del buzzer:
 *  - Alarma activa   : beep continuo de 200 ms cada segundo.
 *  - Sin alarma      : buzzer apagado.
 *
 * El modulo no gestiona los LEDs ? eso es responsabilidad de leds.h.
 * Solo reporta si hay alarma activa para que main.c coordine el estado.
 */

#ifndef ALARMAS_H
#define ALARMAS_H

#include "config.h"

/* =========================================================
 *  PROTOTIPOS PUBLICOS
 * ========================================================= */

/**
 * @brief  Inicializa el pin del buzzer como salida y lo apaga.
 *         Debe llamarse una vez al inicio tras LEDS_Init() (que configura
 *         TRISD completo ? este modulo solo asegura que el buzzer este apagado).
 */
void ALARMAS_Init(void);

/**
 * @brief  Evalua los valores de BPM y temperatura contra los umbrales
 *         clinicos y controla el buzzer en consecuencia.
 *
 * Si alguno de los valores esta fuera de rango, activa el buzzer con un
 * pulso de 200 ms y retorna 1. Si ambos estan en rango, apaga el buzzer
 * y retorna 0.
 *
 * Llamar desde el bucle principal una vez por segundo junto con la
 * actualizacion de LEDs.
 *
 * @note   Si bpm == 0 (sensor sin datos aun), no se evalua el umbral
 *         de frecuencia cardiaca para evitar falsas alarmas al inicio.
 *
 * @param  bpm           Frecuencia cardiaca medida (latidos por minuto).
 * @param  temp_decimas  Temperatura medida en decimas de grado (365 = 36.5 C).
 * @return 1 si hay alarma activa (algun valor fuera de rango).
 * @return 0 si todos los valores estan dentro del rango normal.
 */
unsigned char ALARMAS_Evaluar(unsigned int bpm, unsigned int temp_decimas);

/**
 * @brief  Apaga el buzzer inmediatamente sin evaluar umbrales.
 *         Util al entrar en modo pausa para silenciar la alarma.
 */
void ALARMAS_Silenciar(void);

#endif /* ALARMAS_H */