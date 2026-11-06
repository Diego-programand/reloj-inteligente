# 01. Integración Inteligente: Google Calendar y Timeline Ejecutivo

Documento de ingeniería y especificación de producto para la sincronización y visualización de **Google Calendar** en el reloj inteligente **ESP32-S3-Touch-AMOLED-2.06**.

---

## 1. Visión General (Parte Humana / Accesible)

El objetivo es tener tu agenda de trabajo de Google en la muñeca sin fricción. No queremos una copia caótica del móvil, sino una **línea de tiempo ejecutiva de alta gama**:
* **En la esfera principal:** Bajo la hora grande, un texto sobrio en tono titanio (`#71717A`) que te dice exactamente qué viene después (ejemplo: `16:30 · Presentación Q4 (en 18 min)`).
* **Al deslizar a la izquierda:** Una pantalla completa con la agenda del día: las próximas 3 a 5 reuniones en tarjetas minimalistas con fondo gris oscuro (`#18181B`) sobre el negro absoluto de la pantalla AMOLED.
* **Avisos inteligentes:** 10 minutos antes de que empiece un evento, el reloj emite una alerta sutil y te muestra opciones de un toque:
  1. `[Ver detalles]`: Ubicación, notas y asistentes.
  2. `[Avisar retraso]`: Envía un mensaje instantáneo a través de tu bot de Telegram o tu móvil notificando al equipo que llegarás 5 minutos tarde.

```
+------------------------------------------+
|  15:42           JUE 06 NOV        100%  |
|                                          |
|                1 5 : 4 2                 |
|                                          |
|     16:00 · Comité de Innovación         |
|     (Comienza en 18 minutos)             |
|                                          |
|      [ Avisar +5m ]      [ Ver Sala ]    |
+------------------------------------------+
```

---

## 2. Análisis Técnico Comparativo de Arquitecturas

Para conectar el chip `ESP32-S3` con Google Calendar existen 3 caminos posibles:

| Criterio | Método 1: Google Apps Script Bridge (Recomendado) | Método 2: Google Calendar REST API v3 Directo | Método 3: Vía Bluetooth (Android Calendar Push) |
| :--- | :--- | :--- | :--- |
| **¿Cómo funciona?** | Un script gratuito de 20 líneas en Google Cloud consulta tu calendario y le entrega al reloj un JSON limpio de 400 bytes. | El ESP32 se autentica directamente con los servidores de Google mediante tokens OAuth 2.0 y lee la API REST v3. | Una app en Android lee el calendario del móvil y lo envía por Bluetooth BLE al reloj. |
| **Complejidad de código en el reloj** | **Mínima:** Una simple petición HTTPS GET estándar con `WiFiClientSecure`. | **Muy Alta:** Requiere manejar renovación de tokens OAuth, certificados SSL pesados y parsear JSONs de más de 15 KB. | **Media:** Depende de una app en segundo plano en el móvil siempre activa. |
| **Consumo de Batería (Reloj)** | **Óptimo:** Wi-Fi encendido solo 2.2 segundos cada 30–60 min (`~0.05 mAh` por sincronización). | **Alto:** Las peticiones OAuth toman de 5 a 8 segundos con la radio Wi-Fi a máxima potencia. | **Excelente:** No usa Wi-Fi, solo micro-paquetes BLE. |
| **Independencia del celular** | **Total:** El reloj se sincroniza solo si hay Wi-Fi (casa/oficina), sin depender de que el móvil esté cerca. | **Total:** Independiente del móvil. | **Nula:** Requiere que el teléfono esté a menos de 5 metros conectado por BLE. |
| **Privacidad y Seguridad** | **100% Tuya:** El script corre dentro de tu propia cuenta de Google (`script.google.com`); no hay servidores de terceros. | **100% Tuya:** Conexión directa a Google. | **Depende:** La app de Android requiere permisos de lectura del sistema. |

---

## 3. Especificación Técnica de la Arquitectura Recomendada (Google Apps Script Bridge)

### 3.1. Código del Webhook en Google Cloud (`Code.gs`)
Se publica como una aplicación web en `script.google.com` con permiso de ejecución para tu usuario y acceso mediante token secreto por parámetro:

```javascript
function doGet(e) {
  // Token de seguridad para evitar accesos no autorizados
  const SECRET_TOKEN = "MI_TOKEN_SUPER_SECRETO_RELOJ_2026";
  if (!e.parameter.key || e.parameter.key !== SECRET_TOKEN) {
    return ContentService.createTextOutput(JSON.stringify({ error: "Unauthorized" }))
                         .setMimeType(ContentService.MimeType.JSON);
  }

  try {
    const calendar = CalendarApp.getDefaultCalendar();
    const now = new Date();
    // Consultar eventos desde este momento hasta el final del día de mañana (36 horas)
    const future = new Date(now.getTime() + (36 * 60 * 60 * 1000));
    const events = calendar.getEvents(now, future);

    const payload = events.slice(0, 5).map(evt => {
      return {
        id: evt.getId().substring(0, 12),
        title: evt.getTitle(),
        start: Math.floor(evt.getStartTime().getTime() / 1000), // Epoch Unix en segundos
        end: Math.floor(evt.getEndTime().getTime() / 1000),
        loc: evt.getLocation() || "",
        all_day: evt.isAllDayEvent()
      };
    });

    return ContentService.createTextOutput(JSON.stringify({
      status: "ok",
      count: payload.length,
      server_time: Math.floor(now.getTime() / 1000),
      events: payload
    })).setMimeType(ContentService.MimeType.JSON);

  } catch (err) {
    return ContentService.createTextOutput(JSON.stringify({ error: err.toString() }))
                         .setMimeType(ContentService.MimeType.JSON);
  }
}
```

### 3.2. Formato del JSON Optimizado para el ESP32
El reloj recibe exactamente lo que necesita en menos de 500 bytes:
```json
{
  "status": "ok",
  "count": 2,
  "server_time": 1762462800,
  "events": [
    {
      "id": "evt_98a7bc",
      "title": "Reunión de Estrategia",
      "start": 1762464600,
      "end": 1762468200,
      "loc": "Google Meet",
      "all_day": false
    },
    {
      "id": "evt_34f1de",
      "title": "Cena Familiar",
      "start": 1762479000,
      "end": 1762482600,
      "loc": "Restaurante Central",
      "all_day": false
    }
  ]
}
```

---

## 4. Estructura de Software en el Firmware del Reloj

Para mantener la arquitectura limpia y modular del proyecto:

```
firmware/
├── services/
│   ├── calendar_service.h    // Estructuras de datos, caché de eventos y alarmas
│   └── calendar_service.cpp  // Petición HTTPS, seguimiento de redirecciones (302) y parseo JSON
└── ui/
    └── screens/
        ├── scr_timeline.h    // Pantalla de lista completa de eventos en LVGL
        └── scr_timeline.cpp  // Widgets tipo tarjeta minimalista en blanco y titanio
```

### Estructura en C++ del Evento:
```cpp
struct CalendarEvent {
    char id[16];
    char title[48];
    char location[32];
    uint32_t startTimeUnix;
    uint32_t endTimeUnix;
    bool isAllDay;
    bool notified10Min;
};
```

### Manejo de Redirecciones HTTP 302 de Google:
Google Apps Script responde siempre con una redirección HTTP 302 a `script.googleusercontent.com`.
* **Regla estricta:** En el ESP32 se debe configurar `http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS)` con `WiFiClientSecure::setInsecure()` para resolver la redirección en una sola transacción sin agotar la memoria dinámica.

---

## 5. Presupuesto de Memoria y Batería

* **RAM necesaria:** ~`4 KB` para el buffer de `ArduinoJson v6/v7` ubicado en PSRAM (`MALLOC_CAP_SPIRAM`).
* **Flash:** ~`18 KB` adicionales de código compilado.
* **Ciclo de Energía:**
  1. Reloj en reposo: Wi-Fi apagado (`radio OFF`). Consumo base ~`12 mA` con pantalla encendida o ~`0.8 mA` en suspensión ligera (*light sleep*).
  2. Disparo de sincronización (cada 30 min):
     * Despertar Wi-Fi: ~200 ms (`~110 mA`).
     * Conexión a router conocido: ~800 ms.
     * Petición HTTPS y descarga de 450 bytes: ~1100 ms.
     * Parseo y guardado en RAM: ~50 ms.
     * Apagado de radio Wi-Fi (`WiFi.disconnect(true)` / `WiFi.mode(WIFI_OFF)`).
  3. **Energía total consumida por sync:** $110\text{ mA} \times \frac{2.15\text{ s}}{3600\text{ s}} \approx 0.065\text{ mAh}$.
  4. En una batería típica de smartwatch de `400 mAh`, realizar 24 sincronizaciones al día consume **menos del 0.4% de la batería total diaria**.

---

## 6. Recursos y Librerías Evaluadas

* **`bblanchon/ArduinoJson` (v6.21.3 o v7):** Estándar de la industria en microcontroladores. Ultrarrápida y permite alocar memoria en la Octal PSRAM sin fragmentar la SRAM interna.
* **`WiFiClientSecure` + `HTTPClient`:** Componentes nativos de ESP-IDF / Arduino ESP32.
* **Google Apps Script Documentation:** [Google Developers Calendar Service](https://developers.google.com/apps-script/reference/calendar).
