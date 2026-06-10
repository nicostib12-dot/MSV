
/**
 * @file ds18b20.c
 * @brief Driver OneWire para el sensor de temperatura DS18B20 en PIC18F4550.
 *
 * Este archivo implementa las funciones necesarias para comunicarse con el
 * sensor DS18B20 usando el protocolo OneWire. El sensor se conecta al pin RD6
 * del PIC18F4550 y requiere una resistencia pull-up externa de 4.7 k? hacia VDD.
 *
 * El driver permite:
 * - Generar el pulso de reset OneWire.
 * - Detectar la presencia del sensor.
 * - Escribir bits y bytes en el bus.
 * - Leer bits y bytes desde el bus.
 * - Iniciar la conversión de temperatura.
 * - Esperar la conversión.
 * - Leer la temperatura en grados Celsius.
 *
 * Conexión usada:
 * - RD6: línea de datos OneWire del DS18B20.
 * - Pull-up externo: 4.7 k? entre RD6 y VDD.
 *
 * Lógica del bus OneWire:
 * - OW_DIR = 0: el PIC configura el pin como salida y domina el bus.
 * - OW_DIR = 1: el PIC libera el bus y el pull-up externo lo lleva a nivel alto.
 * - OW_LOW = 0: el PIC fuerza el bus a nivel bajo.
 * - OW_LOW = 1: el latch queda en alto, útil antes de liberar el bus.
 *
 * @note Regla importante: siempre se debe colocar OW_LOW antes de cambiar
 * OW_DIR para evitar cambios bruscos o glitches en el bus OneWire.
 */

#include "config.h"
#include "ds18b20.h"

/* -------------------------------------------------------------------------- */
/**
 * @brief Libera el bus OneWire y lo deja en estado de reposo.
 *
 * Esta función coloca primero el latch del pin en alto y luego configura
 * el pin como entrada. Al quedar como entrada, el PIC deja de dominar el bus
 * y la resistencia pull-up externa eleva la línea a VDD.
 *
 * Esta operación es necesaria después de escribir un bit, después del pulso
 * de reset y antes de permitir que el sensor responda.
 */
static void OW_Release(void) {
    OW_LOW = 1;    /**< Coloca el latch en alto antes de liberar el bus. */
    OW_DIR = 1;    /**< Configura el pin como entrada para que el pull-up suba el bus. */
}

/* -------------------------------------------------------------------------- */
/**
 * @brief Genera el pulso de reset OneWire y detecta la presencia del DS18B20.
 *
 * El protocolo OneWire inicia con un pulso de reset generado por el maestro.
 * El PIC fuerza el bus a nivel bajo durante aproximadamente 500 us y luego
 * lo libera. Si el sensor está presente, este responde llevando el bus a nivel
 * bajo durante la ventana de presencia.
 *
 * Procedimiento:
 * - Liberar el bus.
 * - Verificar que el bus esté en alto.
 * - Forzar el bus a bajo durante 500 us.
 * - Liberar el bus.
 * - Esperar la ventana de presencia.
 * - Leer si el sensor respondió.
 *
 * @return 1 si el sensor respondió con pulso de presencia.
 * @return 0 si no se detectó sensor o el bus estaba en estado incorrecto.
 */
unsigned char DS18B20_Reset(void) {
    unsigned char presence;

    /*
     * Se asegura que el bus esté libre antes de iniciar la secuencia de reset.
     */
    OW_Release();
    __delay_us(10);

    /*
     * Si el bus está en bajo antes del reset, puede existir un problema:
     * sensor desconectado, corto a GND o error en la conexión.
     */
    if (OW_PIN == 0) {
        return 0;
    }

    /*
     * Pulso de reset:
     * el maestro OneWire fuerza el bus a bajo durante aproximadamente 500 us.
     */
    OW_LOW = 0;
    OW_DIR = 0;
    __delay_us(500);

    /*
     * El maestro libera el bus. Si el sensor está presente, responderá
     * llevando el bus a nivel bajo dentro de la ventana de presencia.
     */
    OW_Release();
    __delay_us(70);

    /*
     * Si el pin se lee en bajo, significa que el sensor respondió.
     */
    presence = (OW_PIN == 0);

    /*
     * Se espera el resto del tiempo de reset para completar la ranura.
     */
    __delay_us(430);

    return presence;
}

/* -------------------------------------------------------------------------- */
/**
 * @brief Escribe un bit en el bus OneWire.
 *
 * El protocolo OneWire transmite bits mediante ranuras de tiempo. Para escribir
 * un bit en 1, el maestro baja brevemente el bus y luego lo libera. Para
 * escribir un bit en 0, el maestro mantiene el bus en bajo durante casi toda
 * la ranura.
 *
 * @param bit Bit que se desea escribir.
 *            - 0: escribe un cero lógico.
 *            - distinto de 0: escribe un uno lógico.
 */
static void OW_WriteBit(unsigned char bit) {
    if (bit) {
        /*
         * Escritura de un '1':
         * se baja el bus muy poco tiempo y luego se libera para que el pull-up
         * lo mantenga en alto.
         */
        OW_LOW = 0;
        OW_DIR = 0;
        __delay_us(2);

        OW_Release();
        __delay_us(62);
    } else {
        /*
         * Escritura de un '0':
         * se mantiene el bus en bajo durante casi toda la ranura de tiempo.
         */
        OW_LOW = 0;
        OW_DIR = 0;
        __delay_us(64);

        OW_Release();
        __delay_us(2);
    }
}

/* -------------------------------------------------------------------------- */
/**
 * @brief Lee un bit desde el bus OneWire.
 *
 * Para leer un bit, el PIC inicia la ranura bajando brevemente el bus y luego
 * lo libera. Después de liberar el bus, el DS18B20 coloca el valor del bit y
 * el PIC lo muestrea dentro del tiempo especificado.
 *
 * @return Bit leído desde el DS18B20.
 * @retval 0 Bit en bajo.
 * @retval 1 Bit en alto.
 */
static unsigned char OW_ReadBit(void) {
    unsigned char bit;

    /*
     * El maestro inicia la ranura de lectura llevando el bus a bajo.
     */
    OW_LOW = 0;
    OW_DIR = 0;
    __delay_us(2);

    /*
     * Se libera el bus para que el sensor pueda colocar su dato.
     */
    OW_Release();

    /*
     * Se muestrea el bus dentro de la ventana de lectura.
     */
    __delay_us(10);
    bit = OW_PIN;

    /*
     * Se completa la ranura de tiempo.
     */
    __delay_us(52);

    return bit;
}

/* -------------------------------------------------------------------------- */
/**
 * @brief Escribe un byte completo en el bus OneWire.
 *
 * El DS18B20 recibe los datos empezando por el bit menos significativo.
 * Por eso se escribe primero el bit 0 del byte y luego se desplaza el dato
 * hacia la derecha.
 *
 * @param byte Byte que se desea transmitir al DS18B20.
 */
void DS18B20_WriteByte(unsigned char byte) {
    unsigned char i;

    for (i = 0; i < 8; i++) {
        OW_WriteBit(byte & 0x01);
        byte >>= 1;
    }
}

/* -------------------------------------------------------------------------- */
/**
 * @brief Lee un byte completo desde el bus OneWire.
 *
 * El DS18B20 transmite los datos empezando por el bit menos significativo.
 * La función lee ocho bits y los organiza dentro de una variable de 8 bits.
 *
 * @return Byte leído desde el DS18B20.
 */
unsigned char DS18B20_ReadByte(void) {
    unsigned char i;
    unsigned char data = 0;

    for (i = 0; i < 8; i++) {
        data >>= 1;

        if (OW_ReadBit()) {
            data |= 0x80;
        }
    }

    return data;
}

/* -------------------------------------------------------------------------- */
/**
 * @brief Inicia una conversión de temperatura en el DS18B20.
 *
 * Esta función envía los comandos necesarios para que el DS18B20 comience a
 * medir temperatura.
 *
 * Comandos usados:
 * - 0xCC: Skip ROM, usado cuando solo hay un sensor en el bus.
 * - 0x44: Convert T, inicia la conversión de temperatura.
 *
 * @return 1 si la conversión fue iniciada correctamente.
 * @return 0 si no se detectó el sensor.
 *
 * @note Esta función no espera a que la conversión termine. Después de llamarla
 * se debe esperar el tiempo requerido antes de leer la temperatura.
 */
unsigned char DS18B20_StartConversion(void) {
    if (!DS18B20_Reset()) {
        return 0;
    }

    DS18B20_WriteByte(0xCC);    /**< Skip ROM: selecciona el único sensor del bus. */
    DS18B20_WriteByte(0x44);    /**< Convert T: inicia la conversión de temperatura. */

    return 1;
}

/* -------------------------------------------------------------------------- */
/**
 * @brief Espera el tiempo necesario para finalizar la conversión de temperatura.
 *
 * El DS18B20 puede tardar hasta 750 ms en realizar una conversión con resolución
 * de 12 bits. Por seguridad, se espera aproximadamente 800 ms.
 *
 * En este proyecto el sensor se usa con alimentación externa, por lo que se
 * emplea un retardo fijo en lugar de hacer polling del bus.
 *
 * @note Esta función debe llamarse después de DS18B20_StartConversion() antes
 * de leer la temperatura con DS18B20_ReadTemp().
 */
void DS18B20_WaitConversion(void) {
    __delay_ms(100);
    __delay_ms(100);
    __delay_ms(100);
    __delay_ms(100);
    __delay_ms(100);
    __delay_ms(100);
    __delay_ms(100);
    __delay_ms(100);
}

/* -------------------------------------------------------------------------- */
/**
 * @brief Lee la temperatura medida por el DS18B20 en grados Celsius.
 *
 * Esta función lee los dos primeros bytes del scratchpad del DS18B20, que
 * corresponden al LSB y MSB de la temperatura. Luego combina ambos bytes para
 * formar el valor crudo de temperatura y lo convierte a grados Celsius.
 *
 * Procedimiento:
 * - Verificar presencia del sensor.
 * - Enviar comando Skip ROM.
 * - Enviar comando Read Scratchpad.
 * - Leer LSB y MSB.
 * - Validar posibles errores.
 * - Convertir el valor crudo a grados Celsius.
 *
 * @return Temperatura real en grados Celsius si la lectura es válida.
 * @return -999.0f si no se detecta el sensor.
 * @return -998.0f si los bytes leídos son inválidos.
 * @return -997.0f si se lee el valor de power-on reset, equivalente a 85.0 °C.
 *
 * @note Esta función debe llamarse después de esperar la conversión, ya sea
 * usando DS18B20_WaitConversion() o dejando pasar al menos 800 ms después de
 * DS18B20_StartConversion().
 */
float DS18B20_ReadTemp(void) {
    unsigned char lsb;
    unsigned char msb;
    int raw;

    /*
     * Si el sensor no responde al reset, se retorna un código de error.
     */
    if (!DS18B20_Reset()) {
        return -999.0f;
    }

    /*
     * Se selecciona el sensor y se solicita la lectura del scratchpad.
     */
    DS18B20_WriteByte(0xCC);    /**< Skip ROM. */
    DS18B20_WriteByte(0xBE);    /**< Read Scratchpad. */

    /*
     * Los dos primeros bytes del scratchpad corresponden a la temperatura.
     */
    lsb = DS18B20_ReadByte();
    msb = DS18B20_ReadByte();

    /*
     * Validaciones para detectar lecturas erróneas.
     */
    if (lsb == 0xFF && msb == 0xFF) {
        return -998.0f;
    }

    if (lsb == 0x00 && msb == 0x00) {
        return -998.0f;
    }

    /*
     * El DS18B20 puede entregar 85.0 °C después de encender si se lee demasiado
     * rápido antes de una conversión válida.
     */
    if (lsb == 0x50 && msb == 0x05) {
        return -997.0f;
    }

    /*
     * Se combinan MSB y LSB para formar el valor crudo de 16 bits.
     * En resolución de 12 bits, cada unidad equivale a 0.0625 °C,
     * es decir, temperatura = raw / 16.
     */
    raw = (int)((unsigned int)((msb << 8) | lsb));

    return (float)raw / 16.0f;
}