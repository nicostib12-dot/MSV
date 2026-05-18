/**
 * @file    oled.c
 * @brief   Driver para pantalla OLED SSD1306 de 128x64 pixeles via I2C.
 *
 * Usa las funciones de i2c.h (modulo MSSP hardware) en lugar de
 * llamadas directas al bus como en el proyecto anterior.
 *
 * Protocolo SSD1306 via I2C:
 *  - Byte de control 0x00 + byte de comando : configura el controlador.
 *  - Byte de control 0x40 + byte de datos   : escribe pixeles en GDDRAM.
 *
 * Con el nuevo i2c.h esto se mapea directamente a:
 *  - I2C_WriteReg(OLED_ADDR, 0x00, cmd)  -> enviar comando
 *  - I2C_WriteReg(OLED_ADDR, 0x40, data) -> enviar dato de pixel
 */

#include "oled.h"
#include "i2c.h"

/* =========================================================
 *  FUENTE DE MAPA DE BITS 5x7
 *
 *  Cada entrada es un arreglo de 5 bytes. Cada byte representa
 *  una columna de 8 pixeles (bit 0 = fila superior de la pagina).
 *
 *  Indices:
 *    0      -> ' ' (espacio)
 *    1-10   -> '0'-'9'
 *    11-36  -> 'A'-'Z'
 *    37     -> ':'
 *    38     -> '%'
 *    39     -> '.'
 *    40     -> '-'
 *    41     -> '/'
 * ========================================================= */
static const unsigned char font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* [0]  ' ' espacio  */
    {0x3E,0x51,0x49,0x45,0x3E}, /* [1]  '0'          */
    {0x00,0x42,0x7F,0x40,0x00}, /* [2]  '1'          */
    {0x42,0x61,0x51,0x49,0x46}, /* [3]  '2'          */
    {0x21,0x41,0x45,0x4B,0x31}, /* [4]  '3'          */
    {0x18,0x14,0x12,0x7F,0x10}, /* [5]  '4'          */
    {0x27,0x45,0x45,0x45,0x39}, /* [6]  '5'          */
    {0x3C,0x4A,0x49,0x49,0x30}, /* [7]  '6'          */
    {0x01,0x71,0x09,0x05,0x03}, /* [8]  '7'          */
    {0x36,0x49,0x49,0x49,0x36}, /* [9]  '8'          */
    {0x06,0x49,0x49,0x29,0x1E}, /* [10] '9'          */
    {0x7E,0x11,0x11,0x11,0x7E}, /* [11] 'A'          */
    {0x7F,0x49,0x49,0x49,0x36}, /* [12] 'B'          */
    {0x3E,0x41,0x41,0x41,0x22}, /* [13] 'C'          */
    {0x7F,0x41,0x41,0x22,0x1C}, /* [14] 'D'          */
    {0x7F,0x49,0x49,0x49,0x41}, /* [15] 'E'          */
    {0x7F,0x09,0x09,0x09,0x01}, /* [16] 'F'          */
    {0x3E,0x41,0x49,0x49,0x7A}, /* [17] 'G'          */
    {0x7F,0x08,0x08,0x08,0x7F}, /* [18] 'H'          */
    {0x00,0x41,0x7F,0x41,0x00}, /* [19] 'I'          */
    {0x20,0x40,0x41,0x3F,0x01}, /* [20] 'J'          */
    {0x7F,0x08,0x14,0x22,0x41}, /* [21] 'K'          */
    {0x7F,0x40,0x40,0x40,0x40}, /* [22] 'L'          */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* [23] 'M'          */
    {0x7F,0x04,0x08,0x10,0x7F}, /* [24] 'N'          */
    {0x3E,0x41,0x41,0x41,0x3E}, /* [25] 'O'          */
    {0x7F,0x09,0x09,0x09,0x06}, /* [26] 'P'          */
    {0x3E,0x41,0x51,0x21,0x5E}, /* [27] 'Q'          */
    {0x7F,0x09,0x19,0x29,0x46}, /* [28] 'R'          */
    {0x46,0x49,0x49,0x49,0x31}, /* [29] 'S'          */
    {0x01,0x01,0x7F,0x01,0x01}, /* [30] 'T'          */
    {0x3F,0x40,0x40,0x40,0x3F}, /* [31] 'U'          */
    {0x1F,0x20,0x40,0x20,0x1F}, /* [32] 'V'          */
    {0x3F,0x40,0x38,0x40,0x3F}, /* [33] 'W'          */
    {0x63,0x14,0x08,0x14,0x63}, /* [34] 'X'          */
    {0x07,0x08,0x70,0x08,0x07}, /* [35] 'Y'          */
    {0x61,0x51,0x49,0x45,0x43}, /* [36] 'Z'          */
    {0x00,0x00,0x5F,0x00,0x00}, /* [37] ':'          */
    {0x00,0x06,0x09,0x09,0x06}, /* [38] '%'          */
    {0x00,0x00,0x08,0x00,0x00}, /* [39] '.'          */
    {0x08,0x08,0x08,0x08,0x08}, /* [40] '-'          */
    {0x20,0x10,0x08,0x04,0x02}, /* [41] '/'          */
};

/* =========================================================
 *  FUNCIONES INTERNAS
 * ========================================================= */

/**
 * @brief  Envia un comando de configuracion al SSD1306.
 * @param  cmd  Codigo de comando (ver datasheet SSD1306).
 */
static void OLED_Command(unsigned char cmd) {
    I2C_WriteReg(OLED_ADDR, 0x00, cmd);
}

/**
 * @brief  Envia un byte de dato (pixeles) al SSD1306.
 * @param  data  Byte de 8 pixeles verticales.
 */
static void OLED_Data(unsigned char data) {
    I2C_WriteReg(OLED_ADDR, 0x40, data);
}

/**
 * @brief  Convierte un caracter ASCII al indice de la tabla font5x7.
 * @param  c  Caracter a convertir.
 * @return Indice en font5x7, o 0 (espacio) si no esta soportado.
 */
static unsigned char charToIndex(char c) {
    if (c == ' ')              return 0;
    if (c >= '0' && c <= '9') return (unsigned char)(c - '0' + 1);
    if (c >= 'A' && c <= 'Z') return (unsigned char)(c - 'A' + 11);
    if (c >= 'a' && c <= 'z') return (unsigned char)(c - 'a' + 11); /* Mayuscula */
    if (c == ':')              return 37;
    if (c == '%')              return 38;
    if (c == '.')              return 39;
    if (c == '-')              return 40;
    if (c == '/')              return 41;
    return 0; /* Caracter no soportado -> espacio */
}

/* =========================================================
 *  OLED_Init
 * ========================================================= */
void OLED_Init(void) {
    __delay_ms(100); /* Esperar arranque del SSD1306 */

    /* Secuencia de inicializacion estandar del SSD1306 */
    OLED_Command(0xAE); /* Display OFF                        */
    OLED_Command(0xD5); /* Set display clock divide ratio     */
    OLED_Command(0x80); /* Ratio recomendado por Adafruit     */
    OLED_Command(0xA8); /* Set multiplex ratio                */
    OLED_Command(0x3F); /* 64 lineas (0x3F = 63)              */
    OLED_Command(0xD3); /* Set display offset                 */
    OLED_Command(0x00); /* Sin offset                         */
    OLED_Command(0x40); /* Set start line = 0                 */
    OLED_Command(0x8D); /* Charge pump setting                */
    OLED_Command(0x14); /* Enable charge pump                 */
    OLED_Command(0x20); /* Set memory addressing mode         */
    OLED_Command(0x00); /* Modo horizontal                    */
    OLED_Command(0xA1); /* Set segment re-map (columna 127=SEG0) */
    OLED_Command(0xC8); /* Set COM output scan direction      */
    OLED_Command(0xDA); /* Set COM pins hardware config       */
    OLED_Command(0x12); /* Alternativo, disable left/right    */
    OLED_Command(0x81); /* Set contrast control               */
    OLED_Command(0xCF); /* Contraste maximo                   */
    OLED_Command(0xD9); /* Set pre-charge period              */
    OLED_Command(0xF1); /* Phase 1: 1 DCLK, Phase 2: 15 DCLK */
    OLED_Command(0xDB); /* Set VCOMH deselect level           */
    OLED_Command(0x40); /* 0.89 x VCC                        */
    OLED_Command(0xA4); /* Display from RAM (no all-on)       */
    OLED_Command(0xA6); /* Normal display (no invertido)      */
    OLED_Command(0xAF); /* Display ON                         */
}

/* =========================================================
 *  OLED_Clear
 * ========================================================= */
void OLED_Clear(void) {
    unsigned char page, col;
    for (page = 0; page < OLED_PAGES; page++) {
        OLED_Command(0xB0 | page);   /* Seleccionar pagina        */
        OLED_Command(0x00);           /* Columna baja = 0          */
        OLED_Command(0x10);           /* Columna alta = 0          */
        for (col = 0; col < OLED_COLS; col++) {
            OLED_Data(0x00);          /* Apagar todos los pixeles  */
        }
    }
}

/* =========================================================
 *  OLED_ClearLine
 * ========================================================= */
void OLED_ClearLine(unsigned char page) {
    unsigned char col;
    if (page >= OLED_PAGES) return;
    OLED_Command(0xB0 | page);
    OLED_Command(0x00);
    OLED_Command(0x10);
    for (col = 0; col < OLED_COLS; col++) {
        OLED_Data(0x00);
    }
}

/* =========================================================
 *  OLED_SetCursor
 * ========================================================= */
void OLED_SetCursor(unsigned char col, unsigned char page) {
    /* Aplicar offset de hardware del SSD1306 */
    col += OLED_COL_OFFSET;

    OLED_Command(0xB0 | (page & 0x07));          /* Pagina          */
    OLED_Command(0x00 | (col & 0x0F));            /* Columna nibble bajo */
    OLED_Command(0x10 | ((col >> 4) & 0x0F));     /* Columna nibble alto */
}

/* =========================================================
 *  OLED_WriteChar
 * ========================================================= */
void OLED_WriteChar(char c) {
    unsigned char i;
    unsigned char idx = charToIndex(c);

    for (i = 0; i < 5; i++) {
        OLED_Data(font5x7[idx][i]);
    }
    OLED_Data(0x00); /* Espacio de separacion entre caracteres */
}

/* =========================================================
 *  OLED_WriteString
 * ========================================================= */
void OLED_WriteString(const char *str) {
    if (str == 0) return;
    while (*str) {
        OLED_WriteChar(*str);
        str++;
    }
}

/* =========================================================
 *  OLED_WriteInt
 *
 *  Convierte un entero a caracteres sin usar sprintf.
 *  Algoritmo: extraer digitos de mayor a menor usando division.
 * ========================================================= */
void OLED_WriteInt(unsigned int value) {
    unsigned char digits[5]; /* Maximo 5 digitos para un uint16 (0-65535) */
    signed char   i = 0;
    signed char   j;

    if (value == 0) {
        OLED_WriteChar('0');
        return;
    }

    /* Extraer digitos en orden inverso */
    while (value > 0 && i < 5) {
        digits[i++] = (unsigned char)(value % 10) + '0';
        value /= 10;
    }

    /* Mostrar en orden correcto (de mayor a menor) */
    for (j = i - 1; j >= 0; j--) {
        OLED_WriteChar((char)digits[j]);
    }
}

/* =========================================================
 *  OLED_WriteTemp
 *
 *  Muestra temperatura con un decimal a partir de un valor
 *  en decimas de grado. Ejemplo: 365 -> "36.5"
 * ========================================================= */
void OLED_WriteTemp(unsigned int decimas) {
    OLED_WriteInt(decimas / 10);   /* Parte entera  */
    OLED_WriteChar('.');
    OLED_WriteChar((char)((decimas % 10) + '0')); /* Un decimal */
}
