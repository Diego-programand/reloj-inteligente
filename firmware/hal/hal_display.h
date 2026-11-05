/**
 * @file hal_display.h
 * @brief Capa de Abstracción de Hardware (HAL) para la pantalla AMOLED QSPI (410x502).
 *
 * ¿CÓMO FUNCIONA ESTA PANTALLA?
 * - Es un panel AMOLED de 410 x 502 píxeles con el chip controlador CO5300 / SH8601.
 * - Se comunica con el procesador ESP32-S3 por una autopista de 4 carriles llamada QSPI
 *   (Quad SPI: envía 4 bits por cada pulso de reloj usando los pines GPIO 4, 5, 6 y 7).
 * - Descubrimiento clave del respaldo de fábrica: La memoria interna del chip tiene un
 *   desplazamiento horizontal (offset) de 22 píxeles (0x16). Por eso la columna 0 visible
 *   empieza internamente en la columna 22 (0x0016) y termina en la 431 (0x01AF).
 * - El brillo (0 a 255) no usa un LED trasero, sino el comando digital 0x51 que regula
 *   la intensidad propia de los píxeles orgánicos (AMOLED).
 */

#pragma once

#include <Arduino.h>
#include "config/pins_config.h"

// Colores básicos en formato RGB565 (16 bits por píxel)
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x001F
#define COLOR_CYAN      0x07FF
#define COLOR_YELLOW    0xFFE0
#define COLOR_ORANGE    0xFD20
#define COLOR_DARKGRAY  0x2104

/**
 * @brief Inicializa el bus QSPI de 4 carriles, reinicia físicamente la pantalla (GPIO 8)
 *        y envía la secuencia exacta de encendido del chip CO5300 / SH8601.
 * @return true si se inicializó correctamente.
 */
bool hal_display_init();

/**
 * @brief Ajusta el nivel de brillo de la pantalla AMOLED.
 * @param brightness Valor de 0 (apagada / 0%) a 255 (máximo brillo / 100%).
 */
void hal_display_set_brightness(uint8_t brightness);

/**
 * @brief Obtiene el nivel de brillo actual (0 - 255).
 */
uint8_t hal_display_get_brightness();

/**
 * @brief Enciende o apaga el panel AMOLED para ahorrar batería.
 */
void hal_display_set_power(bool on);

/**
 * @brief Envía un bloque rectangular de píxeles (RGB565) a la pantalla por QSPI.
 *        Esta función es la que usará tanto nuestro código como el motor gráfico LVGL.
 */
void hal_display_push_colors(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, const uint16_t *colorData);

/**
 * @brief Rellena toda la pantalla (410 x 502) con un color sólido (en formato RGB565).
 */
void hal_display_fill_screen(uint16_t colorRgb565);

/**
 * @brief Rellena un rectángulo en pantalla con un color sólido (en formato RGB565).
 */
void hal_display_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t colorRgb565);
