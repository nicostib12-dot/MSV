/**
 * @file    i2c.h
 * @brief   Driver I2C maestro por hardware (modulo MSSP) para el PIC18F4550.
 *
 * Pines fisicos:
 *  - RB0 (pin 33) : SDA  (datos)
 *  - RB1 (pin 34) : SCL  (reloj)
 *
 * Velocidad configurada: 100 kHz (modo estandar).
 * Formula: SSPADD = (Fosc / (4 * Fscl)) - 1 = (8000000 / 400000) - 1 = 19
 *
 * Diferencia clave respecto al proyecto anterior:
 *  El proyecto anterior usaba bit-banging (control manual de pines con
 *  retardos __delay_us). Este modulo usa el periferico MSSP del PIC, que
 *  maneja el protocolo I2C por hardware: timing, start, stop y ACK son
 *  generados automaticamente, liberando la CPU y garantizando estabilidad.
 *
 * Codigos de retorno:
 *  @see I2C_OK
 *  @see I2C_ERR_TIMEOUT
 *  @see I2C_ERR_NACK
 *
 * @note  Requiere resistencias pull-up externas en SDA y SCL (tipicamente
 *        4.7 kohm conectadas a VDD). Las pull-ups internas del PORTB no
 *        son suficientes para I2C por hardware a 100 kHz.
 */

#ifndef I2C_H
#define I2C_H

#include "config.h"

/* =========================================================
 *  CODIGOS DE RETORNO
 * ========================================================= */

#define I2C_OK           0   /**< Operacion completada sin errores          */
#define I2C_ERR_TIMEOUT  1   /**< El bus no respondio dentro del tiempo max */
#define I2C_ERR_NACK     2   /**< El esclavo rechazo la direccion o el dato */

/* =========================================================
 *  PROTOTIPOS DE FUNCIONES PUBLICAS
 * ========================================================= */

/**
 * @brief  Inicializa el modulo MSSP en modo I2C maestro a 100 kHz.
 */
void I2C_Init(void);

/**
 * @brief  Genera la condicion de START en el bus I2C.
 * @return I2C_OK si tuvo exito, I2C_ERR_TIMEOUT si el bus no quedo libre.
 */
unsigned char I2C_Start(void);

/**
 * @brief  Genera la condicion de STOP en el bus I2C.
 * @return I2C_OK si tuvo exito, I2C_ERR_TIMEOUT en caso contrario.
 */
unsigned char I2C_Stop(void);

/**
 * @brief  Genera una condicion de START repetido (Repeated Start).
 *         Se usa para cambiar de escritura a lectura sin soltar el bus.
 * @return I2C_OK si tuvo exito, I2C_ERR_TIMEOUT en caso contrario.
 */
unsigned char I2C_RepeatStart(void);

/**
 * @brief  Transmite un byte por el bus I2C y verifica el ACK del esclavo.
 * @param  data  Byte a enviar (puede ser direccion + R/W o dato).
 * @return I2C_OK si el esclavo respondio con ACK,
 *         I2C_ERR_NACK si respondio con NACK,
 *         I2C_ERR_TIMEOUT si el bus no completo la transferencia.
 */
unsigned char I2C_Write(unsigned char data);

/**
 * @brief  Recibe un byte del bus I2C.
 * @param  ack  1 -> envia ACK  (maestro quiere mas bytes).
 *              0 -> envia NACK (ultimo byte, finaliza la lectura).
 * @return Byte recibido del esclavo.
 */
unsigned char I2C_Read(unsigned char ack);

/**
 * @brief  Escribe un byte en un registro interno de un dispositivo I2C.
 *         Secuencia: START -> dir+W -> reg -> dato -> STOP.
 * @param  addr  Direccion I2C de 7 bits del dispositivo.
 * @param  reg   Registro destino dentro del dispositivo.
 * @param  data  Valor a escribir.
 * @return I2C_OK si tuvo exito, codigo de error en caso contrario.
 */
unsigned char I2C_WriteReg(unsigned char addr,
                           unsigned char reg,
                           unsigned char data);

/**
 * @brief  Lee un byte de un registro interno de un dispositivo I2C.
 *         Secuencia: START -> dir+W -> reg -> RepStart -> dir+R -> dato -> STOP.
 * @param  addr  Direccion I2C de 7 bits del dispositivo.
 * @param  reg   Registro a leer dentro del dispositivo.
 * @return Byte leido. En caso de error retorna 0xFF.
 */
unsigned char I2C_ReadReg(unsigned char addr, unsigned char reg);

/**
 * @brief  Lee multiples bytes consecutivos de un dispositivo I2C.
 *         Secuencia: START -> dir+W -> reg -> RepStart -> dir+R
 *                    -> byte0..byteN-1 (ACK) -> byteN (NACK) -> STOP.
 * @param  addr   Direccion I2C de 7 bits del dispositivo.
 * @param  reg    Registro inicial de la lectura.
 * @param  buf    Buffer donde se almacenan los bytes leidos.
 * @param  len    Numero de bytes a leer.
 * @return I2C_OK si tuvo exito, codigo de error en caso contrario.
 */
unsigned char I2C_ReadBurst(unsigned char  addr,
                            unsigned char  reg,
                            unsigned char *buf,
                            unsigned char  len);

#endif /* I2C_H */