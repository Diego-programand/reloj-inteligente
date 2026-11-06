/**
 * @file ui_manager.h
 * @brief Gestor del motor gráfico LVGL (con buffers de pantalla completa en los 8 MB de Octal PSRAM).
 *
 * ESTRUCTURA DE PANTALLAS:
 * - Pantalla 1 (Principal): Watchface minimalista (scr_watchface) en formato 12H con fecha.
 * - Pantalla 2 (Al deslizar a la izquierda): Panel de Ajustes simplificado
 *   (Control de Brillo, Control de Volumen y Vista de Batería).
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
uint8_t ui_manager_get_volume();
