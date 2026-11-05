/**
 * @file ui_manager.h
 * @brief Gestor del motor gráfico LVGL (con buffers en los 8 MB de Octal PSRAM).
 *
 * ¿QUÉ HACE ESTE MÓDULO?
 * 1. Reserva dos lienzos grandes (buffers) dentro de los 8 MB de memoria RAM extra (PSRAM).
 * 2. Conecta LVGL con nuestro driver de pantalla QSPI (hal_display) y nuestro lector
 *    táctil FT3168 (hal_touch).
 * 3. Crea una interfaz interactiva deslizable (2 pantallas con gesto swipe horizontal):
 *    - Pantalla 1: Estado de batería en vivo, barra deslizante (slider) de brillo AMOLED
 *      y monitor en pantalla de coordenadas táctiles y botones físicos.
 *    - Pantalla 2: Anillo interactivo y contador táctil a 60 FPS.
 */

#pragma once

#include <Arduino.h>
#include "hal/hal_power.h"

/**
 * @brief Inicializa LVGL, reserva los buffers en Octal PSRAM y construye la interfaz gráfica.
 * @return true si LVGL y los buffers en PSRAM se crearon con éxito.
 */
bool ui_manager_init();

/**
 * @brief Ejecuta el ciclo de renderizado y eventos táctiles de LVGL (llamar en loop()).
 */
void ui_manager_loop();

/**
 * @brief Actualiza los textos y barras de batería en la pantalla con los datos reales del AXP2101.
 */
void ui_manager_update_power(const WatchPowerStatus &status);

/**
 * @brief Muestra en la interfaz gráfica cuando se presiona un botón físico lateral (BOOT o PWR).
 */
void ui_manager_notify_button(const char *buttonText);

/**
 * @brief Sincroniza la barra deslizante de brillo en pantalla cuando se cambia con el botón PWR.
 */
void ui_manager_sync_brightness(uint8_t brightness);

