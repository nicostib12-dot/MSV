/**
 * @file    max30102.h
 * @brief   Driver para el sensor MAX30102 ? medicion de frecuencia cardiaca.
 *
 * El MAX30102 es un sensor de fotopletismografia (PPG) que emite luz LED
 * y mide el reflejo sobre el tejido del dedo. La variacion en la intensidad
 * reflejada corresponde al pulso cardiaco. El sensor NO entrega BPM
 * directamente ? entrega muestras crudas del fotodetector que deben
 * procesarse para extraer la frecuencia cardiaca.
 *
 * Modo de operacion: Heart Rate (HR) ? solo LED Rojo activo.
 *  - 3 bytes por muestra en el FIFO.
 *  - Tasa de muestreo: 50 Hz.
 *  - Resolucion ADC: 18 bits.
 *
 * Algoritmo de deteccion de BPM:
 *  Se mantiene un buffer deslizante de 100 muestras (2 segundos a 50 Hz).
 *  En cada ciclo del sistema se agregan ~50 muestras nuevas y se descartan
 *  las 50 mas antiguas. Sobre el buffer completo se calcula:
 *   1. Umbral adaptativo (70% del rango min-max).
 *   2. Deteccion de flancos ascendentes (inicio de cada pico PPG).
 *   3. Intervalo promedio entre picos en muestras.
 *   4. BPM = (60 * SAMPLE_RATE) / intervalo_promedio.
 *
 * Limitaciones conocidas del algoritmo simplificado:
 *  - Requiere que el dedo este quieto sobre el sensor.
 *  - El primer valor de BPM solo esta disponible tras 2 segundos de datos.
 *  - En senales muy debiles (dedo mal colocado) retorna el ultimo BPM valido.
 *
 * Direccion I2C: 0x57 (fija, no configurable en el MAX30102).
 *
 * @note  Verificar que el Part ID (registro 0xFF) retorne 0x15.
 *        Algunos modulos clon pueden retornar valores distintos.
 *        En ese caso comentar la verificacion en MAX30102_Init().
 */

#ifndef MAX30102_H
#define MAX30102_H

#include "config.h"

/* =========================================================
 *  CONSTANTES DEL DISPOSITIVO
 * ========================================================= */

#define MAX30102_ADDR          0x57   /**< Direccion I2C de 7 bits (fija)          */
#define MAX30102_PART_ID       0x15   /**< Valor esperado en registro 0xFF          */

/* =========================================================
 *  PARAMETROS DEL ALGORITMO
 * ========================================================= */

#define MAX30102_SAMPLE_RATE   50U    /**< Tasa de muestreo en Hz                  */
#define MAX30102_BUF_SIZE      100U   /**< Tamano del buffer deslizante (muestras) */
#define MAX30102_HALF_BUF      50U    /**< Muestras nuevas leidas por ciclo        */

/** Amplitud minima de la senal para considerarla valida (en cuentas ADC 16-bit).
 *  Si (max - min) del buffer es menor a este valor, la senal se considera
 *  ruido o ausencia de dedo. Valor conservador: ajustar si hay falsos positivos. */
#define MAX30102_MIN_AMPLITUDE 1000U

/* =========================================================
 *  CODIGOS DE RETORNO
 * ========================================================= */

#define MAX30102_OK            0   /**< Operacion exitosa                           */
#define MAX30102_ERR_NODEV     1   /**< Sensor no encontrado o Part ID incorrecto   */
#define MAX30102_ERR_I2C       2   /**< Error en la comunicacion I2C                */

/* =========================================================
 *  PROTOTIPOS PUBLICOS
 * ========================================================= */

/**
 * @brief  Verifica el Part ID del MAX30102, realiza reset y configura el
 *         sensor en modo Heart Rate a 50 Hz con resolucion de 18 bits.
 *
 * Debe llamarse una vez tras I2C_Init() y antes de MAX30102_GetBPM().
 *
 * @return MAX30102_OK       si el sensor responde correctamente.
 * @return MAX30102_ERR_NODEV si Part ID incorrecto (sensor ausente o clon).
 * @return MAX30102_ERR_I2C  si hay fallo en la comunicacion I2C.
 */
unsigned char MAX30102_Init(void);

/**
 * @brief  Lee muestras nuevas del FIFO, actualiza el buffer deslizante y
 *         retorna la frecuencia cardiaca estimada en BPM.
 *
 * Llamar periodicamente desde el bucle principal (una vez por segundo).
 * El primer valor valido estara disponible tras ~2 segundos de datos.
 *
 * Si no hay suficiente senal, retorna el ultimo BPM valido o 0 si no
 * hay medicion previa disponible.
 *
 * @return Frecuencia cardiaca estimada en latidos por minuto (30 a 250).
 *         Retorna 0 si no hay datos suficientes aun.
 */
unsigned int MAX30102_GetBPM(void);

#endif /* MAX30102_H */