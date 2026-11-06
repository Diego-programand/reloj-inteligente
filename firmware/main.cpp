/**
 * @file main.cpp
 * @brief Fase 4: Sensor de Movimiento (QMI8658), Encendido por Gesto de Muñeca
 * y Ahorro de Energía
 *
 * Qué hace nuestro smartwatch al arrancar:
 *   1. Inicializa la gestión de energía (AXP2101) en 0x34.
 *   2. Inicializa el Reloj en Tiempo Real (PCF85063) en 0x51.
 *   3. Inicializa el sensor de movimiento de 6 ejes (QMI8658) en 0x6B.
 *   4. Inicializa el panel táctil (FT3168) en 0x38 y la pantalla AMOLED
 * (410x502) por QSPI a 80 MHz.
 *   5. Carga nuestra Watchface minimalista (Pantalla 1) y el Panel de Ajustes
 * (Pantalla 2).
 *   6. Activa el gestor inteligente de pantalla (power_manager):
 *      - Apaga la pantalla tras 15 s de inactividad o al bajar el brazo.
 *      - Enciende la pantalla automáticamente al levantar/girar la muñeca o
 * pulsar el botón PWR.
 */

#include "config/pins_config.h"
#include "hal/hal_display.h"
#include "hal/hal_imu.h"
#include "hal/hal_power.h"
#include "hal/hal_rtc.h"
#include "hal/hal_touch.h"
#include "services/power_manager.h"
#include "services/time_sync.h"
#include "ui/ui_manager.h"
#include <Arduino.h>
#include <Wire.h>

static uint32_t lastClockTickMs = 0;
static uint32_t lastTelemetryMs = 0;
static bool lastBootButtonState = HIGH;
static ImuData latestImu = {0, 0, 1.0f, 0, 0, 0, 0, false};

const char *identifyI2CChip(uint8_t address) {
  switch (address) {
  case I2C_ADDR_ES8311:
    return "ES8311   -> Chip de Audio DAC (Altavoz)";
  case I2C_ADDR_AXP2101:
    return "AXP2101  -> PMIC Gestor de Energia y Bateria";
  case I2C_ADDR_FT3168:
    return "FT3168   -> Controlador de Pantalla Tactil";
  case I2C_ADDR_ES7210:
    return "ES7210   -> Chip de Audio ADC (2 Microfonos)";
  case I2C_ADDR_PCF85063:
    return "PCF85063 -> Reloj de Tiempo Real (RTC)";
  case I2C_ADDR_QMI8658:
    return "QMI8658  -> Sensor de Movimiento IMU 6 Ejes";
  default:
    return "Dispositivo I2C";
  }
}

void scanI2CBus() {
  Serial.println(
      "\n============================================================");
  Serial.println(" ESCANER DEL BUS I2C (Pines SDA=GPIO 15, SCL=GPIO 14)");
  Serial.println(
      "============================================================");

  int foundCount = 0;
  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      foundCount++;
      Serial.printf("  [OK] Direccion 0x%02X : %s\n", addr,
                    identifyI2CChip(addr));
    }
  }

  Serial.println(
      "------------------------------------------------------------");
  Serial.printf(" Total de chips encontrados en el bus I2C: %d\n", foundCount);
  Serial.println(
      "============================================================\n");
}

void updateAndPrintStatus() {
  WatchDateTime dt = hal_rtc_get_time();
  WatchPowerStatus st = hal_power_get_status();

  if (hal_display_is_on()) {
    ui_manager_update_time(dt);
    ui_manager_update_power(st);
  }

  Serial.printf("[RELOJ] %04u-%02u-%02u %02u:%02u:%02u | Bat: %4u mV (%3d%%) | "
                "Pantalla: %s | IMU Z: %+.2fg Giro: %3.0f dps\n",
                dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second,
                st.batteryVoltageMv, st.batteryPercent,
                hal_display_is_on() ? "ON " : "OFF", latestImu.accelZ,
                latestImu.gyroTotal);
}

void printSystemReport() {
  Serial.println(
      "\n\n############################################################");
  Serial.println(
      "#  FIRMWARE PROPIO - RELOJ INTELIGENTE ESP32-S3 (FASE 4)   #");
  Serial.println(
      "############################################################");
  Serial.printf("  * Procesador (CPU) : %s @ %u MHz (%u nucleos)\n",
                ESP.getChipModel(), ESP.getCpuFreqMHz(), ESP.getChipCores());
  Serial.printf("  * Memoria Flash    : %u MB\n",
                ESP.getFlashChipSize() / (1024 * 1024));
  Serial.printf("  * Memoria PSRAM    : %u KB total (%u KB libres)\n",
                ESP.getPsramSize() / 1024, ESP.getFreePsram() / 1024);
  Serial.printf("  * Reloj Hardware   : PCF85063 RTC activo en 0x51\n");
  Serial.printf(
      "  * Sensor IMU 6 Ejes: QMI8658 activo en 0x6B (Gesto muneca ON)\n");

  scanI2CBus();
  updateAndPrintStatus();
  Serial.println("\n>>> COMANDOS POR CONSOLA USB (COM5): <<<");
  Serial.println("  * Envia 'i' -> Ver este reporte completo.");
  Serial.println("  * Envia 'T2026-11-06 13:25:00' -> Ajustar fecha y hora "
                 "exacta en el chip RTC.\n");
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

  // 2. Inicializar gestor de energía AXP2101 (0x34)
  Serial.println("\n[1/6] Inicializando gestor de energia AXP2101 (0x34)...");
  hal_power_init(Wire);

  // 3. Inicializar chip de Reloj en Tiempo Real PCF85063 (0x51)
  Serial.println("[2/6] Inicializando chip de reloj RTC PCF85063 (0x51)...");
  hal_rtc_init(Wire);

  // 4. Inicializar sensor de movimiento IMU QMI8658 (0x6B)
  Serial.println(
      "[3/6] Inicializando sensor de movimiento IMU QMI8658 (0x6B)...");
  if (hal_imu_init(Wire)) {
    Serial.println("  -> [EXITO] Sensor QMI8658 activo (Acelerometro 3D + "
                   "Giroscopio 3D).");
  } else {
    Serial.println("  -> [ERROR] No se detecto el sensor QMI8658.");
  }

  // 5. Inicializar panel táctil FT3168 (0x38) y pantalla AMOLED QSPI (410x502)
  Serial.println("[4/6] Inicializando panel tactil FT3168 (0x38)...");
  hal_touch_init(Wire);

  Serial.println(
      "[5/6] Inicializando pantalla AMOLED QSPI (410x502 @ 80MHz)...");
  hal_display_init();

  // 6. Inicializar motor gráfico LVGL y gestor inteligente de pantalla
  Serial.println(
      "[6/6] Construyendo Watchface minimalista y gestor de energia...");
  ui_manager_init();
  power_manager_init();

  ui_manager_update_time(hal_rtc_get_time());
  ui_manager_update_power(hal_power_get_status());

  printSystemReport();
  lastClockTickMs = millis();
  lastTelemetryMs = millis();
}

void loop() {
  // A. Actualizar gestor de movimiento (QMI8658) y ahorro de pantalla
  power_manager_update(&latestImu);

  // B. Si la pantalla está encendida, ejecutar LVGL y verificar minuto del RTC
  if (hal_display_is_on()) {
    ui_manager_loop();

    if (millis() - lastClockTickMs >= 250) {
      lastClockTickMs = millis();
      ui_manager_update_time(hal_rtc_get_time());
    }
  }

  // C. Detectar botón físico BOOT (GPIO 0)
  bool currentBootState = digitalRead(WATCH_BTN_BOOT_PIN);
  if (currentBootState != lastBootButtonState) {
    delay(20);
    currentBootState = digitalRead(WATCH_BTN_BOOT_PIN);
    if (currentBootState != lastBootButtonState) {
      lastBootButtonState = currentBootState;
      if (currentBootState == LOW) {
        power_manager_notify_user_activity();
        Serial.println(">>> [BOTON] Presionado BOOT (GPIO 0) <<<");
        ui_manager_notify_button("BOOT presionado");
      }
    }
  }

  // D. Detectar botón físico PWR (Encender / Apagar la pantalla con 1 toque)
  PwrButtonEvent pwrEvent = hal_power_poll_button();
  if (pwrEvent == PWR_BTN_SHORT_PRESS) {
    power_manager_toggle_screen_manual();
  }

  // E. Procesar comandos recibidos por el cable USB (COM5)
  if (Serial.available() > 0) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.equalsIgnoreCase("i")) {
      printSystemReport();
    } else if (line.startsWith("T") || line.startsWith("t")) {
      line.setCharAt(0, 'T');
      if (time_sync_from_serial_string(line)) {
        ui_manager_update_time(hal_rtc_get_time());
      }
    }
  }

  // F. Actualizar telemetría cada 3 segundos
  if (millis() - lastTelemetryMs >= 3000) {
    lastTelemetryMs = millis();
    updateAndPrintStatus();
  }

  delay(5);
}
