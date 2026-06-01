/*
 * uart.c  -  Driver EUSART hardware del PIC18F4550
 */

#include "config.h"
#include "uart.h"

/* ------------------------------------------------------------------ */
void UART_Init(void) {
    /* RC6 = TX: el modulo EUSART lo controla automaticamente
     * RC7 = RX: idem
     * Poner ambos como entradas primero (requerido por datasheet) */
    TRISCbits.TRISC6 = 1;
    TRISCbits.TRISC7 = 1;

    /* Baud rate: 9600 con Fosc=8MHz
     * BRGH=1 (alta velocidad), BRG16=0 -> SPBRG = Fosc/(16*baud) - 1
     * SPBRG = 8000000 / (16 * 9600) - 1 = 51 */
    SPBRG  = 51;
    SPBRGH = 0;

    /* TXSTA: habilitar transmisor, modo asincrono, alta velocidad */
    TXSTAbits.TXEN  = 1;   /* TX habilitado      */
    TXSTAbits.SYNC  = 0;   /* modo asincrono     */
    TXSTAbits.BRGH  = 1;   /* alta velocidad     */

    /* RCSTA: habilitar puerto serie y receptor */
    RCSTAbits.SPEN  = 1;   /* puerto serie ON    */
    RCSTAbits.CREN  = 1;   /* receptor continuo  */

    /* Limpiar flag de recepcion por si acaso */
    (void)RCREG;
}

/* ------------------------------------------------------------------ */
void UART_WriteChar(unsigned char c) {
    while (!TXSTAbits.TRMT);   /* esperar que el buffer este libre */
    TXREG = c;
}

/* ------------------------------------------------------------------ */
void UART_WriteString(const char *str) {
    while (*str) {
        UART_WriteChar((unsigned char)*str);
        str++;
    }
}

/* ------------------------------------------------------------------ */
/* Convierte un numero de 0-999 a string y lo envia por UART          */
static void send_uint(unsigned int n) {
    char buf[4];
    unsigned char i = 0, j = 0;
    char tmp[4];

    if (n == 0) { UART_WriteChar('0'); return; }

    while (n > 0) {
        tmp[i++] = '0' + (n % 10);
        n /= 10;
    }
    /* invertir */
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = '\0';

    j = 0;
    while (buf[j]) UART_WriteChar((unsigned char)buf[j++]);
}

/* ------------------------------------------------------------------ */
void UART_SendVitals(unsigned char temp_int, unsigned char temp_dec,
                     unsigned char bpm, unsigned char perfil,
                     unsigned char alarma) {
    /* Formato: "T:36.7 B:075 P:NOR AL:0\r\n" */

    UART_WriteString("T:");
    send_uint(temp_int);
    UART_WriteChar('.');
    send_uint(temp_dec);

    UART_WriteString(" B:");
    /* BPM con ceros a la izquierda para ancho fijo de 3 digitos */
    if (bpm < 100) UART_WriteChar('0');
    if (bpm < 10)  UART_WriteChar('0');
    send_uint(bpm);

    UART_WriteString(" P:");
    if (perfil == 0)
        UART_WriteString("NOR");
    else
        UART_WriteString("DEP");

    UART_WriteString(" AL:");
    UART_WriteChar(alarma ? '1' : '0');

    UART_WriteString("\r\n");
}

