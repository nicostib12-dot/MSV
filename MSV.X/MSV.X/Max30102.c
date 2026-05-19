/**
 * @file    max30102.c
 * @brief   Driver para el sensor MAX30102 ? medicion de frecuencia cardiaca.
 *
 * -------------------------------------------------------------------------
 * FUNCIONAMIENTO DEL HARDWARE
 * -------------------------------------------------------------------------
 * El MAX30102 ilumina el tejido del dedo con un LED Rojo (660 nm). Un
 * fotodetector mide la luz reflejada. Con cada latido, el volumen de sangre
 * en los capilares aumenta brevemente, absorbiendo mas luz y generando un
 * pico caracteristico en la senal. Esta senal se llama PPG (Photo-
 * PletismoGraphy).
 *
 * El sensor almacena las muestras ADC en un FIFO circular de 32 posiciones.
 * En modo Heart Rate, cada posicion = 3 bytes = 1 muestra de 18 bits.
 * El PIC lee el FIFO periodicamente para no perder muestras.
 *
 * -------------------------------------------------------------------------
 * ALGORITMO DE DETECCION DE BPM
 * -------------------------------------------------------------------------
 * Se usa un buffer deslizante de 100 muestras (2 segundos a 50 Hz):
 *
 *   Segundo N:   [m0  m1  ... m49  m50 m51 ... m99]
 *   Segundo N+1: [m50 m51 ... m99  m100 ...   m149]
 *                 ^ 50 antiguas    ^ 50 nuevas
 *
 * Sobre el buffer completo se aplica deteccion de picos:
 *  1. Calcular umbral adaptativo = min + 70% * (max - min).
 *     Un umbral al 70% del rango es mas robusto que el punto medio (50%)
 *     para PPG, donde los picos son agudos y los valles son anchos.
 *  2. Detectar flancos ascendentes: transicion de (senal < umbral) a
 *     (senal >= umbral). Cada flanco = inicio de un latido.
 *  3. Medir el intervalo promedio entre flancos consecutivos (en muestras).
 *  4. BPM = (60 * SAMPLE_RATE) / intervalo_promedio.
 *
 * -------------------------------------------------------------------------
 * USO DE MEMORIA RAM
 * -------------------------------------------------------------------------
 *  ir_buf[100]  : 200 bytes (buffer principal, uint16_t)
 *  raw_buf[150] : 150 bytes (buffer auxiliar de lectura FIFO, uint8_t)
 *  Total modulo :~355 bytes de 2048 disponibles en el PIC18F4550.
 */

#include "max30102.h"
#include "i2c.h"

/* =========================================================
 *  REGISTROS DEL MAX30102
 * ========================================================= */

#define REG_INT_STATUS1   0x00  /**< Estado interrupciones 1              */
#define REG_INT_ENABLE1   0x02  /**< Habilitacion interrupciones 1        */
#define REG_INT_ENABLE2   0x03  /**< Habilitacion interrupciones 2        */
#define REG_FIFO_WR_PTR   0x04  /**< Puntero de escritura del FIFO        */
#define REG_FIFO_OVF_CTR  0x05  /**< Contador de overflow del FIFO        */
#define REG_FIFO_RD_PTR   0x06  /**< Puntero de lectura del FIFO          */
#define REG_FIFO_DATA     0x07  /**< Registro de datos del FIFO           */
#define REG_FIFO_CONFIG   0x08  /**< Configuracion del FIFO               */
#define REG_MODE_CONFIG   0x09  /**< Configuracion del modo de operacion  */
#define REG_SPO2_CONFIG   0x0A  /**< Configuracion ADC / tasa de muestreo */
#define REG_LED1_PA       0x0C  /**< Amplitud del LED Rojo (corriente)    */
#define REG_PART_ID       0xFF  /**< Identificador de parte (debe = 0x15) */

/* =========================================================
 *  VARIABLES DE MODULO (privadas)
 * ========================================================= */

/** Buffer deslizante de muestras IR en 16 bits.
 *  Cada muestra es el valor ADC de 18 bits desplazado 2 bits a la derecha
 *  para caber en uint16_t. La precision perdida no afecta la deteccion. */
static unsigned short ir_buf[MAX30102_BUF_SIZE];

/** Buffer auxiliar para lectura cruda del FIFO.
 *  Cada muestra ocupa 3 bytes -> 50 muestras = 150 bytes.
 *  Se declara static para evitar consumo de stack en XC8. */
static unsigned char raw_buf[MAX30102_HALF_BUF * 3U];

/** Cantidad de muestras validas actualmente en ir_buf (0 a BUF_SIZE). */
static unsigned char samples_in_buf;

/** Ultimo valor de BPM calculado correctamente. Se retorna en caso de
 *  error o senal insuficiente para mantener continuidad en la pantalla. */
static unsigned int last_bpm;

/* =========================================================
 *  FUNCION INTERNA: leer muestras del FIFO
 *
 *  Determina cuantas muestras hay disponibles, lee hasta HALF_BUF
 *  y las incorpora al buffer deslizante ir_buf.
 *
 *  El buffer deslizante funciona asi:
 *   - Si ya esta lleno: desplaza (elimina) las HALF_BUF muestras mas
 *     antiguas y agrega las nuevas al final.
 *   - Si no esta lleno: simplemente agrega al final hasta llenarlo.
 * ========================================================= */
static void MAX30102_UpdateBuffer(void) {
    unsigned char wr_ptr, rd_ptr;
    unsigned char num_samples;
    unsigned char i, j;
    unsigned long raw_value;

    /* Leer punteros del FIFO para saber cuantas muestras hay disponibles */
    wr_ptr = I2C_ReadReg(MAX30102_ADDR, REG_FIFO_WR_PTR);
    rd_ptr = I2C_ReadReg(MAX30102_ADDR, REG_FIFO_RD_PTR);

    /* Calculo de muestras disponibles con modulo 32 (FIFO es circular de 32) */
    num_samples = (unsigned char)((wr_ptr - rd_ptr + 32U) & 0x1FU);

    /* Limitar a HALF_BUF para no saturar el buffer ni exceder raw_buf */
    if (num_samples > MAX30102_HALF_BUF) {
        num_samples = (unsigned char)MAX30102_HALF_BUF;
    }

    if (num_samples == 0U) return;

    /* Leer num_samples * 3 bytes del registro FIFO_DATA en una sola transaccion.
     * El MAX30102 incrementa el puntero de lectura del FIFO automaticamente
     * conforme se leen los bytes de registro 0x07. */
    if (I2C_ReadBurst(MAX30102_ADDR, REG_FIFO_DATA,
                      raw_buf, (unsigned char)(num_samples * 3U)) != I2C_OK) {
        return; /* Si falla la lectura, no actualizar el buffer */
    }

    /* Si el buffer ya estaba lleno, desplazar para hacer espacio:
     * Eliminar las num_samples muestras mas antiguas del inicio. */
    if (samples_in_buf >= (unsigned char)MAX30102_BUF_SIZE) {
        for (i = 0; i < (unsigned char)(MAX30102_BUF_SIZE - num_samples); i++) {
            ir_buf[i] = ir_buf[i + num_samples];
        }
        samples_in_buf = (unsigned char)(MAX30102_BUF_SIZE - num_samples);
    }

    /* Reconstruir cada muestra de 18 bits y agregarla al buffer.
     *
     * Formato de 3 bytes en modo 18-bit (datasheet MAX30102, p.16):
     *  Byte 0: [7:2] = bits[17:12] del dato, [1:0] = 0 (reservados)
     *  Byte 1: [7:0] = bits[11:4]  del dato
     *  Byte 2: [7:4] = bits[3:0]   del dato, [3:0] = 0 (reservados)
     *
     * Reconstruccion de los 18 bits:
     *  raw_value = (byte0 & 0x03) << 16 | byte1 << 8 | byte2
     *
     * Conversion a 16 bits para el buffer (desplazar 2 bits):
     *  ir_buf[pos] = (uint16_t)(raw_value >> 2)
     */
    for (i = 0; i < num_samples; i++) {
        j = (unsigned char)(i * 3U);

        raw_value = ((unsigned long)(raw_buf[j]     & 0x03U) << 16U)
                  | ((unsigned long) raw_buf[j + 1U]         <<  8U)
                  |  (unsigned long) raw_buf[j + 2U];

        ir_buf[samples_in_buf] = (unsigned short)(raw_value >> 2U);
        samples_in_buf++;
    }
}

/* =========================================================
 *  FUNCION INTERNA: calcular BPM desde el buffer
 *
 *  Aplica el algoritmo de deteccion de picos sobre ir_buf completo.
 *  Retorna el BPM calculado, o last_bpm si la senal es invalida.
 * ========================================================= */
static unsigned int MAX30102_CalcBPM(void) {
    unsigned short min_val, max_val, threshold;
    unsigned char  i;
    unsigned char  above;
    unsigned char  peak_idx[8];   /* Posiciones de hasta 8 picos en el buffer */
    unsigned char  peak_count;
    unsigned short total_interval;
    unsigned short avg_interval;
    unsigned int   bpm;

    /* --- Paso 1: Encontrar minimo y maximo del buffer --- */
    min_val = ir_buf[0];
    max_val = ir_buf[0];
    for (i = 1U; i < (unsigned char)MAX30102_BUF_SIZE; i++) {
        if (ir_buf[i] < min_val) min_val = ir_buf[i];
        if (ir_buf[i] > max_val) max_val = ir_buf[i];
    }

    /* --- Paso 2: Verificar amplitud minima de senal ---
     * Si la diferencia entre max y min es muy pequena, la senal es ruido
     * (sin dedo, mala colocacion o movimiento excesivo). */
    if ((unsigned short)(max_val - min_val) < (unsigned short)MAX30102_MIN_AMPLITUDE) {
        return last_bpm; /* Senal insuficiente: mantener ultimo valor */
    }

    /* --- Paso 3: Umbral adaptativo al 70% del rango ---
     * El 70% funciona mejor que el 50% para PPG porque los picos son
     * agudos y breves respecto al tiempo total del ciclo cardiaco.
     * Se usa aritmetica de 32 bits para evitar desbordamiento. */
    threshold = min_val + (unsigned short)(
                    ((unsigned long)(max_val - min_val) * 7UL) / 10UL);

    /* --- Paso 4: Detectar flancos ascendentes (inicio de cada pico) ---
     * Un flanco ascendente ocurre cuando la senal sube por encima del
     * umbral despues de haber estado por debajo. Cada flanco = 1 latido. */
    peak_count = 0U;
    above      = 0U;

    for (i = 0U; i < (unsigned char)MAX30102_BUF_SIZE && peak_count < 8U; i++) {
        if ((above == 0U) && (ir_buf[i] > threshold)) {
            /* Flanco ascendente detectado: registrar posicion */
            peak_idx[peak_count++] = i;
            above = 1U;
        } else if ((above != 0U) && (ir_buf[i] <= threshold)) {
            /* Flanco descendente: senal bajo umbral, esperar siguiente pico */
            above = 0U;
        }
    }

    /* --- Paso 5: Verificar que hay suficientes picos ---
     * Se necesitan al menos 2 picos para calcular un intervalo. */
    if (peak_count < 2U) {
        return last_bpm; /* Pocos picos detectados: mantener ultimo valor */
    }

    /* --- Paso 6: Calcular intervalo promedio entre picos ---
     * Se promedian todos los intervalos inter-pico disponibles para
     * reducir el efecto de latidos irregulares o artefactos. */
    total_interval = 0U;
    for (i = 1U; i < peak_count; i++) {
        total_interval += (unsigned short)(peak_idx[i] - peak_idx[i - 1U]);
    }
    avg_interval = total_interval / (unsigned short)(peak_count - 1U);

    /* Proteccion contra division por cero */
    if (avg_interval == 0U) return last_bpm;

    /* --- Paso 7: Convertir intervalo a BPM ---
     * BPM = (60 segundos * SAMPLE_RATE muestras/segundo) / intervalo_muestras
     * Se usa aritmetica de 32 bits para el numerador (60 * 50 = 3000). */
    bpm = (unsigned int)((60UL * (unsigned long)MAX30102_SAMPLE_RATE)
                         / (unsigned long)avg_interval);

    /* --- Paso 8: Filtro de rango fisiologico ---
     * Valores fuera del rango 30-250 BPM son artefactos o errores.
     * Se conserva el ultimo valor valido en lugar de propagar el error. */
    if (bpm < 30U || bpm > 250U) {
        return last_bpm;
    }

    /* Actualizar y retornar el nuevo BPM valido */
    last_bpm = bpm;
    return last_bpm;
}

/* =========================================================
 *  MAX30102_Init
 * ========================================================= */
unsigned char MAX30102_Init(void) {
    unsigned char part_id;
    unsigned char i;

    /* --- Verificar Part ID ---
     * El registro 0xFF debe retornar 0x15 para el MAX30102 original.
     * Si retorna 0xFF probablemente el sensor no esta conectado. */
    part_id = I2C_ReadReg(MAX30102_ADDR, REG_PART_ID);
    if (part_id != (unsigned char)MAX30102_PART_ID) {
        return MAX30102_ERR_NODEV;
    }

    /* --- Reset por software ---
     * Bit 6 (RESET) del registro MODE_CONFIG = 1 dispara el reset.
     * El bit se limpia automaticamente cuando el reset termina (~1 ms). */
    if (I2C_WriteReg(MAX30102_ADDR, REG_MODE_CONFIG, 0x40U) != I2C_OK) {
        return MAX30102_ERR_I2C;
    }
    __delay_ms(10); /* Margen seguro tras el reset */

    /* --- Deshabilitar interrupciones ---
     * No usamos interrupciones del sensor ? se hace polling del FIFO. */
    I2C_WriteReg(MAX30102_ADDR, REG_INT_ENABLE1, 0x00U);
    I2C_WriteReg(MAX30102_ADDR, REG_INT_ENABLE2, 0x00U);

    /* --- Limpiar punteros del FIFO ---
     * Garantiza que empezamos a leer desde el inicio del FIFO sin datos
     * residuales del reset. */
    I2C_WriteReg(MAX30102_ADDR, REG_FIFO_WR_PTR, 0x00U);
    I2C_WriteReg(MAX30102_ADDR, REG_FIFO_OVF_CTR, 0x00U);
    I2C_WriteReg(MAX30102_ADDR, REG_FIFO_RD_PTR,  0x00U);

    /* --- Configuracion del FIFO (0x08) ---
     * Bits [7:5] SMP_AVE   = 000 (sin promediado, 1 muestra por slot)
     * Bit  [4]   ROLLOVER  = 1   (FIFO circular: si se llena, sobreescribe)
     * Bits [3:0] FIFO_A_FULL = 1111 (no usamos interrupcion, valor default)
     * Valor: 0b00011111 = 0x1F */
    I2C_WriteReg(MAX30102_ADDR, REG_FIFO_CONFIG, 0x1FU);

    /* --- Modo Heart Rate (0x09) ---
     * Bits [2:0] MODE = 010 -> solo LED Rojo activo, 3 bytes/muestra FIFO.
     * Valor: 0x02 */
    I2C_WriteReg(MAX30102_ADDR, REG_MODE_CONFIG, 0x02U);

    /* --- Configuracion ADC y tasa de muestreo (0x0A) ---
     * Bits [6:5] ADC_RGE = 01  -> rango full scale 4096 nA
     * Bits [4:2] SR      = 000 -> 50 muestras/segundo
     * Bits [1:0] LED_PW  = 11  -> 411 us, resolucion 18 bits
     * Valor: 0b00100011 = 0x23 */
    I2C_WriteReg(MAX30102_ADDR, REG_SPO2_CONFIG, 0x23U);

    /* --- Corriente del LED Rojo (0x0C) ---
     * Valor 0x24 corresponde aproximadamente a 7.1 mA.
     * Rango: 0x00 (0 mA) a 0xFF (51 mA).
     * Si la senal es muy debil, aumentar este valor (hasta ~0x47 = 14 mA). */
    I2C_WriteReg(MAX30102_ADDR, REG_LED1_PA, 0x24U);

    /* --- Inicializar variables del modulo --- */
    for (i = 0U; i < (unsigned char)MAX30102_BUF_SIZE; i++) {
        ir_buf[i] = 0U;
    }
    samples_in_buf = 0U;
    last_bpm       = 0U;

    return MAX30102_OK;
}

/* =========================================================
 *  MAX30102_GetBPM
 * ========================================================= */
unsigned int MAX30102_GetBPM(void) {
    /* Leer nuevas muestras del FIFO y actualizar el buffer deslizante */
    MAX30102_UpdateBuffer();

    /* Solo calcular BPM cuando el buffer tenga datos suficientes (2 segundos).
     * Antes de eso retornar 0 para indicar que la medicion no esta lista. */
    if (samples_in_buf < (unsigned char)MAX30102_BUF_SIZE) {
        return 0U;
    }

    /* Calcular y retornar BPM desde el buffer completo */
    return MAX30102_CalcBPM();
}
