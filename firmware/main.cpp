/**
 * @file main.cpp
 * @brief Fase 1: Diagnóstico de Hardware, Escáner I2C y Gestión de Energía (AXP2101)
 *
 * Este es nuestro primer firmware propio corriendo en el reloj ESP32-S3-Touch-AMOLED-2.06.
 * Qué hace al arrancar:
 *   1. Abre el canal USB (COM5) y reporta las capacidades reales del cerebro (CPU, Flash y PSRAM).
 *   2. Inicia el bus de comunicación interna I2C (pines SDA=15, SCL=14).
 *   3. Inicializa el chip de energía AXP2101 (hal_power) y enciende la corriente de los periféricos.
 *   4. Escanea el bus I2C pasando lista a los 6 chips soldados en la placa.
 *   5. En bucle continuo:
 *      - Detecta al instante cuando presionas los botones físicos laterales (PWR y BOOT).
 *      - Muestra cada 3 segundos el estado en vivo de la batería (voltaje mV, % y estado de carga).
 */

#include <Arduino.h>
#include <Wire.h>
#include "config/pins_config.h"
#include "hal/hal_power.h"

// Variables para controlar el tiempo sin bloquear el procesador
static uint32_t lastTelemetryMs = 0;
static bool lastBootButtonState = HIGH; // El botón BOOT está en HIGH cuando no se presiona, y pasa a LOW al pulsarlo

/**
 * @brief Devuelve el nombre y función de cada chip según su dirección I2C en esta placa.
 */
const char* identifyI2CChip(uint8_t address) {
    switch (address) {
        case I2C_ADDR_ES8311:   return "ES8311   -> Chip de Audio DAC (Altavoz)";
        case I2C_ADDR_AXP2101:  return "AXP2101  -> PMIC Gestor de Energia y Bateria";
        case I2C_ADDR_FT3168:   return "FT3168   -> Controlador de Pantalla Tactil";
        case I2C_ADDR_ES7210:   return "ES7210   -> Chip de Audio ADC (2 Microfonos)";
        case I2C_ADDR_PCF85063: return "PCF85063 -> Reloj de Tiempo Real (RTC)";
        case I2C_ADDR_QMI8658:  return "QMI8658  -> Sensor de Movimiento IMU 6 Ejes";
        case 0x6A:              return "QMI8658 (Alt) -> Sensor de Movimiento IMU 6 Ejes";
        default:                return "Dispositivo I2C desconocido";
    }
}

/**
 * @brief Recorre todas las direcciones posibles del bus I2C (0x01 a 0x7E)
 *        y muestra en pantalla qué chips contestaron.
 */
void scanI2CBus() {
    Serial.println("\n============================================================");
    Serial.println(" ESCANER DEL BUS I2C (Pines SDA=GPIO 15, SCL=GPIO 14)");
    Serial.println("============================================================");

    int foundCount = 0;
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        Wire.beginTransmission(addr);
        uint8_t error = Wire.endTransmission();

        if (error == 0) {
            foundCount++;
            Serial.printf("  [OK] Direccion 0x%02X : %s\n", addr, identifyI2CChip(addr));
        }
    }

    Serial.println("------------------------------------------------------------");
    Serial.printf(" Total de chips encontrados en el bus I2C: %d\n", foundCount);
    Serial.println("============================================================\n");
}

/**
 * @brief Imprime en la consola del PC el estado actual de la batería y voltajes.
 */
void printPowerTelemetry() {
    WatchPowerStatus st = hal_power_get_status();

    Serial.printf(
        "[ENERGIA] Bateria: %4u mV (%3d%%) | Cargando: %-2s | USB (VBUS): %4u mV | Sistema: %4u mV\n",
        st.batteryVoltageMv,
        st.batteryPercent,
        st.isCharging ? "SI" : "NO",
        st.vbusVoltageMv,
        st.sysVoltageMv
    );
}

/**
 * @brief Muestra la radiografía completa del procesador, memorias y escáner I2C.
 */
void printSystemReport() {
    Serial.println("\n\n############################################################");
    Serial.println("#  FIRMWARE PROPIO - RELOJ INTELIGENTE ESP32-S3 (FASE 1)   #");
    Serial.println("############################################################");

    Serial.printf("  * Procesador (CPU) : %s @ %u MHz (%u nucleos)\n",
                  ESP.getChipModel(), ESP.getCpuFreqMHz(), ESP.getChipCores());
    Serial.printf("  * Memoria Flash    : %u MB\n", ESP.getFlashChipSize() / (1024 * 1024));
    Serial.printf("  * Memoria PSRAM    : %u KB (~%u MB Octal PSRAM activa)\n",
                  ESP.getPsramSize() / 1024, ESP.getPsramSize() / (1024 * 1024));

    scanI2CBus();
    printPowerTelemetry();
    Serial.println("\n>>> LISTO: Presiona los botones fisicos PWR o BOOT del reloj (o envia 'i' por consola) <<<\n");
}

void setup() {
    // 1. Configurar el botón físico BOOT (GPIO 0) como entrada con resistencia interna (pull-up)
    //    NOTA: Jamás tocamos GPIO 35 para proteger la memoria Octal PSRAM.
    pinMode(WATCH_BTN_BOOT_PIN, INPUT_PULLUP);

    // 2. Iniciar comunicación por cable USB (puerto COM5)
    Serial.begin(115200);

    // Esperar hasta 3 segundos a que el PC abra el monitor serie para no perder el encabezado
    uint32_t waitStart = millis();
    while (!Serial && (millis() - waitStart < 3000)) {
        delay(10);
    }
    delay(300);

    // 3. Iniciar el canal de comunicación compartido I2C (SDA=15, SCL=14 a 400 kHz)
    Wire.begin(WATCH_I2C_SDA_PIN, WATCH_I2C_SCL_PIN, WATCH_I2C_FREQ_HZ);

    // 4. Inicializar primero el chip de energía AXP2101 (0x34) para encender la corriente de los demás chips
    Serial.println("\n[1/2] Inicializando chip de energia AXP2101 (0x34)...");
    if (hal_power_init(Wire)) {
        Serial.println("  -> [EXITO] AXP2101 inicializado y rieles de 3.3V encendidos.");
    } else {
        Serial.println("  -> [ERROR] No se pudo comunicar con el chip AXP2101 en 0x34.");
    }

    // Despertar el chip táctil FT3168 liberando su pin de Reset (GPIO 9 = HIGH)
    pinMode(WATCH_TP_RST_PIN, OUTPUT);
    digitalWrite(WATCH_TP_RST_PIN, LOW);
    delay(50);
    digitalWrite(WATCH_TP_RST_PIN, HIGH);
    delay(300); // El FT3168 necesita ~250-300 ms tras el reset para responder por I2C

    // 5. Mostrar el reporte completo de sistema, escáner I2C y batería
    printSystemReport();
    lastTelemetryMs = millis();
}

void loop() {
    // A. Revisar el botón físico BOOT (conectado a la patita GPIO 0)
    bool currentBootState = digitalRead(WATCH_BTN_BOOT_PIN);
    if (currentBootState != lastBootButtonState) {
        delay(20); // Pequeño filtro anti-rebote mecánico (debounce)
        currentBootState = digitalRead(WATCH_BTN_BOOT_PIN);
        if (currentBootState != lastBootButtonState) {
            lastBootButtonState = currentBootState;
            if (currentBootState == LOW) {
                Serial.println(">>> [BOTON] Presionaste el boton lateral BOOT (GPIO 0) <<<");
            } else {
                Serial.println(">>> [BOTON] Soltaste el boton lateral BOOT (GPIO 0) <<<");
            }
        }
    }

    // B. Revisar el botón físico PWR preguntándole al chip AXP2101 por I2C (polling seguro)
    PwrButtonEvent pwrEvent = hal_power_poll_button();
    if (pwrEvent == PWR_BTN_SHORT_PRESS) {
        Serial.println(">>> [BOTON] Pulsacion CORTA detectada en el boton lateral PWR (AXP2101) <<<");
        printPowerTelemetry();
    } else if (pwrEvent == PWR_BTN_LONG_PRESS) {
        Serial.println(">>> [BOTON] Pulsacion LARGA detectada en el boton lateral PWR (AXP2101) <<<");
    }

    // C. Si el usuario envía 'i' desde el PC, mostrar el reporte completo e I2C bajo demanda
    if (Serial.available() > 0) {
        char cmd = (char)Serial.read();
        if (cmd == 'i' || cmd == 'I') {
            printSystemReport();
        }
    }

    // D. Imprimir telemetría de batería automáticamente cada 3 segundos (3000 ms)
    if (millis() - lastTelemetryMs >= 3000) {
        lastTelemetryMs = millis();
        printPowerTelemetry();
    }

    // Descanso de 20 milisegundos por ciclo (50 revisiones por segundo)
    delay(20);
}

