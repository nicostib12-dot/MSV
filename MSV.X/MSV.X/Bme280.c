/**
 * @file    bme280.c
 * @brief   Driver para el sensor BME280 ? lectura de temperatura calibrada.
 *
 * Implementa la formula de compensacion entera de 32 bits de Bosch
 * (datasheet BME280, seccion 4.2.3) sin usar flotantes ni sprintf.
 *
 * Flujo de operacion en cada medicion (Forced Mode):
 *  1. Escribir 0x21 en ctrl_meas (0xF4) para disparar una medicion.
 *  2. Esperar ~5 ms (tiempo tipico de conversion para temperatura x1).
 *  3. Leer 3 bytes del registro 0xFA (temp_msb, temp_lsb, temp_xlsb).
 *  4. Reconstruir el valor crudo de 20 bits (adc_T).
 *  5. Aplicar la formula de compensacion con los coeficientes dig_T1/T2/T3.
 *  6. Convertir el resultado de centesimas a decimas de grado.
 */

#include "bme280.h"
#include "i2c.h"

/* =========================================================
 *  REGISTROS DEL BME280
 * ========================================================= */

#define BME280_REG_ID        0xD0   /**< Chip ID ? debe leer 0x60                */
#define BME280_REG_RESET     0xE0   /**< Soft reset ? escribir 0xB6              */
#define BME280_REG_CTRL_HUM  0xF2   /**< Control humedad (osrs_h)                */
#define BME280_REG_STATUS    0xF3   /**< Estado: bit3=measuring, bit0=im_update  */
#define BME280_REG_CTRL_MEAS 0xF4   /**< Control temperatura, presion y modo     */
#define BME280_REG_CONFIG    0xF5   /**< Configuracion: t_sb, filter, spi3w_en   */
#define BME280_REG_TEMP_MSB  0xFA   /**< Byte alto temperatura cruda             */

/* Registros de calibracion temperatura (NVM del sensor) */
#define BME280_REG_DIG_T1    0x88   /**< dig_T1 LSB (unsigned 16 bits)           */
#define BME280_REG_DIG_T2    0x8A   /**< dig_T2 LSB (signed 16 bits)             */
#define BME280_REG_DIG_T3    0x8C   /**< dig_T3 LSB (signed 16 bits)             */

/* =========================================================
 *  COEFICIENTES DE CALIBRACION (leidos de NVM en Init)
 *
 *  dig_T1: unsigned ? representa el offset base de temperatura.
 *  dig_T2: signed   ? coeficiente lineal.
 *  dig_T3: signed   ? coeficiente cuadratico (correccion fina).
 *
 *  Se guardan como variables de modulo (static) para no contaminar
 *  el espacio de nombres global y solo ser accesibles desde este archivo.
 * ========================================================= */
static unsigned short dig_T1;   /**< Coeficiente calibracion T1 (unsigned) */
static signed   short dig_T2;   /**< Coeficiente calibracion T2 (signed)   */
static signed   short dig_T3;   /**< Coeficiente calibracion T3 (signed)   */

/* Ultimo valor valido de temperatura, retornado en caso de error I2C */
static unsigned int last_temp_decimas = 0;

/* =========================================================
 *  FUNCION INTERNA: leer coeficientes de calibracion
 *
 *  Cada coeficiente ocupa 2 bytes en formato little-endian
 *  (byte bajo en la direccion mas baja).
 *  Se usa I2C_ReadBurst para leer los 6 bytes de una sola vez.
 * ========================================================= */
static unsigned char BME280_ReadCalibration(void) {
    unsigned char buf[6];
    unsigned char status;

    /* Leer los 6 bytes de calibracion de temperatura de corrido:
     * buf[0] = dig_T1 LSB  (0x88)
     * buf[1] = dig_T1 MSB  (0x89)
     * buf[2] = dig_T2 LSB  (0x8A)
     * buf[3] = dig_T2 MSB  (0x8B)
     * buf[4] = dig_T3 LSB  (0x8C)
     * buf[5] = dig_T3 MSB  (0x8D) */
    status = I2C_ReadBurst(BME280_ADDR, BME280_REG_DIG_T1, buf, 6);
    if (status != I2C_OK) return BME280_ERR_I2C;

    /* Reconstruir coeficientes en formato little-endian */
    dig_T1 = (unsigned short)((unsigned short)buf[1] << 8 | buf[0]);
    dig_T2 = (signed   short)((unsigned short)buf[3] << 8 | buf[2]);
    dig_T3 = (signed   short)((unsigned short)buf[5] << 8 | buf[4]);

    return BME280_OK;
}

/* =========================================================
 *  FUNCION INTERNA: formula de compensacion entera Bosch
 *
 *  Formula oficial del datasheet BME280 seccion 4.2.3 (version entera).
 *  Recibe el valor crudo de 20 bits (adc_T) y retorna la temperatura
 *  en centesimas de grado Celsius.
 *  Ejemplo: retorna 3650 para 36.50 C.
 *
 *  Usa long (signed 32 bits en XC8) para las operaciones intermedias.
 *  t_fine es un valor auxiliar que en el datasheet original se comparte
 *  con las compensaciones de presion y humedad. Aqui se usa localmente.
 * ========================================================= */
static long BME280_CompensateTemp(long adc_T) {
    long var1, var2, T, t_fine;

    /* Formula Bosch ? compensacion temperatura entera 32 bits */
    var1 = ((((adc_T >> 3) - ((long)dig_T1 << 1)))
             * ((long)dig_T2)) >> 11;

    var2 = (((((adc_T >> 4) - ((long)dig_T1))
             * ((adc_T >> 4) - ((long)dig_T1))) >> 12)
             * ((long)dig_T3)) >> 14;

    t_fine = var1 + var2;

    /* T en centesimas de grado: (t_fine * 5 + 128) >> 8 */
    T = (t_fine * 5L + 128L) >> 8;

    return T;
}

/* =========================================================
 *  BME280_Init
 * ========================================================= */
unsigned char BME280_Init(void) {
    unsigned char chip_id;
    unsigned char status;

    /* -- Verificar identidad del sensor --
     * El registro 0xD0 debe retornar 0x60 para el BME280.
     * Si retorna otro valor, el sensor no esta presente o no responde. */
    chip_id = I2C_ReadReg(BME280_ADDR, BME280_REG_ID);
    if (chip_id != BME280_CHIP_ID) return BME280_ERR_NODEV;

    /* -- Soft reset del sensor --
     * Escribe 0xB6 en el registro de reset para iniciar desde
     * un estado conocido, descartando cualquier configuracion previa. */
    status = I2C_WriteReg(BME280_ADDR, BME280_REG_RESET, 0xB6);
    if (status != I2C_OK) return BME280_ERR_I2C;

    /* Esperar que el reset interno se complete (~10 ms segun datasheet) */
    __delay_ms(10);

    /* -- Leer coeficientes de calibracion de fabrica --
     * Deben leerse despues del reset y antes de cualquier medicion. */
    status = BME280_ReadCalibration();
    if (status != BME280_OK) return status;

    /* -- Desactivar humedad --
     * ctrl_hum (0xF2) debe escribirse ANTES de ctrl_meas para que
     * el cambio tenga efecto (requerimiento del datasheet Bosch).
     * osrs_h = 000 -> humedad desactivada (skip). */
    status = I2C_WriteReg(BME280_ADDR, BME280_REG_CTRL_HUM, 0x00);
    if (status != I2C_OK) return BME280_ERR_I2C;

    /* -- Configuracion del filtro y standby --
     * config (0xF5): filter=off, t_sb=0.5ms, spi3w=off.
     * En Forced Mode el campo t_sb no aplica, pero se inicializa
     * con ceros para evitar valores indeterminados. */
    status = I2C_WriteReg(BME280_ADDR, BME280_REG_CONFIG, 0x00);
    if (status != I2C_OK) return BME280_ERR_I2C;

    /* -- ctrl_meas (0xF4) en modo sleep por ahora --
     * osrs_t = 001 (x1 oversampling temperatura)
     * osrs_p = 000 (presion desactivada)
     * mode   = 00  (sleep ? se activa Forced en cada lectura)
     * Valor  = 0b00100000 = 0x20 */
    status = I2C_WriteReg(BME280_ADDR, BME280_REG_CTRL_MEAS, 0x20);
    if (status != I2C_OK) return BME280_ERR_I2C;

    return BME280_OK;
}

/* =========================================================
 *  BME280_ReadTemp
 * ========================================================= */
unsigned int BME280_ReadTemp(void) {
    unsigned char buf[3];
    long adc_T;
    long temp_centesimas;
    unsigned char status;

    /* -- Disparar medicion en Forced Mode --
     * Escribir mode=01 (Forced) en ctrl_meas. El sensor realiza
     * una medicion y vuelve automaticamente a sleep al terminar.
     * osrs_t=001, osrs_p=000, mode=01 -> 0b00100001 = 0x21 */
    status = I2C_WriteReg(BME280_ADDR, BME280_REG_CTRL_MEAS, 0x21);
    if (status != I2C_OK) return last_temp_decimas;

    /* -- Esperar conversion --
     * Tiempo tipico con osrs_t=1 (x1): 2.3 ms segun datasheet.
     * Se espera 5 ms para tener margen seguro sin necesidad de
     * leer el registro de status en un bucle. */
    __delay_ms(5);

    /* -- Leer 3 bytes de temperatura cruda: 0xFA, 0xFB, 0xFC --
     * buf[0] = temp_msb  (bits 19:12)
     * buf[1] = temp_lsb  (bits 11:4)
     * buf[2] = temp_xlsb (bits  3:0 en el nibble alto) */
    status = I2C_ReadBurst(BME280_ADDR, BME280_REG_TEMP_MSB, buf, 3);
    if (status != I2C_OK) return last_temp_decimas;

    /* -- Reconstruir valor crudo de 20 bits (adc_T) --
     * El valor crudo ocupa los bits [19:0] de los 3 bytes leidos.
     * temp_xlsb aporta los 4 bits menos significativos en [7:4]. */
    adc_T = ((long)buf[0] << 12)
          | ((long)buf[1] <<  4)
          | ((long)buf[2] >>  4);

    /* Valor 0x80000 indica que la medicion fue omitida (skip).
     * No deberia ocurrir ya que osrs_t != 0, pero se verifica. */
    if (adc_T == 0x80000L) return last_temp_decimas;

    /* -- Aplicar formula de compensacion Bosch --
     * Resultado en centesimas de grado: 3650 = 36.50 C */
    temp_centesimas = BME280_CompensateTemp(adc_T);

    /* -- Convertir a decimas de grado --
     * De centesimas (0.01 C) a decimas (0.1 C): dividir por 10.
     * 3650 / 10 = 365  ->  365 representa 36.5 C
     * Compatible con OLED_WriteTemp() y los umbrales de config.h */
    last_temp_decimas = (unsigned int)(temp_centesimas / 10L);

    return last_temp_decimas;
}
