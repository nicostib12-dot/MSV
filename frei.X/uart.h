/**
 * @file uart.h
 * @brief Archivo de cabecera para el driver EUSART del PIC18F4550.
 *
 * Este archivo declara las funciones utilizadas para manejar la comunicación
 * serial UART mediante el módulo EUSART hardware del PIC18F4550.
 *
 * En el proyecto Cardio.X, la UART se utiliza para enviar información del
 * sistema hacia un computador, Arduino o conversor USB-TTL. Los datos enviados
 * incluyen temperatura, BPM, perfil seleccionado y estado de alarma.
 *
 * Pines utilizados:
 * - RC6: TX, transmisión UART.
 * - RC7: RX, recepción UART.
 *
 * Configuración usada:
 * - Velocidad: 9600 baudios.
 * - Formato: 8 bits de datos, sin paridad, 1 bit de parada.
 * - Modo: asíncrono.
 * - Oscilador: 8 MHz interno.
 *
 * Fórmula para el baud rate:
 *
 * @code
 * SPBRG = (Fosc / (16 * BaudRate)) - 1
 * SPBRG = (8000000 / (16 * 9600)) - 1
 * SPBRG = 51
 * @endcode
 */

#ifndef UART_H
#define UART_H

#include <xc.h>

/**
 * @brief Inicializa el módulo EUSART del PIC18F4550.
 *
 * Configura la UART por hardware para trabajar en modo asíncrono a
 * 9600 baudios. También habilita el transmisor y el receptor continuo.
 *
 * Esta función debe llamarse antes de enviar cualquier dato por UART.
 *
 * @note La configuración interna de velocidad se encuentra en uart.c,
 * dentro de la función UART_Init().
 */
void UART_Init(void);

/**
 * @brief Envía un carácter por UART.
 *
 * Esta función transmite un solo byte por el pin TX del PIC18F4550.
 *
 * @param c Carácter o byte que se desea enviar.
 */
void UART_WriteChar(unsigned char c);

/**
 * @brief Envía una cadena de texto por UART.
 *
 * Recorre la cadena recibida y envía cada carácter usando UART_WriteChar().
 * La cadena debe terminar con el carácter nulo '\0'.
 *
 * @param str Cadena de caracteres que se desea transmitir.
 *
 * @note Para que el mensaje aparezca en una nueva línea en el monitor serial,
 * se recomienda terminar la cadena con "\r\n".
 */
void UART_WriteString(const char *str);

/**
 * @brief Envía una trama con los signos vitales del sistema.
 *
 * Esta función transmite por UART una línea con la temperatura, el BPM,
 * el perfil activo y el estado de alarma.
 *
 * Formato enviado:
 *
 * @code
 * T:36.7 B:075 P:NOR AL:0
 * T:38.9 B:130 P:DEP AL:1
 * @endcode
 *
 * Significado:
 * - T: temperatura medida por el DS18B20.
 * - B: ritmo cardíaco en BPM.
 * - P: perfil seleccionado.
 *   - NOR: perfil normal.
 *   - DEP: perfil deportista.
 * - AL: estado de alarma.
 *   - 0: sin alarma.
 *   - 1: con alarma.
 *
 * @param temp_int Parte entera de la temperatura.
 * @param temp_dec Parte decimal de la temperatura.
 * @param bpm Ritmo cardíaco en latidos por minuto.
 * @param perfil Perfil seleccionado.
 *               - 0: perfil normal.
 *               - 1: perfil deportista.
 * @param alarma Estado de alarma.
 *               - 0: no hay alarma.
 *               - 1: hay alarma.
 */
void UART_SendVitals(unsigned char temp_int, unsigned char temp_dec,
                     unsigned char bpm, unsigned char perfil,
                     unsigned char alarma);

#endif /* UART_H */