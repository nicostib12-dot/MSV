/**
 * @file    i2c.h
 * @brief   Interfaz pública del módulo I2C por software (bit-banging).
 *
 * Implementa el protocolo I2C estándar usando dos pines GPIO del PORTB:
 *   - **RB0** ? línea SCL (reloj)
 *   - **RB1** ? línea SDA (datos)
 *
 * Las resistencias pull-up internas del PORTB se habilitan en `I2C_Init()`
 * eliminando la necesidad de resistencias externas en placas de prototipado.
 *
 * Velocidad aproximada: **~50 kHz** (modo estándar lento), determinada por
 * los retardos de 5 µs entre cada transición de señal.
 *
 * Todos los dispositivos I2C del sistema comparten este bus:
 *   - Pantalla OLED SSD1306  (dirección 0x3C)
 *   - Sensor de humedad BME280 (dirección 0x76)
 *   - Sensor de luz BH1750   (dirección 0x23)
 *
 * @note   Esta implementación no admite modo multi-maestro ni detección
 *         de colisiones en el bus. Apta para sistemas de un solo maestro
 *         como este proyecto.
 */

#ifndef I2C_H
#define I2C_H
/** @brief  Configura los pines SCL (RB0) y SDA (RB1) como salidas y los pone en alto (bus libre). Habilita pull-ups internos del PORTB. */
void I2C_Init(void);

/** @brief  Genera la condición de START del protocolo I2C. Flanco de bajada de SDA mientras SCL está en alto. */
void I2C_Start(void);

/** @brief  Genera la condición de STOP del protocolo I2C. Flanco de subida de SDA mientras SCL está en alto. */
void I2C_Stop(void);

/**
 * @brief  Transmite un byte por el bus I2C, bit más significativo primero.
 *
 * Por cada bit: coloca el bit en SDA, sube SCL (el esclavo muestrea),
 * baja SCL. Al terminar los 8 bits genera un pulso adicional de SCL
 * para recibir el bit ACK/NACK del esclavo (se descarta en esta implementación).
 *
 * @param  data  Byte a transmitir (dirección I2C con bit R/W o dato).
 */
void I2C_Write(unsigned char data);

/**
 * @brief  Recibe un byte del bus I2C, bit más significativo primero.
 *
 * Por cada bit: sube SCL, lee SDA, baja SCL.
 * Al terminar los 8 bits envía ACK o NACK según el parámetro.
 *
 * @param  ack  `1` ? enviar ACK  (el maestro espera más bytes del esclavo).
 *              `0` ? enviar NACK (último byte de la trama, finalizar lectura).
 * @return Byte recibido del dispositivo esclavo.
 */
unsigned char I2C_Read(char ack);

/**
 * @brief  Placeholder para condición de Repeated START.
 * @note   Declarada por compatibilidad futura; no implementada en i2c.c.
 *         En su lugar, el código actual usa STOP + START consecutivos.
 */
void I2C_RepeatedStart(void);


#endif /* I2C_H */
