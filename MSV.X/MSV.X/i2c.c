/**
 * @file    i2c.c
 * @brief   Driver I2C maestro por hardware (modulo MSSP) para el PIC18F4550.
 *
 * Implementacion basada en el periferico MSSP del PIC18F4550 configurado
 * en modo I2C maestro. A diferencia del proyecto anterior (bit-banging),
 * el hardware genera automaticamente las condiciones del protocolo I2C,
 * lo que garantiza timing preciso y libera la CPU durante las transferencias.
 *
 * Registros utilizados:
 *  - SSPCON1  : Configuracion del modulo SSP (modo, habilitacion).
 *  - SSPCON2  : Control de condiciones I2C (start, stop, ack, repeated start).
 *  - SSPSTAT  : Estado del bus (buffer lleno, lectura/escritura en curso).
 *  - SSPADD   : Valor del generador de baud rate (velocidad del bus).
 *  - SSPBUF   : Buffer de datos (lectura y escritura).
 *  - PIR1     : Flags de interrupcion (SSPIF indica fin de operacion).
 *
 * Mecanismo de espera:
 *  Cada operacion espera activamente con un contador de timeout
 *  (I2C_TIMEOUT_MAX definido en config.h). Si el bus no responde,
 *  se retorna el codigo de error correspondiente sin bloquear el sistema.
 *
 * @note  Resistencias pull-up externas requeridas en SDA (RB0) y SCL (RB1).
 *        Valor recomendado: 4.7 kohm a VDD para 100 kHz.
 */

#include "i2c.h"

/* =========================================================
 *  MACRO INTERNA: ESPERA CON TIMEOUT
 *
 *  Espera activa mientras la condicion 'cond' sea verdadera.
 *  Si se supera I2C_TIMEOUT_MAX iteraciones, retorna el
 *  codigo de error especificado sin colgar el sistema.
 *
 *  Uso interno unicamente (no expuesta en el .h).
 * ========================================================= */
#define I2C_WAIT_TIMEOUT(cond, err_code)        \
    do {                                        \
        unsigned int _t = 0;                    \
        while ((cond)) {                        \
            if (++_t >= I2C_TIMEOUT_MAX)        \
                return (err_code);              \
        }                                       \
    } while(0)

/* =========================================================
 *  MACRO INTERNA: ESPERAR BUS LIBRE
 *
 *  El bus esta ocupado si cualquiera de las condiciones del
 *  SSPCON2 esta activa (SEN, RSEN, PEN, RCEN, ACKEN) o si
 *  el modulo esta en medio de una transferencia (R_W=1).
 *
 *  Mascara 0x1F cubre los bits ACKEN, RCEN, PEN, RSEN, SEN.
 * ========================================================= */
#define I2C_WAIT_IDLE()                                         \
    I2C_WAIT_TIMEOUT(((SSPCON2 & 0x1F) || SSPSTATbits.R_W),    \
                     I2C_ERR_TIMEOUT)

/* =========================================================
 *  I2C_Init
 * ========================================================= */
void I2C_Init(void) {
    /* -- Configuracion de pines --
     * RB0 (SDA) y RB1 (SCL) deben estar como entradas para que
     * el modulo MSSP controle la logica open-drain correctamente.
     * El MSSP los manejara internamente segun el protocolo. */
    TRISBbits.TRISB0 = 1;   /* RB0/SDA como entrada */
    TRISBbits.TRISB1 = 1;   /* RB1/SCL como entrada */

    /* -- Deshabilitar analogico en RB0 y RB1 --
     * ANCON1 controla los pines analogicos del PORTB en el PIC18F4550.
     * AN12 = RB0, AN10 = RB1 deben ser digitales para I2C. */

    /* -- Configurar modulo MSSP --
     * SSPCON1: SSPEN=1 habilita el modulo.
     * SSPM3:SSPM0 = 1000 selecciona modo I2C maestro con baud rate
     * generado por SSPADD. */
    SSPCON1 = 0x28;   /* 0b00101000: SSPEN=1, modo I2C maestro */

    /* -- Velocidad del bus: 100 kHz --
     * SSPADD = (Fosc / (4 * Fscl)) - 1
     *        = (8000000 / (4 * 100000)) - 1
     *        = 20 - 1 = 19 */
    SSPADD = 19;

    /* -- Limpiar flag de interrupcion SSP --
     * PIR1bits.SSPIF se pone en 1 al completar cada operacion.
     * Debe limpiarse manualmente antes de cada espera. */
    PIR1bits.SSPIF = 0;

    /* -- SSPSTAT: modo estandar (100 kHz) --
     * SMP=0: slew rate habilitado (requerido para 400 kHz;
     *        indiferente a 100 kHz pero es buena practica).
     * CKE=0: niveles de voltaje I2C estandar. */
    SSPSTAT = 0x00;
}

/* =========================================================
 *  I2C_Start
 * ========================================================= */
unsigned char I2C_Start(void) {
    /* Esperar a que el bus este libre antes de iniciar */
    I2C_WAIT_IDLE();

    /* Generar condicion START: SDA baja mientras SCL esta alto */
    SSPCON2bits.SEN = 1;

    /* Esperar a que el hardware limpie SEN (operacion completa) */
    I2C_WAIT_TIMEOUT(SSPCON2bits.SEN, I2C_ERR_TIMEOUT);

    return I2C_OK;
}

/* =========================================================
 *  I2C_Stop
 * ========================================================= */
unsigned char I2C_Stop(void) {
    I2C_WAIT_IDLE();

    /* Generar condicion STOP: SDA sube mientras SCL esta alto */
    SSPCON2bits.PEN = 1;

    I2C_WAIT_TIMEOUT(SSPCON2bits.PEN, I2C_ERR_TIMEOUT);

    return I2C_OK;
}

/* =========================================================
 *  I2C_RepeatStart
 * ========================================================= */
unsigned char I2C_RepeatStart(void) {
    I2C_WAIT_IDLE();

    /* Generar Repeated START sin soltar el bus */
    SSPCON2bits.RSEN = 1;

    I2C_WAIT_TIMEOUT(SSPCON2bits.RSEN, I2C_ERR_TIMEOUT);

    return I2C_OK;
}

/* =========================================================
 *  I2C_Write
 * ========================================================= */
unsigned char I2C_Write(unsigned char data) {
    I2C_WAIT_IDLE();

    /* Cargar el byte en el buffer de transmision.
     * El MSSP inicia la transmision automaticamente. */
    PIR1bits.SSPIF = 0;
    SSPBUF = data;

    /* Esperar a que se complete la transmision del byte */
    I2C_WAIT_TIMEOUT(!PIR1bits.SSPIF, I2C_ERR_TIMEOUT);
    PIR1bits.SSPIF = 0;

    /* Verificar ACK del esclavo.
     * ACKSTAT=0 significa que el esclavo respondio con ACK. */
    if (SSPCON2bits.ACKSTAT) {
        return I2C_ERR_NACK;
    }

    return I2C_OK;
}

/* =========================================================
 *  I2C_Read
 * ========================================================= */
unsigned char I2C_Read(unsigned char ack) {
    unsigned char data;

    I2C_WAIT_IDLE();

    /* Habilitar modo recepcion: el MSSP genera los pulsos de
     * reloj para que el esclavo coloque los bits en SDA. */
    PIR1bits.SSPIF = 0;
    SSPCON2bits.RCEN = 1;

    /* Esperar a que el buffer se llene (byte recibido completo) */
    I2C_WAIT_TIMEOUT(!SSPSTATbits.BF, I2C_ERR_TIMEOUT);

    /* Leer el byte recibido del buffer */
    data = SSPBUF;

    I2C_WAIT_IDLE();

    /* Enviar ACK o NACK al esclavo segun el parametro.
     * ACKDT=0 -> ACK  (SDA baja: maestro quiere mas datos).
     * ACKDT=1 -> NACK (SDA alta: ultimo byte, fin de lectura). */
    SSPCON2bits.ACKDT  = (ack) ? 0 : 1;
    SSPCON2bits.ACKEN  = 1;

    /* Esperar a que se complete la secuencia de ACK/NACK */
    I2C_WAIT_TIMEOUT(SSPCON2bits.ACKEN, I2C_ERR_TIMEOUT);

    return data;
}

/* =========================================================
 *  I2C_WriteReg
 *  Funcion de conveniencia: escribe un byte en un registro.
 * ========================================================= */
unsigned char I2C_WriteReg(unsigned char addr,
                           unsigned char reg,
                           unsigned char data) {
    unsigned char status;

    status = I2C_Start();
    if (status != I2C_OK) { I2C_Stop(); return status; }

    /* Direccion del dispositivo + bit de escritura (R/W = 0) */
    status = I2C_Write((addr << 1) & 0xFE);
    if (status != I2C_OK) { I2C_Stop(); return status; }

    status = I2C_Write(reg);
    if (status != I2C_OK) { I2C_Stop(); return status; }

    status = I2C_Write(data);
    if (status != I2C_OK) { I2C_Stop(); return status; }

    return I2C_Stop();
}

/* =========================================================
 *  I2C_ReadReg
 *  Funcion de conveniencia: lee un byte de un registro.
 * ========================================================= */
unsigned char I2C_ReadReg(unsigned char addr, unsigned char reg) {
    unsigned char data;
    unsigned char status;

    /* Fase escritura: apuntar al registro */
    status = I2C_Start();
    if (status != I2C_OK) { I2C_Stop(); return 0xFF; }

    status = I2C_Write((addr << 1) & 0xFE);
    if (status != I2C_OK) { I2C_Stop(); return 0xFF; }

    status = I2C_Write(reg);
    if (status != I2C_OK) { I2C_Stop(); return 0xFF; }

    /* Fase lectura: repeated start + direccion + R/W=1 */
    status = I2C_RepeatStart();
    if (status != I2C_OK) { I2C_Stop(); return 0xFF; }

    status = I2C_Write((unsigned char)((addr << 1) | 0x01));
    if (status != I2C_OK) { I2C_Stop(); return 0xFF; }

    /* Leer el unico byte con NACK (fin de transferencia) */
    data = I2C_Read(0);

    I2C_Stop();
    return data;
}

/* =========================================================
 *  I2C_ReadBurst
 *  Funcion de conveniencia: lee N bytes consecutivos.
 *  Util para leer los registros de temperatura del BME280
 *  y el FIFO del MAX30102 de una sola vez.
 * ========================================================= */
unsigned char I2C_ReadBurst(unsigned char  addr,
                            unsigned char  reg,
                            unsigned char *buf,
                            unsigned char  len) {
    unsigned char i;
    unsigned char status;

    if (buf == 0 || len == 0) return I2C_ERR_TIMEOUT;

    /* Apuntar al registro inicial */
    status = I2C_Start();
    if (status != I2C_OK) { I2C_Stop(); return status; }

    status = I2C_Write((addr << 1) & 0xFE);
    if (status != I2C_OK) { I2C_Stop(); return status; }

    status = I2C_Write(reg);
    if (status != I2C_OK) { I2C_Stop(); return status; }

    /* Repeated start para cambiar a modo lectura */
    status = I2C_RepeatStart();
    if (status != I2C_OK) { I2C_Stop(); return status; }

    status = I2C_Write((unsigned char)((addr << 1) | 0x01));
    if (status != I2C_OK) { I2C_Stop(); return status; }

    /* Leer los bytes: ACK en todos excepto el ultimo (NACK) */
    for (i = 0; i < len; i++) {
        buf[i] = I2C_Read((i < (len - 1)) ? 1 : 0);
    }

    return I2C_Stop();
}