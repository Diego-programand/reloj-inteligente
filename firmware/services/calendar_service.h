/**
 * @file calendar_service.h
 * @brief Servicio de sincronización y gestión de agenda (Google Calendar vía HTTPS + Modo Demo).
 *
 * ¿CÓMO FUNCIONA LA AGENDA DENTRO DEL RELOJ INTELIGENTE?
 * ------------------------------------------------------
 * 1. ARQUITECTURA DE BAJO CONSUMO ("BURST WI-FI"):
 *    La antena Wi-Fi del procesador ESP32-S3 es el componente que más batería consume
 *    (alrededor de 150 a 250 mA cuando transmite). Si la dejáramos encendida todo el día,
 *    la batería del reloj se agotaría en pocas horas.
 *    Por eso usamos una técnica llamada "ráfaga" (Burst):
 *      - El reloj mantiene el Wi-Fi completamente apagado (`WIFI_OFF`) el 99.7% del tiempo.
 *      - Cada 30 minutos (o cuando lo pidas manualmente), enciende la antena Wi-Fi con un
 *        tiempo límite estricto de 5 segundos, descarga el resumen JSON desde nuestro puente
 *        de Google Apps Script, guarda los eventos en memoria y apaga la antena al instante.
 *
 * 2. USO DE LA MEMORIA OCTAL PSRAM (8 MB):
 *    Para evitar fragmentar la memoria interna principal del procesador (SRAM) al descargar
 *    y analizar (parsear) texto JSON desde internet, alojamos el búfer de descarga y el
 *    árbol de datos de `ArduinoJson` directamente en los 8 MB de memoria externa (`Octal PSRAM`,
 *    usando `MALLOC_CAP_SPIRAM`).
 *    Importante: La PSRAM se comunica internamente por el bus octal que ocupa el `GPIO 35`,
 *    el cual jamás tocamos por software.
 *
 * 3. MODO DEMO AUTOMÁTICO (DEFENSIVE MOCK DATA):
 *    Si aún no has creado el archivo `firmware/config/wifi_secrets.h` con tu clave Wi-Fi
 *    y tu URL de Google Apps Script (o si no hay conexión Wi-Fi cerca), el reloj NO falla
 *    ni deja la pantalla vacía: genera automáticamente una agenda ejecutiva de demostración
 *    calculada en tiempo real a partir de la hora actual del chip de reloj físico `PCF85063`.
 */

#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>
#include "hal/hal_rtc.h"

// Cantidad máxima de eventos almacenados en caché (alineado con calendar_bridge.gs)
#define CALENDAR_MAX_EVENTS         5

// Intervalo de sincronización automática en segundo plano: 30 minutos en milisegundos
#define CALENDAR_SYNC_INTERVAL_MS   (30UL * 60UL * 1000UL)

// Timeout máximo para conectar al Wi-Fi: 5 segundos (protección de batería y fluidez UI)
#define CALENDAR_WIFI_TIMEOUT_MS    5000UL

// Zona horaria local almacenada en el chip RTC PCF85063: UTC-5 (Colombia / Perú / EST)
#define CALENDAR_TZ_OFFSET_SEC      (-5L * 3600L)

/**
 * @brief Estructura compacta de un evento de calendario en memoria.
 *        Diseñada con tamaños fijos (`char[]`) para cero fragmentación de memoria.
 */
struct CalendarEvent {
    char id[16];            // Identificador corto único del evento (ej. "mock_ev_01" o ID de Google)
    char title[48];         // Título limpio del evento (máx. 47 caracteres + '\0')
    char location[32];      // Ubicación o sala (máx. 31 caracteres + '\0')
    uint32_t startUnix;     // Inicio del evento en segundos Unix Epoch (UTC)
    uint32_t endUnix;       // Fin del evento en segundos Unix Epoch (UTC)
    bool allDay;            // true si es un evento de día completo
    bool valid;             // true si esta ranura contiene un evento activo/válido
};

/**
 * @brief Inicializa el servicio de calendario durante el `setup()` del reloj.
 *        Carga inmediatamente datos iniciales (Mock Data ejecutiva si no hay secretos
 *        configurados) sin bloquear el encendido visual de la pantalla AMOLED.
 */
void calendar_service_init();

/**
 * @brief Bucle no bloqueante del servicio de calendario (llamar desde `loop()`).
 *        Verifica si han transcurrido 30 minutos desde la última sincronización
 *        (o si hay una primera sincronización pendiente tras arrancar) y ejecuta
 *        `calendar_service_sync_now()`.
 */
void calendar_service_loop();

/**
 * @brief Fuerza una sincronización inmediata con Google Calendar.
 *        - Enciende el Wi-Fi (`WIFI_STA`) con timeout estricto de 5 segundos.
 *        - Realiza petición HTTPS GET siguiendo redirecciones HTTP 302 (`HTTPC_STRICT_FOLLOW_REDIRECTS`).
 *        - Parsea el JSON en la memoria Octal PSRAM (`MALLOC_CAP_SPIRAM`).
 *        - Apaga de inmediato el módulo Wi-Fi (`WiFi.mode(WIFI_OFF)`).
 *        - Si no hay credenciales reales o falla la red y no hay caché previo,
 *          carga/actualiza la Mock Data ejecutiva relativa al reloj RTC PCF85063.
 * @return true si sincronizó eventos reales por HTTPS; false si usó/mantuvo respaldo local/Mock.
 */
bool calendar_service_sync_now();

/**
 * @brief Obtiene el evento más próximo a ocurrir (o el que está ocurriendo en este instante)
 *        comparando la agenda en memoria contra la hora actual del chip RTC PCF85063.
 *
 * @param outEvent Referencia donde se copiará el evento encontrado.
 * @param outMinutesUntilStart Minutos restantes para que inicie (0 si ya está en curso).
 * @param outIsOngoing true si el evento está ocurriendo ahora mismo (`now >= start && now < end`).
 * @return true si hay un evento próximo o en curso disponible; false si la agenda está libre.
 */
bool calendar_service_get_next_event(CalendarEvent &outEvent,
                                     int32_t &outMinutesUntilStart,
                                     bool &outIsOngoing);

/**
 * @brief Devuelve un contador de versión que se incrementa cada vez que la lista de eventos
 *        cambia (por sincronización HTTPS o regeneración de Mock Data).
 *        Útil para que la pantalla LVGL (`scr_watchface`) sepa si debe redibujar sin gastar CPU.
 */
uint32_t calendar_service_get_sync_version();

/**
 * @brief Entrega acceso de solo lectura al arreglo interno de eventos y devuelve cuántos hay.
 * @param outArray Puntero donde se escribirá la dirección del arreglo de eventos.
 * @return Cantidad de eventos válidos actualmente almacenados (0 a CALENDAR_MAX_EVENTS).
 */
size_t calendar_service_get_events(const CalendarEvent **outArray);

/**
 * @brief Indica si los eventos actuales en memoria provienen del Modo Demo (Mock Data).
 * @return true si está mostrando Mock Data; false si son eventos reales de Google Calendar.
 */
bool calendar_service_is_using_mock();

/**
 * @brief Indica si el firmware cuenta con credenciales reales configuradas en `config/wifi_secrets.h`.
 * @return true si `wifi_secrets.h` existe y no tiene los valores de plantilla por defecto.
 */
bool calendar_service_has_real_secrets();

/**
 * @brief Lee la hora local actual (`UTC-5`) desde el chip físico RTC `PCF85063` y la convierte
 *        a segundos Unix Epoch UTC.
 */
uint32_t calendar_service_now_unix();

/**
 * @brief Convierte una estructura `WatchDateTime` (en hora local `UTC-5`) a segundos Unix Epoch UTC.
 */
uint32_t calendar_service_datetime_to_unix(const WatchDateTime &localDt);

/**
 * @brief Convierte segundos Unix Epoch UTC a una estructura `WatchDateTime` en hora local (`UTC-5`).
 */
WatchDateTime calendar_service_unix_to_datetime(uint32_t unixUtc);

/**
 * @brief Extrae directamente la hora (0..23) y minuto (0..59) en zona local (`UTC-5`)
 *        a partir de un timestamp Unix Epoch UTC.
 */
void calendar_service_unix_to_local_hm(uint32_t unixUtc, uint8_t &outHour24, uint8_t &outMinute);

/**
 * @brief Formatea un timestamp Unix Epoch UTC como cadena de 12 horas en zona local (`UTC-5`),
 *        por ejemplo `"10:30 AM"` o `"04:15 PM"`.
 */
void calendar_service_format_time_12h(uint32_t unixUtc, char *outBuf, size_t outBufSize);
