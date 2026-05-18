/**
 * @file    oled.h
 * @brief   Driver para pantalla OLED SSD1306 de 128x64 pixeles via I2C.
 *
 * La pantalla se organiza en 8 paginas de 8 pixeles de alto cada una,
 * por 128 columnas. Cada byte de datos representa una columna de 8 pixeles
 * verticales (bit 0 = fila superior de la pagina).
 *
 * Direccion I2C: 0x3C (7 bits). El driver usa las funciones de i2c.h
 * que manejan el desplazamiento de direccion internamente.
 *
 * Cambios respecto al proyecto anterior:
 *  - Usa I2C_WriteReg() en lugar de llamadas directas a I2C_Start/Write/Stop.
 *  - OLED_ADDR definido como direccion de 7 bits (0x3C), no desplazada.
 *  - Se agrega OLED_WriteInt() para mostrar enteros sin usar sprintf/float.
 *  - Se agrega OLED_WriteTemp() para mostrar temperatura con un decimal.
 */

#ifndef OLED_H
#define OLED_H

#include "config.h"

/* =========================================================
 *  CONSTANTES DE LA PANTALLA
 * ========================================================= */

#define OLED_ADDR       0x3C   /**< Direccion I2C de 7 bits del SSD1306     */
#define OLED_PAGES         8   /**< Numero de paginas (8 filas de 8 px)     */
#define OLED_COLS        128   /**< Numero de columnas de pixeles            */
#define OLED_COL_OFFSET    2   /**< Offset de hardware del controlador SSD1306 */

/* =========================================================
 *  PROTOTIPOS DE FUNCIONES PUBLICAS
 * ========================================================= */

/**
 * @brief  Inicializa el controlador SSD1306 con la secuencia estandar.
 *         Debe llamarse una vez tras I2C_Init() y antes de cualquier
 *         operacion de escritura en la pantalla.
 */
void OLED_Init(void);

/**
 * @brief  Borra toda la pantalla (escribe ceros en todos los pixeles).
 */
void OLED_Clear(void);

/**
 * @brief  Borra una pagina completa (una fila de texto de 8 pixeles de alto).
 * @param  page  Pagina a borrar (0 a 7).
 */
void OLED_ClearLine(unsigned char page);

/**
 * @brief  Posiciona el cursor en una pagina y columna especificas.
 * @param  col   Columna de inicio en pixeles (0 a 127).
 * @param  page  Pagina de inicio (0 a 7).
 */
void OLED_SetCursor(unsigned char col, unsigned char page);

/**
 * @brief  Escribe un caracter en la posicion actual del cursor.
 *         Avanza el cursor automaticamente 6 pixeles (5 de caracter + 1 espacio).
 * @param  c  Caracter a mostrar. Soporta: espacio, '0'-'9', 'A'-'Z', ':', '%', '.'.
 */
void OLED_WriteChar(char c);

/**
 * @brief  Escribe una cadena de caracteres en la posicion actual del cursor.
 * @param  str  Cadena terminada en '\0'. Solo caracteres soportados por la fuente.
 */
void OLED_WriteString(const char *str);

/**
 * @brief  Escribe un numero entero sin signo en la posicion actual del cursor.
 *         Evita el uso de sprintf con enteros, que consume mucha memoria en XC8.
 * @param  value  Valor a mostrar (0 a 65535).
 */
void OLED_WriteInt(unsigned int value);

/**
 * @brief  Escribe una temperatura en formato "XX.X" usando aritmetica entera.
 *         El valor se pasa en decimas de grado para evitar flotantes.
 *         Ejemplo: 365 -> muestra "36.5"
 * @param  decimas  Temperatura en decimas de grado Celsius.
 */
void OLED_WriteTemp(unsigned int decimas);

#endif /* OLED_H */