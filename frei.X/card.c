#include "card.h"
#include "max30102.h"

long CAR_ultimo_delta = 0;

#define IR_UMBRAL_DEDO   50000UL
#define BPM_MINIMO       40
#define BPM_MAXIMO       180
#define BUFFER_SIZE      16

static unsigned long ir_buffer[BUFFER_SIZE];
static unsigned char buf_idx            = 0;
static unsigned long ir_promedio_ant    = 0;
static unsigned char bpm_actual         = 0;
static unsigned int  muestras_pico      = 0;
static unsigned char en_pico            = 0;
static unsigned char picos_contados     = 0;

static unsigned long calcular_promedio(void) {
    unsigned long suma = 0;
    unsigned char i;
    for (i = 0; i < BUFFER_SIZE; i++) suma += ir_buffer[i];
    return suma / BUFFER_SIZE;
}

unsigned char CAR_HayDedo(unsigned long ir) {
    return (ir > IR_UMBRAL_DEDO) ? 1 : 0;
}

unsigned char CAR_ProcesarMuestra(unsigned long red, unsigned long ir) {
    unsigned long promedio;
    long delta;

    if (!CAR_HayDedo(ir)) {
        unsigned char i;
        buf_idx          = 0;
        bpm_actual       = 0;
        muestras_pico    = 0;
        en_pico          = 0;
        picos_contados   = 0;
        ir_promedio_ant  = 0;
        CAR_ultimo_delta = 0;
        for (i = 0; i < BUFFER_SIZE; i++) ir_buffer[i] = 0;
        return 0;
    }

    ir_buffer[buf_idx] = ir;
    buf_idx = (buf_idx + 1) % BUFFER_SIZE;
    muestras_pico++;

    promedio         = calcular_promedio();
    delta            = (long)ir_promedio_ant - (long)promedio;
    CAR_ultimo_delta = delta;

    if (delta > 80 && !en_pico) {
        en_pico = 1;
        picos_contados++;
        if (picos_contados >= 2 && muestras_pico > 0) {
            unsigned int bpm_calc = (unsigned int)(3960UL / muestras_pico);
            if (bpm_calc >= BPM_MINIMO && bpm_calc <= BPM_MAXIMO) {
                bpm_actual = (unsigned char)bpm_calc;
            }
        }
        muestras_pico  = 0;
        picos_contados = (picos_contados > 8) ? 1 : picos_contados;
    } else if (delta < -40) {
        en_pico = 0;
    }

    ir_promedio_ant = promedio;
    return bpm_actual;
}

unsigned char CAR_GetBPM(void) { return bpm_actual; }