/**
 * @file hal_touch.h
 * @brief Capa de Abstracción de Hardware (HAL) para el panel táctil FT3168 (I2C 0x38).
 *
 * ¿CÓMO FUNCIONA EL PANEL TÁCTIL?
 * - Bajo el cristal de la pantalla hay una malla invisible que mide cambios diminutos
 *   de electricidad cuando acercas tu dedo (panel táctil capacitivo).
 * - El chip FT3168 traduce ese contacto en dos números exactos:
 *     * Coordenada X (Horizontal): de 0 (izquierda) a 409 (derecha).
 *     * Coordenada Y (Vertical):   de 0 (arriba)    a 501 (abajo).
 * - Se comunica con el procesador por el mismo bus I2C (pines SDA=15, SCL=14) en la dirección 0x38.
 */

#pragma once

#include <Arduino.h>
#include <Wire.h>

struct TouchPoint {
    bool touched;       // true si hay un dedo apoyado en la pantalla ahora mismo
    uint16_t x;         // Posición horizontal (0 a 409)
    uint16_t y;         // Posición vertical (0 a 501)
};

/**
 * @brief Despierta el chip táctil FT3168 usando su pin de Reset (GPIO 9) y verifica
 *        su presencia en la dirección I2C 0x38.
 */
bool hal_touch_init(TwoWire &wirePort);

/**
 * @brief Lee si el usuario está tocando la pantalla y en qué coordenadas (X, Y).
 */
TouchPoint hal_touch_read();
