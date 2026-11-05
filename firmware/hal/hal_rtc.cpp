/**
 * @file hal_rtc.cpp
 * @brief Implementación del driver I2C para el reloj de cuarzo PCF85063 (dirección 0x51).
 *
 * NOTA DIDÁCTICA SOBRE EL FORMATO BCD:
 * Los chips de reloj como el PCF85063 guardan los números en un formato llamado BCD
 * (Binary-Coded Decimal), donde cada grupo de 4 bits representa un dígito decimal (del 0 al 9).
 * Por eso convertimos con decToBcd() al guardar la hora y con bcdToDec() al leerla.
 */

#include "hal_rtc.h"
#include "config/pins_config.h"

#define PCF85063_REG_CTRL1      0x00
#define PCF85063_REG_SECONDS    0x04

static TwoWire *rtcWire = nullptr;
static bool rtcInitialized = false;

static uint8_t bcdToDec(uint8_t val) {
    return (uint8_t)((val / 16 * 10) + (val % 16));
}

static uint8_t decToBcd(uint8_t val) {
    return (uint8_t)((val / 10 * 16) + (val % 10));
}

uint8_t hal_rtc_calc_weekday(uint16_t y, uint8_t m, uint8_t d) {
    // Algoritmo de Sakamoto para obtener el día de la semana (0 = Domingo .. 6 = Sábado)
    static const uint8_t t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y -= 1;
    return (uint8_t)((y + y/4 - y/100 + y/400 + t[m - 1] + d) % 7);
}

bool hal_rtc_init(TwoWire &wirePort) {
    rtcWire = &wirePort;

    rtcWire->beginTransmission(I2C_ADDR_PCF85063);
    if (rtcWire->endTransmission() != 0) {
        rtcInitialized = false;
        return false;
    }

    // Asegurar que el registro de control 1 tenga el reloj en marcha (STOP bit = 0)
    rtcWire->beginTransmission(I2C_ADDR_PCF85063);
    rtcWire->write(PCF85063_REG_CTRL1);
    rtcWire->write(0x00);
    rtcWire->endTransmission();

    rtcInitialized = true;

    // Verificar si el chip ya tiene una fecha válida (>= 2025)
    WatchDateTime now = hal_rtc_get_time();
    if (!now.valid || now.year < 2025 || now.month < 1 || now.month > 12) {
        // Cargar fecha/hora inicial por defecto si estaba vacío de fábrica
        WatchDateTime defaultDt;
        defaultDt.year    = 2026;
        defaultDt.month   = 11;
        defaultDt.day     = 5;
        defaultDt.hour    = 16;
        defaultDt.minute  = 50;
        defaultDt.second  = 0;
        defaultDt.weekday = hal_rtc_calc_weekday(2026, 11, 5);
        defaultDt.valid   = true;
        hal_rtc_set_time(defaultDt);
    }

    return true;
}

WatchDateTime hal_rtc_get_time() {
    WatchDateTime dt = {2026, 1, 1, 0, 0, 0, 0, false};
    if (!rtcInitialized || !rtcWire) {
        return dt;
    }

    rtcWire->beginTransmission(I2C_ADDR_PCF85063);
    rtcWire->write(PCF85063_REG_SECONDS);
    if (rtcWire->endTransmission(true) != 0) {
        return dt;
    }

    if (rtcWire->requestFrom((uint8_t)I2C_ADDR_PCF85063, (uint8_t)7) < 7) {
        return dt;
    }

    uint8_t rawSec   = rtcWire->read();
    uint8_t rawMin   = rtcWire->read();
    uint8_t rawHour  = rtcWire->read();
    uint8_t rawDay   = rtcWire->read();
    uint8_t rawWday  = rtcWire->read();
    uint8_t rawMonth = rtcWire->read();
    uint8_t rawYear  = rtcWire->read();

    bool oscStopped = (rawSec & 0x80) != 0; // El bit 7 indica si el oscilador se detuvo alguna vez

    dt.second  = bcdToDec(rawSec & 0x7F);
    dt.minute  = bcdToDec(rawMin & 0x7F);
    dt.hour    = bcdToDec(rawHour & 0x3F);
    dt.day     = bcdToDec(rawDay & 0x3F);
    dt.weekday = rawWday & 0x07;
    dt.month   = bcdToDec(rawMonth & 0x1F);
    dt.year    = 2000 + bcdToDec(rawYear);
    dt.valid   = !oscStopped && (dt.month >= 1 && dt.month <= 12) && (dt.day >= 1 && dt.day <= 31);

    return dt;
}

bool hal_rtc_set_time(const WatchDateTime &dt) {
    if (!rtcInitialized || !rtcWire) {
        return false;
    }

    uint8_t y2d  = (dt.year >= 2000) ? (uint8_t)(dt.year - 2000) : (uint8_t)dt.year;
    uint8_t wday = hal_rtc_calc_weekday(dt.year, dt.month, dt.day);

    rtcWire->beginTransmission(I2C_ADDR_PCF85063);
    rtcWire->write(PCF85063_REG_SECONDS);
    rtcWire->write(decToBcd(dt.second) & 0x7F); // Bit 7 = 0 limpia el indicador de parada (OS)
    rtcWire->write(decToBcd(dt.minute));
    rtcWire->write(decToBcd(dt.hour));
    rtcWire->write(decToBcd(dt.day));
    rtcWire->write(wday & 0x07);
    rtcWire->write(decToBcd(dt.month));
    rtcWire->write(decToBcd(y2d));

    return (rtcWire->endTransmission(true) == 0);
}

