/**
 * @file scr_watchface.h
 * @brief Pantalla principal del reloj (Watchface Minimalista Ejecutiva) para 410x502 AMOLED.
 *
 * ELEMENTOS DE LA WATCHFACE:
 * 1. Día de la semana en español (24pt, tono titanio #8E8E93).
 * 2. Hora principal gigante en formato 12H (108pt tabular anti-aliased, blanco alabastro #F4F4F5).
 * 3. Indicador AM/PM (28pt, plata cepillada #A1A1AA).
 * 4. Línea divisoria minimalista (64x2 px, grafito #27272A).
 * 5. Fecha del mes en español (24pt, plata cepillada #A1A1AA).
 * 6. Línea interactiva de Google Calendar (18pt, titanio #71717A) con modal emergente de detalles.
 */

#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include "hal/hal_rtc.h"
#include "hal/hal_power.h"

/**
 * @brief Construye todos los elementos visuales de la Watchface (y el modal de agenda)
 *        dentro del mosaico (tile) principal.
 */
void scr_watchface_build(lv_obj_t *parentTile);

/**
 * @brief Actualiza la hora, fecha y línea de próximo evento en la Watchface.
 *        Aplica política de consumo cero: solo redibuja cuando cambia el minuto,
 *        el día o la versión de sincronización de Google Calendar.
 */
void scr_watchface_update_time(const WatchDateTime &dt);

/**
 * @brief Fuerza el refresco inmediato de la línea resumen de Google Calendar y del modal
 *        (útil tras una sincronización manual o actualización de agenda).
 */
void scr_watchface_refresh_calendar();

/**
 * @brief Actualiza los indicadores de batería y energía de la Watchface.
 */
void scr_watchface_update_power(const WatchPowerStatus &power, uint8_t brightness);
