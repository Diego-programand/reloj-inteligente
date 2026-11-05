/**
 * @file main.cpp
 * @brief Fase 2 (Parte A): Prueba de Pantalla AMOLED QSPI (410x502) y Panel Táctil (FT3168)
 *
 * Qué hace este firmware al arrancar:
 *   1. Inicializa la energía (AXP2101) en 0x34.
 *   2. Despierta e inicializa el panel táctil (FT3168) en 0x38.
 *   3. Reinicia e inicializa la pantalla AMOLED (CO5300/SH8601) por bus QSPI de 4 carriles.
 *   4. Dibuja en pantalla una interfaz de prueba con bandas de color puro (Rojo, Verde, Azul, Cian)
 *      y un lienzo interactivo:
 *        - Al tocar o arrastrar el dedo sobre el cristal, dibuja un trazo luminoso bajo tu dedo
 *          y envía las coordenadas exactas (X, Y) al PC por COM5.
 *        - Al presionar el botón lateral BOOT, limpia el lienzo y cambia el color de pintura.
 *        - Al presionar el botón lateral PWR, cambia el nivel de brillo (100% -> 35% -> Apagada -> 100%).
 */

#include <Arduino.h>
#include <Wire.h>
#include "config/pins_config.h"
#include "hal/hal_power.h"
#include "hal/hal_display.h"
#include "hal/hal_touch.h"

static uint32_t lastTelemetryMs = 0;
static uint32_t lastTouchPrintMs = 0;
static bool lastBootButtonState = HIGH;

// Paleta de colores para probar al presionar el botón BOOT
static const uint16_t BRUSH_COLORS[] = {
    COLOR_CYAN,
    COLOR_GREEN,
    COLOR_YELLOW,
    COLOR_ORANGE,
    COLOR_RED,
    COLOR_WHITE
};
static const char* BRUSH_NAMES[] = {
    "CIAN",
    "VERDE",
    "AMARILLO",
    "NARANJA",
    "ROJO",
    "BLANCO"
};
static int currentBrushIndex = 0;

// Niveles de brillo para probar al presionar el botón PWR
static const uint8_t BRIGHTNESS_LEVELS[] = {255, 90, 20, 0};
static int currentBrightnessIndex = 0;

/**
 * @brief Dibuja la pantalla base de prueba con franjas de referencia RGB y marco activo.
 */
void drawTestCanvas() {
    // 1. Fondo negro puro (en AMOLED los píxeles negros están físicamente apagados)
    hal_display_fill_screen(COLOR_BLACK);

    // 2. Franjas superiores de prueba de color (Rojo, Verde, Azul, Blanco)
    uint16_t barW = WATCH_LCD_WIDTH / 4;
    hal_display_fill_rect(0 * barW, 0, barW, 18, COLOR_RED);
    hal_display_fill_rect(1 * barW, 0, barW, 18, COLOR_GREEN);
    hal_display_fill_rect(2 * barW, 0, barW, 18, COLOR_BLUE);
    hal_display_fill_rect(3 * barW, 0, WATCH_LCD_WIDTH - (3 * barW), 18, COLOR_WHITE);

    // 3. Barra indicadora del color de pincel actual en la parte superior e inferior
    uint16_t brushColor = BRUSH_COLORS[currentBrushIndex];
    hal_display_fill_rect(0, 20, WATCH_LCD_WIDTH, 6, brushColor);
    hal_display_fill_rect(0, WATCH_LCD_HEIGHT - 8, WATCH_LCD_WIDTH, 8, brushColor);

    // 4. Bordes laterales guía para verificar que las 410 columnas x 502 filas están bien centradas
    hal_display_fill_rect(0, 26, 3, WATCH_LCD_HEIGHT - 34, COLOR_DARKGRAY);
    hal_display_fill_rect(WATCH_LCD_WIDTH - 3, 26, 3, WATCH_LCD_HEIGHT - 34, COLOR_DARKGRAY);

    // 5. Mira central en (205, 251)
    hal_display_fill_rect(WATCH_LCD_WIDTH / 2 - 12, WATCH_LCD_HEIGHT / 2 - 1, 25, 3, brushColor);
    hal_display_fill_rect(WATCH_LCD_WIDTH / 2 - 1, WATCH_LCD_HEIGHT / 2 - 12, 3, 25, brushColor);
}

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

void printPowerTelemetry() {
    WatchPowerStatus st = hal_power_get_status();
    Serial.printf(
        "[ENERGIA] Bateria: %4u mV (%3d%%) | Cargando: %-2s | Brillo LCD: %3u/255\n",
        st.batteryVoltageMv,
        st.batteryPercent,
        st.isCharging ? "SI" : "NO",
        hal_display_get_brightness()
    );
}

void printSystemReport() {
    Serial.println("\n\n############################################################");
    Serial.println("#  FIRMWARE PROPIO - RELOJ INTELIGENTE ESP32-S3 (FASE 2)   #");
    Serial.println("############################################################");
    Serial.printf("  * Procesador (CPU) : %s @ %u MHz (%u nucleos)\n",
                  ESP.getChipModel(), ESP.getCpuFreqMHz(), ESP.getChipCores());
    Serial.printf("  * Memoria Flash    : %u MB\n", ESP.getFlashChipSize() / (1024 * 1024));
    Serial.printf("  * Memoria PSRAM    : %u KB (~%u MB Octal PSRAM activa)\n",
                  ESP.getPsramSize() / 1024, ESP.getPsramSize() / (1024 * 1024));
    Serial.printf("  * Pantalla AMOLED  : %u x %u px (QSPI 4 carriles, Offset X=22)\n",
                  WATCH_LCD_WIDTH, WATCH_LCD_HEIGHT);

    scanI2CBus();
    printPowerTelemetry();
    Serial.println("\n>>> CONTROLES ACTIVOS EN TU RELOJ: <<<");
    Serial.println("  1. Toca o desliza el dedo por la pantalla -> Dibuja en vivo y muestra (X, Y) aqui.");
    Serial.println("  2. Boton lateral BOOT -> Limpia la pantalla y cambia el color del trazo.");
    Serial.println("  3. Boton lateral PWR  -> Cambia el nivel de brillo (100% -> 35% -> 8% -> Apagada).\n");
}

void setup() {
    pinMode(WATCH_BTN_BOOT_PIN, INPUT_PULLUP);
    Serial.begin(115200);

    uint32_t waitStart = millis();
    while (!Serial && (millis() - waitStart < 2500)) {
        delay(10);
    }
    delay(200);

    // 1. Iniciar bus I2C compartido
    Wire.begin(WATCH_I2C_SDA_PIN, WATCH_I2C_SCL_PIN, WATCH_I2C_FREQ_HZ);

    // 2. Encender rieles de alimentación del AXP2101 (imprescindible antes de pantalla y táctil)
    Serial.println("\n[1/3] Inicializando gestor de energia AXP2101 (0x34)...");
    hal_power_init(Wire);

    // 3. Inicializar panel táctil FT3168 (0x38)
    Serial.println("[2/3] Inicializando panel tactil FT3168 (0x38)...");
    if (hal_touch_init(Wire)) {
        Serial.println("  -> [EXITO] Panel tactil FT3168 listo.");
    } else {
        Serial.println("  -> [ERROR] Panel tactil FT3168 no respondio.");
    }

    // 4. Inicializar pantalla AMOLED QSPI (410x502) y dibujar lienzo de prueba
    Serial.println("[3/3] Inicializando pantalla AMOLED QSPI (410x502)...");
    if (hal_display_init()) {
        Serial.println("  -> [EXITO] Pantalla AMOLED QSPI encendida.");
        drawTestCanvas();
    } else {
        Serial.println("  -> [ERROR] Fallo al inicializar bus QSPI de pantalla.");
    }

    printSystemReport();
    lastTelemetryMs = millis();
}

void loop() {
    // A. Leer el panel táctil en tiempo real
    TouchPoint tp = hal_touch_read();
    if (tp.touched) {
        // Dibujar un cuadrado de 12x12 píxeles centrado bajo el dedo
        uint16_t drawX = (tp.x > 6) ? (tp.x - 6) : 0;
        uint16_t drawY = (tp.y > 6) ? (tp.y - 6) : 0;
        hal_display_fill_rect(drawX, drawY, 12, 12, BRUSH_COLORS[currentBrushIndex]);

        // Imprimir coordenada (X, Y) en el puerto serie hasta 10 veces por segundo para no saturar
        if (millis() - lastTouchPrintMs >= 100) {
            lastTouchPrintMs = millis();
            Serial.printf(">>> [TOUCH] Dedo detectado en X = %3u, Y = %3u (Color: %s)\n",
                          tp.x, tp.y, BRUSH_NAMES[currentBrushIndex]);
        }
    }

    // B. Botón físico BOOT: limpiar lienzo y cambiar color de pincel
    bool currentBootState = digitalRead(WATCH_BTN_BOOT_PIN);
    if (currentBootState != lastBootButtonState) {
        delay(20);
        currentBootState = digitalRead(WATCH_BTN_BOOT_PIN);
        if (currentBootState != lastBootButtonState) {
            lastBootButtonState = currentBootState;
            if (currentBootState == LOW) {
                currentBrushIndex = (currentBrushIndex + 1) % 6;
                drawTestCanvas();
                Serial.printf(">>> [BOTON BOOT] Lienzo limpio | Nuevo color: %s <<<\n",
                              BRUSH_NAMES[currentBrushIndex]);
            }
        }
    }

    // C. Botón físico PWR: cambiar nivel de brillo de la pantalla AMOLED
    PwrButtonEvent pwrEvent = hal_power_poll_button();
    if (pwrEvent == PWR_BTN_SHORT_PRESS) {
        currentBrightnessIndex = (currentBrightnessIndex + 1) % 4;
        uint8_t newBright = BRIGHTNESS_LEVELS[currentBrightnessIndex];
        hal_display_set_brightness(newBright);
        Serial.printf(">>> [BOTON PWR] Brillo de pantalla ajustado a: %u / 255 (%d%%) <<<\n",
                      newBright, (newBright * 100) / 255);
    }

    // D. Comando 'i' desde consola
    if (Serial.available() > 0) {
        char cmd = (char)Serial.read();
        if (cmd == 'i' || cmd == 'I') {
            printSystemReport();
        }
    }

    // E. Telemetría periódica cada 5 segundos
    if (millis() - lastTelemetryMs >= 5000) {
        lastTelemetryMs = millis();
        printPowerTelemetry();
    }

    delay(10);
}
