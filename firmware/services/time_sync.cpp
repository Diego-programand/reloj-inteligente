/**
 * @file time_sync.cpp
 * @brief Implementación del servicio de sincronización de tiempo (NTP UTC-5 y Serie USB).
 */

#include "time_sync.h"
#include <WiFi.h>
#include <time.h>

// Zona horaria UTC-5 (Colombia / Perú / Ecuador / EST sin horario de verano): -5 * 3600 = -18000 segundos
#define TIMEZONE_OFFSET_SEC     (-5L * 3600L)
#define DAYLIGHT_OFFSET_SEC     0

bool time_sync_from_wifi_ntp(const char *ssid, const char *password, uint32_t timeoutMs) {
    if (!ssid || strlen(ssid) == 0) {
        return false;
    }

    Serial.printf("[TIME_SYNC] Conectando a Wi-Fi '%s' para sincronizar hora NTP...\n", ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    uint32_t startMs = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startMs < timeoutMs)) {
        delay(200);
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[TIME_SYNC] No se pudo conectar al Wi-Fi. Apagando radio para ahorrar bateria.");
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        return false;
    }

    // Consultar servidores de hora atómica NTP ajustados a UTC-5
    configTime(TIMEZONE_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, "pool.ntp.org", "time.google.com");

    struct tm timeinfo;
    bool synced = false;
    if (getLocalTime(&timeinfo, 5000)) {
        WatchDateTime dt;
        dt.year    = (uint16_t)(timeinfo.tm_year + 1900);
        dt.month   = (uint8_t)(timeinfo.tm_mon + 1);
        dt.day     = (uint8_t)timeinfo.tm_mday;
        dt.hour    = (uint8_t)timeinfo.tm_hour;
        dt.minute  = (uint8_t)timeinfo.tm_min;
        dt.second  = (uint8_t)timeinfo.tm_sec;
        dt.weekday = (uint8_t)timeinfo.tm_wday;
        dt.valid   = true;

        synced = hal_rtc_set_time(dt);
        if (synced) {
            Serial.printf("[TIME_SYNC] Hora NTP (UTC-5) grabada en PCF85063: %04u-%02u-%02u %02u:%02u:%02u\n",
                          dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
        }
    }

    // Apagar completamente el Wi-Fi tras sincronizar para cuidar la batería
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return synced;
}

bool time_sync_from_serial_string(const String &cmdLine) {
    // Formato esperado: T2026-11-05 16:50:30
    int y = 0, m = 0, d = 0, hh = 0, mm = 0, ss = 0;
    if (sscanf(cmdLine.c_str(), "T%d-%d-%d %d:%d:%d", &y, &m, &d, &hh, &mm, &ss) == 6) {
        if (y >= 2024 && y <= 2099 && m >= 1 && m <= 12 && d >= 1 && d <= 31 &&
            hh >= 0 && hh <= 23 && mm >= 0 && mm <= 59 && ss >= 0 && ss <= 59) {
            WatchDateTime dt;
            dt.year    = (uint16_t)y;
            dt.month   = (uint8_t)m;
            dt.day     = (uint8_t)d;
            dt.hour    = (uint8_t)hh;
            dt.minute  = (uint8_t)mm;
            dt.second  = (uint8_t)ss;
            dt.weekday = hal_rtc_calc_weekday(dt.year, dt.month, dt.day);
            dt.valid   = true;

            if (hal_rtc_set_time(dt)) {
                Serial.printf("[TIME_SYNC] Hora actualizada en chip RTC PCF85063: %04u-%02u-%02u %02u:%02u:%02u\n",
                              dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
                return true;
            }
        }
    }
    return false;
}

