/**
 * @file    main.c
 * @brief   Monitor Portatil de Signos Vitales ? archivo principal.
 *
 * Practica 3 ? Microcontroladores Lenguaje C
 * Ingenieria Electronica y Telecomunicaciones
 * Universidad del Cauca
 *
 * -------------------------------------------------------------------------
 * DESCRIPCION DEL SISTEMA
 * -------------------------------------------------------------------------
 * Sistema embebido basado en PIC18F4550 que mide frecuencia cardiaca
 * (MAX30102) y temperatura corporal (BME280), muestra los valores en
 * pantalla OLED SSD1306 via I2C, los transmite periodicamente via UART
 * y genera alarmas visuales (LEDs) y sonoras (buzzer) cuando los valores
 * superan umbrales clinicos predefinidos.
 *
 * -------------------------------------------------------------------------
 * MAQUINA DE ESTADOS PRINCIPAL
 * -------------------------------------------------------------------------
 *
 *   [INIT] --> [PREPARANDO] --> [EN_ESPERA] <--> [ALARMA]
 *                                    ^
 *                                    | (boton 3s)
 *                                    v
 *                               [PAUSA] <-- (boton 3s otra vez)
 *
 *   INIT       : Configuracion del oscilador y perifericos.
 *   PREPARANDO : Inicializacion de sensores I2C, duracion ~500 ms.
 *   EN_ESPERA  : Ciclo normal: leer -> evaluar -> mostrar -> transmitir.
 *   ALARMA     : Igual que EN_ESPERA pero con buzzer activo y LED rojo.
 *   PAUSA      : Sensores detenidos. OLED y UART en espera. Boton reinicia.
 *
 * -------------------------------------------------------------------------
 * CICLO PRINCIPAL (aprox. 1 segundo por iteracion)
 * -------------------------------------------------------------------------
 *   1. Limpiar Watchdog Timer (CLRWDT)
 *   2. Leer BME280 (temperatura)
 *   3. Leer MAX30102 (BPM)
 *   4. Evaluar umbrales ? activar/desactivar buzzer
 *   5. Actualizar estado del sistema
 *   6. Actualizar LEDs
 *   7. Actualizar pantalla OLED
 *   8. Transmitir trama UART
 *   9. Verificar boton de pausa
 *  10. Esperar para completar el ciclo de ~1 segundo
 *
 * -------------------------------------------------------------------------
 * LAYOUT DE LA PANTALLA OLED (128x64, fuente 5x7)
 * -------------------------------------------------------------------------
 *   Pagina 0 : "MONITOR VITAL"    (titulo fijo)
 *   Pagina 2 : "BPM: XXX LPM"    (frecuencia cardiaca)
 *   Pagina 4 : "TEMP: XX.X C"    (temperatura)
 *   Pagina 6 : "NORMAL" / "ALARMA" / "EN PAUSA"  (estado)
 *
 * -------------------------------------------------------------------------
 * NOTAS DE IMPLEMENTACION
 * -------------------------------------------------------------------------
 *  - Las interrupciones globales se habilitan con ei() tras inicializar
 *    todos los modulos. Esto evita que el ISR de UART se dispare durante
 *    la inicializacion del bus I2C.
 *  - CLRWDT() se llama al inicio de cada ciclo. Con WDT periodo ~1 s
 *    y ciclo de ~1 s, hay margen suficiente. Si el sistema se cuelga,
 *    el WDT genera un reset automatico.
 *  - El primer valor de BPM estara disponible tras ~2 segundos de datos.
 *    Durante ese tiempo la pantalla muestra "BPM: ---".
 *  - La deteccion de pulsacion larga del boton (3 segundos) se hace por
 *    polling: se cuentan ciclos consecutivos con el boton presionado.
 */

#include "config.h"
#include "i2c.h"
#include "oled.h"
#include "bme280.h"
#include "max30102.h"
#include "uart.h"
#include "leds.h"
#include "alarmas.h"

/* =========================================================
 *  VARIABLES GLOBALES DEL SISTEMA
 * ========================================================= */

/** Estado actual de la maquina de estados principal */
static EstadoSistema estado_sistema = ESTADO_INIT;

/** Ultima medicion de frecuencia cardiaca en BPM (0 = sin datos aun) */
static unsigned int bpm_actual = 0U;

/** Ultima medicion de temperatura en decimas de grado (0 = sin datos) */
static unsigned int temp_actual = 0U;

/** Contador de ciclos con boton presionado para deteccion de pulsacion larga */
static unsigned char boton_count = 0U;

/** Bandera de modo pausa (1 = pausado, 0 = activo) */
static unsigned char en_pausa = 0U;

/* =========================================================
 *  PROTOTIPOS DE FUNCIONES INTERNAS
 * ========================================================= */
static void Sistema_ConfigOscilador(void);
static void Sistema_ConfigPines(void);
static void Sistema_InitModulos(void);
static void OLED_MostrarVitales(void);
static void OLED_MostrarEstado(void);
static void OLED_MostrarPausa(void);
static void Boton_Verificar(void);

/* =========================================================
 *  MAIN
 * ========================================================= */
int main(void) {

    /* --- Fase 1: Configuracion del hardware base --- */
    Sistema_ConfigOscilador();
    Sistema_ConfigPines();

    /* --- Fase 2: Inicializacion de modulos --- */
    Sistema_InitModulos();

    /* --- Fase 3: Habilitar interrupciones globales ---
     * Se hace DESPUES de inicializar todos los modulos para evitar que
     * el ISR de UART se dispare mientras el bus I2C aun se esta configurando. */
    INTCONbits.GIE  = 1;   /* Interrupciones globales ON  */
    INTCONbits.PEIE = 1;   /* Interrupciones de perifericos ON */

    /* --- Transicion a estado operativo --- */
    estado_sistema = ESTADO_EN_ESPERA;
    LEDS_SetEstado(estado_sistema);

    /* Mensaje de bienvenida en UART */
    UART_SendString("SISTEMA INICIADO\r\n");

    /* =====================================================
     *  BUCLE PRINCIPAL
     *  Duracion aproximada: 1 segundo por iteracion.
     *  El buzzer agrega ~200 ms en ciclos con alarma activa.
     * ===================================================== */
    while (1) {

        /* --- Paso 1: Limpiar Watchdog Timer ---
         * Debe llamarse al menos una vez por periodo del WDT (~1 s).
         * Si el firmware se cuelga y no llega a este punto, el WDT
         * genera un reset automatico del PIC. */
        CLRWDT();

        /* --- Paso 2: Ciclo activo (solo si no esta pausado) --- */
        if (!en_pausa) {

            /* Leer temperatura del BME280 con compensacion de fabrica.
             * Resultado en decimas de grado: 365 = 36.5 C */
            temp_actual = BME280_ReadTemp();

            /* Leer y calcular BPM desde el buffer del MAX30102.
             * Retorna 0 si aun no hay 2 segundos de datos acumulados. */
            bpm_actual = MAX30102_GetBPM();

            /* Evaluar umbrales clinicos y controlar buzzer.
             * Retorna 1 si hay alarma activa, 0 si todo esta normal. */
            if (ALARMAS_Evaluar(bpm_actual, temp_actual)) {
                estado_sistema = ESTADO_ALARMA;
            } else {
                estado_sistema = ESTADO_EN_ESPERA;
            }

            /* Actualizar pantalla OLED con mediciones y estado */
            OLED_MostrarVitales();
            OLED_MostrarEstado();

            /* Transmitir trama de signos vitales via UART.
             * La transmision es asincrona (interrupcion) ? no bloquea. */
            UART_SendVitals(bpm_actual, temp_actual);

        } else {
            /* En pausa: solo actualizar la pantalla con mensaje de pausa */
            OLED_MostrarPausa();
            ALARMAS_Silenciar();
            estado_sistema = ESTADO_PAUSA;
        }

        /* --- Paso 3: Actualizar LEDs segun estado actual ---
         * Se hace SIEMPRE, incluso en pausa, para mantener el LED de
         * encendido activo y el parpadeo del estado de pausa. */
        LEDS_SetEstado(estado_sistema);

        /* --- Paso 4: Verificar boton de pausa ---
         * Polling de pulsacion larga: 3 ciclos consecutivos presionado
         * equivalen a ~3 segundos con el boton sostenido. */
        Boton_Verificar();

        /* --- Paso 5: Esperar para completar el ciclo de ~1 segundo ---
         * El tiempo real del ciclo depende de las operaciones anteriores:
         *   Sin alarma: ~15 ms operaciones + 985 ms delay = ~1000 ms
         *   Con alarma: ~215 ms operaciones + 785 ms delay = ~1000 ms */
        if (estado_sistema == ESTADO_ALARMA) {
            /* El beep del buzzer ya tomo ~200 ms dentro de ALARMAS_Evaluar */
            __delay_ms(785);
        } else {
            __delay_ms(985);
        }
    }

    return 0; /* Nunca se alcanza */
}

/* =========================================================
 *  Sistema_ConfigOscilador
 *
 *  Configura el oscilador interno del PIC18F4550 a 8 MHz.
 *  IRCF[6:4] = 111 -> 8 MHz
 *  SCS[1:0]  = 10  -> fuente: oscilador interno
 * ========================================================= */
static void Sistema_ConfigOscilador(void) {
    OSCCON = 0x72;

    /* Esperar a que el oscilador interno se estabilice.
     * IOFS = 1 indica que la frecuencia es estable y confiable. */
    while (!OSCCONbits.IOFS);
}

/* =========================================================
 *  Sistema_ConfigPines
 *
 *  Configura los pines que no son inicializados por los modulos
 *  individuales. Garantiza que no queden pines en estado flotante.
 * ========================================================= */
static void Sistema_ConfigPines(void) {
    /* Deshabilitar comparadores analogicos para usar PORTB como digital.
     * CMCON = 0x07 -> todos los comparadores OFF. */
    CMCON = 0x07;

    /* Configurar todos los pines analogicos como digital.
     * ADCON1 = 0x0F -> PCFG = 1111 -> todos digitales. */
    ADCON1 = 0x0F;

    /* PORTA: no usado activamente ? configurar como salida baja
     * para evitar pines flotantes que consuman corriente. */
    TRISA = 0x00;
    LATA  = 0x00;

    /* PORTB: RB0 y RB1 se configuran en I2C_Init() como entradas.
     *        RB2 (boton): entrada sin pull-up (pull-up externo en hardware).
     *        RB3-RB7: salidas bajas para evitar flotantes. */
    TRISB = 0x04;   /* Solo RB2 como entrada (0b00000100) */
    LATB  = 0x00;

    /* PORTC: RC6 (TX) y RC7 (RX) se configuran en UART_Init().
     *        Resto como salidas bajas. */
    TRISC = 0x00;
    LATC  = 0x00;

    /* PORTD: configurado completo como salida por LEDS_Init(). */

    /* PORTE: no usado ? salidas bajas. */
    TRISE = 0x00;
    LATE  = 0x00;
}

/* =========================================================
 *  Sistema_InitModulos
 *
 *  Inicializa todos los modulos en el orden correcto.
 *  El orden importa: I2C debe estar listo antes que los sensores,
 *  y los LEDs antes que las alarmas (comparten PORTD).
 * ========================================================= */
static void Sistema_InitModulos(void) {
    unsigned char init_ok = 1U;

    /* Indicar estado PREPARANDO mientras se inicializa */
    LEDS_Init();
    LEDS_SetEstado(ESTADO_PREPARANDO);

    /* 1. Bus I2C por hardware (MSSP) */
    I2C_Init();

    /* 2. Pantalla OLED ? primera en inicializarse para poder mostrar
     *    mensajes de estado durante la inicializacion de sensores. */
    OLED_Init();
    OLED_Clear();
    OLED_SetCursor(0, 0);
    OLED_WriteString("INICIANDO");

    /* 3. Sensor de temperatura BME280 */
    OLED_SetCursor(0, 2);
    if (BME280_Init() == BME280_OK) {
        OLED_WriteString("BME280 OK");
    } else {
        OLED_WriteString("BME280 ERR");
        init_ok = 0U;
    }
    __delay_ms(300);

    /* 4. Sensor de frecuencia cardiaca MAX30102 */
    OLED_SetCursor(0, 4);
    if (MAX30102_Init() == MAX30102_OK) {
        OLED_WriteString("MAX30102 OK");
    } else {
        OLED_WriteString("MAX30102 ERR");
        init_ok = 0U;
    }
    __delay_ms(300);

    /* 5. UART */
    UART_Init();

    /* 6. Buzzer */
    ALARMAS_Init();

    /* Mostrar resultado de la inicializacion */
    OLED_SetCursor(0, 6);
    if (init_ok) {
        OLED_WriteString("LISTO");
    } else {
        OLED_WriteString("ERROR SENSOR");
    }
    __delay_ms(800);

    /* Limpiar pantalla antes de entrar al bucle principal */
    OLED_Clear();

    /* Titulo fijo en la parte superior ? se escribe una sola vez */
    OLED_SetCursor(4, 0);
    OLED_WriteString("MONITOR VITAL");
}

/* =========================================================
 *  OLED_MostrarVitales
 *
 *  Actualiza las lineas de BPM y temperatura en la OLED.
 *  Se llama en cada ciclo del bucle principal.
 * ========================================================= */
static void OLED_MostrarVitales(void) {
    /* --- Linea de frecuencia cardiaca (pagina 2) --- */
    OLED_ClearLine(2);
    OLED_SetCursor(0, 2);
    OLED_WriteString("BPM");
    OLED_SetCursor(24, 2);

    if (bpm_actual == 0U) {
        /* Sin datos aun: mostrar guiones en lugar de cero */
        OLED_WriteString("---");
    } else {
        OLED_WriteInt(bpm_actual);
    }

    /* --- Linea de temperatura (pagina 4) --- */
    OLED_ClearLine(4);
    OLED_SetCursor(0, 4);
    OLED_WriteString("TEMP");
    OLED_SetCursor(30, 4);

    if (temp_actual == 0U) {
        OLED_WriteString("--.-");
    } else {
        OLED_WriteTemp(temp_actual);
        OLED_WriteString(" C");
    }
}

/* =========================================================
 *  OLED_MostrarEstado
 *
 *  Actualiza la linea de estado del sistema (pagina 6).
 * ========================================================= */
static void OLED_MostrarEstado(void) {
    OLED_ClearLine(6);
    OLED_SetCursor(0, 6);

    if (estado_sistema == ESTADO_ALARMA) {
        OLED_WriteString("ALARMA");
    } else {
        OLED_WriteString("NORMAL");
    }
}

/* =========================================================
 *  OLED_MostrarPausa
 *
 *  Actualiza la pantalla cuando el sistema esta pausado.
 * ========================================================= */
static void OLED_MostrarPausa(void) {
    OLED_ClearLine(2);
    OLED_ClearLine(4);
    OLED_ClearLine(6);

    OLED_SetCursor(10, 3);
    OLED_WriteString("EN PAUSA");
    OLED_SetCursor(4, 5);
    OLED_WriteString("HOLD 3S REANUDAR");
}

/* =========================================================
 *  Boton_Verificar
 *
 *  Detecta pulsacion larga del boton (RB2, activo bajo).
 *  Se llama una vez por ciclo del bucle (~1 segundo).
 *
 *  Si el boton esta presionado durante 3 ciclos consecutivos
 *  (aproximadamente 3 segundos), alterna el estado de pausa.
 *  El contador se resetea si el boton se suelta antes.
 * ========================================================= */
static void Boton_Verificar(void) {
    /* BOTON_PAUSA = RB2. El boton se conecta entre RB2 y GND.
     * Con pull-up externo: RB2 = 1 (suelto), RB2 = 0 (presionado). */
    if (BOTON_PAUSA == 0U) {
        /* Boton presionado: incrementar contador */
        boton_count++;

        if (boton_count >= 3U) {
            /* Pulsacion larga detectada: alternar modo pausa */
            en_pausa = (unsigned char)(!en_pausa);
            boton_count = 0U;

            /* Si se reanuda, limpiar pantalla y restaurar titulo */
            if (!en_pausa) {
                OLED_Clear();
                OLED_SetCursor(4, 0);
                OLED_WriteString("MONITOR VITAL");
                UART_SendString("REANUDADO\r\n");
            } else {
                UART_SendString("EN PAUSA\r\n");
            }
        }
    } else {
        /* Boton suelto: resetear contador */
        boton_count = 0U;
    }
}
