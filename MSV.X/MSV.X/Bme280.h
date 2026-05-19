/**
 * @file    bme280.h
 * @brief   Driver para el sensor BME280 ? lectura de temperatura calibrada.
 *
 * El BME280 almacena en su memoria interna (NVM) coeficientes de calibracion
 * unicos por unidad, grabados en fabrica. Sin aplicar estos coeficientes,
 * la temperatura cruda puede desviarse varios grados del valor real.
 *
 * Este driver implementa la formula de compensacion entera de 32 bits
 * documentada por Bosch en el datasheet del BME280 (seccion 4.2.3),
 * completamente sin flotantes. El resultado se entrega en decimas de grado
 * Celsius para compatibilidad con OLED_WriteTemp() y la logica de alarmas.
 *
 * Modo de operacion: Forced Mode.
 *  En cada ciclo el PIC ordena una medicion, espera ~5 ms y lee el resultado.
 *  El sensor vuelve a sleep automaticamente, reduciendo consumo.
 *  Solo se mide temperatura (presion y humedad desactivadas).
 *
 * Direccion I2C: 0x76 (SDO conectado a GND).
 *
 * Diferencia respecto al proyecto anterior:
 *  El proyecto anterior solo leia humedad y no aplicaba calibracion.
 *  Este driver lee temperatura con compensacion completa de fabrica.
 *
 * @note  Requiere i2c.h inicializado antes de llamar a BME280_Init().
 */

#ifndef BME280_H
#define BME280_H

#include "config.h"

/* =========================================================
 *  CONSTANTES DEL DISPOSITIVO
 * ========================================================= */

#define BME280_ADDR       0x76   /**< Direccion I2C de 7 bits (SDO=GND)      */
#define BME280_CHIP_ID    0x60   /**< ID de chip esperado en registro 0xD0    */

/* =========================================================
 *  CODIGOS DE RETORNO
 * ========================================================= */

#define BME280_OK           0   /**< Operacion exitosa                        */
#define BME280_ERR_NODEV    1   /**< Sensor no encontrado en el bus I2C       */
#define BME280_ERR_I2C      2   /**< Error de comunicacion I2C                */

/* =========================================================
 *  PROTOTIPOS PUBLICOS
 * ========================================================= */

/**
 * @brief  Verifica la presencia del BME280, lee sus coeficientes de
 *         calibracion de fabrica y configura el sensor en modo temperatura
 *         unicamente (presion y humedad desactivadas).
 *
 * Debe llamarse una vez tras I2C_Init() y antes de BME280_ReadTemp().
 *
 * @return BME280_OK       si el sensor responde y se inicializa correctamente.
 * @return BME280_ERR_NODEV si el chip ID no coincide (sensor ausente o daniado).
 * @return BME280_ERR_I2C  si hay fallo en la comunicacion I2C.
 */
unsigned char BME280_Init(void);

/**
 * @brief  Dispara una medicion en Forced Mode, espera su finalizacion y
 *         retorna la temperatura compensada en decimas de grado Celsius.
 *
 * Ejemplo: retorna 365 para una temperatura de 36.5 C.
 * Rango valido: 0 a 850 (0.0 C a 85.0 C segun datasheet BME280).
 *
 * En caso de error de comunicacion retorna el ultimo valor valido,
 * o 0 si aun no hay medicion previa.
 *
 * @return Temperatura en decimas de grado Celsius (unsigned int).
 */
unsigned int BME280_ReadTemp(void);

#endif /* BME280_H */