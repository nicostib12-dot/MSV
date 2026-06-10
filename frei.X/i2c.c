/**
 * @file i2c.c
 * @brief Implementación de comunicación I2C por software para PIC18F4550.
 *
 * Este archivo implementa un protocolo I2C básico por software, también
 * conocido como bit-banging. Se usan dos pines del puerto B para generar
 * manualmente las señales de reloj y datos necesarias para comunicarse con
 * dispositivos I2C como la pantalla OLED y el sensor MAX30102.
 *
 * Pines utilizados:
 * - RB0: SCL, línea de reloj I2C.
 * - RB1: SDA, línea de datos I2C.
 *
 * El protocolo implementa:
 * - Inicialización del bus.
 * - Condición de inicio.
 * - Condición de parada.
 * - Inicio repetido.
 * - Escritura de bytes.
 * - Lectura de bytes con ACK o NACK.
 */

#include <xc.h>
#include "config.h"
#include "i2c.h"

/**
 * @brief Dirección del pin SCL.
 *
 * Controla si RB0 funciona como entrada o salida.
 */
#define SCL_DIR  TRISBbits.TRISB0

/**
 * @brief Dirección del pin SDA.
 *
 * Controla si RB1 funciona como entrada o salida.
 */
#define SDA_DIR  TRISBbits.TRISB1

/**
 * @brief Salida digital para la línea SCL.
 *
 * Permite colocar la línea de reloj en alto o bajo.
 */
#define SCL      LATBbits.LATB0

/**
 * @brief Salida digital para la línea SDA.
 *
 * Permite colocar la línea de datos en alto o bajo cuando el pin está
 * configurado como salida.
 */
#define SDA      LATBbits.LATB1

/**
 * @brief Lectura de la línea SDA.
 *
 * Se usa cuando SDA se configura como entrada, especialmente durante
 * la lectura de datos desde un dispositivo I2C.
 */
#define SDA_IN   PORTBbits.RB1

/**
 * @brief Inicializa el bus I2C por software.
 *
 * Configura las líneas SCL y SDA como salidas y las deja en estado alto,
 * que corresponde al estado de reposo del bus I2C. También activa los
 * pull-ups internos del puerto B.
 *
 * @note En un bus I2C real normalmente se usan resistencias pull-up externas.
 * En este proyecto se habilitan también los pull-ups internos del puerto B.
 */
void I2C_Init(void) {
    SCL_DIR = 0;
    SDA_DIR = 0;

    SCL = 1;
    SDA = 1;

    INTCON2bits.RBPU = 0;
}

/**
 * @brief Genera una condición de inicio en el bus I2C.
 *
 * La condición START se genera cuando SDA pasa de alto a bajo mientras
 * SCL permanece en alto. Esta condición indica a los dispositivos conectados
 * que inicia una comunicación.
 */
void I2C_Start(void) {
    SDA_DIR = 0;

    SDA = 1;
    __delay_us(5);

    SCL = 1;
    __delay_us(5);

    SDA = 0;
    __delay_us(5);

    SCL = 0;
    __delay_us(5);
}

/**
 * @brief Genera una condición de parada en el bus I2C.
 *
 * La condición STOP se genera cuando SDA pasa de bajo a alto mientras
 * SCL está en alto. Esta condición indica el final de una comunicación I2C.
 */
void I2C_Stop(void) {
    SDA_DIR = 0;

    SDA = 0;
    __delay_us(5);

    SCL = 1;
    __delay_us(5);

    SDA = 1;
    __delay_us(5);
}

/**
 * @brief Genera una condición de inicio repetido en el bus I2C.
 *
 * El inicio repetido se utiliza cuando el maestro desea iniciar una nueva
 * operación sin liberar completamente el bus. En esta implementación se
 * realiza mediante una condición STOP seguida de una condición START.
 *
 * @note En algunos dispositivos I2C se usa para cambiar de escritura a lectura
 * sin perder el control del bus.
 */
void I2C_RepeatedStart(void) {
    I2C_Stop();
    __delay_us(5);
    I2C_Start();
}

/**
 * @brief Escribe un byte en el bus I2C.
 *
 * Envía los 8 bits del byte, comenzando por el bit más significativo.
 * Por cada bit, se coloca el valor en SDA y luego se genera un pulso
 * en SCL para que el dispositivo esclavo lo lea.
 *
 * Después de enviar los 8 bits, se libera SDA para permitir que el esclavo
 * responda con el bit de reconocimiento ACK.
 *
 * @param data Byte que se desea transmitir por el bus I2C.
 *
 * @note En esta implementación se genera el ciclo de ACK, pero no se evalúa
 * si el dispositivo respondió con ACK o NACK.
 */
void I2C_Write(unsigned char data) {
    unsigned char i;

    for (i = 0; i < 8; i++) {
        /*
         * Se envía primero el bit más significativo.
         */
        SDA = (data & 0x80) ? 1 : 0;
        data <<= 1;

        __delay_us(5);

        /*
         * Pulso de reloj para que el esclavo capture el bit.
         */
        SCL = 1;
        __delay_us(5);

        SCL = 0;
        __delay_us(5);
    }

    /*
     * Después de enviar 8 bits, el maestro libera SDA para que el esclavo
     * pueda responder con ACK.
     */
    SDA_DIR = 1;
    __delay_us(5);

    SCL = 1;
    __delay_us(5);

    SCL = 0;

    /*
     * Se vuelve a configurar SDA como salida para futuras transmisiones.
     */
    SDA_DIR = 0;
    __delay_us(5);
}

/**
 * @brief Lee un byte desde el bus I2C.
 *
 * Configura SDA como entrada y lee 8 bits enviados por el dispositivo esclavo.
 * Los bits se leen comenzando por el más significativo. Después de leer el byte,
 * el maestro envía un ACK o NACK dependiendo del valor del parámetro ack.
 *
 * @param ack Controla la respuesta del maestro después de la lectura.
 *            - 1: envía ACK, indicando que desea continuar leyendo.
 *            - 0: envía NACK, indicando que finaliza la lectura.
 *
 * @return Byte leído desde el bus I2C.
 */
unsigned char I2C_Read(char ack) {
    unsigned char data = 0;
    unsigned char i;

    /*
     * Se libera SDA para que el esclavo pueda controlar la línea de datos.
     */
    SDA_DIR = 1;

    for (i = 0; i < 8; i++) {
        data <<= 1;

        __delay_us(5);

        /*
         * Se sube SCL para que el esclavo coloque el bit y el maestro lo lea.
         */
        SCL = 1;
        __delay_us(5);

        if (SDA_IN) {
            data |= 0x01;
        }

        SCL = 0;
        __delay_us(5);
    }

    /*
     * Después de leer el byte, el maestro responde con ACK o NACK.
     */
    SDA_DIR = 0;

    /*
     * En I2C:
     * ACK  = 0
     * NACK = 1
     *
     * Si ack vale 1, se envía ACK.
     * Si ack vale 0, se envía NACK.
     */
    SDA = (ack) ? 0 : 1;

    __delay_us(5);

    SCL = 1;
    __delay_us(5);

    SCL = 0;

    /*
     * Se deja SDA en alto al finalizar la operación.
     */
    SDA = 1;
    __delay_us(5);

    return data;
}