/**
 * @file scr_watchface.h
 * @brief Pantalla principal del reloj (Watchface Personalizada) para 410x502 AMOLED.
 *
 * ELEMENTOS DE TU WATCHFACE:
 * 1. Barra superior: Estado del chip de reloj (RTC OK) + Porcentaje y barra de batería.
 * 2. Dial circular central: Anillo animado de 310x310 px que avanza cada segundo (0 a 59 s).
 * 3. Centro del dial:
 *    - Día de la semana y fecha en español (ej. "JUEVES, 05 NOV 2026").
 *    - Hora y minutos de gran tamaño (48 pt).
 *    - Segundos e indicador AM/PM o 24H (tocar el centro del reloj alterna entre 12H y 24H).
 * 4. Dos tarjetas inferiores de resumen (Complicaciones): Voltaje de batería (mV) y Nivel de brillo.
 */

#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include "hal/hal_rtc.h"
#include "hal/hal_power.h"

/**
 * @brief Construye todos los elementos visuales de la Watchface dentro del mosaico (tile) principal.
 */
void scr_watchface_build(lv_obj_t *parentTile);

/**
 * @brief Actualiza la hora, segundos y fecha mostrados en la Watchface.
 */
void scr_watchface_update_time(const WatchDateTime &dt);

/**
 * @brief Actualiza los indicadores de batería y energía de la Watchface.
 */
void scr_watchface_update_power(const WatchPowerStatus &power, uint8_t brightness);

