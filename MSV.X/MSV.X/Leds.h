/**
 * @file    leds.h
 * @brief   Maquina de estados para los 4 LEDs de notificacion del sistema.
 *
 * Cada estado del sistema corresponde a un patron de LEDs especifico.
 * Solo un LED (ademas del LED de encendido) esta activo a la vez,
 * lo que hace el estado del sistema visible de un vistazo.
 *
 * Estados disponibles:
 *
 *  ESTADO_INIT       : Sistema arrancando. Solo LED_ENCENDIDO activo.
 *
 *  ESTADO_PREPARANDO : Inicializando sensores y bus I2C.
 *                      LED_ENCENDIDO + LED_PREPARANDO activos.
 *
 *  ESTADO_EN_ESPERA  : Midiendo, valores dentro del rango normal.
 *                      LED_ENCENDIDO + LED_EN_ESPERA activos.
 *
 *  ESTADO_ALARMA     : Umbral superado (BPM o temperatura fuera de rango).
 *                      LED_ENCENDIDO + LED_FUNCIONAL activos.
 *                      LED_FUNCIONAL parpadea cada 500 ms para mayor visibilidad.
 *
 *  ESTADO_PAUSA      : Sistema pausado por pulsacion larga del boton.
 *                      LED_ENCENDIDO activo. LED_EN_ESPERA parpadea lentamente.
 *
 * Mapa de pines (definido en config.h):
 *  RD0 -> LED_ENCENDIDO  (verde)
 *  RD1 -> LED_PREPARANDO (amarillo)
 *  RD2 -> LED_EN_ESPERA  (azul)
 *  RD3 -> LED_FUNCIONAL  (rojo)
 */

#ifndef LEDS_H
#define LEDS_H

#include "config.h"

/* =========================================================
 *  ESTADOS DEL SISTEMA
 * ========================================================= */

typedef enum {
    ESTADO_INIT = 0,   /**< Arranque inicial del sistema              */
    ESTADO_PREPARANDO, /**< Inicializando perifericos y sensores       */
    ESTADO_EN_ESPERA,  /**< Funcionando, valores en rango normal       */
    ESTADO_ALARMA,     /**< Umbral superado ? alarma activa            */
    ESTADO_PAUSA       /**< Sistema pausado por el usuario             */
} EstadoSistema;

/* =========================================================
 *  PROTOTIPOS PUBLICOS
 * ========================================================= */

/**
 * @brief  Inicializa PORTD como salida digital y apaga todos los LEDs.
 *         Debe llamarse una vez al inicio antes de LEDS_SetEstado().
 */
void LEDS_Init(void);

/**
 * @brief  Aplica el patron de LEDs correspondiente al estado indicado.
 *         En estados con parpadeo (ALARMA, PAUSA), alterna el LED
 *         correspondiente cada vez que se llama. Diseñado para llamarse
 *         desde el bucle principal una vez por segundo.
 * @param  estado  Estado del sistema a representar (@see EstadoSistema).
 */
void LEDS_SetEstado(EstadoSistema estado);

/**
 * @brief  Retorna el estado actual del sistema registrado en el modulo.
 * @return Ultimo estado configurado con LEDS_SetEstado().
 */
EstadoSistema LEDS_GetEstado(void);

#endif /* LEDS_H */