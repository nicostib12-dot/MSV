/**
 * @file card.c
 * @brief Procesamiento de señal del sensor MAX30102 para detección de dedo y cálculo de BPM.
 *
 * Este archivo implementa las funciones encargadas de analizar las muestras
 * infrarrojas obtenidas desde el sensor MAX30102. A partir de la señal IR se
 * determina si existe un dedo colocado sobre el sensor y se realiza un cálculo
 * aproximado del ritmo cardíaco en BPM mediante detección de picos.
 */

#include "card.h"
#include "max30102.h"

/**
 * @brief Última variación calculada entre promedios de la señal infrarroja.
 *
 * Esta variable permite observar el cambio de la señal IR filtrada entre una
 * muestra y otra. Puede ser útil para depuración o visualización del
 * comportamiento de la señal.
 */
long CAR_ultimo_delta = 0;

/**
 * @brief Umbral mínimo de señal infrarroja para considerar que hay dedo.
 *
 * Si la señal IR supera este valor, se interpreta que el dedo está colocado
 * sobre el sensor MAX30102.
 */
#define IR_UMBRAL_DEDO   50000UL

/**
 * @brief Valor mínimo permitido de BPM.
 *
 * Si el BPM calculado está por debajo de este valor, se descarta.
 */
#define BPM_MINIMO       40

/**
 * @brief Valor máximo permitido de BPM.
 *
 * Si el BPM calculado está por encima de este valor, se descarta.
 */
#define BPM_MAXIMO       180

/**
 * @brief Tamaño del buffer usado para suavizar la señal infrarroja.
 *
 * Se almacenan las últimas muestras de la señal IR para calcular un promedio
 * móvil y reducir variaciones bruscas o ruido.
 */
#define BUFFER_SIZE      16

/**
 * @brief Buffer circular para almacenar muestras infrarrojas.
 */
static unsigned long ir_buffer[BUFFER_SIZE];

/**
 * @brief Índice actual dentro del buffer circular.
 */
static unsigned char buf_idx = 0;

/**
 * @brief Promedio anterior de la señal infrarroja.
 *
 * Se usa para calcular la variación o delta entre el promedio actual y el
 * promedio anterior.
 */
static unsigned long ir_promedio_ant = 0;

/**
 * @brief Último valor válido de BPM calculado.
 */
static unsigned char bpm_actual = 0;

/**
 * @brief Contador de muestras transcurridas entre picos detectados.
 *
 * Se utiliza para estimar el periodo de la señal cardíaca y calcular el BPM.
 */
static unsigned int muestras_pico = 0;

/**
 * @brief Bandera que indica si actualmente se está dentro de un pico.
 *
 * Evita contar varias veces el mismo pico de la señal.
 *
 * - 0: no está dentro de un pico.
 * - 1: está dentro de un pico.
 */
static unsigned char en_pico = 0;

/**
 * @brief Número de picos detectados.
 *
 * Se usa para evitar calcular BPM con el primer pico, ya que se necesita una
 * referencia temporal entre picos.
 */
static unsigned char picos_contados = 0;

/**
 * @brief Calcula el promedio de las muestras almacenadas en el buffer IR.
 *
 * Esta función recorre el buffer de muestras infrarrojas y calcula un promedio
 * simple. Dicho promedio ayuda a suavizar la señal antes de detectar picos.
 *
 * @return Promedio de las muestras infrarrojas almacenadas.
 */
static unsigned long calcular_promedio(void) {
    unsigned long suma = 0;
    unsigned char i;

    for (i = 0; i < BUFFER_SIZE; i++) {
        suma += ir_buffer[i];
    }

    return suma / BUFFER_SIZE;
}

/**
 * @brief Determina si hay un dedo colocado sobre el MAX30102.
 *
 * La detección se realiza comparando la señal infrarroja recibida con un
 * umbral mínimo. Si la señal IR es mayor que el umbral, se considera que hay
 * dedo sobre el sensor.
 *
 * @param ir Valor de la muestra infrarroja obtenida del MAX30102.
 *
 * @return 1 si hay dedo detectado.
 * @return 0 si no hay dedo detectado.
 */
unsigned char CAR_HayDedo(unsigned long ir) {
    return (ir > IR_UMBRAL_DEDO) ? 1 : 0;
}

/**
 * @brief Procesa una muestra del MAX30102 para calcular el ritmo cardíaco.
 *
 * Esta función recibe las muestras roja e infrarroja del sensor MAX30102.
 * En este caso, el cálculo de BPM se basa principalmente en la señal
 * infrarroja, ya que esta permite detectar mejor las variaciones producidas
 * por el pulso sanguíneo.
 *
 * El procedimiento general es:
 * - Verificar si hay dedo sobre el sensor.
 * - Si no hay dedo, reiniciar variables internas.
 * - Guardar la muestra IR en un buffer circular.
 * - Calcular el promedio móvil de la señal IR.
 * - Obtener el delta entre el promedio anterior y el actual.
 * - Detectar picos de la señal.
 * - Calcular un BPM aproximado a partir del tiempo entre picos.
 *
 * @param red Muestra del LED rojo del MAX30102.
 * @param ir Muestra del LED infrarrojo del MAX30102.
 *
 * @return Último BPM válido calculado.
 * @return 0 si no hay dedo detectado.
 *
 * @note El parámetro red se recibe porque forma parte de la lectura del
 * MAX30102, pero en este algoritmo el cálculo de BPM se realiza usando
 * principalmente la componente infrarroja.
 */
unsigned char CAR_ProcesarMuestra(unsigned long red, unsigned long ir) {
    unsigned long promedio;
    long delta;

    /*
     * Si no se detecta dedo, se reinician las variables internas del algoritmo.
     * Esto evita que queden valores anteriores cuando el usuario retira el dedo.
     */
    if (!CAR_HayDedo(ir)) {
        unsigned char i;

        buf_idx          = 0;
        bpm_actual       = 0;
        muestras_pico    = 0;
        en_pico          = 0;
        picos_contados   = 0;
        ir_promedio_ant  = 0;
        CAR_ultimo_delta = 0;

        for (i = 0; i < BUFFER_SIZE; i++) {
            ir_buffer[i] = 0;
        }

        return 0;
    }

    /*
     * Almacena la muestra infrarroja en el buffer circular.
     * Cuando llega al final del buffer, vuelve al inicio.
     */
    ir_buffer[buf_idx] = ir;
    buf_idx = (buf_idx + 1) % BUFFER_SIZE;

    /*
     * Aumenta el contador de muestras desde el último pico detectado.
     */
    muestras_pico++;

    /*
     * Calcula el promedio móvil de la señal IR y luego calcula el delta.
     * El delta representa cómo cambia la señal suavizada entre una lectura
     * y la siguiente.
     */
    promedio         = calcular_promedio();
    delta            = (long)ir_promedio_ant - (long)promedio;
    CAR_ultimo_delta = delta;

    /*
     * Detección de pico.
     *
     * Si el delta supera cierto umbral positivo y no se estaba dentro de un pico,
     * se considera que apareció un nuevo pico de la señal.
     */
    if (delta > 80 && !en_pico) {
        en_pico = 1;
        picos_contados++;

        /*
         * Después de detectar al menos dos picos, se calcula el BPM.
         * La constante 3960 depende del tiempo de muestreo usado en el programa
         * principal y permite convertir la cantidad de muestras entre picos
         * en latidos por minuto.
         */
        if (picos_contados >= 2 && muestras_pico > 0) {
            unsigned int bpm_calc = (unsigned int)(3960UL / muestras_pico);

            /*
             * Solo se acepta el BPM si está dentro de un rango fisiológico
             * razonable.
             */
            if (bpm_calc >= BPM_MINIMO && bpm_calc <= BPM_MAXIMO) {
                bpm_actual = (unsigned char)bpm_calc;
            }
        }

        /*
         * Reinicia el contador de muestras para medir el tiempo hasta el
         * siguiente pico.
         */
        muestras_pico = 0;

        /*
         * Evita que el contador de picos crezca indefinidamente.
         */
        picos_contados = (picos_contados > 8) ? 1 : picos_contados;

    } else if (delta < -40) {
        /*
         * Cuando el delta baja lo suficiente, se considera que el pico terminó.
         * Esto permite detectar un nuevo pico más adelante.
         */
        en_pico = 0;
    }

    /*
     * Guarda el promedio actual para usarlo como referencia en la siguiente
     * llamada de la función.
     */
    ir_promedio_ant = promedio;

    /*
     * Retorna el último BPM válido calculado.
     */
    return bpm_actual;
}

/**
 * @brief Obtiene el último BPM válido calculado.
 *
 * Esta función permite consultar el valor actual del ritmo cardíaco sin
 * procesar una nueva muestra.
 *
 * @return Último valor de BPM calculado.
 */
unsigned char CAR_GetBPM(void) {
    return bpm_actual;
}