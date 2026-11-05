/**
 * @file hal_rtc.h
 * @brief Capa de Abstracción de Hardware (HAL) para el Reloj de Tiempo Real PCF85063 (I2C 0x51).
 *
 * ¿QUÉ ES EL CHIP RTC (PCF85063) Y PARA QUÉ SIRVE?
 * - "RTC" significa Real-Time Clock (Reloj en Tiempo Real).
 * - Es un chip diminuto con su propio cristal de cuarzo que mide el paso de cada segundo
 *   de forma autónoma, consumiendo casi cero batería (menos de 1 microamperio).
 * - Aunque el procesador principal (ESP32-S3) se reinicie o entre en sueño profundo,
 *   el chip PCF85063 sigue contando los segundos, minutos, horas, días, meses y años.
 */

#pragma once

#include <Arduino.h>
#include <Wire.h>

struct WatchDateTime {
    uint16_t year;      // Año completo (ej. 2026)
    uint8_t month;      // Mes (1 a 12)
    uint8_t day;        // Día del mes (1 a 31)
    uint8_t weekday;    // Día de la semana (0 = Domingo, 1 = Lunes, ..., 6 = Sábado)
    uint8_t hour;       // Hora en formato 24h (0 a 23)
    uint8_t minute;     // Minuto (0 a 59)
    uint8_t second;     // Segundo (0 a 59)
    bool valid;         // true si la hora leída del chip es válida
};

/**
 * @brief Inicializa la comunicación con el chip PCF85063 en la dirección I2C 0x51.
 *        Si el reloj nunca había sido puesto en hora, le carga una hora inicial base.
 */
bool hal_rtc_init(TwoWire &wirePort);

/**
 * @brief Lee la fecha y hora actual directamente desde los registros del chip PCF85063.
 */
WatchDateTime hal_rtc_get_time();

/**
 * @brief Graba una nueva fecha y hora exacta dentro del chip PCF85063.
 */
bool hal_rtc_set_time(const WatchDateTime &dt);

/**
 * @brief Calcula automáticamente el día de la semana (0=Dom..6=Sáb) para cualquier fecha.
 */
uint8_t hal_rtc_calc_weekday(uint16_t year, uint8_t month, uint8_t day);

