/**
 * @file oled.h
 * @brief Archivo de cabecera para el manejo de pantalla OLED SSD1306 128x64 por I2C.
 *
 * Este archivo declara las funciones utilizadas para controlar una pantalla
 * OLED basada en el controlador SSD1306. La comunicación se realiza mediante
 * I2C por software usando los pines RB0 y RB1 del PIC18F4550.
 *
 * La pantalla OLED se usa en el proyecto Cardio.X para mostrar:
 * - Pantalla de inicio.
 * - Temperatura medida por el DS18B20.
 * - Ritmo cardíaco calculado con el MAX30102.
 * - Perfil seleccionado.
 * - Estado de alarma.
 * - Estado de pausa.
 * - Mensajes de error.
 *
 * Características principales:
 * - Resolución: 128 x 64 píxeles.
 * - Organización interna: 8 páginas de 128 columnas.
 * - Cada byte representa 8 píxeles verticales.
 * - Dirección I2C típica del SSD1306: 0x3C.
 * - Trama I2C desplazada usada en el código: 0x78.
 *
 * @note Este módulo depende del driver I2C definido en i2c.h.
 */

#ifndef OLED_H
#define OLED_H

/**
 * @brief Inicializa la pantalla OLED SSD1306.
 *
 * Esta función configura el controlador SSD1306 mediante una secuencia de
 * comandos de inicialización. Debe llamarse antes de escribir texto o datos
 * en la pantalla.
 *
 * Generalmente se llama en el main después de inicializar el bus I2C.
 */
void OLED_Init(void);

/**
 * @brief Envía un comando al controlador SSD1306.
 *
 * Los comandos permiten configurar el comportamiento de la pantalla, como
 * encendido, modo de direccionamiento, contraste, posición de memoria y otros
 * parámetros internos del controlador.
 *
 * @param cmd Comando que se enviará al SSD1306.
 */
void OLED_Command(unsigned char cmd);

/**
 * @brief Envía un byte de datos gráficos a la pantalla OLED.
 *
 * Cada byte enviado representa una columna de 8 píxeles verticales dentro de
 * una página de memoria del SSD1306.
 *
 * @param data Byte de datos que se escribirá en la memoria gráfica de la OLED.
 */
void OLED_Data(unsigned char data);

/**
 * @brief Limpia completamente la pantalla OLED.
 *
 * Esta función borra todas las páginas y columnas de la pantalla, dejando la
 * memoria gráfica en cero.
 */
void OLED_Clear(void);

/**
 * @brief Limpia una línea o página específica de la pantalla OLED.
 *
 * La pantalla SSD1306 está organizada en 8 páginas. Cada página corresponde
 * a una franja horizontal de 8 píxeles de alto.
 *
 * @param linea Página o línea que se desea limpiar.
 *              Valores típicos: 0 a 7.
 */
void OLED_ClearLine(unsigned char linea);

/**
 * @brief Ubica el cursor en una columna y página específica.
 *
 * Antes de escribir texto o datos, se debe ubicar el cursor en la posición
 * deseada de la pantalla.
 *
 * @param col Columna horizontal de la pantalla.
 *            Valores típicos: 0 a 127.
 * @param page Página vertical de la pantalla.
 *             Valores típicos: 0 a 7.
 */
void OLED_SetCursor(unsigned char col, unsigned char page);

/**
 * @brief Escribe un carácter en la posición actual de la OLED.
 *
 * La función utiliza una fuente básica de 5x7 píxeles. Cada carácter ocupa
 * normalmente 6 columnas: 5 columnas de dibujo y 1 columna de separación.
 *
 * @param c Carácter que se desea escribir en pantalla.
 *
 * @note La fuente incluida soporta principalmente espacio, números, letras
 * mayúsculas y algunos símbolos básicos.
 */
void OLED_WriteChar(char c);

/**
 * @brief Escribe una cadena de texto en la OLED.
 *
 * Esta función escribe carácter por carácter hasta encontrar el final de la
 * cadena. Internamente usa OLED_WriteChar().
 *
 * @param str Cadena de texto que se desea mostrar.
 */
void OLED_WriteString(char *str);

/**
 * @brief Muestra la pantalla de inicio del sistema Cardio.X.
 *
 * Esta función presenta una vista inicial o splash screen cuando el sistema
 * arranca.
 */
void OLED_Splash(void);

/**
 * @brief Muestra los signos vitales y el estado del sistema en la OLED.
 *
 * Esta función presenta en pantalla la temperatura, el ritmo cardíaco, el
 * estado de alarma y el perfil seleccionado.
 *
 * @param temp_int Parte entera de la temperatura medida.
 * @param temp_dec Parte decimal de la temperatura medida.
 * @param bpm Ritmo cardíaco en latidos por minuto.
 * @param alarma Estado de alarma.
 *               - 0: sin alarma.
 *               - 1: con alarma.
 * @param perfil Perfil activo.
 *               - 0: perfil normal.
 *               - 1: perfil deportista.
 */
void OLED_Vitals(unsigned char temp_int, unsigned char temp_dec,
                 unsigned char bpm, unsigned char alarma,
                 unsigned char perfil);

/**
 * @brief Muestra la pantalla de pausa del sistema.
 *
 * Esta función se usa cuando el sistema no detecta actividad, es decir,
 * cuando no hay dedo en el MAX30102 ni una condición válida asociada a la
 * temperatura del DS18B20.
 */
void OLED_Paused(void);

/**
 * @brief Muestra un mensaje de error en la pantalla OLED.
 *
 * Se utiliza cuando ocurre un problema durante la inicialización o lectura
 * de algún sensor, por ejemplo error del MAX30102 o del DS18B20.
 *
 * @param msg Mensaje de error que se desea mostrar.
 */
void OLED_Error(char *msg);

#endif /* OLED_H */