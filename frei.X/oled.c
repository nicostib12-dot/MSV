/**
 * @file  oled.c
 * @brief Driver OLED SSD1306 128x64 ? compatible con tu i2c.c (RB0/RB1)
 *
 * COMO USAR (orden obligatorio):
 *   1. I2C_Init();    <- tu funcion existente
 *   2. OLED_Init();   <- configura el SSD1306
 *   3. OLED_Clear();  <- pantalla en negro
 *   4. OLED_SetCursor(0, 0);
 *   5. OLED_WriteString("HOLA");
 *
 * PRUEBA MINIMA (pega esto en main() para verificar que funciona):
 *   I2C_Init();
 *   OLED_Init();
 *   OLED_Clear();
 *   OLED_SetCursor(0, 0);
 *   OLED_WriteString("PRUEBA OLED");
 *   OLED_SetCursor(0, 2);
 *   OLED_WriteString("PIC18F4550");
 *   while(1);
 */

#include <xc.h>
#include "config.h"
#include "i2c.h"
#include "oled.h"

/* ?????????????????????????????????????????????????????????
 * DIRECCION I2C
 * 0x3C es la direccion de 7 bits del SSD1306.
 * Se desplaza 1 bit a la izquierda para formar la trama I2C:
 *   0x3C << 1 = 0x78
 * El bit menos significativo (R/W) lo agrega I2C_Write
 * implicitamente al poner 0 (escritura).
 * ????????????????????????????????????????????????????????? */
#define OLED_ADDR  0x78

/* ?????????????????????????????????????????????????????????
 * TABLA DE FUENTE 5x7
 *
 * Cada caracter = 5 bytes (una columna por byte).
 * Cada byte = 8 bits = 8 pixeles verticales.
 * Bit 0 = pixel superior de la pagina.
 * Bit 6 = pixel inferior (bit 7 no se usa en fuente 7px).
 *
 * Ejemplo: letra 'A' (indice 11)
 *   Col0: 0x7E = 0111 1110 -> pixeles 1-6 encendidos
 *   Col1: 0x11 = 0001 0001 -> pixeles 0 y 4 encendidos
 *   Col2: 0x11 = 0001 0001 -> igual
 *   Col3: 0x11 = 0001 0001 -> igual
 *   Col4: 0x7E = 0111 1110 -> pixeles 1-6 encendidos
 *
 * Indices:
 *   [0]    = espacio ' '
 *   [1-10] = '0' a '9'
 *   [11-36]= 'A' a 'Z'
 *   [37]   = ':'
 *   [38]   = '%'
 *   [39]   = '.'
 * ????????????????????????????????????????????????????????? */
const unsigned char font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* [ 0] espacio */
    {0x3E,0x51,0x49,0x45,0x3E}, /* [ 1] '0'     */
    {0x00,0x42,0x7F,0x40,0x00}, /* [ 2] '1'     */
    {0x42,0x61,0x51,0x49,0x46}, /* [ 3] '2'     */
    {0x21,0x41,0x45,0x4B,0x31}, /* [ 4] '3'     */
    {0x18,0x14,0x12,0x7F,0x10}, /* [ 5] '4'     */
    {0x27,0x45,0x45,0x45,0x39}, /* [ 6] '5'     */
    {0x3C,0x4A,0x49,0x49,0x30}, /* [ 7] '6'     */
    {0x01,0x71,0x09,0x05,0x03}, /* [ 8] '7'     */
    {0x36,0x49,0x49,0x49,0x36}, /* [ 9] '8'     */
    {0x06,0x49,0x49,0x29,0x1E}, /* [10] '9'     */
    {0x7E,0x11,0x11,0x11,0x7E}, /* [11] 'A'     */
    {0x7F,0x49,0x49,0x49,0x36}, /* [12] 'B'     */
    {0x3E,0x41,0x41,0x41,0x22}, /* [13] 'C'     */
    {0x7F,0x41,0x41,0x22,0x1C}, /* [14] 'D'     */
    {0x7F,0x49,0x49,0x49,0x41}, /* [15] 'E'     */
    {0x7F,0x09,0x09,0x09,0x01}, /* [16] 'F'     */
    {0x3E,0x41,0x49,0x49,0x7A}, /* [17] 'G'     */
    {0x7F,0x08,0x08,0x08,0x7F}, /* [18] 'H'     */
    {0x00,0x41,0x7F,0x41,0x00}, /* [19] 'I'     */
    {0x20,0x40,0x41,0x3F,0x01}, /* [20] 'J'     */
    {0x7F,0x08,0x14,0x22,0x41}, /* [21] 'K'     */
    {0x7F,0x40,0x40,0x40,0x40}, /* [22] 'L'     */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* [23] 'M'     */
    {0x7F,0x04,0x08,0x10,0x7F}, /* [24] 'N'     */
    {0x3E,0x41,0x41,0x41,0x3E}, /* [25] 'O'     */
    {0x7F,0x09,0x09,0x09,0x06}, /* [26] 'P'     */
    {0x3E,0x41,0x51,0x21,0x5E}, /* [27] 'Q'     */
    {0x7F,0x09,0x19,0x29,0x46}, /* [28] 'R'     */
    {0x46,0x49,0x49,0x49,0x31}, /* [29] 'S'     */
    {0x01,0x01,0x7F,0x01,0x01}, /* [30] 'T'     */
    {0x3F,0x40,0x40,0x40,0x3F}, /* [31] 'U'     */
    {0x1F,0x20,0x40,0x20,0x1F}, /* [32] 'V'     */
    {0x3F,0x40,0x38,0x40,0x3F}, /* [33] 'W'     */
    {0x63,0x14,0x08,0x14,0x63}, /* [34] 'X'     */
    {0x07,0x08,0x70,0x08,0x07}, /* [35] 'Y'     */
    {0x61,0x51,0x49,0x45,0x43}, /* [36] 'Z'     */
    {0x00,0x00,0x5F,0x00,0x00}, /* [37] ':'     */
    {0x00,0x06,0x09,0x09,0x06}, /* [38] '%'     */
    {0x00,0x00,0x08,0x00,0x00}, /* [39] '.'     */
};

/* =========================================================
 *  FUNCION INTERNA: convierte numero a string en buffer
 *  Sin usar sprintf (ahorra Flash y no necesita stdio.h).
 *
 *  Ejemplo: num=365, buf -> "365\0"
 *  Se usa internamente en OLED_Vitals().
 * ========================================================= */
static void uint_to_str(unsigned int num, char *buf)
{
    unsigned char i = 0;
    unsigned char j = 0;
    char tmp[6];

    if (num == 0) { buf[0]='0'; buf[1]='\0'; return; }

    while (num > 0) {
        tmp[i++] = '0' + (num % 10);
        num /= 10;
    }
    /* Invertir digitos */
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = '\0';
}

/* =========================================================
 *  PRIMITIVAS I2C PARA EL SSD1306
 * ========================================================= */

/**
 * @brief Envia un byte de COMANDO al SSD1306.
 *
 * El byte de control 0x00 le indica al chip que lo siguiente
 * es un comando de configuracion (no datos de pixeles).
 *
 * Trama completa:
 *   [START][0x78][0x00][cmd][STOP]
 */
void OLED_Command(unsigned char cmd)
{
    I2C_Start();
    I2C_Write(OLED_ADDR); /* Direccion + bit W=0 */
    I2C_Write(0x00);       /* Byte de control: modo COMANDO */
    I2C_Write(cmd);        /* El comando en si               */
    I2C_Stop();
}

/**
 * @brief Envia un byte de DATOS (pixeles) al SSD1306.
 *
 * El byte de control 0x40 indica que lo siguiente son datos
 * para la memoria GDDRAM (pixeles de la pantalla).
 * El puntero de columna avanza automaticamente tras cada byte.
 *
 * Trama completa:
 *   [START][0x78][0x40][data][STOP]
 */
void OLED_Data(unsigned char data)
{
    I2C_Start();
    I2C_Write(OLED_ADDR);
    I2C_Write(0x40);  /* Byte de control: modo DATO */
    I2C_Write(data);
    I2C_Stop();
}

/* =========================================================
 *  INICIALIZACION DEL SSD1306
 *
 *  Esta secuencia es obligatoria. El SSD1306 arranca en un
 *  estado indefinido y necesita estos 17 comandos para
 *  quedar en un estado de funcionamiento correcto.
 *
 *  Si omites alguno o cambias el orden, la pantalla puede:
 *    - No mostrar nada (pero si recibir datos)
 *    - Mostrar basura o ruido
 *    - Mostrar imagen invertida o espejada
 * ========================================================= */
void OLED_Init(void)
{
    __delay_ms(200); /* Esperar que el SSD1306 tenga VCC estable */

    OLED_Command(0xAE);           /* 1. Display OFF (configurar con display apagado) */

    OLED_Command(0xD5);           /* 2. Set Display Clock Divide Ratio              */
    OLED_Command(0xF0);           /*    Ratio=0xF0: reloj rapido, sin division       */

    OLED_Command(0xA8);           /* 3. Set Multiplex Ratio                          */
    OLED_Command(0x3F);           /*    0x3F = 63 -> 64 filas (pantalla de 64px)    */

    OLED_Command(0xD3);           /* 4. Set Display Offset                           */
    OLED_Command(0x00);           /*    Sin desplazamiento vertical                  */

    OLED_Command(0x40);           /* 5. Set Start Line = linea 0                     */

    OLED_Command(0x8D);           /* 6. Charge Pump Setting                          */
    OLED_Command(0x14);           /*    0x14 = activar bomba interna (sin VCC ext.)  */

    OLED_Command(0x20);           /* 7. Set Memory Addressing Mode                   */
    OLED_Command(0x02);           /*    0x02 = Page Addressing Mode                  */

    OLED_Command(0xA1);           /* 8. Set Segment Re-map: columna 127 -> SEG0      */
                                  /*    Espejo horizontal ON (imagen normal)          */

    OLED_Command(0xC8);           /* 9. Set COM Output Scan Direction: invertido      */
                                  /*    Escanea de COM63 a COM0 (imagen normal)       */

    OLED_Command(0xDA);           /* 10. Set COM Pins Hardware Configuration          */
    OLED_Command(0x12);           /*     0x12 = pines COM alternados, sin remapeo    */

    OLED_Command(0x81);           /* 11. Set Contrast Control                         */
    OLED_Command(0xFF);           /*     0xFF = contraste maximo (mas brillante)      */

    OLED_Command(0xD9);           /* 12. Set Pre-charge Period                        */
    OLED_Command(0xF1);           /*     Ajuste de carga para VCC interno             */

    OLED_Command(0xDB);           /* 13. Set VCOMH Deselect Level                     */
    OLED_Command(0x40);           /*     0x40 = nivel tipico recomendado              */

    OLED_Command(0xA4);           /* 14. Entire Display ON: muestra contenido de RAM  */
                                  /*     (0xA5 enciende TODOS los pixeles para test)  */

    OLED_Command(0xA6);           /* 15. Set Normal Display (no invertido)             */
                                  /*     0xA7 invierte: pixel ON = oscuro             */

    OLED_Command(0xAF);           /* 16. Display ON                                   */

    __delay_ms(100); /* Pausa tras encender para estabilizacion */
}

/* =========================================================
 *  LIMPIEZA DE PANTALLA
 * ========================================================= */

/**
 * @brief Borra toda la pantalla llenandola con 0x00.
 *
 * Estrategia: una transaccion I2C por pagina con 128 bytes
 * cada una. Es mas eficiente que enviar byte a byte porque
 * reduce el overhead de START/STOP/ACK.
 *
 * Tiempo aproximado a 50 kHz: ~8 x (128 x 9 bits) / 50000
 * = ~185 ms. Nota: es lento por el bit-banging a 50 kHz.
 * Para refrescar lineas individuales usa OLED_ClearLine().
 */
void OLED_Clear(void)
{
    unsigned char page, i;

    for (page = 0; page < 8; page++)
    {
        OLED_Command(0xB0 + page); /* Seleccionar pagina 0-7     */
        OLED_Command(0x00);        /* Columna baja  = 0           */
        OLED_Command(0x10);        /* Columna alta  = 0           */

        /* Enviar 128 ceros en una sola transaccion I2C */
        I2C_Start();
        I2C_Write(OLED_ADDR);
        I2C_Write(0x40);           /* Modo dato (stream de bytes) */
        for (i = 0; i < 128; i++) I2C_Write(0x00);
        I2C_Stop();
    }
}

/**
 * @brief Borra solo una pagina (fila de 8px de alto).
 *
 * Usar en el ciclo de refresco para actualizar solo las
 * lineas que cambian, evitando el parpadeo de OLED_Clear().
 *
 * Ejemplo: para actualizar la linea de temperatura:
 *   OLED_ClearLine(2);
 *   OLED_SetCursor(0, 2);
 *   OLED_WriteString("TEMP: 37.1");
 *
 * @param linea  Numero de pagina: 0 (arriba) a 7 (abajo).
 */
void OLED_ClearLine(unsigned char linea)
{
    unsigned char i;

    OLED_Command(0xB0 + linea); /* Seleccionar pagina          */
    OLED_Command(0x02);         /* Columna baja = 2 (offset)   */
    OLED_Command(0x10);         /* Columna alta = 0            */

    I2C_Start();
    I2C_Write(OLED_ADDR);
    I2C_Write(0x40);
    for (i = 0; i < 128; i++) I2C_Write(0x00);
    I2C_Stop();
}

/* =========================================================
 *  POSICIONAMIENTO DEL CURSOR
 * ========================================================= */

/**
 * @brief Posiciona el cursor para la siguiente escritura.
 *
 * El SSD1306 tiene un offset de hardware de 2 columnas:
 * la columna 0 visible corresponde a la columna interna 2.
 * Esta funcion suma ese offset automaticamente.
 *
 * Mapa de paginas en pantalla de 64px:
 *   Pagina 0: pixeles Y  0- 7  (fila superior)
 *   Pagina 1: pixeles Y  8-15
 *   Pagina 2: pixeles Y 16-23
 *   Pagina 3: pixeles Y 24-31
 *   Pagina 4: pixeles Y 32-39
 *   Pagina 5: pixeles Y 40-47
 *   Pagina 6: pixeles Y 48-55
 *   Pagina 7: pixeles Y 56-63  (fila inferior)
 *
 * @param col   Columna 0-127 (0=izquierda).
 * @param page  Pagina  0-7   (0=arriba).
 */
void OLED_SetCursor(unsigned char col, unsigned char page)
{
    col += 2;                           /* Compensar offset de hardware     */
    OLED_Command(0xB0 + page);          /* Cmd: seleccionar pagina          */
    OLED_Command(0x00 | (col & 0x0F)); /* Cmd: nibble bajo de columna      */
    OLED_Command(0x10 | (col >> 4));   /* Cmd: nibble alto de columna      */
}

/* =========================================================
 *  ESCRITURA DE TEXTO
 * ========================================================= */

/**
 * @brief Dibuja un caracter en la posicion actual del cursor.
 *
 * Proceso:
 *  1. Determinar el indice del caracter en font5x7[].
 *  2. Enviar los 5 bytes del bitmap en una transaccion I2C.
 *  3. Enviar 1 byte 0x00 como columna de separacion.
 *
 * El cursor avanza automaticamente 6 columnas (5+1).
 *
 * Caracteres soportados: ' ' 0-9 A-Z : % .
 * Cualquier otro caracter se muestra como espacio en blanco.
 *
 * @param c  Caracter ASCII a dibujar.
 */
void OLED_WriteChar(char c)
{
    unsigned char i;
    int index = 0; /* Indice por defecto: espacio */

    /* Resolver indice segun el caracter recibido */
    if      (c == ' ')             index = 0;
    else if (c >= '0' && c <= '9') index = c - '0' + 1;   /* [1..10]  */
    else if (c >= 'A' && c <= 'Z') index = c - 'A' + 11;  /* [11..36] */
    else if (c == ':')             index = 37;
    else if (c == '%')             index = 38;
    else if (c == '.')             index = 39;
    /* Cualquier otro caracter: index=0 (espacio) */

    /* Enviar bitmap del caracter + columna de separacion */
    I2C_Start();
    I2C_Write(OLED_ADDR);
    I2C_Write(0x40);                           /* Modo dato              */
    for (i = 0; i < 5; i++) I2C_Write(font5x7[index][i]);
    I2C_Write(0x00);                           /* Columna separadora     */
    I2C_Stop();
}

/**
 * @brief Escribe una cadena de texto desde la posicion actual.
 *
 * Llama a OLED_WriteChar() para cada caracter hasta '\0'.
 * El cursor avanza automaticamente.
 *
 * Cuantos caracteres caben por pagina:
 *   128 columnas / 6 columnas por caracter = 21 caracteres max.
 *
 * @param str  Cadena terminada en '\0'. Solo mayusculas y digitos.
 */
void OLED_WriteString(char *str)
{
    while (*str)
    {
        OLED_WriteChar(*str);
        str++;
    }
}

/* =========================================================
 *  PANTALLAS COMPLETAS DEL PROYECTO
 *
 *  Estas funciones combinan SetCursor + WriteString para
 *  mostrar pantallas predefinidas del sistema de signos
 *  vitales. Llaman OLED_Clear() o OLED_ClearLine() segun
 *  lo que necesiten actualizar.
 * ========================================================= */

/**
 * @brief Pantalla de bienvenida al encender el sistema.
 *
 * Layout:
 *   Pagina 0: "SIGNOS VITALES"
 *   Pagina 2: "PIC18F4550"
 *   Pagina 4: "I2C Y UART"
 *   Pagina 6: "INICIANDO..."
 */
void OLED_Splash(void)
{
    OLED_Clear();

    OLED_SetCursor(4, 0);
    OLED_WriteString("SIGNOS VITALES");

    OLED_SetCursor(16, 2);
    OLED_WriteString("PIC18F4550");

    OLED_SetCursor(16, 4);
    OLED_WriteString("I2C Y UART");

    OLED_SetCursor(10, 6);
    OLED_WriteString("INICIANDO...");
}

/**
 * @brief Muestra datos de temperatura y frecuencia cardiaca.
 *
 * Layout fijo (no borra toda la pantalla, solo las lineas
 * de datos para evitar parpadeo):
 *
 *   Pagina 0: "MONITOR VITAL"     <- titulo fijo
 *   Pagina 2: "TEMP: XX.X"        <- temperatura
 *   Pagina 4: "BPM:  XXX"         <- frecuencia cardiaca
 *   Pagina 6: "NORMAL" o "ALARMA" <- estado
 *
 * Nota: La primera vez se debe llamar OLED_Clear() antes
 * de llamar OLED_Vitals() para limpiar la pantalla completa.
 *
 * @param temp_int  Parte entera de temperatura (ej: 36)
 * @param temp_dec  Primer decimal 0-9 (ej: 5 para 36.5)
 * @param bpm       Latidos por minuto (0-255)
 * @param alarma    0=normal, 1=alarma activa
 */
void OLED_Vitals(unsigned char temp_int, unsigned char temp_dec,
                 unsigned char bpm, unsigned char alarma,
                 unsigned char perfil)
{
    char buf[6];

    /* -- Linea 0: Perfil activo (se actualiza siempre) -- */
    OLED_ClearLine(0);
    OLED_SetCursor(0, 0);
    if (perfil == 0) {
        OLED_WriteString("PERFIL: NORMAL ");
    } else {
        OLED_WriteString("PERFIL: DEPOR  ");
    }

    /* -- Linea 2: Temperatura -- */
    OLED_ClearLine(2);
    OLED_SetCursor(0, 2);
    OLED_WriteString("TEMP: ");
    uint_to_str(temp_int, buf);
    OLED_WriteString(buf);
    OLED_WriteString(".");
    uint_to_str(temp_dec, buf);
    OLED_WriteString(buf);

    /* -- Linea 4: Frecuencia cardiaca -- */
    OLED_ClearLine(4);
    OLED_SetCursor(0, 4);
    OLED_WriteString("BPM:  ");
    uint_to_str(bpm, buf);
    OLED_WriteString(buf);

    /* -- Linea 6: Estado / alarma -- */
    OLED_ClearLine(6);
    OLED_SetCursor(0, 6);
    if (alarma)
        OLED_WriteString("ALARMA ACTIVA");
    else
        OLED_WriteString("NORMAL");
}

/**
 * @brief Pantalla de sistema en pausa.
 *
 * Layout:
 *   Pagina 2: "SISTEMA"
 *   Pagina 4: "EN PAUSA"
 *   Pagina 6: "BTN: REANUDAR"
 */
void OLED_Paused(void)
{
    OLED_Clear();

    OLED_SetCursor(28, 2);
    OLED_WriteString("SISTEMA");

    OLED_SetCursor(22, 4);
    OLED_WriteString("EN PAUSA");

    OLED_SetCursor(4, 6);
    OLED_WriteString("BTN: REANUDAR");
}

/**
 * @brief Pantalla de error con mensaje descriptivo.
 *
 * Layout:
 *   Pagina 0: "ERROR"
 *   Pagina 3: msg
 *   Pagina 6: "REINICIE"
 *
 * @param msg  Maximo 20 caracteres, solo mayusculas y digitos.
 */
void OLED_Error(char *msg)
{
    OLED_Clear();

    OLED_SetCursor(40, 0);
    OLED_WriteString("ERROR");

    OLED_SetCursor(0, 3);
    OLED_WriteString(msg);

    OLED_SetCursor(22, 6);
    OLED_WriteString("REINICIE");
}
