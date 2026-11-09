# Bitácora Técnica de Ingeniería

Este documento funciona como nuestro **diario de laboratorio**. Cada vez que probemos algo en el reloj, modifiquemos el firmware o resolvamos un problema, añadiremos una entrada cronológica indicando:
1. **Qué se hizo.**
2. **Cómo se verificó.**
3. **Cómo revertirlo si algo falla.**

---

## Entrada #001 — 2026-11-05: Recepción, Conexión e Inspección Profunda del Hardware

* **Autor:** Usuario + Antigravity
* **Estado del Hardware:** Virgen de fábrica (Sin modificaciones en Flash).
* **Acciones Realizadas:**
  1. Se conectó la placa recién comprada en AliExpress al puerto USB de la placa base (`Port_#0010.Hub_#0001`).
  2. Windows la reconoció como un dispositivo compuesto Espressif (`USB\VID_303A&PID_1001`, MAC `B8:1F:3F:D8:2D:24`) asignándole el puerto **`COM5`**.
  3. Mediante lectura no destructiva del puerto serie `COM5` a `115200` baudios durante el reinicio por RTS, se capturó el log completo del bootloader ESP-IDF v5.5.1 y de la aplicación de fábrica (`phone_s3_box_3` / `ESP-Brookesia phone demo`).
  4. Se instaló `esptool v5.6.0` y se ejecutó `python -m esptool --port COM5 flash_id` para confirmar el silicio exacto.
* **Hallazgos Clave:**
  * Aunque el anuncio de AliExpress mencionaba tanto `ESP32-C6` como `ESP32-S3`, el chip real montado es el **ESP32-S3R8** (Dual-Core 240 MHz) acompañado de **8 MB de Octal PSRAM (`AP_3v3`)** y **32 MB de Flash Quad SPI (`c8:4019`)**.
  * Se identificó una restricción crítica de diseño en la placa: el pin **`GPIO 35`** está conectado a la salida `IRQ` del chip de batería `AXP2101`, pero también forma parte del bus interno de la memoria Octal PSRAM (`SPIDQS`). Queda documentado como **regla estricta** no configurar nunca `GPIO 35` como entrada/interrupción en nuestro código.
* **Cómo Revertir:**
  * No se ha modificado un solo byte de la memoria Flash del reloj todavía; el dispositivo conserva el 100% de su programación original.
* **Próximo Paso:**
  * Ejecutar el script `.\scripts\backup_flash_32mb.ps1` para extraer la imagen completa de 32 MB (`backups\factory_backup_32MB.bin`) antes de subir nuestro primer programa de la Fase 1.

---

## Entrada #002 — 2026-11-05: Respaldo Íntegro de Fábrica (32 MB Flash) y Cierre de Fase 0

* **Fase del Roadmap:** Fase 0 (Completada)
* **Archivos Modificados:** `backups/factory_backup_32MB.bin` (local), `backups/factory_backup_32MB.bin.sha256`, `backups/README_BACKUPS.md`, `docs/06-roadmap-y-fases.md`
* **Qué se hizo:**
  * Se ejecutó `powershell -ExecutionPolicy Bypass -File .\scripts\backup_flash_32mb.ps1` con el reloj conectado en `COM5` a `921600` baudios.
  * `esptool v5.6.0` leyó los `33,554,432` bytes (`0x00000000` a `0x02000000`) en 177.9 segundos (1509.3 kbit/s) y reinició el reloj automáticamente vía RTS.
  * Se verificó y registró el hash SHA256 de la imagen extraída: `3883DB5C407AD1ABD9B60942B42ABF30636206DDA3402ACA5D38960B40214D30`.
* **Resultado / Verificación en COM5:**
  * Archivo `backups/factory_backup_32MB.bin` creado con tamaño exacto de `33,554,432` bytes y reloj funcionando con normalidad tras el reinicio.
* **Cómo Revertir (Rollback):**
  * En cualquier fase futura, para volver al estado exacto de fábrica basta con ejecutar:
    `powershell -ExecutionPolicy Bypass -File .\scripts\restore_flash_32mb.ps1 -BackupFile "G:\ITM\reloj-inteligente\backups\factory_backup_32MB.bin"`

---

## Entrada #003 — 2026-11-05: Fase 1 Completada — Primer Firmware Propio, Escáner I2C (6/6 Chips) y Control de Energía `AXP2101`

* **Fase del Roadmap:** Fase 1 (Completada)
* **Archivos Modificados:** `platformio.ini`, `firmware/hal/hal_power.h`, `firmware/hal/hal_power.cpp`, `firmware/main.cpp`
* **Qué se hizo:**
  1. Se configuró **PlatformIO** (`platformio.ini`) con la plataforma `espressif32 @ 6.9.0`, memoria `qio_opi` (32 MB QIO Flash + 8 MB Octal PSRAM), banderas `ARDUINO_USB_MODE=1` y `ARDUINO_USB_CDC_ON_BOOT=1`, y librería `XPowersLib @ 0.2.9`.
  2. Se implementó el módulo `hal_power` para inicializar el PMIC **`AXP2101`** (`0x34`) por I2C (`SDA=15`, `SCL=14`), encendiendo los reguladores `ALDO1..4`, `BLDO1..2` y `DLDO1..2` a `3300 mV`, habilitando el ADC de telemetría de batería y configurando la lectura del botón `PWR` por *polling* I2C (sin tocar jamás `GPIO 35`).
  3. Se descubrió y documentó que el controlador táctil **`FT3168`** (`0x38`) requiere habilitar los rieles `DLDO1/2` y aplicar un pulso de reset en `GPIO 9` (`LOW` 50 ms -> `HIGH` + espera de 300 ms) para despertar y contestar en el bus I2C.
  4. Se implementó `firmware/main.cpp` con reporte de sistema bajo demanda (tecla `'i'`), escáner I2C con identificación de chips, telemetría periódica de batería y detección en tiempo real de los botones `PWR` y `BOOT`.
* **Resultado / Verificación en COM5:**
  * Salida real verificada en `COM5`:
    * CPU: `ESP32-S3 @ 240 MHz (2 nucleos)`, Flash: `32 MB`, PSRAM: `8189 KB (~7 MB Octal PSRAM activa)`.
    * Escáner I2C detectó los **6 chips esperados**: `0x18` (`ES8311`), `0x34` (`AXP2101`), `0x38` (`FT3168`), `0x40` (`ES7210`), `0x51` (`PCF85063`), `0x6B` (`QMI8658`).
    * Telemetría en vivo: `Bateria: 4110 mV (100%) | Cargando: NO | USB (VBUS): 4986 mV | Sistema: 4334 mV`.
* **Cómo Revertir (Rollback):**
  * Para volver a la imagen de fábrica: `powershell -ExecutionPolicy Bypass -File .\scripts\restore_flash_32mb.ps1 -BackupFile "G:\ITM\reloj-inteligente\backups\factory_backup_32MB.bin"`

---

## Entrada #004 — 2026-11-05: Fase 2A — Driver QSPI de Pantalla AMOLED (`CO5300`/`SH8601`) y Panel Táctil (`FT3168`)

* **Fase del Roadmap:** Fase 2 (Tareas 1, 2 y 3 completadas)
* **Archivos Modificados:** `firmware/hal/hal_display.h`, `firmware/hal/hal_display.cpp`, `firmware/hal/hal_touch.h`, `firmware/hal/hal_touch.cpp`, `firmware/main.cpp`
* **Qué se hizo:**
  1. **Ingeniería inversa de la tabla de inicialización de fábrica:** Se analizó el binario original `backups/factory_backup_32MB.bin` en el offset `0x2720C4` y se extrajo la tabla exacta de comandos `sh8601_lcd_init_cmd_t`. Se confirmó que la ventana activa de $410 \times 502$ píxeles utiliza un **desplazamiento horizontal (`LCD_COL_OFFSET`) de `0x0016` (`22` píxeles)** (`CASET 0x2A`: `0x0016..0x01AF`, `RASET 0x2B`: `0x0000..0x01F5`).
  2. **Implementación de `hal_display` (QSPI nativo a 40 MHz con DMA):** Se programó el bus `SPI2_HOST` en modo Quad-SPI de 4 líneas (`GPIO 4, 5, 6, 7`, `SCK=11`, `CS=12`, `RST=8`, `TE=13`) usando `LCD_OPCODE_WRITE_CMD = 0x02` y `LCD_OPCODE_WRITE_COLOR = 0x32` con conversión rápida en buffer DMA y control de brillo digital (`0x51`).
  3. **Implementación de `hal_touch` (`FT3168` en `0x38`):** Se implementó la lectura de coordenadas $(X, Y)$ consultando primero el pin físico de interrupción táctil `WATCH_TP_INT_PIN` (`GPIO 38 == LOW`) para evitar errores I2C (`NACK`) cuando el controlador táctil entra en reposo sin dedos en pantalla.
  4. **Lienzo interactivo de prueba en `main.cpp`:** Dibuja barras de color puro RGBW, mira central, trazo en vivo bajo el dedo, cambio de color con el botón `BOOT` y cambio de nivel de brillo (`100% -> 35% -> 8% -> 0%`) con el botón `PWR`.
* **Resultado / Verificación en COM5:**
  * Inicialización limpia de `hal_display` y `hal_touch` sin errores en el bus I2C (`Brillo LCD: 220/255`).
* **Cómo Revertir (Rollback):**
  * `git checkout a8163d9` para volver al cierre de la Fase 1, o `restore_flash_32mb.ps1` para volver a fábrica.

---

## Entrada #005 — 2026-11-05: Fase 2B Completada — Corrección de Ventana Vertical `CO5300` e Integración de `LVGL` en Octal PSRAM

* **Fase del Roadmap:** Fase 2 (Completada al 100%)
* **Archivos Modificados:** `platformio.ini`, `firmware/lv_conf.h`, `firmware/hal/hal_display.cpp`, `firmware/ui/ui_manager.h`, `firmware/ui/ui_manager.cpp`, `firmware/main.cpp`
* **Qué se hizo:**
  1. **Corrección de barrido por franjas en `hal_display.cpp`:** Se detectó que al liberar `CS` (`GPIO 12`) entre transacciones DMA grandes, el controlador `CO5300` reiniciaba el puntero de fila si se usaba `0x3C` (`RAMWRC`). Se corrigió enviando la ventana vertical exacta (`0x2B` `RASET`: `ys..ye`) y `0x2C` (`RAMWR`) por cada tira de 30 filas (`24,600` bytes < límite DMA de 32 KB), logrando un borrado y renderizado de pantalla completa instantáneo.
  2. **Integración de `LVGL` (`v8.4.0`) en Octal PSRAM:** Se creó `firmware/lv_conf.h` y el módulo `ui_manager` reservando dos *buffers* de media pantalla (`410 × 251 × 2` bytes cada uno = `~402 KB` totales) en la **Octal PSRAM** (`MALLOC_CAP_SPIRAM`), manteniendo el uso de SRAM interna en apenas `6.4%` y dejando `7,787 KB` libres en PSRAM.
  3. **Interfaz interactiva deslizable (`lv_tileview`):**
     * **Pantalla 1:** Tarjeta de batería en tiempo real (`%`, `mV`, estado USB), barra deslizante táctil (*slider*) que regula el brillo AMOLED en vivo, y monitor en pantalla de coordenadas táctiles $(X, Y)$ y botones físicos (`BOOT` y `PWR`).
     * **Pantalla 2 (Swipe a la izquierda):** Anillo interactivo (`lv_arc`) y botón contador de toques a 60 FPS.
  4. **Alineación de píxeles pares (`lvgl_rounder_cb`) y QSPI a 80 MHz:** Al probar actualizaciones parciales de widgets (como arrastrar el *slider* o hacer *swipe*), se observó deformación/rasgado diagonal porque el controlador `CO5300` requiere que toda ventana de memoria (`CASET`/`RASET`) inicie en coordenada par (`0, 2, 4...`) y tenga ancho/alto par. Se registró `dispDrv.rounder_cb = lvgl_rounder_cb` en `ui_manager.cpp` y se elevó el reloj QSPI a **80 MHz**.
* **Resultado / Verificación en COM5:**
  * Salida verificada en `COM5`: `Memoria PSRAM: 8189 KB total (7787 KB libres tras buffers LVGL) | Motor Grafico: LVGL v8.3 activo en 410x502 QSPI`.
* **Cómo Revertir (Rollback):**
  * `git checkout 5742552` para volver al lienzo de dibujo directo de la Fase 2A.

---

## Entrada #006 — 2026-11-05: Fase 3 Completada — Reloj de Tiempo Real (`PCF85063`), Sincronización Horaria y Primera Watchface a Medida

* **Fase del Roadmap:** Fase 3 (Completada)
* **Archivos Modificados:** `firmware/lv_conf.h`, `firmware/hal/hal_rtc.h`, `firmware/hal/hal_rtc.cpp`, `firmware/services/time_sync.h`, `firmware/services/time_sync.cpp`, `firmware/ui/screens/scr_watchface.h`, `firmware/ui/screens/scr_watchface.cpp`, `firmware/ui/ui_manager.h`, `firmware/ui/ui_manager.cpp`, `firmware/main.cpp`, `scripts/monitor_serial.ps1`
* **Qué se hizo:**
  1. **Driver `hal_rtc` (`PCF85063` en `0x51`):** Se implementó la lectura y escritura en ráfaga de los 7 registros de tiempo (`0x04` a `0x0A`) con conversión bidireccional **BCD** (*Binary-Coded Decimal*), limpieza del bit `OS` (*Oscillator Stop*) y cálculo automático del día de la semana mediante el algoritmo de Sakamoto.
  2. **Servicio `time_sync` (`firmware/services/time_sync.*`):** Se implementó triple vía para ajustar la hora del chip `PCF85063`:
     * **Sincronización por USB Serie (`COM5`):** Comando `TYYYY-MM-DD HH:MM:SS` (y parámetro `-SyncTime` en `scripts/monitor_serial.ps1` para copiar en 1 clic la hora exacta de Windows al reloj).
     * **Sincronización por Wi-Fi NTP (`UTC-5`):** Función `time_sync_from_wifi_ntp()` lista para consultar `pool.ntp.org` y `time.google.com` en segundo plano.
     * **Ajuste manual en pantalla:** Botones táctiles `+1 Hora`, `+1 Min` y `Seg = 00` en la pantalla secundaria de Control Rápido.
  3. **Diseño de nuestra primera esfera de reloj (`scr_watchface`):**
     * Se creó el generador [`scripts/generate_watch_font.py`](file:///G:/ITM/reloj-inteligente/scripts/generate_watch_font.py) y la fuente nativa anti-aliased de 4-bpp **`font_watch_time_108`** (`108 pt`, tabular, `290 px` de ancho total en pantalla) en [`firmware/ui/fonts/font_watch_time_108.c`](file:///G:/ITM/reloj-inteligente/firmware/ui/fonts/font_watch_time_108.c), superando el límite de `48 pt` de las fuentes internas de LVGL, y se subieron los textos secundarios a `24 pt` / `28 pt`.
     * **Estética minimalista, formal y exclusiva (tonos neutros):** A petición del usuario, se eliminaron por completo las tarjetas (`cards`), barras de estado, textos de guía y colores neón de la Watchface principal. Sobre un fondo negro puro `#000000` (que apaga los píxeles AMOLED para fundirse con el bisel del reloj), se muestra únicamente el día de la semana en tono titanio (`24 pt`, `#8E8E93`), la hora principal gigante en formato **12 horas (`108 pt`) con indicador `AM`/`PM` (`28 pt`)** en blanco alabastro (`#F4F4F5`) y plata (`#A1A1AA`), una línea divisoria en grafito (`#27272A`) y la fecha (`24 pt`, `#A1A1AA`).
     * **Encendido y apagado de pantalla con botón `PWR`:** Una pulsación corta del botón lateral `PWR` apaga completamente el panel AMOLED (`0x28` Display OFF + `0x10` Sleep IN) y bloquea la lectura táctil para evitar toques accidentales en el bolsillo; otra pulsación corta despierta el panel (`0x11` Sleep OUT + `0x29` Display ON) con la hora ya actualizada.
     * **Eliminación del quiebre central en transiciones (*Screen Tearing* en fila 251):** Al deslizar entre pantallas se notaba un corte horizontal exactamente a mitad de pantalla porque el *buffer* de LVGL medía `WATCH_LCD_HEIGHT / 2` (`251` filas), obligando a enviar la mitad superior (`0..250`), hacer una pausa para renderizar la mitad inferior (`251..501`) y enviarla a destiempo del barrido del panel. Se solucionó ampliando los dos *buffers* en Octal PSRAM a **pantalla completa (`410 × 502` = `502` filas completas, `~804 KB` totales de los `8,192 KB` disponibles)**, activando `LV_COLOR_16_SWAP = 1` en `lv_conf.h` para copiar por DMA con `memcpy` directo en bloques de 38 líneas (`31,160` bytes < `32,768` bytes límite DMA) y sincronizando con el pulso de hardware `TE` (*Tearing Effect* / V-Sync en `GPIO 13`).
     * **Eficiencia energética máxima:** La pantalla permanece 100% estática durante 59 de cada 60 segundos y solo redibuja cuando cambia el minuto, el día o el formato (`24H` vs `12H`).
     * El panel secundario de Ajustes (`ui_manager.cpp`) también se unificó bajo la misma paleta monocromática de grafito y plata cepillada con `pad_all = 0` y sin barras de desplazamiento visibles.
* **Resultado / Verificación en COM5:**
  * Salida verificada en `COM5`:
    * `[TIME_SYNC] Hora actualizada en chip RTC PCF85063: 2026-11-05 16:54:17`
    * `[RELOJ RTC] 2026-11-05 16:54:22 | Bateria: 4108 mV (100%) | Brillo: 220/255`
* **Cómo Revertir (Rollback):**
  * `git checkout 02e4e9f` para volver al cierre de la Fase 2.

---

## Entrada #007 — 2026-11-06: Fase 4 y Simplificación de Ajustes — Tap-to-Wake por Interrupción (`GPIO 38`), Formato 12H Fijo y Panel Brillo/Volumen/Batería

* **Fase del Roadmap:** Fase 4 / Fase 5 (UI y Energía de Pantalla)
* **Archivos Modificados:** `firmware/hal/hal_touch.cpp`, `firmware/hal/hal_display.cpp`, `firmware/services/power_manager.h`, `firmware/services/power_manager.cpp`, `firmware/ui/screens/scr_watchface.cpp`, `firmware/ui/ui_manager.h`, `firmware/ui/ui_manager.cpp`, `firmware/main.cpp`
* **Qué se hizo:**
  1. **Activación por toque al primer intento (*Tap-to-Wake* con interrupción en `GPIO 38`):**
     * Se configuró `attachInterrupt(..., FALLING)` en el pin `WATCH_TP_INT_PIN` (`GPIO 38`) dentro de `hal_touch.cpp` para capturar en hardware cualquier toque breve sobre el cristal `FT3168`, despertando la pantalla en el primer toque sin necesidad de pulsar dos veces.
     * Se desacopló el encendido por movimiento del `QMI8658` por decisión del usuario, dejando únicamente el encendido por toque en pantalla y botón `PWR`, más el apagado automático tras 20 segundos de inactividad.
  2. **Corrección de pantalla blanca al despertar (`hal_display.cpp`):**
     * Se eliminó el comando `0x10` (`Sleep IN`) / `0x11` (`Sleep OUT`) en `hal_display_set_power()`, ya que al despertar rápidamente desconfiguraba la bomba de carga interna del controlador `CO5300` dejando el panel en blanco. Ahora se apaga la matriz AMOLED con brillo `0` (`0x51`) + `Display OFF` (`0x28`) y se restaura con `Display ON` (`0x29`).
  3. **Formato exclusivo de 12 horas (`scr_watchface.cpp`):**
     * Se eliminó el evento de clic que alternaba al formato de 24 horas (hora militar), dejando fijo el formato de 12 horas con indicador `AM`/`PM`.
  4. **Simplificación de la pantalla de Ajustes (`ui_manager.cpp`):**
     * Se eliminaron los botones de ajuste manual de hora y los datos técnicos (`mV`, estado USB).
     * Se dejaron únicamente tres bloques minimalistas y simétricos: **Brillo** (`%` y control deslizante), **Volumen** (`%` y control deslizante conectado al registro `0x32` del DAC `ES8311` en `0x18`) y **Batería** (`%` y barra de carga).
* **Resultado / Verificación en COM5:**
  * Compilación limpia (`RAM: 6.6%`, `Flash: 19.1%`) y carga verificada en `COM5`.
* **Cómo Revertir (Rollback):**
  * `git checkout c6d24f4` para volver al cierre de la Fase 3.

---

## Entrada #008 — 2026-11-06: Análisis Profundo de Integraciones Inteligentes (Google Calendar, Telegram IA, YouTube Music y MicroSD)

* **Fase del Roadmap:** Fase 6 (Planificación y Especificación de Integraciones)
* **Archivos Modificados:**
  * `docs/07-analisis-integraciones/00-resumen-ejecutivo-y-preferencias.md`
  * `docs/07-analisis-integraciones/01-google-calendar-y-timeline.md`
  * `docs/07-analisis-integraciones/02-agente-ia-telegram-y-voz.md`
  * `docs/07-analisis-integraciones/03-youtube-music-llamadas-y-ble.md`
  * `docs/07-analisis-integraciones/04-audio-offline-y-microsd.md`
  * `docs/06-roadmap-y-fases.md`
* **Qué se hizo:**
  1. Se completó un estudio de viabilidad técnica y búsqueda de librerías de código abierto para las 4 áreas deseadas por el usuario:
     * **Google Calendar:** Diseño de puente ligero con Google Apps Script (JSON de ~450 bytes) sin sobrecarga de OAuth 2.0 en el reloj, sincronización en 2.2 s por Wi-Fi y timeline minimalista en LVGL.
     * **Agente IA en Telegram con Voz:** Integración del códec `ES8311` y los micrófonos digitales `ES7210` sobre I2S con 8 MB de Octal PSRAM para modo Walkie-Talkie, dictado de notas y respuestas de voz/texto con Google Gemini.
     * **YouTube Music, Llamadas y Notificaciones:** Evaluación de modos BLE. Selección de la app de código abierto `Chronos` (`fbiego/chronos-esp32` con NimBLE) para metadatos de canciones en vivo, control multimedia, visualización de llamada entrante con silenciador y notificaciones de Telegram/WhatsApp.
     * **Audio Offline en MicroSD:** Aprovechamiento del controlador hardware SDMMC de 1 bit (`GPIO 1, 2, 3`) con `ESP32-audioI2S` para reproducción local de MP3/podcasts y grabadora de notas de voz offline.
  2. Se redactó un cuestionario de preferencias humanas con casillas `[ ]` en `docs/07-analisis-integraciones/00-resumen-ejecutivo-y-preferencias.md` para que el usuario defina sus gustos sin sobrecarga de lectura técnica.
* **Resultado / Verificación:**
  * Documentación completa, modular y estructurada para futuros agentes de IA e implementación paso a paso.
* **Cómo Revertir (Rollback):**
  * `git checkout HEAD~1` para retirar la carpeta de análisis.

---

## Entrada #009 — 2026-11-08: Paso 1 (Fase 6) — Integración de Google Calendar, Puente Google Apps Script y Modal Ejecutivo en Watchface

* **Fase del Roadmap:** Fase 6 — Pilar 1 (Google Calendar & Timeline Ejecutivo — Completado)
* **Archivos Creados / Modificados:**
  * `tools/google-apps-script/calendar_bridge.gs` (nuevo)
  * `tools/google-apps-script/README_SETUP.md` (nuevo)
  * `firmware/config/wifi_secrets.example.h` (nuevo)
  * `.gitignore` (modificado — exclusión de `firmware/config/wifi_secrets.h`)
  * `platformio.ini` (modificado — incorporación de `bblanchon/ArduinoJson @ ^7.2.0`)
  * `firmware/services/calendar_service.h` (nuevo)
  * `firmware/services/calendar_service.cpp` (nuevo)
  * `firmware/ui/screens/scr_watchface.h` (modificado)
  * `firmware/ui/screens/scr_watchface.cpp` (modificado)
  * `firmware/main.cpp` (modificado)
  * `docs/06-roadmap-y-fases.md` (modificado)
* **Qué se hizo:**
  1. **Puente en la nube (`tools/google-apps-script/calendar_bridge.gs`) y Gestión Segura de Secretos:**
     * Se creó el script de Google Apps Script `doGet(e)` que actúa como intermediario ligero ("recepcionista privado" en la nube): valida el token `?key=...`, consulta `CalendarApp.getDefaultCalendar()` para las próximas 36 horas y empaqueta hasta 5 eventos en un JSON ultracompacto (< 500 bytes) con timestamps Unix Epoch (`start`, `end`, `server_time`).
     * Se creó la plantilla `firmware/config/wifi_secrets.example.h` y se protegió `firmware/config/wifi_secrets.h` en `.gitignore` para evitar exponer contraseñas Wi-Fi o URLs de webhook en el repositorio.
  2. **Servicio Embebido de Calendario (`firmware/services/calendar_service.*`):**
     * **Ráfagas Wi-Fi de Ultra-Bajo Consumo (*Burst Wi-Fi*):** El módulo Wi-Fi permanece apagado (`WIFI_OFF`) el 99.7% del tiempo; cada 30 minutos (o al enviar `'c'` por `COM5`) se activa con timeout estricto de 5 segundos (`CALENDAR_WIFI_TIMEOUT_MS`), sigue redirecciones HTTP 302 de Google (`HTTPC_STRICT_FOLLOW_REDIRECTS`), descarga el JSON y apaga la antena (`WiFi.disconnect(true, true); WiFi.mode(WIFI_OFF)`) de inmediato.
     * **Cero Fragmentación en SRAM (Parseo en Octal PSRAM):** Tanto el búfer de descarga HTTP (`4096` bytes) como el árbol `JsonDocument` de `ArduinoJson v7.4.3` utilizan un asignador personalizado (`SpiRamJsonAllocator`) sobre los 8 MB de Octal PSRAM (`MALLOC_CAP_SPIRAM`), respetando el guardrail estricto de no tocar jamás `GPIO 35`.
     * **Sincronización Horaria Atómica y Modo Demo Defensivo (*Mock Data*):** Si el JSON incluye `server_time` y difiere en > 2 s respecto al chip RTC `PCF85063`, ajusta automáticamente la hora local (`UTC-5`). Si `wifi_secrets.h` aún no existe o no hay red Wi-Fi disponible, genera una agenda ejecutiva de demostración (*Reunión de Estrategia*, *Revisión de Arquitectura*, *Comité Ejecutivo Q4*) relativa a la hora actual del RTC para poder probar la interfaz AMOLED desde el primer segundo.
  3. **Interfaz Ejecutiva en la Watchface (`firmware/ui/screens/scr_watchface.*`) e Integración en `main.cpp`:**
     * Se añadió bajo la fecha una línea interactiva minimalista en tono titanio (`#71717A`, `18 pt`, ej. `"16:00 • Reunion de Estr... (en 20m)"`) respetando la política de consumo cero (redibuja solo cuando cambia el minuto del RTC o la versión de sincronización de la agenda).
     * Al tocar la línea del evento, se despliega un **Modal Emergente Ejecutivo** sobre fondo traslúcido (`410 × 502` px) con tarjeta en grafito oscuro (`#18181B`, borde `#27272A`, `366 × 284` px, coordenadas pares para el controlador `CO5300`) mostrando el estado del evento, título completo en blanco alabastro (`#F4F4F5`), horario inicio-fin y tiempo restante en plata (`#A1A1AA`), ubicación/sala y botón `[CERRAR]`.
     * Se integró `calendar_service` en `setup()`, `loop()`, comando `'T...'` y el nuevo comando `'c'` por consola USB (`COM5`) para forzar la sincronización manual y ver la traza completa de red.
* **Resultado / Verificación de Compilación (`pio run`):**
  * Compilación 100% limpia (`[SUCCESS]`) en PlatformIO (`espressif32 @ 6.9.0`, `ArduinoJson @ 7.4.3`, `lvgl @ 8.4.0`, `XPowersLib @ 0.2.9`):
    * **RAM:** `15.0%` (`49,108` bytes utilizados de `327,680` bytes de SRAM interna; búferes de LVGL y JSON en los 8 MB de Octal PSRAM).
    * **Flash:** `38.1%` (`1,273,761` bytes utilizados de `3,342,336` bytes en partición de aplicación).
  * *(Nota: El puerto `COM5` no se encontraba conectado físicamente al momento de cerrar la compilación; el binario `.pio/build/esp32s3_amoled_206/firmware.bin` quedó generado y listo para flashear con `pio run -t upload` apenas se conecte el reloj).*
* **Cómo Revertir (Rollback):**
  * Para volver por código al estado previo a la integración de Google Calendar:
    `git checkout 3014e64`
  * Para restaurar el firmware completo de fábrica (32 MB):
    `powershell -ExecutionPolicy Bypass -File .\scripts\restore_flash_32mb.ps1 -BackupFile ".\backups\factory_backup_32MB.bin"`

---

## Plantilla para Futuras Entradas (Copiar y Pegar)

```markdown
## Entrada #00X — YYYY-MM-DD: [Título Breve de la Tarea]

* **Fase del Roadmap:** Fase X
* **Archivos Modificados:** `firmware/...`
* **Qué se hizo:**
  * Descripción concisa de los cambios.
* **Resultado / Verificación en COM5:**
  * Qué mostró el reloj en pantalla y por puerto serie.
* **Cómo Revertir (Rollback):**
  * Comando Git (`git checkout ...`) o script de restauración para volver al estado estable anterior.
```

