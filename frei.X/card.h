/**
 * @file card.h
 * @brief Archivo de cabecera para el procesamiento de ritmo cardiaco.
 *
 * Este archivo declara las funciones utilizadas para procesar las muestras
 * obtenidas desde el sensor MAX30102. Permite detectar si hay un dedo sobre
 * el sensor, calcular el ritmo cardiaco en BPM y consultar el último valor
 * válido obtenido.
 */

#ifndef CAR_H
#define CAR_H

#include <xc.h>

/**
 * @brief Última variación calculada de la señal infrarroja.
 *
 * Esta variable almacena el último delta obtenido al comparar el promedio
 * anterior y el promedio actual de la señal IR. Puede utilizarse para
 * depuración o para analizar el comportamiento de la señal del MAX30102.
 */
extern long CAR_ultimo_delta;

/**
 * @brief Determina si hay un dedo colocado sobre el sensor MAX30102.
 *
 * La detección se realiza comparando el valor de la señal infrarroja con
 * un umbral definido en el archivo `card.c`.
 *
 * @param ir Valor de la muestra infrarroja leída desde el MAX30102.
 *
 * @return 1 si se detecta dedo.
 * @return 0 si no se detecta dedo.
 */
unsigned char CAR_HayDedo(unsigned long ir);

/**
 * @brief Procesa una muestra del MAX30102 para calcular el BPM.
 *
 * Esta función recibe las muestras roja e infrarroja del sensor MAX30102.
 * El cálculo del ritmo cardiaco se realiza principalmente con la señal
 * infrarroja, ya que esta permite detectar variaciones asociadas al pulso.
 *
 * @param red Muestra del LED rojo del MAX30102.
 * @param ir Muestra del LED infrarrojo del MAX30102.
 *
 * @return Último valor válido de BPM calculado.
 * @return 0 si no hay dedo detectado.
 *
 * @note Aunque la función recibe la muestra roja, el algoritmo actual usa
 * principalmente la señal IR para estimar el ritmo cardiaco.
 */
unsigned char CAR_ProcesarMuestra(unsigned long red, unsigned long ir);

/**
 * @brief Obtiene el último BPM válido calculado.
 *
 * Esta función permite consultar el valor actual de BPM sin procesar
 * una nueva muestra del sensor.
 *
 * @return Último valor de BPM calculado.
 */
unsigned char CAR_GetBPM(void);

#endif