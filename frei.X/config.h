/**
 * @file config.h
 * @brief Configuración general del PIC18F4550 y definición de pines del sistema.
 *
 * Este archivo contiene la configuración principal del microcontrolador,
 * incluyendo la frecuencia de trabajo, los bits de configuración del PIC
 * y las macros usadas para controlar salidas como LEDs, alarma y otros
 * actuadores del sistema Cardio.X.
 *
 * En este proyecto se utiliza el oscilador interno del PIC18F4550 a 8 MHz.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <xc.h>

/**
 * @brief Frecuencia del oscilador del microcontrolador.
 *
 * Esta constante es usada por las funciones de retardo del compilador XC8,
 * como __delay_ms() y __delay_us().
 *
 * @note Si se cambia la frecuencia en el registro OSCCON dentro del main.c,
 * también debe actualizarse este valor para que los retardos sean correctos.
 */
#define _XTAL_FREQ 8000000

/**
 * @brief Selección del oscilador del PIC18F4550.
 *
 * INTOSCIO_EC configura el uso del oscilador interno. Esto permite trabajar
 * sin cristal externo para la lógica principal del sistema.
 */
#pragma config FOSC = INTOSCIO_EC

/**
 * @brief Deshabilita el Watchdog Timer.
 *
 * El Watchdog Timer se mantiene apagado para evitar reinicios automáticos
 * del microcontrolador durante la ejecución normal del programa.
 */
#pragma config WDT = OFF

/**
 * @brief Deshabilita la programación en bajo voltaje.
 *
 * Al desactivar LVP se evita que el pin asociado a programación en bajo
 * voltaje interfiera con el funcionamiento normal del circuito.
 */
#pragma config LVP = OFF

/**
 * @brief Configura los pines PORTB como digitales al iniciar.
 *
 * PBADEN en OFF evita que algunos pines del puerto B arranquen como entradas
 * analógicas. Esto es importante porque el sistema usa botones y líneas
 * digitales en PORTB.
 */
#pragma config PBADEN = OFF

/**
 * @brief Habilita el pin MCLR como reset externo.
 *
 * Con MCLRE en ON, el pin MCLR funciona como entrada de reset físico del PIC.
 */
#pragma config MCLRE = ON

/**
 * @brief Salida asociada al calentador.
 *
 * Esta macro apunta al pin RD0. En el sistema actual puede compartir pin con
 * LED1, por lo que debe evitarse usar ambas funciones al mismo tiempo si no
 * están realmente conectadas al mismo actuador.
 */
#define CALENTADOR  LATDbits.LATD0

/**
 * @brief Salida asociada a una tira LED.
 *
 * Esta macro apunta al pin RD2. En el sistema actual comparte ubicación con
 * LED3.
 */
#define LED_TIRA    LATDbits.LATD2

/**
 * @brief Salida asociada a una alarma auxiliar.
 *
 * Esta macro apunta al pin RD5.
 */
#define ALARMA      LATDbits.LATD5

/**
 * @brief LED1 del sistema.
 *
 * En el proyecto Cardio.X se usa como indicador de sistema encendido.
 * Está conectado al pin RD0.
 */
#define LED1        LATDbits.LATD0

/**
 * @brief LED2 del sistema.
 *
 * Se usa como indicador de sistema activo.
 * Está conectado al pin RD1.
 */
#define LED2        LATDbits.LATD1

/**
 * @brief LED3 del sistema.
 *
 * Se usa como indicador auxiliar, especialmente durante el arranque.
 * Está conectado al pin RD2.
 */
#define LED3        LATDbits.LATD2

/**
 * @brief LED4 del sistema.
 *
 * En la versión actual del proyecto se usa como indicador de pausa.
 * Se enciende cuando no hay dedo detectado en el MAX30102 y no hay actividad
 * válida asociada a la temperatura.
 *
 * Está conectado al pin RD3.
 */
#define LED4        LATDbits.LATD3

/**
 * @brief LED5 del sistema.
 *
 * LED adicional disponible en el pin RD4.
 */
#define LED5        LATDbits.LATD4

#endif /* CONFIG_H */