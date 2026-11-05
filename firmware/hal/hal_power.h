/**
 * @file hal_power.h
 * @brief Capa de Abstracción de Hardware (HAL) para el chip de energía AXP2101 (PMIC).
 *
 * ¿QUÉ HACE ESTE MÓDULO?
 * El chip AXP2101 (dirección I2C 0x34) es el "cuadro eléctrico" del reloj:
 * 1. Enciende los canales de electricidad (reguladores LDO) que alimentan la pantalla
 *    y los sensores internos.
 * 2. Mide el voltaje real (en milivoltios) y el porcentaje (%) de la batería.
 * 3. Detecta si el cable USB está cargando la batería.
 * 4. Detecta pulsaciones del botón físico lateral PWR consultando por I2C (polling).
 *
 * REGLA DE ORO DEL HARDWARE:
 * Jamás usar el pin GPIO 35 para interrupciones del AXP2101, porque ese pin pertenece
 * al bus de la memoria RAM extra de 8 MB (Octal PSRAM). Todo se consulta por I2C.
 */

#pragma once

#include <Arduino.h>
#include <Wire.h>

// Estructura simple con los datos actuales de energía del reloj
struct WatchPowerStatus {
    bool batteryConnected;      // ¿Hay batería detectada?
    bool usbConnected;          // ¿Está conectado el cable USB (VBUS)?
    bool isCharging;            // ¿Se está cargando la batería en este momento?
    uint16_t batteryVoltageMv;  // Voltaje de la batería en milivoltios (ej. 4100 mV = 4.10 V)
    int batteryPercent;         // Porcentaje estimado de carga (0 a 100 %)
    uint16_t vbusVoltageMv;     // Voltaje que entra por el cable USB (aprox. 5000 mV)
    uint16_t sysVoltageMv;      // Voltaje interno del sistema (aprox. 3300 - 4200 mV)
};

// Eventos del botón físico de encendido (PWR)
enum PwrButtonEvent {
    PWR_BTN_NONE = 0,
    PWR_BTN_SHORT_PRESS,        // Pulsación corta del botón PWR
    PWR_BTN_LONG_PRESS          // Pulsación larga del botón PWR (más de 1.5 s)
};

/**
 * @brief Inicializa el chip AXP2101 en el bus I2C, activa los rieles de energía
 *        de los periféricos y arranca el medidor de batería.
 * @return true si el chip AXP2101 respondió correctamente en 0x34, false si falló.
 */
bool hal_power_init(TwoWire &wirePort);

/**
 * @brief Lee el estado actual de la batería y del cargador USB.
 */
WatchPowerStatus hal_power_get_status();

/**
 * @brief Consulta por I2C (polling) si el usuario presionó el botón físico PWR.
 *        Limpia automáticamente el aviso interno del chip tras leerlo.
 */
PwrButtonEvent hal_power_poll_button();

