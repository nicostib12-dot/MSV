/**
 * @file main.c
 * @brief Programa principal del sistema Cardio.X.
 *
 * Este archivo contiene la lógica principal del sistema Cardio.X basado en
 * el microcontrolador PIC18F4550. El sistema permite medir ritmo cardíaco
 * mediante el sensor MAX30102, temperatura mediante el sensor DS18B20,
 * mostrar los datos en una pantalla OLED, enviar información por UART y
 * activar un buzzer cuando se detectan valores fuera de rango.
 *
 * Distribución de pines:
 *
 * PORTD:
 * - RD0: LED1, indicador de sistema encendido.
 * - RD1: LED2, indicador de sistema activo.
 * - RD2: LED3, indicador auxiliar usado en el arranque.
 * - RD3: LED4, indicador de sistema en pausa.
 * - RD6: línea OneWire para el sensor DS18B20.
 *
 * PORTB:
 * - RB0: línea SCL para comunicación I2C.
 * - RB1: línea SDA para comunicación I2C.
 * - RB2: buzzer.
 * - RB3: botón de cambio de perfil.
 * - RB4: botón de apagado/encendido lógico del sistema.
 *
 * PORTC:
 * - RC6: transmisión UART.
 * - RC7: recepción UART.
 */

#include <xc.h>
#include "config.h"
#include "i2c.h"
#include "oled.h"
#include "ds18b20.h"
#include "max30102.h"
#include "card.h"
#include "uart.h"

/**
 * @brief Pin de control del buzzer.
 *
 * El buzzer se encuentra conectado al pin RB2 del PIC.
 */
#define BUZZER              LATBbits.LATB2

/**
 * @brief Botón de cambio de perfil.
 *
 * Este botón está conectado al pin RB3. Permite alternar entre
 * perfil normal y perfil deportista.
 */
#define BTN_PERFIL          PORTBbits.RB3

/**
 * @brief Botón de apagado/encendido lógico.
 *
 * Este botón está conectado al pin RB4. No corta físicamente la alimentación,
 * pero permite suspender o reactivar el funcionamiento del sistema.
 */
#define BTN_POWER           PORTBbits.RB4

/**
 * @brief Temperatura mínima para considerar que existe actividad térmica.
 *
 * Se usa para determinar si hay una lectura de temperatura asociada
 * al cuerpo del usuario.
 */
#define TEMP_UMBRAL_CUERPO  30.0f

/**
 * @brief Límite inferior de BPM para el perfil normal.
 */
#define BPM_MIN_NORMAL       50

/**
 * @brief Límite superior de BPM para el perfil normal.
 */
#define BPM_MAX_NORMAL      120

/**
 * @brief Temperatura máxima permitida para el perfil normal.
 */
#define TEMP_MAX_NORMAL      38.5f

/**
 * @brief Límite inferior de BPM para el perfil deportista.
 */
#define BPM_MIN_DEPORTISTA   40

/**
 * @brief Límite superior de BPM para el perfil deportista.
 */
#define BPM_MAX_DEPORTISTA  180

/**
 * @brief Temperatura máxima permitida para el perfil deportista.
 */
#define TEMP_MAX_DEPORTISTA  39.0f

/**
 * @brief Número de ciclos necesarios para alternar el estado del buzzer.
 *
 * Un valor menor hace que el buzzer cambie más rápido entre encendido
 * y apagado. Un valor mayor lo hace más lento.
 */
#define BUZZER_TOGGLE_CICLOS  7

/**
 * @brief Número de ciclos para enviar datos por UART.
 *
 * Controla cada cuánto se envían los signos vitales por comunicación serial.
 */
#define UART_CADA_CICLOS     15

/* -------------------------------------------------------------------------- */
/**
 * @brief Configura el oscilador y los puertos del microcontrolador.
 *
 * Esta función inicializa el oscilador interno, configura los puertos de
 * entrada y salida, habilita los pull-ups internos del puerto B y desactiva
 * funciones analógicas que podrían interferir con el uso digital de los pines.
 *
 * Configuración principal:
 * - PORTD: RD6 como entrada para DS18B20 y el resto como salidas.
 * - PORTB: RB0, RB1 y RB2 como salidas; RB3 y RB4 como entradas.
 * - Pull-ups internos del puerto B activados.
 * - Comparadores analógicos desactivados.
 * - Pines analógicos configurados como digitales.
 */
static void setup(void) {
    OSCCON = 0x72;
    while (!OSCCONbits.IOFS);
    __delay_ms(100);

    /*
     * TRISD = 0x40:
     * 0b01000000
     *
     * RD6 queda como entrada para el sensor DS18B20.
     * Los demás pines del puerto D quedan como salidas para LEDs.
     */
    TRISD = 0x40;
    LATD  = 0x00;

    /*
     * TRISB = 0xF8:
     * 0b11111000
     *
     * RB0 = salida SCL.
     * RB1 = salida SDA.
     * RB2 = salida buzzer.
     * RB3 = entrada botón perfil.
     * RB4 = entrada botón power.
     * RB5, RB6 y RB7 quedan como entradas no usadas.
     */
    TRISB = 0xF8;
    LATB  = 0x00;

    /*
     * Activa los pull-ups internos del puerto B.
     * Esto permite conectar los botones entre el pin y GND.
     */
    INTCON2bits.RBPU = 0;

    /*
     * Desactiva los comparadores analógicos.
     */
    CMCON  = 0x07;

    /*
     * Configura los pines analógicos como digitales.
     */
    ADCON1 = 0x0F;
}

/* -------------------------------------------------------------------------- */
/**
 * @brief Hace parpadear el LED amarillo durante el arranque.
 *
 * Esta función se usa como indicación visual de inicio del sistema.
 *
 * @param n Número de veces que debe parpadear el LED amarillo.
 */
static void blink_amarillo(unsigned char n) {
    unsigned char k;

    for (k = 0; k < n; k++) {
        LED3 = 1;
        __delay_ms(200);

        LED3 = 0;
        __delay_ms(200);
    }
}

/* -------------------------------------------------------------------------- */
/**
 * @brief Muestra en la OLED el perfil actualmente seleccionado.
 *
 * El sistema maneja dos perfiles:
 * - Perfil normal.
 * - Perfil deportista.
 *
 * @param deportista Valor que indica el perfil activo.
 *                   - 0: perfil normal.
 *                   - 1: perfil deportista.
 */
static void mostrar_perfil(unsigned char deportista) {
    OLED_Clear();
    OLED_SetCursor(10, 2);

    if (deportista)
        OLED_WriteString("PERFIL: DEPORTISTA");
    else
        OLED_WriteString("PERFIL: NORMAL");

    __delay_ms(1000);
    OLED_Clear();
}

/* -------------------------------------------------------------------------- */
/**
 * @brief Apaga lógicamente el sistema.
 *
 * Esta función no corta la alimentación física del circuito. Lo que hace es
 * apagar las salidas principales y limpiar la pantalla OLED.
 *
 * Acciones realizadas:
 * - Apaga los LED.
 * - Apaga el buzzer.
 * - Limpia la pantalla OLED.
 * - Envía un mensaje por UART indicando que el sistema fue apagado.
 */
static void apagar_sistema(void) {
    LED1 = 0;
    LED2 = 0;
    LED3 = 0;
    LED4 = 0;
    BUZZER = 0;

    OLED_Clear();

    UART_WriteString("=== SISTEMA APAGADO ===\r\n");
}

/* -------------------------------------------------------------------------- */
/**
 * @brief Enciende lógicamente el sistema.
 *
 * Esta función reactiva el funcionamiento del sistema después de haber sido
 * apagado de forma lógica mediante el botón conectado a RB4.
 *
 * Acciones realizadas:
 * - Muestra un mensaje de encendido en la OLED.
 * - Envía un mensaje por UART.
 * - Enciende el LED1 como indicador de sistema encendido.
 */
static void encender_sistema(void) {
    OLED_Clear();
    OLED_SetCursor(20, 2);
    OLED_WriteString("CARDIO.X");

    OLED_SetCursor(15, 4);
    OLED_WriteString("SISTEMA ON");

    UART_WriteString("=== SISTEMA ENCENDIDO ===\r\n");

    LED1 = 1;
    __delay_ms(1000);

    OLED_Clear();
}

/* -------------------------------------------------------------------------- */
/**
 * @brief Función principal del programa.
 *
 * Esta función inicializa todos los módulos del sistema y ejecuta el ciclo
 * principal de monitoreo. En el ciclo principal se gestionan:
 *
 * - Botón de apagado/encendido lógico.
 * - Botón de cambio de perfil.
 * - Lectura del sensor MAX30102.
 * - Lectura del sensor DS18B20.
 * - Evaluación de alarmas.
 * - Control del buzzer.
 * - Actualización de LEDs.
 * - Actualización de pantalla OLED.
 * - Envío de datos por UART.
 */
void main(void) {
    /**
     * @brief Variables para almacenar las muestras roja e infrarroja del MAX30102.
     */
    unsigned long red, ir;

    /**
     * @brief Ritmo cardíaco calculado en BPM.
     */
    unsigned char bpm = 0;

    /**
     * @brief Indica si el MAX30102 detecta un dedo.
     *
     * - 0: no hay dedo detectado.
     * - 1: hay dedo detectado.
     */
    unsigned char hay_dedo = 0;

    /**
     * @brief Indica si existe una lectura de temperatura asociada al cuerpo.
     *
     * Se activa cuando la temperatura medida supera TEMP_UMBRAL_CUERPO.
     */
    unsigned char hay_temp = 0;

    /**
     * @brief Estado general de actividad del sistema.
     *
     * El sistema se considera activo si hay dedo detectado en el MAX30102
     * o si existe una lectura de temperatura asociada al cuerpo.
     */
    unsigned char activo = 0;

    /**
     * @brief Estado de alarma del sistema.
     *
     * - 0: no hay alarma.
     * - 1: hay alarma por BPM o temperatura fuera de rango.
     */
    unsigned char alarma = 0;

    /**
     * @brief Variable auxiliar usada en ciclos repetitivos.
     */
    unsigned char i;

    /**
     * @brief Contador para controlar la actualización de la OLED.
     */
    unsigned char ciclo_oled = 0;

    /**
     * @brief Contador para controlar cada cuánto se lee la temperatura.
     */
    unsigned char ciclo_temp = 0;

    /**
     * @brief Contador para controlar la intermitencia del buzzer.
     */
    unsigned char ciclo_buzzer = 0;

    /**
     * @brief Contador para controlar cada cuánto se envían datos por UART.
     */
    unsigned char ciclo_uart = 0;

    /**
     * @brief Estado actual del buzzer.
     *
     * Permite alternar el buzzer entre encendido y apagado.
     */
    unsigned char buzzer_estado = 0;

    /**
     * @brief Perfil seleccionado.
     *
     * - 0: perfil normal.
     * - 1: perfil deportista.
     */
    unsigned char perfil = 0;

    /**
     * @brief Estado anterior del botón de perfil.
     *
     * Se usa para detectar el flanco de pulsación del botón.
     */
    unsigned char btn_perf_ant = 1;

    /**
     * @brief Estado lógico del sistema.
     *
     * - 0: sistema apagado lógicamente.
     * - 1: sistema encendido.
     */
    unsigned char sistema_encendido = 1;

    /**
     * @brief Temperatura medida por el DS18B20 en grados Celsius.
     */
    float temp_f = 0.0f;

    /**
     * @brief Parte entera de la temperatura.
     */
    unsigned char temp_int = 0;

    /**
     * @brief Parte decimal de la temperatura.
     */
    unsigned char temp_dec = 0;

    /**
     * @brief Indica si la lectura de temperatura es válida.
     *
     * - 0: temperatura inválida.
     * - 1: temperatura válida.
     */
    unsigned char temp_valida = 0;

    /*
     * Inicialización general del sistema.
     */
    setup();
    I2C_Init();
    UART_Init();
    OLED_Init();

    UART_WriteString("=== CARDIO.X INICIANDO ===\r\n");

    /*
     * Secuencia visual de inicio.
     */
    blink_amarillo(3);
    OLED_Splash();
    __delay_ms(2000);

    /*
     * Inicialización y primera lectura del sensor DS18B20.
     */
    LED3 = 1;

    if (!DS18B20_Reset()) {
        OLED_Error("DS18 ERR");
        UART_WriteString("ERR: DS18B20 no encontrado\r\n");
        __delay_ms(2000);
    } else {
        DS18B20_StartConversion();
        DS18B20_WaitConversion();

        temp_f = DS18B20_ReadTemp();

        if (temp_f > -55.0f && temp_f < 125.0f) {
            temp_valida = 1;
            temp_int = (unsigned char)temp_f;
            temp_dec = (unsigned char)((temp_f - (float)temp_int) * 10.0f);
            hay_temp = (temp_f >= TEMP_UMBRAL_CUERPO) ? 1 : 0;

            UART_WriteString("DS18B20 OK\r\n");
        } else {
            if (temp_f == -997.0f) {
                OLED_Error("POR 85C");
                UART_WriteString("ERR: POR 85C\r\n");
            } else if (temp_f == -998.0f) {
                OLED_Error("RAW ERR");
                UART_WriteString("ERR: RAW\r\n");
            } else {
                OLED_Error("NO SENS");
                UART_WriteString("ERR: SIN SENSOR\r\n");
            }

            __delay_ms(2000);
        }

        DS18B20_StartConversion();
    }

    LED3 = 0;

    /*
     * Inicialización del sensor MAX30102.
     */
    if (!MAX30102_Init()) {
        OLED_Error("MAX ERR");
        UART_WriteString("ERR: MAX30102 no encontrado\r\n");

        while (1);
    }

    UART_WriteString("MAX30102 OK\r\n");
    UART_WriteString("=== SISTEMA LISTO ===\r\n");

    LED1 = 1;
    OLED_Clear();

    /*
     * Ciclo principal del sistema.
     */
    while (1) {

        /**
         * @brief Gestión del botón de apagado/encendido lógico.
         *
         * El botón conectado a RB4 funciona en lógica activa baja.
         * Esto significa que:
         * - Sin presionar: RB4 = 1.
         * - Presionado: RB4 = 0.
         *
         * Cada pulsación cambia el estado del sistema entre encendido
         * y apagado lógico.
         */
        if (BTN_POWER == 0) {
            __delay_ms(30);

            if (BTN_POWER == 0) {
                sistema_encendido ^= 1;

                if (sistema_encendido) {
                    encender_sistema();
                } else {
                    apagar_sistema();
                }

                while (BTN_POWER == 0);
                __delay_ms(100);
            }
        }

        /**
         * @brief Estado de apagado lógico.
         *
         * Si el sistema se encuentra apagado lógicamente, se mantienen
         * desactivadas las salidas principales y se evita ejecutar el resto
         * del ciclo de monitoreo.
         */
        if (!sistema_encendido) {
            LED1 = 0;
            LED2 = 0;
            LED3 = 0;
            LED4 = 0;
            BUZZER = 0;

            __delay_ms(100);
            continue;
        }

        /**
         * @brief Gestión del botón de perfil.
         *
         * El botón conectado a RB3 permite alternar entre perfil normal
         * y perfil deportista. Se usa antirrebote por software para evitar
         * cambios múltiples por una sola pulsación.
         */
        {
            unsigned char btn_ahora = BTN_PERFIL;

            if (btn_perf_ant == 1 && btn_ahora == 0) {
                __delay_ms(30);

                if (BTN_PERFIL == 0) {
                    perfil ^= 1;
                    mostrar_perfil(perfil);

                    if (perfil == 0)
                        UART_WriteString("PERFIL: NORMAL\r\n");
                    else
                        UART_WriteString("PERFIL: DEPORTISTA\r\n");

                    while (BTN_PERFIL == 0);
                    __delay_ms(100);
                }
            }

            btn_perf_ant = btn_ahora;
        }

        /**
         * @brief Lectura de muestras del sensor MAX30102.
         *
         * Se toman 20 muestras del sensor. Por cada muestra válida se procesan
         * los valores rojo e infrarrojo para calcular el BPM y determinar si
         * existe un dedo colocado sobre el sensor.
         */
        for (i = 0; i < 20; i++) {
            if (MAX30102_ReadSample(&red, &ir)) {
                bpm = CAR_ProcesarMuestra(red, ir);
                hay_dedo = CAR_HayDedo(ir);
            }

            __delay_ms(15);
        }

        /**
         * @brief Lectura periódica de temperatura.
         *
         * La temperatura no se lee en cada ciclo para evitar saturar la lectura
         * del DS18B20. Se utiliza un contador para leerla cada cierto número
         * de ciclos.
         */
        ciclo_temp++;

        if (ciclo_temp >= 25) {
            ciclo_temp = 0;

            temp_f = DS18B20_ReadTemp();

            if (temp_f > -55.0f && temp_f < 125.0f) {
                temp_valida = 1;
                temp_int = (unsigned char)temp_f;
                temp_dec = (unsigned char)((temp_f - (float)temp_int) * 10.0f);
                hay_temp = (temp_f >= TEMP_UMBRAL_CUERPO) ? 1 : 0;
            }

            DS18B20_StartConversion();
        }

        /**
         * @brief Determina si el sistema se encuentra activo.
         *
         * El sistema se considera activo cuando hay dedo detectado en el
         * MAX30102 o cuando existe una lectura de temperatura asociada al
         * cuerpo del usuario.
         */
        activo = (hay_dedo || hay_temp) ? 1 : 0;

        /**
         * @brief Evaluación de alarmas del sistema.
         *
         * Se evalúan los límites de BPM y temperatura según el perfil activo.
         * La temperatura puede activar la alarma aunque el BPM sea cero.
         */
        alarma = 0;

        if (activo) {
            if (perfil == 0) {
                /*
                 * Perfil normal:
                 * Se genera alarma si el BPM está por debajo o por encima
                 * de los límites definidos para una persona en condición normal.
                 */
                if (bpm > 0 && (bpm < BPM_MIN_NORMAL || bpm > BPM_MAX_NORMAL)) {
                    alarma = 1;
                }

                /*
                 * Perfil normal:
                 * Se genera alarma si la temperatura supera el límite máximo.
                 */
                if (temp_valida && temp_f > TEMP_MAX_NORMAL) {
                    alarma = 1;
                }
            } else {
                /*
                 * Perfil deportista:
                 * Se genera alarma si el BPM está fuera del rango permitido
                 * para una persona deportista.
                 */
                if (bpm > 0 && (bpm < BPM_MIN_DEPORTISTA || bpm > BPM_MAX_DEPORTISTA)) {
                    alarma = 1;
                }

                /*
                 * Perfil deportista:
                 * Se genera alarma si la temperatura supera el límite máximo.
                 */
                if (temp_valida && temp_f > TEMP_MAX_DEPORTISTA) {
                    alarma = 1;
                }
            }
        }

        /**
         * @brief Actualización de indicadores LED.
         *
         * LED1 indica que el sistema está encendido.
         * LED2 indica que el sistema está activo.
         * LED3 se mantiene apagado durante la ejecución normal.
         * LED4 indica que el sistema está en pausa.
         *
         * @note LED4 ya no indica alarma. Ahora se enciende cuando el sistema
         * está en pausa, es decir, cuando no hay dedo detectado y tampoco hay
         * actividad válida asociada a la temperatura.
         */
        LED1 = 1;
        LED2 = activo ? 1 : 0;
        LED3 = 0;
        LED4 = activo ? 0 : 1;

        /**
         * @brief Control del buzzer.
         *
         * Si existe una alarma, el buzzer se activa de forma intermitente.
         * Si no hay alarma, el buzzer se mantiene apagado.
         */
        if (alarma) {
            ciclo_buzzer++;

            if (ciclo_buzzer >= BUZZER_TOGGLE_CICLOS) {
                ciclo_buzzer = 0;
                buzzer_estado ^= 1;
                BUZZER = buzzer_estado;
            }
        } else {
            BUZZER = 0;
            buzzer_estado = 0;
            ciclo_buzzer = 0;
        }

        /**
         * @brief Actualización periódica de la pantalla OLED.
         *
         * La OLED no se actualiza en todos los ciclos para evitar parpadeos
         * excesivos. Si el sistema está en pausa, se muestra la pantalla de
         * pausa. Si está activo, se muestran los signos vitales.
         */
        ciclo_oled++;

        if (ciclo_oled >= 2) {
            ciclo_oled = 0;

            if (!activo)
                OLED_Paused();
            else
                OLED_Vitals(temp_int, temp_dec, bpm, alarma, perfil);
        }

        /**
         * @brief Envío periódico de información por UART.
         *
         * Si el sistema está activo, se envían los signos vitales. Si el
         * sistema está en pausa, se envía un mensaje indicando dicho estado.
         */
        ciclo_uart++;

        if (ciclo_uart >= UART_CADA_CICLOS) {
            ciclo_uart = 0;

            if (activo)
                UART_SendVitals(temp_int, temp_dec, bpm, perfil, alarma);
            else
                UART_WriteString("ESTADO: EN PAUSA\r\n");
        }
    }
}