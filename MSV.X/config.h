/**
 * @file    config.h
 * @brief   Configuracion global del proyecto Monitor Portatil de Signos Vitales.
 *
 * Define:
 *  - Frecuencia del oscilador (requerida por __delay_ms / __delay_us).
 *  - Bits de configuracion del PIC18F4550 (grabados en flash, no modificables
 *    en tiempo de ejecucion).
 *  - Alias de pines para LEDs, buzzer y boton.
 *  - Umbrales de alarma para frecuencia cardiaca y temperatura.
 *  - Constantes de temporizado del sistema.
 *
 * @note  Todos los modulos del proyecto deben incluir este archivo.
 *        No incluir directamente <xc.h> en otros modulos; se obtiene
 *        transitivamente desde aqui.
 *
 * Microcontrolador : PIC18F4550
 * Compilador       : XC8 v2.x
 * IDE              : MPLAB X
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <xc.h>

/* =========================================================
 *  FRECUENCIA DEL OSCILADOR
 *  Requerida por las macros __delay_ms() y __delay_us() del
 *  compilador XC8. Debe coincidir con OSCCON = 0x72 (8 MHz).
 * ========================================================= */

#define _XTAL_FREQ 8000000UL   /**< Oscilador interno a 8 MHz */

/* =========================================================
 *  BITS DE CONFIGURACION DEL PIC18F4550
 *
 *  Se graban en la memoria de programa en el momento de
 *  programar el dispositivo. No se pueden cambiar en tiempo
 *  de ejecucion.
 *
 *  Cambios respecto al proyecto anterior:
 *   - WDT = ON  : Watchdog activo, reinicia el PIC si el
 *                 firmware se cuelga (imprescindible para
 *                 una demo de 15 minutos continua).
 *   - WDTPS = 32: Periodo del WDT ~1 segundo. El firmware
 *                 debe llamar CLRWDT() en cada ciclo del
 *                 bucle principal para evitar el reset.
 * ========================================================= */

#pragma config FOSC   = INTOSCIO_EC  /**< Oscilador interno; RA6 como I/O */
#pragma config WDT    = ON           /**< Watchdog Timer habilitado        */
#pragma config WDTPS  = 32           /**< Preescaler WDT -> periodo ~1 s   */
#pragma config LVP    = OFF          /**< Programacion de bajo voltaje OFF  */
#pragma config PBADEN = OFF          /**< PORTB digital por defecto         */
#pragma config MCLRE  = ON           /**< Pin MCLR como reset externo       */
#pragma config VREGEN = OFF          /**< Regulador USB desactivado (no USB)*/

/* =========================================================
 *  ALIAS DE PINES - LEDs DE ESTADO (PORTD)
 *
 *  Secuencia de estados del sistema:
 *    LED_ENCENDIDO  -> Sistema con alimentacion (siempre ON)
 *    LED_PREPARANDO -> Inicializando sensores / bus I2C
 *    LED_EN_ESPERA  -> Midiendo, valores dentro de rango normal
 *    LED_FUNCIONAL  -> Alarma activa (umbral superado)
 *
 *  Se usa LATDbits para escritura (evita el problema de
 *  lectura-modificacion-escritura sobre PORTx).
 * ========================================================= */

#define LED_ENCENDIDO   LATDbits.LATD0   /**< RD0 (pin 19) -> LED verde:    alimentado   */
#define LED_PREPARANDO  LATDbits.LATD1   /**< RD1 (pin 20) -> LED amarillo: inicializando*/
#define LED_EN_ESPERA   LATDbits.LATD2   /**< RD2 (pin 21) -> LED azul:     funcionando  */
#define LED_FUNCIONAL   LATDbits.LATD3   /**< RD3 (pin 22) -> LED rojo:     alarma activa*/

/* =========================================================
 *  ALIAS DE PINES - BUZZER Y BOTON (PORTD / PORTB)
 * ========================================================= */

#define BUZZER          LATDbits.LATD4   /**< RD4 (pin 27) -> Buzzer de alarma            */
#define BOTON_PAUSA     PORTBbits.RB2    /**< RB2 (pin 35) -> Boton de pausa (entrada)    */

/* =========================================================
 *  UMBRALES DE ALARMA
 *
 *  Basados en rangos clinicos estandar (ISO/IEEE y criterios
 *  clinicos ampliamente aceptados).
 *
 *  Frecuencia cardiaca:
 *    Normal  : 60 - 100 bpm
 *    Alarma  : < 60 bpm (bradicardia) o > 100 bpm (taquicardia)
 *
 *  Temperatura corporal:
 *    Normal  : 36.1 C - 37.2 C
 *    Alarma  : < 36.0 C (hipotermia leve) o > 37.5 C (fiebre)
 * ========================================================= */

#define BPM_MIN          60     /**< Frecuencia cardiaca minima normal (bpm)  */
#define BPM_MAX         100     /**< Frecuencia cardiaca maxima normal (bpm)  */

/** Temperatura minima normal en decimas de grado (36.0 C -> 360) */
#define TEMP_MIN_DECIMAS  360
/** Temperatura maxima normal en decimas de grado (37.5 C -> 375) */
#define TEMP_MAX_DECIMAS  375

/* =========================================================
 *  CONSTANTES DE TEMPORIZADO
 *
 *  CICLO_MS : Periodo del bucle principal en milisegundos.
 *             El WDT debe limpiarse dentro de este intervalo.
 *
 *  BOTON_HOLD_MS : Tiempo minimo de pulsacion larga para
 *                  activar la pausa del sistema (3 segundos).
 *
 *  UART_BAUD : Velocidad de comunicacion serial.
 *              Valor del registro SPBRG calculado para 8 MHz:
 *              SPBRG = (Fosc / (16 * baud)) - 1
 *                    = (8000000 / (16 * 9600)) - 1 = 51
 * ========================================================= */

#define CICLO_MS         1000U  /**< Periodo del bucle principal: 1 segundo       */
#define BOTON_HOLD_MS    3000U  /**< Pulsacion larga para pausar: 3 segundos      */
#define UART_BAUD        9600U  /**< Velocidad UART en bps                        */
#define UART_SPBRG_VAL     51U  /**< Valor de SPBRG para 9600 bps a 8 MHz         */

/* =========================================================
 *  CONSTANTE DE TIMEOUT PARA EL BUS I2C
 *
 *  Numero maximo de iteraciones de espera activa antes de
 *  abortar una operacion I2C. Evita bloqueos indefinidos si
 *  un sensor no responde o el bus queda en estado invalido.
 * ========================================================= */

#define I2C_TIMEOUT_MAX  1000U  /**< Iteraciones maximas de espera en I2C         */

#endif /* CONFIG_H */