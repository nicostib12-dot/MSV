/**
 * @file max30102.c
 * @brief Driver básico para el sensor MAX30102 usando comunicación I2C.
 *
 * Este archivo implementa las funciones necesarias para comunicarse con el
 * sensor MAX30102 desde el PIC18F4550. El sensor se comunica mediante el bus
 * I2C y permite obtener muestras de luz roja e infrarroja, las cuales luego
 * son procesadas para estimar el ritmo cardíaco.
 *
 * Funciones principales:
 * - Escritura de registros internos del MAX30102.
 * - Lectura de registros internos del MAX30102.
 * - Verificación del identificador del sensor.
 * - Configuración inicial del sensor.
 * - Consulta de muestras disponibles en la FIFO.
 * - Lectura de muestras RED e IR desde la FIFO.
 *
 * El MAX30102 se comunica por I2C con:
 * - SCL: línea de reloj.
 * - SDA: línea de datos.
 */

#include <xc.h>
#include "config.h"
#include "i2c.h"
#include "max30102.h"

/**
 * @brief Escribe un dato en un registro interno del MAX30102.
 *
 * Esta función realiza una comunicación I2C de escritura hacia el MAX30102.
 * Primero envía la dirección del sensor en modo escritura, luego el registro
 * que se desea modificar y finalmente el dato que será almacenado.
 *
 * Secuencia I2C:
 * - START.
 * - Dirección del MAX30102 en escritura.
 * - Dirección del registro.
 * - Dato a escribir.
 * - STOP.
 *
 * @param reg Dirección del registro interno del MAX30102.
 * @param data Valor que se desea escribir en el registro.
 *
 * @return 1 si se completó la secuencia de escritura.
 *
 * @note En esta implementación la función I2C_Write() no verifica el ACK del
 * esclavo, por eso siempre retorna 1 al finalizar la secuencia.
 */
unsigned char MAX30102_WriteRegister(unsigned char reg, unsigned char data) {
    I2C_Start();
    I2C_Write(MAX30102_ADDR_WRITE);
    I2C_Write(reg);
    I2C_Write(data);
    I2C_Stop();

    return 1;
}

/**
 * @brief Lee un registro interno del MAX30102.
 *
 * Esta función realiza una escritura inicial para indicar qué registro se desea
 * leer y luego ejecuta un inicio repetido para cambiar a modo lectura.
 *
 * Secuencia I2C:
 * - START.
 * - Dirección del MAX30102 en escritura.
 * - Dirección del registro.
 * - REPEATED START.
 * - Dirección del MAX30102 en lectura.
 * - Lectura del dato.
 * - STOP.
 *
 * @param reg Dirección del registro interno que se desea leer.
 * @param data Puntero donde se almacenará el dato leído.
 *
 * @return 1 si se completó la secuencia de lectura.
 *
 * @note La lectura se realiza con NACK al final porque solo se lee un byte.
 */
unsigned char MAX30102_ReadRegister(unsigned char reg, unsigned char *data) {
    I2C_Start();
    I2C_Write(MAX30102_ADDR_WRITE);
    I2C_Write(reg);

    I2C_RepeatedStart();
    I2C_Write(MAX30102_ADDR_READ);

    *data = I2C_Read(0);

    I2C_Stop();

    return 1;
}

/**
 * @brief Verifica si el sensor MAX30102 está conectado correctamente.
 *
 * La función lee el registro PART ID del sensor. Para el MAX30102, el valor
 * esperado del identificador es 0x15. Si el valor leído coincide, se considera
 * que el sensor está presente y responde correctamente en el bus I2C.
 *
 * @return 1 si el sensor responde y el PART ID es correcto.
 * @return 0 si no responde o el identificador no coincide.
 */
unsigned char MAX30102_Check(void) {
    unsigned char part_id;

    if (!MAX30102_ReadRegister(MAX30102_PART_ID, &part_id)) {
        return 0;
    }

    return (part_id == 0x15);
}

/**
 * @brief Inicializa y configura el sensor MAX30102.
 *
 * Esta función verifica primero la presencia del sensor mediante el registro
 * PART ID. Si el sensor responde correctamente, se configuran sus registros
 * principales para operar en modo SpO2, usando los LED rojo e infrarrojo.
 *
 * Configuraciones realizadas:
 * - Reset interno del sensor.
 * - Reinicio de punteros FIFO.
 * - Configuración de FIFO.
 * - Configuración SpO2.
 * - Corriente de los LED rojo e infrarrojo.
 * - Modo de operación SpO2.
 * - Limpieza de registros de interrupción.
 *
 * @return 1 si el sensor fue inicializado correctamente.
 * @return 0 si el sensor no fue detectado.
 */
unsigned char MAX30102_Init(void) {
    unsigned char status;

    /*
     * Se verifica que el sensor esté presente leyendo su PART ID.
     */
    if (!MAX30102_Check()) {
        return 0;
    }

    /*
     * Reset interno del MAX30102.
     * El bit 6 del registro MODE_CONFIG reinicia el sensor.
     */
    MAX30102_WriteRegister(MAX30102_MODE_CONFIG, 0x40);
    __delay_ms(20);

    /*
     * Reinicio de los punteros de la FIFO.
     * Esto asegura que las lecturas empiecen desde un estado limpio.
     */
    MAX30102_WriteRegister(MAX30102_FIFO_WR_PTR, 0x00);
    MAX30102_WriteRegister(MAX30102_OVF_COUNTER, 0x00);
    MAX30102_WriteRegister(MAX30102_FIFO_RD_PTR, 0x00);

    /*
     * Configuración de la FIFO.
     * El valor 0x50 define parámetros como promedio de muestras y modo FIFO.
     */
    MAX30102_WriteRegister(MAX30102_FIFO_CONFIG, 0x50);

    /*
     * Configuración del modo SpO2.
     * El valor 0x27 define parámetros como resolución ADC y tasa de muestreo.
     */
    MAX30102_WriteRegister(MAX30102_SPO2_CONFIG, 0x27);

    /*
     * Configuración de amplitud de los LED.
     * LED1 corresponde al LED rojo.
     * LED2 corresponde al LED infrarrojo.
     */
    MAX30102_WriteRegister(MAX30102_LED1_PA, 0x1F);
    MAX30102_WriteRegister(MAX30102_LED2_PA, 0x1F);

    /*
     * Configura el sensor en modo SpO2.
     * En este modo se activan las muestras del LED rojo e infrarrojo.
     */
    MAX30102_WriteRegister(MAX30102_MODE_CONFIG, 0x03);

    /*
     * Lectura de registros de interrupción para limpiar banderas pendientes.
     */
    MAX30102_ReadRegister(MAX30102_INT_STATUS_1, &status);
    MAX30102_ReadRegister(MAX30102_INT_STATUS_2, &status);

    return 1;
}

/**
 * @brief Consulta cuántas muestras hay disponibles en la FIFO del MAX30102.
 *
 * El MAX30102 almacena las muestras en una memoria FIFO interna. Esta función
 * lee el puntero de escritura y el puntero de lectura para calcular cuántas
 * muestras están pendientes por leer.
 *
 * @return Número de muestras disponibles en la FIFO.
 *
 * @note La operación se limita con 0x1F porque la FIFO del MAX30102 trabaja
 * con punteros de 5 bits.
 */
unsigned char MAX30102_SamplesAvailable(void) {
    unsigned char write_ptr;
    unsigned char read_ptr;

    MAX30102_ReadRegister(MAX30102_FIFO_WR_PTR, &write_ptr);
    MAX30102_ReadRegister(MAX30102_FIFO_RD_PTR, &read_ptr);

    return (write_ptr - read_ptr) & 0x1F;
}

/**
 * @brief Lee una muestra completa RED e IR desde la FIFO del MAX30102.
 *
 * Cada muestra en modo SpO2 está compuesta por 6 bytes:
 * - 3 bytes para el LED rojo.
 * - 3 bytes para el LED infrarrojo.
 *
 * El MAX30102 entrega datos de 18 bits, por lo que se toman los 2 bits menos
 * significativos del primer byte y se combinan con los otros dos bytes.
 *
 * Secuencia de lectura:
 * - Verificar que existan muestras disponibles.
 * - Apuntar al registro FIFO_DATA.
 * - Leer 6 bytes por I2C.
 * - Reconstruir el valor RED de 18 bits.
 * - Reconstruir el valor IR de 18 bits.
 *
 * @param red Puntero donde se almacenará la muestra del LED rojo.
 * @param ir Puntero donde se almacenará la muestra del LED infrarrojo.
 *
 * @return 1 si se leyó una muestra correctamente.
 * @return 0 si no había muestras disponibles en la FIFO.
 */
unsigned char MAX30102_ReadSample(unsigned long *red, unsigned long *ir) {
    unsigned char data[6];

    /*
     * Si no hay muestras disponibles, no se realiza lectura.
     */
    if (MAX30102_SamplesAvailable() == 0) {
        return 0;
    }

    /*
     * Se selecciona el registro FIFO_DATA y luego se leen seis bytes.
     */
    I2C_Start();
    I2C_Write(MAX30102_ADDR_WRITE);
    I2C_Write(MAX30102_FIFO_DATA);

    I2C_RepeatedStart();
    I2C_Write(MAX30102_ADDR_READ);

    /*
     * Se leen los primeros cinco bytes con ACK porque se continuará leyendo.
     * El último byte se lee con NACK para finalizar la lectura.
     */
    data[0] = I2C_Read(1);
    data[1] = I2C_Read(1);
    data[2] = I2C_Read(1);
    data[3] = I2C_Read(1);
    data[4] = I2C_Read(1);
    data[5] = I2C_Read(0);

    I2C_Stop();

    /*
     * Reconstrucción de la muestra del LED rojo.
     * Se enmascaran los bits superiores porque el dato útil es de 18 bits.
     */
    *red = (((unsigned long)data[0] & 0x03) << 16) |
           ((unsigned long)data[1] << 8) |
           data[2];

    /*
     * Reconstrucción de la muestra del LED infrarrojo.
     */
    *ir = (((unsigned long)data[3] & 0x03) << 16) |
          ((unsigned long)data[4] << 8) |
          data[5];

    return 1;
}