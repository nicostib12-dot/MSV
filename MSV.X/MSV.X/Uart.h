/**
 * @file    uart.h
 * @brief   Driver UART por hardware (modulo USART) para el PIC18F4550.
 *
 * Pines fisicos:
 *  - RC6 (pin 25) : TX ? transmision hacia la terminal serial del PC.
 *  - RC7 (pin 26) : RX ? recepcion (configurado pero no usado activamente).
 *
 * Parametros de comunicacion:
 *  - Velocidad  : 9600 bps
 *  - Bits dato  : 8
 *  - Paridad    : ninguna
 *  - Stop bits  : 1
 *  - Modo       : asincrono
 *
 * Mecanismo de transmision: interrupcion por TX (TXIF).
 *  La funcion UART_SendChar() deposita bytes en un buffer circular de 32
 *  bytes y habilita la interrupcion TXIE. El ISR envia los bytes uno a uno
 *  conforme el registro TXREG queda libre, sin bloquear el bucle principal.
 *
 * Formato de trama de signos vitales:
 *  "BPM:XXX,TEMP:XX.X\r\n"
 *  Ejemplo: "BPM:075,TEMP:36.5\r\n"
 *
 * @note  El ISR de UART esta definido en uart.c con la directiva
 *        __interrupt() de XC8. Solo puede existir un ISR por proyecto.
 *        Si se requieren mas fuentes de interrupcion, centralizar el ISR
 *        en main.c y llamar a UART_TxISR() desde alli.
 */

#ifndef UART_H
#define UART_H

#include "config.h"

/* =========================================================
 *  TAMANO DEL BUFFER DE TRANSMISION
 *  32 bytes es suficiente para la trama "BPM:XXX,TEMP:XX.X\r\n"
 *  mas un margen de seguridad.
 * ========================================================= */
#define UART_TX_BUF_SIZE   32U

/* =========================================================
 *  PROTOTIPOS PUBLICOS
 * ========================================================= */

/**
 * @brief  Inicializa el modulo USART del PIC18F4550 en modo asincrono
 *         a 9600 bps con transmision habilitada por interrupcion.
 *
 * Configura TXSTA, RCSTA y SPBRG segun los valores de config.h.
 * Debe llamarse una vez antes de cualquier funcion de envio.
 * Las interrupciones globales deben habilitarse en main.c con ei().
 */
void UART_Init(void);

/**
 * @brief  Agrega un caracter al buffer circular de transmision.
 *         Si el buffer esta lleno, el caracter se descarta silenciosamente.
 *         Habilita la interrupcion TXIE para iniciar o continuar el envio.
 * @param  c  Caracter a transmitir.
 */
void UART_SendChar(char c);

/**
 * @brief  Agrega una cadena de caracteres al buffer de transmision.
 * @param  str  Cadena terminada en '\0'. Cada caracter se encola via UART_SendChar().
 */
void UART_SendString(const char *str);

/**
 * @brief  Convierte un entero sin signo a texto y lo agrega al buffer.
 *         No usa sprintf. Soporta valores de 0 a 65535.
 * @param  value  Valor entero a transmitir.
 */
void UART_SendInt(unsigned int value);

/**
 * @brief  Transmite la trama completa de signos vitales con formato fijo:
 *         "BPM:XXX,TEMP:XX.X\r\n"
 *
 * Garantiza que BPM se muestra siempre con 3 digitos (ceros a la izquierda)
 * para facilitar el parsing en la terminal o en software externo.
 *
 * Ejemplo de salida: "BPM:075,TEMP:36.5\r\n"
 *
 * @param  bpm           Frecuencia cardiaca en latidos por minuto.
 * @param  temp_decimas  Temperatura en decimas de grado (365 = 36.5 C).
 */
void UART_SendVitals(unsigned int bpm, unsigned int temp_decimas);

/**
 * @brief  Manejador de la interrupcion TX del USART.
 *         Llamar desde el ISR global si este se centraliza en main.c.
 *         Si el ISR esta en uart.c (configuracion por defecto), esta
 *         funcion no necesita llamarse externamente.
 */
void UART_TxISR(void);

#endif /* UART_H */