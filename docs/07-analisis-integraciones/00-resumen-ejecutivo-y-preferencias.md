# 00. Resumen Ejecutivo y Cuestionario de Preferencias del Usuario

> [!IMPORTANT]
> **Documento para lectura humana rápida y toma de decisiones:**
> Este documento resume en formato accesible (3 a 5 minutos de lectura) todo el análisis profundo de integraciones inteligentes posibles para tu reloj (`ESP32-S3-Touch-AMOLED-2.06`).
> Al final de cada sección encontrarás un **cuestionario tipo test** con casillas `[ ]` para que simplemente marques tus gustos y prioridades.
> Los detalles técnicos exhaustivos (para que los agentes IA los implementen sin ambigüedades) se encuentran en los documentos técnicos adjuntos dentro de esta misma carpeta.

---

## 1. El Mapa General: Tus Pasiones Conectadas a la Muñeca

Tu reloj tiene piezas de hardware de nivel profesional: **pantalla AMOLED de $410 \times 502$ px**, **procesador de dos núcleos con 8 MB de RAM extra (PSRAM)**, **Wi-Fi**, **Bluetooth BLE**, **2 micrófonos digitales**, **altavoz con amplificador** y **lector de tarjetas MicroSD**.

```
                       ┌───────────────────────────────────────────────┐
                       │   TU RELOJ INTELIGENTE (ESP32-S3 AMOLED)      │
                       │  - Pantalla AMOLED 410x502 (Negro Puro)       │
                       │  - 2 Micrófonos (ES7210) + Altavoz (ES8311)   │
                       │  - Bluetooth BLE 5.0 + Wi-Fi 2.4 GHz + MicroSD│
                       └───────────────┬───────────────┬───────────────┘
                                       │               │
                     [Bluetooth BLE]   │               │   [Wi-Fi Directo]
                    (Bajísimo consumo) │               │   (Conexión nube)
                                       ▼               ▼
                 ┌───────────────────────────┐   ┌───────────────────────────┐
                 │     TU TELÉFONO MÓVIL     │   │      LA NUBE / APIS       │
                 │ - YouTube Music (Control) │   │ - Google Calendar (API)   │
                 │ - Notificaciones (Avisos) │   │ - Agentes IA en Telegram  │
                 │ - Llamadas (Colgar/Mute)  │   │ - Google Gemini (Voz/IA)  │
                 └───────────────────────────┘   └───────────────────────────┘
```

---

## 2. Resumen de las 4 Grandes Integraciones

### Pilar A: Google Calendar & Timeline Ejecutivo
* **Qué hace:** Tu agenda de Google Calendar se refleja en la pantalla con estética minimalista (fondo negro puro, letras en blanco alabastro y titanio). Muestra el próximo evento y cuánto falta.
* **Función estrella:** 10 minutos antes de una reunión, el reloj vibra discretamente y te da un botón: *"Avisar que llegaré 5 min tarde"*, que dispara un mensaje automático a tu Telegram o WhatsApp.
* **Cómo se conecta:** A través de un pequeño script gratuito en **Google Apps Script** (funciona en los servidores de Google sin instalar nada raro en tu teléfono ni gastar batería).
* 📄 *Documento técnico completo:* [`01-google-calendar-y-timeline.md`](file:///G:/ITM/reloj-inteligente/docs/07-analisis-integraciones/01-google-calendar-y-timeline.md)

### Pilar B: Agente de IA y Telegram en la Muñeca (Voz Bidireccional)
* **Qué hace:** Convierte tu reloj en un "Walkie-Talkie" inteligente. Mantienes pulsado el botón lateral o tocas un icono, hablas al micrófono del reloj y tu bot de Telegram procesa tu petición con Google Gemini.
* **Función estrella:** La respuesta de la IA puede aparecer como texto nítido en la pantalla AMOLED o incluso sonar hablada a través del altavoz integrado del reloj. Además, puedes dictar notas de voz rápidas que se guardan en tus chats de Telegram.
* **Cómo se conecta:** Por Wi-Fi directo a la API de Telegram y la API de Gemini (o mediante un webhook ligero que procesa el audio).
* 📄 *Documento técnico completo:* [`02-agente-ia-telegram-y-voz.md`](file:///G:/ITM/reloj-inteligente/docs/07-analisis-integraciones/02-agente-ia-telegram-y-voz.md)

### Pilar C: YouTube Music, Llamadas y Notificaciones (Bluetooth BLE)
* **Qué hace:** Controla YouTube Music en tu teléfono (Play/Pausa, Siguiente, Anterior, Volumen) y gestiona llamadas entrantes.
* **Función estrella:** Cuando te llaman, ves el nombre en la pantalla AMOLED y puedes tocar `[Silenciar móvil]` o `[Rechazar y enviar mensaje]`.
* **Aclaración honesta:** El reloj no sirve para hablar la llamada tradicional de teléfono por Bluetooth (el procesador tiene Bluetooth BLE moderno, no el Bluetooth clásico de auriculares antiguos), pero sí gestiona la llamada y puede enviar notas de voz por Telegram.
* **Cómo se conecta:** Por Bluetooth BLE usando la app de código abierto **Chronos** (o modo mando directo sin app).
* 📄 *Documento técnico completo:* [`03-youtube-music-llamadas-y-ble.md`](file:///G:/ITM/reloj-inteligente/docs/07-analisis-integraciones/03-youtube-music-llamadas-y-ble.md)

### Pilar D: Audio Offline y Tarjeta MicroSD
* **Qué hace:** Aprovecha la ranura para tarjetas MicroSD que tiene tu placa (`GPIO 1, 2, 3`). Puedes guardar música en MP3, podcasts o audiolibros y reproducirlos directamente por el altavoz sin necesidad de llevar el móvil.
* **Función estrella:** Grabadora de notas de voz offline (graba audios en la MicroSD aunque estés en un sótano sin Wi-Fi ni cobertura).
* 📄 *Documento técnico completo:* [`04-audio-offline-y-microsd.md`](file:///G:/ITM/reloj-inteligente/docs/07-analisis-integraciones/04-audio-offline-y-microsd.md)

---

## 3. Cuestionario de Preferencias del Usuario (Para responder marcando `[x]`)

Copia y pega este bloque en tus respuestas o indícame tus números preferidos:

### Bloque 1: Google Calendar
* **1.1. ¿Cómo prefieres ver tus eventos en la pantalla principal?**
  * [ ] **Opción A (Ultra minimalista):** Solo una línea sutil bajo la hora: *"16:00 · Reunión (en 20m)"*. Al tocarla, se abre el detalle.
  * [ ] **Opción B (Línea de tiempo / Timeline):** Una pantalla dedicada al deslizar hacia la izquierda con los 3 próximos eventos del día en tarjetas titanio.
  * [ ] **Opción C:** Ambas cosas (resumen sutil en la hora + pantalla completa de agenda al deslizar).
* **1.2. Frecuencia de sincronización:**
  * [ ] **Automática:** Se conecta al Wi-Fi 3 segundos cada 30 minutos (óptimo para batería).
  * [ ] **Bajo demanda:** Solo se sincroniza al amanecer o cuando tocas el botón de "Actualizar".

### Bloque 2: Agente de IA en Telegram
* **2.1. ¿Cómo quieres interactuar con la IA?**
  * [ ] **Modo Walkie-Talkie:** Mantener pulsado el botón físico lateral, hablar y soltar.
  * [ ] **Modo Táctil:** Tocar un botón en la pantalla para empezar a grabar y tocar otra vez para enviar.
* **2.2. ¿Cómo prefieres recibir las respuestas de la IA?**
  * [ ] **Solo Texto en Pantalla:** Lectura rápida en la pantalla AMOLED (silencioso y discreto en reuniones).
  * [ ] **Voz por Altavoz:** Que el reloj te hable en voz alta con audio sintetizado.
  * [ ] **Híbrido:** Muestra el texto y reproduce audio solo si mantienes pulsado el altavoz.

### Bloque 3: Control de YouTube Music y Llamadas
* **3.1. Método de conexión al celular:**
  * [ ] **Opción A (Sin instalar nada en el móvil - BLE HID):** El móvil reconoce el reloj como un teclado/mando multimedia Bluetooth. Permite Play/Pausa, Skip y Volumen al instante. (No muestra título de la canción).
  * [ ] **Opción B (Con App Compañera Gratuita - Chronos / Gadgetbridge):** Instalas una app liviana en Android. Permite ver el **Título de la canción**, el **Artista**, ver **quién te llama por teléfono** con botón de silenciar y recibir notificaciones de WhatsApp/Telegram.
* **3.2. ¿Qué notificaciones quieres en la muñeca?**
  * [ ] Solo llamadas entrantes y recordatorios de Google Calendar.
  * [ ] Todo: Llamadas, Calendar, mensajes de Telegram y WhatsApp.

### Bloque 4: Prioridad de Implementación
* **¿En qué orden te gustaría que construyamos estas funciones?**
  * (Ejemplo: *1º Calendar, 2º YouTube Music, 3º Telegram IA, 4º MicroSD*)
  * Tu orden: `________________________________________`

---

## 4. Índice de Documentación Técnica Detallada

Para consultar el análisis exhaustivo con diagramas de flujo, esquemas de conexión, librerías probadas y código fuente:

1. [`docs/07-analisis-integraciones/01-google-calendar-y-timeline.md`](file:///G:/ITM/reloj-inteligente/docs/07-analisis-integraciones/01-google-calendar-y-timeline.md)
2. [`docs/07-analisis-integraciones/02-agente-ia-telegram-y-voz.md`](file:///G:/ITM/reloj-inteligente/docs/07-analisis-integraciones/02-agente-ia-telegram-y-voz.md)
3. [`docs/07-analisis-integraciones/03-youtube-music-llamadas-y-ble.md`](file:///G:/ITM/reloj-inteligente/docs/07-analisis-integraciones/03-youtube-music-llamadas-y-ble.md)
4. [`docs/07-analisis-integraciones/04-audio-offline-y-microsd.md`](file:///G:/ITM/reloj-inteligente/docs/07-analisis-integraciones/04-audio-offline-y-microsd.md)
