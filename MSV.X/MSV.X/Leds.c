/**
 * @file    leds.c
 * @brief   Maquina de estados para los 4 LEDs de notificacion del sistema.
 *
 * El parpadeo en ESTADO_ALARMA y ESTADO_PAUSA se implementa con una
 * variable estatica de toggle que se invierte en cada llamada a
 * LEDS_SetEstado(). Como el bucle principal llama a esta funcion una
 * vez por segundo, el parpadeo resultante es de 0.5 Hz (1 ciclo/2s),
 * suficientemente visible sin distraer.
 */

#include "leds.h"

/* Estado actual del sistema, inicializado en INIT */
static EstadoSistema estado_actual = ESTADO_INIT;

/* Variable de toggle para efectos de parpadeo */
static unsigned char toggle = 0U;

/* =========================================================
 *  FUNCION INTERNA: apagar todos los LEDs de estado
 *  (no apaga LED_ENCENDIDO que siempre permanece encendido)
 * ========================================================= */
static void LEDS_ApagarTodos(void) {
    LED_PREPARANDO = 0;
    LED_EN_ESPERA  = 0;
    LED_FUNCIONAL  = 0;
}

/* =========================================================
 *  LEDS_Init
 * ========================================================= */
void LEDS_Init(void) {
    /* Configurar PORTD como salida digital en su totalidad.
     * Los 4 LEDs usan RD0-RD3 y el buzzer RD4.
     * Se configura todo el puerto para evitar pines flotantes. */
    TRISD = 0x00;   /* Todo PORTD como salida */
    LATD  = 0x00;   /* Apagar todos al inicio  */

    estado_actual = ESTADO_INIT;
    toggle        = 0U;
}

/* =========================================================
 *  LEDS_SetEstado
 * ========================================================= */
void LEDS_SetEstado(EstadoSistema estado) {
    estado_actual = estado;
    toggle ^= 1U;   /* Invertir toggle en cada llamada para parpadeo */

    /* LED_ENCENDIDO siempre activo: indica que el sistema tiene alimentacion */
    LED_ENCENDIDO = 1;

    switch (estado) {

        case ESTADO_INIT:
            /* Solo LED de encendido ? sistema arrancando */
            LEDS_ApagarTodos();
            break;

        case ESTADO_PREPARANDO:
            /* LED amarillo fijo: sensores inicializandose */
            LEDS_ApagarTodos();
            LED_PREPARANDO = 1;
            break;

        case ESTADO_EN_ESPERA:
            /* LED azul fijo: midiendo, todo normal */
            LEDS_ApagarTodos();
            LED_EN_ESPERA = 1;
            break;

        case ESTADO_ALARMA:
            /* LED rojo parpadeando: umbral superado, requiere atencion.
             * El toggle cambia el estado en cada llamada (cada segundo),
             * generando un parpadeo de 0.5 Hz visible y urgente. */
            LEDS_ApagarTodos();
            LED_FUNCIONAL = toggle;
            break;

        case ESTADO_PAUSA:
            /* LED azul parpadeando lentamente: sistema en pausa voluntaria */
            LEDS_ApagarTodos();
            LED_EN_ESPERA = toggle;
            break;

        default:
            /* Estado desconocido: apagar todo excepto encendido */
            LEDS_ApagarTodos();
            break;
    }
}

/* =========================================================
 *  LEDS_GetEstado
 * ========================================================= */
EstadoSistema LEDS_GetEstado(void) {
    return estado_actual;
}
