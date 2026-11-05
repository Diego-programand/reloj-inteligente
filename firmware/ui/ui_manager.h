/**
 * @file ui_manager.h
 * @brief Gestor del motor gráfico LVGL (con buffers en los 8 MB de Octal PSRAM).
 *
 * ESTRUCTURA DE PANTALLAS (FASE 3):
 * - Pantalla 1 (Principal): Watchface personalizada (scr_watchface) conectada al chip RTC PCF85063.
 * - Pantalla 2 (Al deslizar a la izquierda): Centro de Control Rápido (Brillo AMOLED,
 *   botones táctiles para ajustar +1 Hora / +1 Minuto / Seg=00 en el chip RTC, y telemetría).
 */

#pragma once

#include <Arduino.h>
#include "hal/hal_power.h"
#include "hal/hal_rtc.h"

bool ui_manager_init();
void ui_manager_loop();
void ui_manager_update_time(const WatchDateTime &dt);
void ui_manager_update_power(const WatchPowerStatus &status);
void ui_manager_notify_button(const char *buttonText);
void ui_manager_sync_brightness(uint8_t brightness);
