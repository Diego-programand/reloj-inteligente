/**
 * @file main.cpp
 * @brief Fase 2 Completa: Pantalla AMOLED QSPI + Panel Táctil FT3168 + Motor Gráfico LVGL en PSRAM
 *
 * Qué hace nuestro sistema al arrancar:
 *   1. Inicializa la energía (AXP2101) en 0x34.
 *   2. Inicializa el panel táctil (FT3168) en 0x38.
 *   3. Inicializa la pantalla AMOLED (410x502) por bus QSPI de 4 carriles.
 *   4. Inicia el motor gráfico LVGL reservando doble buffer (~410 KB) en los 8 MB de Octal PSRAM.
 *   5. Muestra en la pantalla del reloj un centro de control interactivo con 2 pantallas
 *      deslizables con el dedo (Swipe izquierda/derecha), control táctil de brillo,
 *      nivel real de batería y monitor de eventos.
 */

#include <Arduino.h>
#include <Wire.h>
#include "config/pins_config.h"
#include "hal/hal_power.h"
#include "hal/hal_display.h"
#include "hal/hal_touch.h"
#include "ui/ui_manager.h"

static uint32_t lastTelemetryMs = 0;
static bool lastBootButtonState = HIGH;
static const uint8_t BRIGHTNESS_LEVELS[] = {255, 140, 45, 0};
static int currentBrightnessIndex = 0;

const char* identifyI2CChip(uint8_t address) {
    switch (address) {
        case I2C_ADDR_ES8311:   return "ES8311   -> Chip de Audio DAC (Altavoz)";
        case I2C_ADDR_AXP2101:  return "AXP2101  -> PMIC Gestor de Energia y Bateria";
        case I2C_ADDR_FT3168:   return "FT3168   -> Controlador de Pantalla Tactil";
        case I2C_ADDR_ES7210:   return "ES7210   -> Chip de Audio ADC (2 Microfonos)";
        case I2C_ADDR_PCF85063: return "PCF85063 -> Reloj de Tiempo Real (RTC)";
        case I2C_ADDR_QMI8658:  return "QMI8658  -> Sensor de Movimiento IMU 6 Ejes";
        default:                return "Dispositivo I2C";
    }
}

void scanI2CBus() {
    Serial.println("\n============================================================");
    Serial.println(" ESCANER DEL BUS I2C (Pines SDA=GPIO 15, SCL=GPIO 14)");
    Serial.println("============================================================");

    int foundCount = 0;
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            foundCount++;
            Serial.printf("  [OK] Direccion 0x%02X : %s\n", addr, identifyI2CChip(addr));
        }
    }

    Serial.println("------------------------------------------------------------");
    Serial.printf(" Total de chips encontrados en el bus I2C: %d\n", foundCount);
    Serial.println("============================================================\n");
}

void updateAndPrintPower() {
    WatchPowerStatus st = hal_power_get_status();
    ui_manager_update_power(st);

    Serial.printf(
        "[ENERGIA] Bateria: %4u mV (%3d%%) | Cargando: %-2s | Brillo LCD: %3u/255 | PSRAM libre: %u KB\n",
        st.batteryVoltageMv,
        st.batteryPercent,
        st.isCharging ? "SI" : "NO",
        hal_display_get_brightness(),
        ESP.getFreePsram() / 1024
    );
}

void printSystemReport() {
    Serial.println("\n\n############################################################");
    Serial.println("#  FIRMWARE PROPIO - RELOJ INTELIGENTE ESP32-S3 (FASE 2)   #");
    Serial.println("############################################################");
    Serial.printf("  * Procesador (CPU) : %s @ %u MHz (%u nucleos)\n",
                  ESP.getChipModel(), ESP.getCpuFreqMHz(), ESP.getChipCores());
    Serial.printf("  * Memoria Flash    : %u MB\n", ESP.getFlashChipSize() / (1024 * 1024));
    Serial.printf("  * Memoria PSRAM    : %u KB total (%u KB libres tras buffers LVGL)\n",
                  ESP.getPsramSize() / 1024, ESP.getFreePsram() / 1024);
    Serial.printf("  * Motor Grafico    : LVGL v8.3 activo en 410x502 QSPI\n");

    scanI2CBus();
    updateAndPrintPower();
}

void setup() {
    pinMode(WATCH_BTN_BOOT_PIN, INPUT_PULLUP);
    Serial.begin(115200);

    uint32_t waitStart = millis();
    while (!Serial && (millis() - waitStart < 2000)) {
        delay(10);
    }
    delay(200);

    // 1. Iniciar bus I2C compartido
    Wire.begin(WATCH_I2C_SDA_PIN, WATCH_I2C_SCL_PIN, WATCH_I2C_FREQ_HZ);

    // 2. Inicializar gestor de energía AXP2101
    Serial.println("\n[1/4] Inicializando gestor de energia AXP2101 (0x34)...");
    hal_power_init(Wire);

    // 3. Inicializar panel táctil FT3168
    Serial.println("[2/4] Inicializando panel tactil FT3168 (0x38)...");
    hal_touch_init(Wire);

    // 4. Inicializar pantalla AMOLED QSPI (410x502)
    Serial.println("[3/4] Inicializando pantalla AMOLED QSPI (410x502)...");
    hal_display_init();

    // 5. Inicializar motor gráfico LVGL con doble buffer en Octal PSRAM
    Serial.println("[4/4] Inicializando motor grafico LVGL en Octal PSRAM...");
    if (ui_manager_init()) {
        Serial.println("  -> [EXITO] Interfaz LVGL cargada en PSRAM.");
    } else {
        Serial.println("  -> [ERROR] No se pudo reservar memoria PSRAM para LVGL.");
    }

    printSystemReport();
    lastTelemetryMs = millis();
}

void loop() {
    // A. Ejecutar el motor gráfico LVGL (refresco de pantalla, animaciones y gestos táctiles)
    ui_manager_loop();

    // B. Detectar botón físico BOOT (GPIO 0)
    bool currentBootState = digitalRead(WATCH_BTN_BOOT_PIN);
    if (currentBootState != lastBootButtonState) {
        delay(20);
        currentBootState = digitalRead(WATCH_BTN_BOOT_PIN);
        if (currentBootState != lastBootButtonState) {
            lastBootButtonState = currentBootState;
            if (currentBootState == LOW) {
                Serial.println(">>> [BOTON] Presionado BOOT (GPIO 0) <<<");
                ui_manager_notify_button("BOOT presionado");
            } else {
                ui_manager_notify_button("BOOT soltado");
            }
        }
    }

    // C. Detectar botón físico PWR (vía AXP2101 I2C)
    PwrButtonEvent pwrEvent = hal_power_poll_button();
    if (pwrEvent == PWR_BTN_SHORT_PRESS) {
        currentBrightnessIndex = (currentBrightnessIndex + 1) % 4;
        uint8_t newBright = BRIGHTNESS_LEVELS[currentBrightnessIndex];
        hal_display_set_brightness(newBright);
        ui_manager_sync_brightness(newBright);
        ui_manager_notify_button("PWR (Cambio de brillo)");
        Serial.printf(">>> [BOTON PWR] Brillo ajustado a %u/255 <<<\n", newBright);
    } else if (pwrEvent == PWR_BTN_LONG_PRESS) {
        ui_manager_notify_button("PWR (Pulsacion larga)");
        Serial.println(">>> [BOTON PWR] Pulsacion larga detectada <<<");
    }

    // D. Comando 'i' por consola USB
    if (Serial.available() > 0) {
        char cmd = (char)Serial.read();
        if (cmd == 'i' || cmd == 'I') {
            printSystemReport();
        }
    }

    // E. Actualizar datos de batería en pantalla y consola cada 3 segundos
    if (millis() - lastTelemetryMs >= 3000) {
        lastTelemetryMs = millis();
        updateAndPrintPower();
    }

    delay(5);
}
