/**
 * @file max30102.h
 * @brief Archivo de cabecera para el driver del sensor MAX30102.
 *
 * Este archivo contiene las direcciones I2C, registros internos y prototipos
 * de funciones necesarios para comunicarse con el sensor MAX30102.
 *
 * El MAX30102 es un sensor óptico que permite obtener muestras de luz roja
 * e infrarroja. En este proyecto se usa principalmente para detectar presencia
 * de dedo y calcular el ritmo cardíaco en BPM.
 *
 * Comunicación:
 * - Protocolo: I2C.
 * - Dirección de escritura: 0xAE.
 * - Dirección de lectura: 0xAF.
 */

#ifndef MAX30102_H
#define MAX30102_H

#include <xc.h>

/**
 * @brief Dirección I2C del MAX30102 para operación de escritura.
 *
 * Esta dirección se usa cuando el PIC necesita escribir en un registro interno
 * del sensor.
 */
#define MAX30102_ADDR_WRITE   0xAE

/**
 * @brief Dirección I2C del MAX30102 para operación de lectura.
 *
 * Esta dirección se usa cuando el PIC necesita leer un registro interno
 * del sensor.
 */
#define MAX30102_ADDR_READ    0xAF

/**
 * @brief Registro de estado de interrupción 1.
 *
 * Se utiliza para consultar o limpiar banderas de interrupción internas
 * del sensor.
 */
#define MAX30102_INT_STATUS_1  0x00

/**
 * @brief Registro de estado de interrupción 2.
 *
 * Complementa el registro de estado de interrupción principal.
 */
#define MAX30102_INT_STATUS_2  0x01

/**
 * @brief Registro de habilitación de interrupciones 1.
 *
 * Permite habilitar ciertas interrupciones internas del MAX30102.
 */
#define MAX30102_INT_ENABLE_1  0x02

/**
 * @brief Registro de habilitación de interrupciones 2.
 *
 * Permite habilitar interrupciones adicionales del sensor.
 */
#define MAX30102_INT_ENABLE_2  0x03

/**
 * @brief Puntero de escritura de la FIFO.
 *
 * Indica la posición donde el sensor está escribiendo nuevas muestras.
 */
#define MAX30102_FIFO_WR_PTR   0x04

/**
 * @brief Contador de desbordamiento de la FIFO.
 *
 * Registra eventos donde la FIFO se llena y se pierden muestras.
 */
#define MAX30102_OVF_COUNTER   0x05

/**
 * @brief Puntero de lectura de la FIFO.
 *
 * Indica la posición desde donde el microcontrolador leerá las muestras.
 */
#define MAX30102_FIFO_RD_PTR   0x06

/**
 * @brief Registro de datos de la FIFO.
 *
 * Desde este registro se leen las muestras RED e IR generadas por el sensor.
 */
#define MAX30102_FIFO_DATA     0x07

/**
 * @brief Registro de configuración de la FIFO.
 *
 * Permite configurar parámetros relacionados con el promedio de muestras,
 * rollover y profundidad de la FIFO.
 */
#define MAX30102_FIFO_CONFIG   0x08

/**
 * @brief Registro de configuración de modo.
 *
 * Permite reiniciar el sensor y seleccionar el modo de operación, por ejemplo
 * modo SpO2.
 */
#define MAX30102_MODE_CONFIG   0x09

/**
 * @brief Registro de configuración SpO2.
 *
 * Define parámetros como resolución del ADC, frecuencia de muestreo y ancho
 * de pulso de los LED.
 */
#define MAX30102_SPO2_CONFIG   0x0A

/**
 * @brief Registro de amplitud del LED 1.
 *
 * En el modo usado por el proyecto, este registro controla la corriente del
 * LED rojo.
 */
#define MAX30102_LED1_PA       0x0C

/**
 * @brief Registro de amplitud del LED 2.
 *
 * En el modo usado por el proyecto, este registro controla la corriente del
 * LED infrarrojo.
 */
#define MAX30102_LED2_PA       0x0D

/**
 * @brief Registro de identificación de revisión del sensor.
 */
#define MAX30102_REV_ID        0xFE

/**
 * @brief Registro de identificación del componente.
 *
 * Para el MAX30102, el valor esperado en este registro es 0x15.
 */
#define MAX30102_PART_ID       0xFF

/**
 * @brief Lee un registro interno del MAX30102.
 *
 * Esta función accede por I2C a un registro del sensor y almacena el valor
 * leído en la dirección indicada por el puntero.
 *
 * @param reg Dirección del registro que se desea leer.
 * @param data Puntero donde se guardará el dato leído.
 *
 * @return 1 si la lectura se realizó.
 * @return 0 si ocurre algún error de comunicación.
 */
unsigned char MAX30102_ReadRegister(unsigned char reg, unsigned char *data);

/**
 * @brief Escribe un dato en un registro interno del MAX30102.
 *
 * Esta función permite configurar el sensor escribiendo valores en sus
 * registros internos.
 *
 * @param reg Dirección del registro que se desea escribir.
 * @param data Valor que se escribirá en el registro.
 *
 * @return 1 si la escritura se realizó.
 * @return 0 si ocurre algún error de comunicación.
 */
unsigned char MAX30102_WriteRegister(unsigned char reg, unsigned char data);

/**
 * @brief Verifica la presencia del sensor MAX30102.
 *
 * Lee el registro PART ID y compara su valor con el identificador esperado
 * del sensor.
 *
 * @return 1 si el MAX30102 fue detectado correctamente.
 * @return 0 si el sensor no responde o el identificador no coincide.
 */
unsigned char MAX30102_Check(void);

/**
 * @brief Inicializa el sensor MAX30102.
 *
 * Configura el sensor para trabajar en modo SpO2, limpia la FIFO, ajusta la
 * corriente de los LED rojo e infrarrojo y deja listo el sensor para entregar
 * muestras.
 *
 * @return 1 si la inicialización fue exitosa.
 * @return 0 si el sensor no fue detectado.
 */
unsigned char MAX30102_Init(void);

/**
 * @brief Consulta cuántas muestras hay disponibles en la FIFO.
 *
 * Compara el puntero de escritura y el puntero de lectura de la FIFO para
 * calcular cuántas muestras están pendientes por leer.
 *
 * @return Número de muestras disponibles en la FIFO del MAX30102.
 */
unsigned char MAX30102_SamplesAvailable(void);

/**
 * @brief Lee una muestra RED e IR desde la FIFO del MAX30102.
 *
 * Cada lectura completa en modo SpO2 está formada por 6 bytes: tres bytes
 * para la señal roja y tres bytes para la señal infrarroja. La función
 * reconstruye ambos valores como datos de 18 bits.
 *
 * @param red Puntero donde se almacenará la muestra del LED rojo.
 * @param ir Puntero donde se almacenará la muestra del LED infrarrojo.
 *
 * @return 1 si se leyó una muestra correctamente.
 * @return 0 si no había muestras disponibles.
 */
unsigned char MAX30102_ReadSample(unsigned long *red, unsigned long *ir);

#endif