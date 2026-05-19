/**
 * @file    uart.c
 * @brief   Driver UART por hardware (modulo USART) para el PIC18F4550.
 *
 * Transmision no bloqueante mediante buffer circular e interrupcion TXIF:
 *
 *  UART_SendChar() deposita el byte en tx_buf[] y habilita TXIE.
 *  Cuando TXREG queda libre, el hardware activa TXIF.
 *  El ISR saca el siguiente byte del buffer y lo carga en TXREG.
 *  Cuando el buffer se vacia, el ISR desactiva TXIE para no generar
 *  interrupciones innecesarias.
 *
 * Calculo de SPBRG para 9600 bps a 8 MHz con BRGH=1:
 *  SPBRG = Fosc / (16 * baud) - 1 = 8000000 / (16 * 9600) - 1 = 51
 *  Baud real = 8000000 / (16 * 52) = 9615 bps  (error 0.16% ? excelente)
 */

#include "uart.h"

/* =========================================================
 *  BUFFER CIRCULAR DE TRANSMISION
 *
 *  tx_head : indice donde el ISR lee el proximo byte a enviar.
 *  tx_tail : indice donde UART_SendChar() escribe el proximo byte.
 *  tx_count: bytes actualmente en el buffer.
 *
 *  Las variables compartidas con el ISR se declaran volatile para que
 *  el compilador no las optimice y siempre lea el valor real de RAM.
 * ========================================================= */
static volatile unsigned char tx_buf[UART_TX_BUF_SIZE];
static volatile unsigned char tx_head;
static volatile unsigned char tx_tail;
static volatile unsigned char tx_count;

/* =========================================================
 *  UART_Init
 * ========================================================= */
void UART_Init(void) {
    /* -- Configuracion de pines --
     * RC6 (TX) como salida digital.
     * RC7 (RX) como entrada digital (requerido por el modulo USART). */
    TRISCbits.TRISC6 = 0;   /* TX = salida */
    TRISCbits.TRISC7 = 1;   /* RX = entrada */

    /* -- TXSTA: registro de control de transmision --
     * CSRC  = 0  : fuente de reloj (indiferente en modo asincrono)
     * TX9   = 0  : transmision de 8 bits
     * TXEN  = 1  : habilitar transmisor
     * SYNC  = 0  : modo asincrono
     * SENDB = 0  : sin break
     * BRGH  = 1  : generador de baud rate alta velocidad (modo 16x)
     * Valor : 0b00100100 = 0x24 */
    TXSTA = 0x24;

    /* -- RCSTA: registro de control de recepcion --
     * SPEN  = 1  : habilitar puerto serial (RC6/RC7 pasan a USART)
     *              OBLIGATORIO ? sin esto no funciona TX tampoco.
     * RX9   = 0  : recepcion de 8 bits
     * CREN  = 0  : receptor deshabilitado (solo transmitimos)
     * Resto = 0
     * Valor : 0b10000000 = 0x80 */
    RCSTA = 0x80;

    /* -- SPBRG: generador de baud rate --
     * Valor 51 precomputado en config.h (UART_SPBRG_VAL).
     * Baud = 8000000 / (16 * (51+1)) = 9615 bps (error 0.16%). */
    SPBRG = (unsigned char)UART_SPBRG_VAL;

    /* -- Inicializar buffer circular -- */
    tx_head  = 0U;
    tx_tail  = 0U;
    tx_count = 0U;

    /* -- Interrupcion TX deshabilitada al inicio --
     * Se habilita automaticamente en UART_SendChar() cuando hay datos.
     * La prioridad de interrupcion no se configura aqui ? el main.c
     * debe llamar ei() (o INTCONbits.GIE=1) para activar interrupciones. */
    PIE1bits.TXIE = 0;
}

/* =========================================================
 *  UART_TxISR
 *  Logica del manejador de interrupcion TX.
 *  Se llama desde el ISR global cuando TXIF esta activo.
 * ========================================================= */
void UART_TxISR(void) {
    if (tx_count > 0U) {
        /* Enviar el siguiente byte del buffer al registro de transmision */
        TXREG = tx_buf[tx_head];
        tx_head = (unsigned char)((tx_head + 1U) % UART_TX_BUF_SIZE);
        tx_count--;
    }

    /* Si el buffer quedo vacio, deshabilitar la interrupcion TX para
     * no generar interrupciones espurias cuando TXREG este libre. */
    if (tx_count == 0U) {
        PIE1bits.TXIE = 0;
    }
}

/* =========================================================
 *  ISR GLOBAL DEL PIC18F4550
 *
 *  En XC8 solo puede existir una funcion con __interrupt().
 *  Este ISR atiende la interrupcion TX del USART.
 *
 *  Si en el futuro se agregan otras fuentes de interrupcion
 *  (por ejemplo INT2 para el boton), anadir sus manejadores aqui
 *  con los flags correspondientes (INTCONbits.INT2IF, etc.).
 * ========================================================= */
void __interrupt() ISR_Global(void) {
    /* Interrupcion por TX del USART:
     * PIE1bits.TXIE  = interrupcion TX habilitada
     * PIR1bits.TXIF  = flag: TXREG esta vacio y listo para nuevo dato */
    if (PIE1bits.TXIE && PIR1bits.TXIF) {
        UART_TxISR();
    }
}

/* =========================================================
 *  UART_SendChar
 * ========================================================= */
void UART_SendChar(char c) {
    /* Deshabilitar la interrupcion TX momentaneamente para acceder
     * de forma segura a las variables compartidas con el ISR.
     * La seccion critica es muy corta (4-5 instrucciones). */
    PIE1bits.TXIE = 0;

    if (tx_count < (unsigned char)UART_TX_BUF_SIZE) {
        tx_buf[tx_tail] = (unsigned char)c;
        tx_tail  = (unsigned char)((tx_tail + 1U) % UART_TX_BUF_SIZE);
        tx_count++;
    }
    /* Si el buffer esta lleno, el caracter se descarta silenciosamente.
     * A 9600 bps el buffer de 32 bytes se vacia en ~33 ms ? raro llenarse. */

    /* Rehabilitar la interrupcion TX si hay datos para enviar */
    if (tx_count > 0U) {
        PIE1bits.TXIE = 1;
    }
}

/* =========================================================
 *  UART_SendString
 * ========================================================= */
void UART_SendString(const char *str) {
    if (str == 0) return;
    while (*str != '\0') {
        UART_SendChar(*str);
        str++;
    }
}

/* =========================================================
 *  UART_SendInt
 *  Convierte un entero a texto sin usar sprintf.
 * ========================================================= */
void UART_SendInt(unsigned int value) {
    unsigned char digits[5];   /* Maximo 5 digitos para uint16 */
    signed char   i = 0;
    signed char   j;

    if (value == 0U) {
        UART_SendChar('0');
        return;
    }

    while (value > 0U && i < 5) {
        digits[i++] = (unsigned char)((value % 10U) + (unsigned int)'0');
        value /= 10U;
    }

    for (j = i - 1; j >= 0; j--) {
        UART_SendChar((char)digits[j]);
    }
}

/* =========================================================
 *  UART_SendVitals
 *
 *  Formato fijo: "BPM:XXX,TEMP:XX.X\r\n"
 *  BPM con 3 digitos y ceros a la izquierda para parsing uniforme.
 *  Temperatura con un decimal a partir de decimas de grado.
 * ========================================================= */
void UART_SendVitals(unsigned int bpm, unsigned int temp_decimas) {
    /* --- Campo BPM con 3 digitos y cero-padding ---
     * Siempre 3 digitos: "075" en lugar de "75".
     * Facilita el parsing en Python, Arduino u otras terminales. */
    UART_SendString("BPM:");
    if (bpm < 100U) UART_SendChar('0');   /* Cero de centenas si < 100 */
    if (bpm < 10U)  UART_SendChar('0');   /* Cero de decenas  si < 10  */
    UART_SendInt(bpm);

    /* --- Separador --- */
    UART_SendChar(',');

    /* --- Campo TEMP con un decimal ---
     * temp_decimas = 365  ->  "36.5"
     * temp_decimas = 370  ->  "37.0" */
    UART_SendString("TEMP:");
    UART_SendInt(temp_decimas / 10U);    /* Parte entera  */
    UART_SendChar('.');
    UART_SendChar((char)((temp_decimas % 10U) + (unsigned int)'0')); /* Decimal */

    /* --- Fin de trama --- */
    UART_SendChar('\r');
    UART_SendChar('\n');
}
