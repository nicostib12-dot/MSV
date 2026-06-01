/*
 * uart.h  -  Driver EUSART hardware del PIC18F4550
 *
 * Pin TX = RC6  (salida automatica del modulo EUSART)
 * Pin RX = RC7  (entrada automatica del modulo EUSART)
 *
 * Configuracion: 9600 baud, 8 bits, sin paridad, 1 stop bit (8N1)
 * Oscilador:     8 MHz interno
 *
 * Formula baud rate:
 *   SPBRG = (Fosc / (16 * BaudRate)) - 1
 *   SPBRG = (8000000 / (16 * 9600)) - 1 = 51
 */

#ifndef UART_H
#define UART_H

#include <xc.h>

void UART_Init(void);
void UART_WriteChar(unsigned char c);
void UART_WriteString(const char *str);

/* Envia una trama completa de vitales:
 * formato: "T:XX.X B:XXX P:NOR AL:0\r\n"
 *          "T:XX.X B:XXX P:DEP AL:1\r\n"  */
void UART_SendVitals(unsigned char temp_int, unsigned char temp_dec,
                     unsigned char bpm, unsigned char perfil,
                     unsigned char alarma);

#endif