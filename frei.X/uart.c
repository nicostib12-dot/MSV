/**
 * @file uart.c
 * @brief Driver UART/EUSART por hardware para el PIC18F4550.
 *
 * Este archivo implementa las funciones necesarias para enviar información
 * serial desde el PIC18F4550 hacia otro dispositivo, como un Arduino, un
 * conversor USB-TTL o un computador.
 *
 * En este proyecto, la UART se usa para enviar:
 * - Mensajes de inicio del sistema.
 * - Mensajes de error de sensores.
 * - Estado del perfil seleccionado.
 * - Temperatura medida por el DS18B20.
 * - Ritmo cardíaco calculado a partir del MAX30102.
 * - Estado de alarma del sistema.
 *
 * Pines UART del PIC18F4550:
 * - RC6: TX, transmisión de datos.
 * - RC7: RX, recepción de datos.
 *
 * Configuración usada:
 * - Modo asíncrono.
 * - Velocidad: 9600 baudios.
 * - Formato típico: 8 bits de datos, sin paridad y 1 bit de parada.
 */

#include "config.h"
#include "uart.h"

/* -------------------------------------------------------------------------- */
/**
 * @brief Inicializa el módulo EUSART del PIC18F4550.
 *
 * Configura los pines RC6 y RC7 para el uso del módulo UART por hardware.
 * También establece la velocidad de comunicación en 9600 baudios usando
 * el oscilador interno de 8 MHz.
 *
 * Configuración:
 * - RC6 como TX.
 * - RC7 como RX.
 * - Modo asíncrono.
 * - Alta velocidad.
 * - Receptor continuo habilitado.
 * - Transmisor habilitado.
 *
 * Fórmula usada para el baud rate:
 *
 * @code
 * SPBRG = Fosc / (16 * Baud) - 1
 * SPBRG = 8000000 / (16 * 9600) - 1
 * SPBRG = 51
 * @endcode
 *
 * @note Si se cambia la frecuencia del oscilador o la velocidad deseada,
 * también debe modificarse el valor de SPBRG.
 */
void UART_Init(void) {
    /*
     * Según la hoja de datos del PIC18F4550, los pines RC6 y RC7 deben
     * configurarse inicialmente como entradas para que el módulo EUSART
     * pueda controlarlos correctamente.
     */
    TRISCbits.TRISC6 = 1;
    TRISCbits.TRISC7 = 1;

    /*
     * Configuración del baud rate para 9600 baudios con Fosc = 8 MHz.
     *
     * BRGH = 1  -> alta velocidad
     * BRG16 = 0 -> generador de baudios de 8 bits
     * SPBRG = 51
     */
    SPBRG  = 51;
    SPBRGH = 0;

    /*
     * Configuración del registro TXSTA:
     *
     * TXEN = 1 -> habilita la transmisión.
     * SYNC = 0 -> selecciona modo asíncrono.
     * BRGH = 1 -> habilita alta velocidad.
     */
    TXSTAbits.TXEN = 1;
    TXSTAbits.SYNC = 0;
    TXSTAbits.BRGH = 1;

    /*
     * Configuración del registro RCSTA:
     *
     * SPEN = 1 -> habilita el puerto serial.
     * CREN = 1 -> habilita la recepción continua.
     */
    RCSTAbits.SPEN = 1;
    RCSTAbits.CREN = 1;

    /*
     * Lectura de RCREG para limpiar una posible bandera de recepción previa.
     */
    (void)RCREG;
}

/* -------------------------------------------------------------------------- */
/**
 * @brief Envía un carácter por UART.
 *
 * Esta función espera a que el registro de transmisión esté libre y luego
 * carga el carácter en TXREG para que sea enviado por el pin TX del PIC.
 *
 * @param c Carácter que se desea transmitir.
 */
void UART_WriteChar(unsigned char c) {
    /*
     * TRMT indica si el registro de transmisión está vacío.
     * Mientras no esté vacío, se espera.
     */
    while (!TXSTAbits.TRMT);

    /*
     * Al escribir en TXREG, el módulo EUSART inicia la transmisión del carácter.
     */
    TXREG = c;
}

/* -------------------------------------------------------------------------- */
/**
 * @brief Envía una cadena de caracteres por UART.
 *
 * La función recorre la cadena hasta encontrar el carácter nulo '\0'. Cada
 * carácter se envía mediante UART_WriteChar().
 *
 * @param str Puntero a la cadena de texto que se desea enviar.
 */
void UART_WriteString(const char *str) {
    while (*str) {
        UART_WriteChar((unsigned char)*str);
        str++;
    }
}

/* -------------------------------------------------------------------------- */
/**
 * @brief Envía un número entero sin signo por UART.
 *
 * Convierte un número entero de 0 a 999 en caracteres ASCII y los envía por
 * UART. Esta función se usa internamente para transmitir temperatura y BPM.
 *
 * @param n Número entero que se desea enviar.
 *
 * @note La función está diseñada para valores pequeños, como temperatura
 * o BPM, por eso usa buffers de tamaño 4.
 */
static void send_uint(unsigned int n) {
    char buf[4];
    unsigned char i = 0;
    unsigned char j = 0;
    char tmp[4];

    /*
     * Caso especial cuando el número es cero.
     */
    if (n == 0) {
        UART_WriteChar('0');
        return;
    }

    /*
     * Se separan los dígitos del número desde el menos significativo.
     */
    while (n > 0) {
        tmp[i++] = '0' + (n % 10);
        n /= 10;
    }

    /*
     * Como los dígitos quedaron invertidos, se reordenan antes de enviarlos.
     */
    while (i > 0) {
        buf[j++] = tmp[--i];
    }

    buf[j] = '\0';

    /*
     * Envío de los caracteres resultantes por UART.
     */
    j = 0;

    while (buf[j]) {
        UART_WriteChar((unsigned char)buf[j++]);
    }
}

/* -------------------------------------------------------------------------- */
/**
 * @brief Envía los signos vitales y el estado del sistema por UART.
 *
 * Esta función genera una trama de texto con la información principal del
 * sistema Cardio.X. El formato enviado permite observar los datos en el
 * monitor serial de Arduino, un terminal serial o un conversor USB-TTL.
 *
 * Formato enviado:
 *
 * @code
 * T:36.7 B:075 P:NOR AL:0
 * @endcode
 *
 * Donde:
 * - T: temperatura en grados Celsius.
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
 * @param bpm Ritmo cardíaco en BPM.
 * @param perfil Perfil seleccionado.
 *               - 0: perfil normal.
 *               - 1: perfil deportista.
 * @param alarma Estado de alarma.
 *               - 0: no hay alarma.
 *               - 1: hay alarma.
 */
void UART_SendVitals(unsigned char temp_int, unsigned char temp_dec,
                     unsigned char bpm, unsigned char perfil,
                     unsigned char alarma) {
    /*
     * Envío de temperatura.
     */
    UART_WriteString("T:");
    send_uint(temp_int);
    UART_WriteChar('.');
    send_uint(temp_dec);

    /*
     * Envío del BPM.
     * Se agregan ceros a la izquierda para mantener un ancho fijo de 3 dígitos.
     */
    UART_WriteString(" B:");

    if (bpm < 100) {
        UART_WriteChar('0');
    }

    if (bpm < 10) {
        UART_WriteChar('0');
    }

    send_uint(bpm);

    /*
     * Envío del perfil seleccionado.
     */
    UART_WriteString(" P:");

    if (perfil == 0) {
        UART_WriteString("NOR");
    } else {
        UART_WriteString("DEP");
    }

    /*
     * Envío del estado de alarma.
     */
    UART_WriteString(" AL:");
    UART_WriteChar(alarma ? '1' : '0');

    /*
     * Salto de línea para que cada trama aparezca separada en el monitor serial.
     */
    UART_WriteString("\r\n");
}