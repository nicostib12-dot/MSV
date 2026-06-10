/**
 * @file ds18b20.h
 * @brief Archivo de cabecera para el driver del sensor DS18B20.
 *
 * Este archivo contiene las definiciones y prototipos de funciones necesarios
 * para comunicarse con el sensor de temperatura DS18B20 mediante el protocolo
 * OneWire.
 *
 * En este proyecto el DS18B20 se conecta al pin RD6 del PIC18F4550 y utiliza
 * una resistencia pull-up externa de 4.7 k? hacia VDD.
 */

#ifndef DS18B20_H
#define DS18B20_H

#include <xc.h>

/**
 * @brief Lectura lógica del pin OneWire del DS18B20.
 *
 * Esta macro permite leer el estado actual del pin RD6.
 */
#define OW_PIN  PORTDbits.RD6

/**
 * @brief Dirección del pin OneWire del DS18B20.
 *
 * Controla si RD6 funciona como entrada o salida.
 *
 * - 0: salida, el PIC domina el bus.
 * - 1: entrada, el bus queda liberado y sube por el pull-up externo.
 */
#define OW_DIR  TRISDbits.TRISD6

/**
 * @brief Salida lógica del pin OneWire del DS18B20.
 *
 * Esta macro controla el latch de salida LATD6. Se usa principalmente para
 * forzar el bus a nivel bajo cuando el PIC necesita escribir o generar reset.
 */
#define OW_LOW  LATDbits.LATD6

/**
 * @brief Genera un reset OneWire y detecta la presencia del DS18B20.
 *
 * @return 1 si el sensor responde con pulso de presencia.
 * @return 0 si no se detecta el sensor.
 */
unsigned char DS18B20_Reset(void);

/**
 * @brief Escribe un byte en el bus OneWire.
 *
 * @param byte Byte que se desea enviar al DS18B20.
 */
void DS18B20_WriteByte(unsigned char byte);

/**
 * @brief Lee un byte desde el bus OneWire.
 *
 * @return Byte leído desde el DS18B20.
 */
unsigned char DS18B20_ReadByte(void);

/**
 * @brief Inicia una conversión de temperatura en el DS18B20.
 *
 * @return 1 si la conversión se inició correctamente.
 * @return 0 si no se detectó el sensor.
 *
 * @note Esta función no espera a que la conversión termine.
 */
unsigned char DS18B20_StartConversion(void);

/**
 * @brief Espera el tiempo necesario para finalizar la conversión.
 *
 * Esta función realiza una espera bloqueante de aproximadamente 800 ms,
 * suficiente para una conversión del DS18B20 en resolución de 12 bits.
 */
void DS18B20_WaitConversion(void);

/**
 * @brief Lee la temperatura medida por el DS18B20.
 *
 * @return Temperatura en grados Celsius si la lectura es válida.
 * @return -999.0f si no se detecta el sensor.
 * @return -998.0f si la lectura del scratchpad es inválida.
 * @return -997.0f si se obtiene el valor de power-on reset de 85 °C.
 *
 * @note Debe llamarse después de iniciar una conversión y esperar el tiempo
 * suficiente para que el sensor termine la medición.
 */
float DS18B20_ReadTemp(void);

#endif /* DS18B20_H */