/**
 * @file calendar_service.cpp
 * @brief Implementación del servicio de agenda (Google Calendar HTTPS + PSRAM + Mock Data).
 *
 * EXPLICACIÓN TÉCNICA PARA EL DESARROLLADOR:
 * ------------------------------------------
 * - ¿Por qué seguimos redirecciones HTTP 302 (`HTTPC_STRICT_FOLLOW_REDIRECTS`)?
 *   Cuando el reloj hace una petición a `https://script.google.com/macros/s/.../exec`,
 *   el servidor principal de Google ejecuta nuestro script y responde con un código HTTP 302
 *   ("Moved Temporarily"), indicando que el JSON final está listo para recogerse en una
 *   dirección temporal de `https://script.googleusercontent.com/...`.
 *   Con `HTTPC_STRICT_FOLLOW_REDIRECTS`, la librería `HTTPClient` sigue ese desvío
 *   automáticamente de forma transparente.
 *
 * - ¿Por qué alojamos el JSON en la memoria Octal PSRAM (`MALLOC_CAP_SPIRAM`)?
 *   El procesador ESP32-S3 tiene ~512 KB de memoria interna rápida (SRAM) y 8 MB de
 *   memoria externa (Octal PSRAM). Descargar textos de internet y construir árboles JSON
 *   crea y destruye bloques de memoria dinámicamente. Al hacerlo dentro de los 8 MB de PSRAM,
 *   mantenemos la SRAM interna intacta y sin fragmentación para el sistema y el motor LVGL.
 */

#include "calendar_service.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <string.h>

// ============================================================================
// 1. INCLUSIÓN DEFENSIVA DE CREDENCIALES (wifi_secrets.h vs wifi_secrets.example.h)
// ============================================================================
#if __has_include("config/wifi_secrets.h")
    #include "config/wifi_secrets.h"
    #define CALENDAR_SECRETS_FILE_EXISTS 1
#else
    #include "config/wifi_secrets.example.h"
    #define CALENDAR_SECRETS_FILE_EXISTS 0
#endif

static bool evaluateRealSecretsConfig() {
#if CALENDAR_SECRETS_FILE_EXISTS == 0
    return false;
#else
    if (!WIFI_SSID || strlen(WIFI_SSID) == 0) {
        return false;
    }
    if (strcmp(WIFI_SSID, "TU_RED_WIFI_2.4GHZ") == 0) {
        return false;
    }
    if (!GCAL_WEBHOOK_URL || strlen(GCAL_WEBHOOK_URL) == 0) {
        return false;
    }
    if (strstr(GCAL_WEBHOOK_URL, "TU_DEPLOYMENT_ID") != nullptr) {
        return false;
    }
    return true;
#endif
}

// Constante evaluada al inicio que indica si existen secretos reales configurados
static const bool CALENDAR_HAS_REAL_SECRETS = evaluateRealSecretsConfig();

// ============================================================================
// 2. ASIGNADOR DE MEMORIA EN OCTAL PSRAM PARA ARDUINOJSON V7
// ============================================================================
/**
 * @brief Asignador personalizado que redirige todas las reservas de memoria de ArduinoJson
 *        hacia los 8 MB de Octal PSRAM (`MALLOC_CAP_SPIRAM`), con respaldo seguro en SRAM.
 */
class SpiRamJsonAllocator : public ArduinoJson::Allocator {
public:
    void *allocate(size_t size) override {
        void *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!ptr) {
            ptr = malloc(size);
        }
        return ptr;
    }

    void deallocate(void *pointer) override {
        heap_caps_free(pointer);
    }

    void *reallocate(void *ptr, size_t new_size) override {
        void *newPtr = heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!newPtr) {
            newPtr = realloc(ptr, new_size);
        }
        return newPtr;
    }
};

static SpiRamJsonAllocator psramJsonAllocator;

// ============================================================================
// 3. ESTADO INTERNO Y CACHÉ DE EVENTOS
// ============================================================================
static CalendarEvent cachedEvents[CALENDAR_MAX_EVENTS];
static size_t cachedEventCount       = 0;
static bool usingMockData            = true;
static uint32_t syncVersion          = 0;
static uint32_t lastSyncAttemptMs    = 0;
static bool serviceInitialized       = false;
static bool pendingInitialRealSync   = false;
static uint32_t bootTimestampMs      = 0;

// ============================================================================
// 4. UTILIDADES DE TEXTO Y CONVERSIÓN DE FECHA/HORA (RTC PCF85063 <-> UNIX UTC)
// ============================================================================

static bool isLeapYear(uint16_t year) {
    return ((year % 4U == 0U) && (year % 100U != 0U)) || (year % 400U == 0U);
}

static uint8_t daysInMonth(uint16_t year, uint8_t month) {
    static const uint8_t DAYS_TABLE[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) {
        return 30;
    }
    if (month == 2 && isLeapYear(year)) {
        return 29;
    }
    return DAYS_TABLE[month - 1];
}

/**
 * @brief Limpia cadenas UTF-8 convirtiendo tildes/eñes y elipsis a ASCII puro compatible
 *        con las fuentes internas Montserrat de LVGL, evitando caracteres rotos en pantalla.
 */
static void sanitizeUtf8ToAscii(const char *src, char *dst, size_t dstSize) {
    if (!dst || dstSize == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }

    size_t outIdx = 0;
    size_t inIdx  = 0;

    while (src[inIdx] != '\0' && outIdx < (dstSize - 1)) {
        uint8_t c = (uint8_t)src[inIdx];

        // Secuencia UTF-8 de 2 bytes para caracteres latinos comunes (0xC3 ...)
        if (c == 0xC3 && src[inIdx + 1] != '\0') {
            uint8_t next = (uint8_t)src[inIdx + 1];
            char mapped = '\0';
            switch (next) {
                case 0xA1: case 0xA0: case 0xA4: mapped = 'a'; break; // á à ä
                case 0xA9: case 0xA8: case 0xAB: mapped = 'e'; break; // é è ë
                case 0xAD: case 0xAC: case 0xAF: mapped = 'i'; break; // í ì ï
                case 0xB3: case 0xB2: case 0xB6: mapped = 'o'; break; // ó ò ö
                case 0xBA: case 0xB9: case 0xBC: mapped = 'u'; break; // ú ù ü
                case 0xB1:                       mapped = 'n'; break; // ñ
                case 0x81: case 0x80: case 0x84: mapped = 'A'; break; // Á À Ä
                case 0x89: case 0x88: case 0x8B: mapped = 'E'; break; // É È Ë
                case 0x8D: case 0x8C: case 0x8F: mapped = 'I'; break; // Í Ì Ï
                case 0x93: case 0x92: case 0x96: mapped = 'O'; break; // Ó Ò Ö
                case 0x9A: case 0x99: case 0x9C: mapped = 'U'; break; // Ú Ù Ü
                case 0x91:                       mapped = 'N'; break; // Ñ
                default:                         mapped = '?'; break;
            }
            dst[outIdx++] = mapped;
            inIdx += 2;
            continue;
        }

        // Secuencia UTF-8 de 3 bytes para signos tipográficos comunes (0xE2 0x80 ...)
        if (c == 0xE2 && (uint8_t)src[inIdx + 1] == 0x80 && src[inIdx + 2] != '\0') {
            uint8_t third = (uint8_t)src[inIdx + 2];
            if (third == 0xA6) { // Elipsis '…' -> "..."
                for (int k = 0; k < 3 && outIdx < (dstSize - 1); k++) {
                    dst[outIdx++] = '.';
                }
            } else if (third == 0x93 || third == 0x94) { // Guiones largos – —
                dst[outIdx++] = '-';
            }
            inIdx += 3;
            continue;
        }

        // Ignorar otros bytes UTF-8 fuera de rango ASCII imprimible
        if (c >= 0x20 && c <= 0x7E) {
            dst[outIdx++] = (char)c;
        } else if (c == '\t' || c == '\n' || c == '\r') {
            dst[outIdx++] = ' ';
        }
        inIdx++;
    }

    dst[outIdx] = '\0';
}

uint32_t calendar_service_datetime_to_unix(const WatchDateTime &localDt) {
    uint16_t year  = (localDt.year >= 2024 && localDt.year <= 2099) ? localDt.year : 2026;
    uint8_t month  = (localDt.month >= 1 && localDt.month <= 12) ? localDt.month : 11;
    uint8_t day    = (localDt.day >= 1 && localDt.day <= 31) ? localDt.day : 8;
    uint8_t hour   = (localDt.hour <= 23) ? localDt.hour : 0;
    uint8_t minute = (localDt.minute <= 59) ? localDt.minute : 0;
    uint8_t second = (localDt.second <= 59) ? localDt.second : 0;

    uint32_t totalDays = 0;
    for (uint16_t y = 1970; y < year; y++) {
        totalDays += isLeapYear(y) ? 366UL : 365UL;
    }
    for (uint8_t m = 1; m < month; m++) {
        totalDays += daysInMonth(year, m);
    }
    totalDays += (uint32_t)(day - 1U);

    int64_t localSeconds = (int64_t)totalDays * 86400LL +
                           (int64_t)hour * 3600LL +
                           (int64_t)minute * 60LL +
                           (int64_t)second;

    // Convertir de hora local (UTC-5) a Unix Epoch UTC: UTC = Local - (-5h) = Local + 5h
    int64_t utcSeconds = localSeconds - (int64_t)CALENDAR_TZ_OFFSET_SEC;
    return (utcSeconds > 0) ? (uint32_t)utcSeconds : 0U;
}

WatchDateTime calendar_service_unix_to_datetime(uint32_t unixUtc) {
    WatchDateTime dt = {2026, 1, 1, 0, 0, 0, 0, false};
    if (unixUtc == 0) {
        return dt;
    }

    // Convertir de Unix Epoch UTC a hora local (UTC-5): Local = UTC + (-5h)
    int64_t localSec64 = (int64_t)unixUtc + (int64_t)CALENDAR_TZ_OFFSET_SEC;
    if (localSec64 < 0) {
        localSec64 = 0;
    }
    uint32_t localSec = (uint32_t)localSec64;

    dt.second = (uint8_t)(localSec % 60UL);
    dt.minute = (uint8_t)((localSec / 60UL) % 60UL);
    dt.hour   = (uint8_t)((localSec / 3600UL) % 24UL);

    uint32_t days = localSec / 86400UL;
    uint16_t year = 1970;
    while (true) {
        uint16_t daysInYear = isLeapYear(year) ? 366U : 365U;
        if (days < daysInYear) {
            break;
        }
        days -= daysInYear;
        year++;
    }

    uint8_t month = 1;
    while (month <= 12) {
        uint8_t dim = daysInMonth(year, month);
        if (days < dim) {
            break;
        }
        days -= dim;
        month++;
    }

    dt.year    = year;
    dt.month   = month;
    dt.day     = (uint8_t)(days + 1U);
    dt.weekday = hal_rtc_calc_weekday(dt.year, dt.month, dt.day);
    dt.valid   = true;
    return dt;
}

uint32_t calendar_service_now_unix() {
    WatchDateTime nowDt = hal_rtc_get_time();
    return calendar_service_datetime_to_unix(nowDt);
}

void calendar_service_unix_to_local_hm(uint32_t unixUtc, uint8_t &outHour24, uint8_t &outMinute) {
    WatchDateTime dt = calendar_service_unix_to_datetime(unixUtc);
    outHour24 = dt.hour;
    outMinute = dt.minute;
}

void calendar_service_format_time_12h(uint32_t unixUtc, char *outBuf, size_t outBufSize) {
    if (!outBuf || outBufSize == 0) {
        return;
    }
    uint8_t h24 = 0;
    uint8_t min = 0;
    calendar_service_unix_to_local_hm(unixUtc, h24, min);

    const char *ampm = (h24 >= 12) ? "PM" : "AM";
    uint8_t h12 = h24 % 12;
    if (h12 == 0) {
        h12 = 12;
    }
    snprintf(outBuf, outBufSize, "%02u:%02u %s", h12, min, ampm);
}

// ============================================================================
// 5. GENERADOR DEFENSIVO DE MOCK DATA EJECUTIVA (RELATIVA AL RTC PCF85063)
// ============================================================================

static void populateExecutiveMockData(const char *reasonMsg) {
    uint32_t nowUnix = calendar_service_now_unix();
    // Redondear al minuto exacto para que las horas de inicio luzcan limpias (segundos = 00)
    uint32_t baseMinUnix = (nowUnix / 60UL) * 60UL;

    memset(cachedEvents, 0, sizeof(cachedEvents));

    // Evento 1: Inicia en +20 minutos, duración 45 minutos
    strncpy(cachedEvents[0].id, "mock_ev_01", sizeof(cachedEvents[0].id) - 1);
    strncpy(cachedEvents[0].title, "Reunion de Estrategia", sizeof(cachedEvents[0].title) - 1);
    strncpy(cachedEvents[0].location, "Sala Juntas B", sizeof(cachedEvents[0].location) - 1);
    cachedEvents[0].startUnix = baseMinUnix + (20UL * 60UL);
    cachedEvents[0].endUnix   = cachedEvents[0].startUnix + (45UL * 60UL);
    cachedEvents[0].allDay    = false;
    cachedEvents[0].valid     = true;

    // Evento 2: Inicia en +2 horas (120 min), duración 60 minutos
    strncpy(cachedEvents[1].id, "mock_ev_02", sizeof(cachedEvents[1].id) - 1);
    strncpy(cachedEvents[1].title, "Revision de Arquitectura", sizeof(cachedEvents[1].title) - 1);
    strncpy(cachedEvents[1].location, "Google Meet", sizeof(cachedEvents[1].location) - 1);
    cachedEvents[1].startUnix = baseMinUnix + (120UL * 60UL);
    cachedEvents[1].endUnix   = cachedEvents[1].startUnix + (60UL * 60UL);
    cachedEvents[1].allDay    = false;
    cachedEvents[1].valid     = true;

    // Evento 3: Inicia en +4 horas (240 min), duración 45 minutos
    strncpy(cachedEvents[2].id, "mock_ev_03", sizeof(cachedEvents[2].id) - 1);
    strncpy(cachedEvents[2].title, "Comite Ejecutivo Q4", sizeof(cachedEvents[2].title) - 1);
    strncpy(cachedEvents[2].location, "Torre Central P8", sizeof(cachedEvents[2].location) - 1);
    cachedEvents[2].startUnix = baseMinUnix + (240UL * 60UL);
    cachedEvents[2].endUnix   = cachedEvents[2].startUnix + (45UL * 60UL);
    cachedEvents[2].allDay    = false;
    cachedEvents[2].valid     = true;

    cachedEventCount = 3;
    usingMockData    = true;
    syncVersion++;

    Serial.println("------------------------------------------------------------");
    Serial.printf("[CALENDAR] MODO DEMO (MOCK DATA) ACTIVO: %s\n", reasonMsg ? reasonMsg : "Sin credenciales");
    for (size_t i = 0; i < cachedEventCount; i++) {
        char startStr[16];
        char endStr[16];
        calendar_service_format_time_12h(cachedEvents[i].startUnix, startStr, sizeof(startStr));
        calendar_service_format_time_12h(cachedEvents[i].endUnix, endStr, sizeof(endStr));
        Serial.printf("  * [%u] %s - %s | %s (%s)\n",
                      (unsigned)(i + 1), startStr, endStr,
                      cachedEvents[i].title, cachedEvents[i].location);
    }
    Serial.println("------------------------------------------------------------");
}

// ============================================================================
// 6. CICLO DE SINCRONIZACIÓN WI-FI ULTRA-EFICIENTE + PARSEO EN PSRAM
// ============================================================================

static void shutdownWiFiRadioImmediately() {
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
}

bool calendar_service_sync_now() {
    lastSyncAttemptMs = millis();

    // 1. Verificar si existen secretos reales configurados en wifi_secrets.h
    if (!CALENDAR_HAS_REAL_SECRETS) {
        shutdownWiFiRadioImmediately();
        populateExecutiveMockData("Credenciales de plantilla o sin wifi_secrets.h");
        return false;
    }

    Serial.println("\n============================================================");
    Serial.printf("[CALENDAR] Iniciando rafaga Wi-Fi (Timeout max: %lu ms)...\n", CALENDAR_WIFI_TIMEOUT_MS);
    Serial.printf("[CALENDAR] Conectando a SSID: '%s'\n", WIFI_SSID);

    // 2. Encender radio Wi-Fi en modo estación (cliente) con timeout estricto de 5 segundos
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t wifiStartMs = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - wifiStartMs < CALENDAR_WIFI_TIMEOUT_MS)) {
        delay(100);
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.printf("[CALENDAR] Timeout Wi-Fi (%lu ms). Apagando radio de inmediato.\n",
                      millis() - wifiStartMs);
        shutdownWiFiRadioImmediately();

        if (cachedEventCount == 0 || usingMockData) {
            populateExecutiveMockData("Timeout de conexion Wi-Fi (5s)");
        }
        return false;
    }

    Serial.printf("[CALENDAR] Wi-Fi conectado en %lu ms (IP: %s).\n",
                  millis() - wifiStartMs, WiFi.localIP().toString().c_str());

    // 3. Reservar búfer de descarga en la memoria Octal PSRAM (8 MB)
    const size_t PSRAM_HTTP_BUF_SIZE = 4096;
    char *psramHttpBuffer = static_cast<char *>(
        heap_caps_malloc(PSRAM_HTTP_BUF_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
    );
    if (!psramHttpBuffer) {
        psramHttpBuffer = static_cast<char *>(malloc(PSRAM_HTTP_BUF_SIZE));
    }
    if (!psramHttpBuffer) {
        Serial.println("[CALENDAR] Error critico: sin memoria para bufer HTTPS. Apagando Wi-Fi.");
        shutdownWiFiRadioImmediately();
        return false;
    }
    psramHttpBuffer[0] = '\0';

    // 4. Realizar petición HTTPS GET siguiendo redirección HTTP 302 de Google Apps Script
    int httpCode = -1;
    size_t payloadLen = 0;
    {
        WiFiClientSecure secureClient;
        secureClient.setInsecure(); // Cifrado TLS activo sin almacenar cadena raíz CA pesada en Flash

        HTTPClient http;
        http.setTimeout(5000);
        // OBLIGATORIO: Seguir el desvío HTTP 302 de script.google.com -> script.googleusercontent.com
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

        if (http.begin(secureClient, GCAL_WEBHOOK_URL)) {
            http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
            httpCode = http.GET();

            if (httpCode == HTTP_CODE_OK) {
                String responseBody = http.getString();
                payloadLen = responseBody.length();
                size_t copyLen = (payloadLen < (PSRAM_HTTP_BUF_SIZE - 1))
                                 ? payloadLen
                                 : (PSRAM_HTTP_BUF_SIZE - 1);
                memcpy(psramHttpBuffer, responseBody.c_str(), copyLen);
                psramHttpBuffer[copyLen] = '\0';
            }
            http.end();
        }
    }

    // 5. Apagar la antena Wi-Fi DE INMEDIATO tras cerrar el socket HTTP (antes de parsear)
    shutdownWiFiRadioImmediately();
    Serial.println("[CALENDAR] Radio Wi-Fi apagada (WIFI_OFF) para proteger bateria.");

    if (httpCode != HTTP_CODE_OK || payloadLen == 0) {
        Serial.printf("[CALENDAR] Error HTTP GET (codigo: %d). Conservando/cargando respaldo.\n", httpCode);
        heap_caps_free(psramHttpBuffer);
        if (cachedEventCount == 0) {
            populateExecutiveMockData("Error en respuesta HTTPS de Google Script");
        }
        return false;
    }

    // 6. Parsear el documento JSON alojando sus nodos en la Octal PSRAM (MALLOC_CAP_SPIRAM)
    JsonDocument doc(&psramJsonAllocator);
    DeserializationError jsonErr = deserializeJson(doc, psramHttpBuffer);
    heap_caps_free(psramHttpBuffer);
    psramHttpBuffer = nullptr;

    if (jsonErr) {
        Serial.printf("[CALENDAR] Error al parsear JSON en PSRAM: %s\n", jsonErr.c_str());
        if (cachedEventCount == 0) {
            populateExecutiveMockData("JSON invalido recibido del puente");
        }
        return false;
    }

    const char *statusStr = doc["status"] | "error";
    if (strcmp(statusStr, "ok") != 0) {
        const char *errMsg = doc["message"] | "Estado distinto de ok";
        Serial.printf("[CALENDAR] El puente Google Apps Script reporto error: %s\n", errMsg);
        if (cachedEventCount == 0) {
            populateExecutiveMockData(errMsg);
        }
        return false;
    }

    // 7. Sincronizar opcionalmente el chip RTC PCF85063 con el server_time atómico de Google
    uint32_t serverTimeUtc = doc["server_time"] | 0UL;
    if (serverTimeUtc > 1700000000UL) {
        uint32_t currentRtcUtc = calendar_service_now_unix();
        int64_t diffSec = (int64_t)serverTimeUtc - (int64_t)currentRtcUtc;
        if (diffSec < 0) diffSec = -diffSec;

        if (diffSec > 2) {
            WatchDateTime googleLocalDt = calendar_service_unix_to_datetime(serverTimeUtc);
            if (googleLocalDt.valid && hal_rtc_set_time(googleLocalDt)) {
                Serial.printf("[CALENDAR] RTC PCF85063 ajustado con server_time Google (UTC-5): "
                              "%04u-%02u-%02u %02u:%02u:%02u\n",
                              googleLocalDt.year, googleLocalDt.month, googleLocalDt.day,
                              googleLocalDt.hour, googleLocalDt.minute, googleLocalDt.second);
            }
        }
    }

    // 8. Extraer el arreglo de eventos hacia la estructura compacta en memoria
    memset(cachedEvents, 0, sizeof(cachedEvents));
    cachedEventCount = 0;

    JsonArray eventsArr = doc["events"].as<JsonArray>();
    if (!eventsArr.isNull()) {
        for (JsonObject evObj : eventsArr) {
            if (cachedEventCount >= CALENDAR_MAX_EVENTS) {
                break;
            }

            uint32_t startEpoch = evObj["start"] | 0UL;
            uint32_t endEpoch   = evObj["end"]   | 0UL;
            if (startEpoch == 0 || endEpoch == 0) {
                continue;
            }

            CalendarEvent &slot = cachedEvents[cachedEventCount];
            sanitizeUtf8ToAscii(evObj["id"]    | "gcal_ev", slot.id, sizeof(slot.id));
            sanitizeUtf8ToAscii(evObj["title"] | "Evento",  slot.title, sizeof(slot.title));
            sanitizeUtf8ToAscii(evObj["loc"]   | "",        slot.location, sizeof(slot.location));
            slot.startUnix = startEpoch;
            slot.endUnix   = endEpoch;
            slot.allDay    = evObj["all_day"] | false;
            slot.valid     = true;

            cachedEventCount++;
        }
    }

    usingMockData = false;
    syncVersion++;

    Serial.printf("[CALENDAR] Sincronizacion HTTPS exitosa: %u eventos reales cargados.\n",
                  (unsigned)cachedEventCount);
    for (size_t i = 0; i < cachedEventCount; i++) {
        char startStr[16];
        char endStr[16];
        calendar_service_format_time_12h(cachedEvents[i].startUnix, startStr, sizeof(startStr));
        calendar_service_format_time_12h(cachedEvents[i].endUnix, endStr, sizeof(endStr));
        Serial.printf("  * [%u] %s - %s | %s (%s)\n",
                      (unsigned)(i + 1), startStr, endStr,
                      cachedEvents[i].title, cachedEvents[i].location);
    }
    Serial.println("============================================================\n");
    return true;
}

// ============================================================================
// 7. CONSULTA DEL PRÓXIMO EVENTO Y FUNCIONES PÚBLICAS PARA LA INTERFAZ LVGL
// ============================================================================

void calendar_service_init() {
    if (serviceInitialized) {
        return;
    }
    serviceInitialized = true;
    bootTimestampMs    = millis();
    lastSyncAttemptMs  = millis();

    // Asegurar que la antena Wi-Fi arranque apagada para proteger la batería
    WiFi.mode(WIFI_OFF);

    if (CALENDAR_HAS_REAL_SECRETS) {
        // Cargamos respaldo inicial sin bloquear el encendido de la pantalla AMOLED
        // y programamos la primera sincronización real unos segundos después del arranque.
        populateExecutiveMockData("Respaldo inicial pre-sincronizacion Wi-Fi");
        pendingInitialRealSync = true;
    } else {
        populateExecutiveMockData("Modo Demo (config/wifi_secrets.h no configurado)");
        pendingInitialRealSync = false;
    }
}

void calendar_service_loop() {
    if (!serviceInitialized) {
        calendar_service_init();
    }

    // Si hay credenciales reales, ejecutar la primera sincronización 3 segundos después
    // de arrancar para que la pantalla AMOLED se encienda al instante sin esperas.
    if (pendingInitialRealSync && (millis() - bootTimestampMs >= 3000UL)) {
        pendingInitialRealSync = false;
        calendar_service_sync_now();
        return;
    }

    // Sincronización periódica cada 30 minutos
    if (millis() - lastSyncAttemptMs >= CALENDAR_SYNC_INTERVAL_MS) {
        calendar_service_sync_now();
    }
}

bool calendar_service_get_next_event(CalendarEvent &outEvent,
                                     int32_t &outMinutesUntilStart,
                                     bool &outIsOngoing) {
    if (!serviceInitialized) {
        calendar_service_init();
    }

    uint32_t nowUnix = calendar_service_now_unix();

    // Buscar el primer evento válido que esté en curso o próximo a iniciar
    const CalendarEvent *bestCandidate = nullptr;
    for (size_t i = 0; i < cachedEventCount; i++) {
        const CalendarEvent &ev = cachedEvents[i];
        if (!ev.valid) {
            continue;
        }
        // Si el evento ya terminó (nowUnix >= endUnix), pasar al siguiente
        if (nowUnix >= ev.endUnix) {
            continue;
        }
        if (!bestCandidate || ev.startUnix < bestCandidate->startUnix) {
            bestCandidate = &ev;
        }
    }

    // Si estamos en Modo Demo (Mock Data) y todos los eventos de prueba ya expiraron,
    // regeneramos automáticamente la agenda ejecutiva relativa a la hora actual del RTC.
    if (!bestCandidate && usingMockData) {
        populateExecutiveMockData("Renovacion automatica de ciclo Demo");
        nowUnix = calendar_service_now_unix();
        if (cachedEventCount > 0 && cachedEvents[0].valid) {
            bestCandidate = &cachedEvents[0];
        }
    }

    if (!bestCandidate) {
        memset(&outEvent, 0, sizeof(outEvent));
        outMinutesUntilStart = -1;
        outIsOngoing         = false;
        return false;
    }

    outEvent = *bestCandidate;
    if (nowUnix >= bestCandidate->startUnix && nowUnix < bestCandidate->endUnix) {
        outIsOngoing         = true;
        outMinutesUntilStart = 0;
    } else {
        outIsOngoing = false;
        uint32_t diffSec = bestCandidate->startUnix - nowUnix;
        // División con techo (ceiling) para que 19m 15s restantes se muestre como 20 min
        outMinutesUntilStart = (int32_t)((diffSec + 59UL) / 60UL);
    }

    return true;
}

uint32_t calendar_service_get_sync_version() {
    return syncVersion;
}

size_t calendar_service_get_events(const CalendarEvent **outArray) {
    if (outArray) {
        *outArray = cachedEvents;
    }
    return cachedEventCount;
}

bool calendar_service_is_using_mock() {
    return usingMockData;
}

bool calendar_service_has_real_secrets() {
    return CALENDAR_HAS_REAL_SECRETS;
}
