/**
 * @file time_sync.h
 * @brief Servicio de sincronización de hora exacta (por Wi-Fi NTP UTC-5 o por cable USB).
 *
 * ¿CÓMO OBTIENE LA HORA EXACTA NUESTRO RELOJ?
 * Tiene dos mecanismos complementarios que graban la hora en el chip PCF85063:
 * 1. Sincronización por Wi-Fi (NTP - Network Time Protocol):
 *    Enciende la antena Wi-Fi del ESP32-S3 durante unos pocos segundos, consulta los
 *    relojes atómicos de internet (pool.ntp.org) con la zona horaria UTC-5 (Colombia),
 *    guarda la hora en el chip PCF85063 y apaga el Wi-Fi de inmediato para no gastar batería.
 * 2. Sincronización instantánea por cable USB (COM5):
 *    Si el PC envía el texto "TYYYY-MM-DD HH:MM:SS" por el puerto serie, el reloj
 *    ajusta el chip PCF85063 al instante sin siquiera necesitar una red Wi-Fi cerca.
 */

#pragma once

#include <Arduino.h>
#include "hal/hal_rtc.h"

/**
 * @brief Conecta temporalmente a una red Wi-Fi, obtiene la hora exacta por NTP (UTC-5),
 *        la guarda en el chip RTC PCF85063 y apaga el Wi-Fi al terminar.
 * @param ssid Nombre de la red Wi-Fi.
 * @param password Contraseña de la red Wi-Fi.
 * @param timeoutMs Tiempo máximo de espera en milisegundos (ej. 10000 ms = 10 s).
 * @return true si logró sincronizar y grabar la hora en el RTC.
 */
bool time_sync_from_wifi_ntp(const char *ssid, const char *password, uint32_t timeoutMs = 10000);

/**
 * @brief Procesa una cadena de texto con formato "TYYYY-MM-DD HH:MM:SS" recibida por USB
 *        y actualiza el chip RTC PCF85063.
 * @return true si el formato era válido y se actualizó el RTC.
 */
bool time_sync_from_serial_string(const String &cmdLine);

