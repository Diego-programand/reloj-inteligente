# Changelog — Historial de Cambios del Proyecto

Todos los cambios notables en la documentación, herramientas de respaldo y código del firmware de nuestro reloj inteligente se registran en este archivo.

El formato está basado en [Keep a Changelog](https://keepachangelog.com/es-ES/1.0.0/).

---

## [0.7.0] - 2026-11-06 — Análisis Profundo y Especificación Técnica de Integraciones Inteligentes

### Añadido (Added)
* Carpeta documental `docs/07-analisis-integraciones/` con 5 especificaciones modulares:
  * `00-resumen-ejecutivo-y-preferencias.md`: Síntesis humana en 3 minutos con cuestionario interactivo de casillas `[ ]` para selección de opciones y prioridades por el usuario.
  * `01-google-calendar-y-timeline.md`: Arquitectura de sincronización con Google Calendar mediante puente Google Apps Script Webhook (JSON ligero < 500 bytes, sincronización en 2.2 s por Wi-Fi sin sobrecarga de OAuth 2.0) y visualización de línea de tiempo ejecutiva.
  * `02-agente-ia-telegram-y-voz.md`: Integración del subsistema de audio digital (`ES7210` micrófonos duales + `ES8311` altavoz amplificado) vía I2S en búferes de Octal PSRAM para modo Walkie-Talkie con agentes de Telegram y Google Gemini.
  * `03-youtube-music-llamadas-y-ble.md`: Control multimedia de YouTube Music, identificador de llamadas entrantes con botón de silenciar y notificaciones telefónicas vía Bluetooth BLE (utilizando la app libre `Chronos` y pila NimBLE).
  * `04-audio-offline-y-microsd.md`: Reproductor de MP3/podcasts local autónomo vía SDMMC 1-bit (`GPIO 1, 2, 3`) con `ESP32-audioI2S` y grabadora de notas de voz offline directa a MicroSD.
* Actualización de `docs/06-roadmap-y-fases.md` vinculando el análisis de la Fase 6.

---

## [0.6.0] - 2026-11-06 — Fase 4 y Simplificación de Ajustes: Tap-to-Wake por Interrupción, Formato 12H Fijo y Ajustes de Brillo/Volumen/Batería

### Añadido (Added)
* Módulo `power_manager` (`firmware/services/power_manager.h` y `firmware/services/power_manager.cpp`): Gestión de encendido de pantalla por **toque en cristal (*Tap-to-Wake*)** y **botón `PWR`**, con apagado automático tras 20 segundos de inactividad.
* Captura por interrupción de hardware (`FALLING`) en `GPIO 38` (`firmware/hal/hal_touch.cpp`) para garantizar que el primer toque en la pantalla apagada siempre despierte el reloj sin perder pulsos cortos.
* Control de **Volumen (`0%`–`100%`)** en el panel de Ajustes (`firmware/ui/ui_manager.cpp`) conectado por I2C al registro de volumen digital (`0x32`) del chip DAC de audio `ES8311` (`0x18`).
* Módulo `hal_imu` (`firmware/hal/hal_imu.h` y `firmware/hal/hal_imu.cpp`): Driver I2C disponible para el sensor `QMI8658` (`0x6B`) (desacoplado del encendido de pantalla por preferencia del usuario).

### Cambiado (Changed)
* **Formato exclusivo de 12 horas (`AM` / `PM`)** en `scr_watchface.cpp`: Se eliminó el cambio a formato militar (24 horas) al tocar la pantalla.
* **Simplificación del panel de Ajustes (`ui_manager.cpp`):** Ahora muestra exclusivamente 3 tarjetas limpias en estética monocromática: **Brillo (`%` + slider)**, **Volumen (`%` + slider)** y **Batería (`%` + barra)**, eliminando el ajuste manual de hora y los textos técnicos de milivoltios.
* **Apagado limpio de matriz AMOLED (`hal_display.cpp`):** Uso de `0x28`/`0x29` + brillo `0` sin `Sleep IN/OUT` (`0x10`/`0x11`) para evitar pantallas blancas al despertar rápido.

---

## [0.5.0] - 2026-11-05 — Fase 3 Completada: Reloj de Tiempo Real (`PCF85063`), Sincronización Horaria y Primera Watchface Propia

### Añadido (Added)
* Módulo `hal_rtc` (`firmware/hal/hal_rtc.h` y `firmware/hal/hal_rtc.cpp`): Driver I2C para el reloj de tiempo real `PCF85063` (`0x51`) con conversión BCD, detección de parada de oscilador y cálculo automático del día de la semana.
* Módulo `time_sync` (`firmware/services/time_sync.h` y `firmware/services/time_sync.cpp`): Sincronización horaria por comando serie USB (`TYYYY-MM-DD HH:MM:SS`) y cliente NTP Wi-Fi para zona horaria `UTC-5`.
* Fuente tipográfica a medida **`font_watch_time_108`** (`108 pt`, 4-bpp anti-aliased tabular, generada con `scripts/generate_watch_font.py` en `firmware/ui/fonts/font_watch_time_108.*`) para aprovechar la alta densidad de píxeles del panel AMOLED ($410 \times 502$).
* Esfera de reloj personalizada `scr_watchface` (`firmware/ui/screens/scr_watchface.h` y `firmware/ui/screens/scr_watchface.cpp`): Diseño minimalista formal y exclusivo sobre negro puro AMOLED (`#000000`), sin tarjetas ni colores estridentes, mostrando hora gigante (`108 pt`) en formato **12H con `AM`/`PM` (`28 pt`)** y fecha/día (`24 pt`) en tonos neutros de titanio y plata, con redibujado únicamente 1 vez por minuto.
* Encendido y apagado directo de pantalla (`Sleep IN` / `Sleep OUT` en `CO5300` + bloqueo táctil en reposo) con 1 toque del botón lateral `PWR`.
* Doble *buffer* de **pantalla completa (`410 × 502` líneas)** en Octal PSRAM (`ui_manager.cpp`), `LV_COLOR_16_SWAP = 1` (`lv_conf.h`) con `memcpy` DMA directo y sincronización vertical `V-Sync` en el pin `TE` (`GPIO 13`) para eliminar el quiebre a mitad de pantalla durante las transiciones deslizables.
* Panel de ajustes secundario (`ui_manager.cpp`) unificado en paleta monocromática sobria con controles de brillo, ajuste de hora (`+1 Hora`, `+1 Min`, `-1 Min`) y estado de batería.
* Parámetro `-SyncTime` en `scripts/monitor_serial.ps1` para sincronizar automáticamente la hora de Windows al reloj por `COM5`.

---

## [0.4.0] - 2026-11-05 — Fase 2B Completada: Motor Gráfico `LVGL` en Octal PSRAM y Pantallas Deslizables

### Añadido (Added)
* Configuración `firmware/lv_conf.h` y librería `lvgl @ 8.4.0` integrada en `platformio.ini`.
* Módulo `ui_manager` (`firmware/ui/ui_manager.h` y `firmware/ui/ui_manager.cpp`) con doble *buffer* de renderizado (~402 KB) alojado en la Octal PSRAM de 8 MB (`MALLOC_CAP_SPIRAM`).
* Interfaz táctil deslizable (`lv_tileview`) con 2 pantallas: panel en vivo de batería (`%` y `mV`), control deslizante (*slider*) de brillo AMOLED, monitor de toque/botones y pantalla secundaria con `lv_arc` interactivo y contador táctil.

### Corregido (Fixed)
* Actualización explícita de ventana vertical (`0x2B` `RASET`) por cada bloque DMA de 30 filas en `hal_display.cpp` para garantizar el pintado y limpieza completa de las 502 filas en el controlador `CO5300`.
* Callback `lvgl_rounder_cb` en `ui_manager.cpp` que alinea todas las áreas parciales de refresco de LVGL a coordenadas y dimensiones pares (eliminando el rasgado diagonal en `CO5300`) y aumento del bus QSPI a **80 MHz**.

---

## [0.3.0] - 2026-11-05 — Fase 2A: Controlador QSPI de Pantalla AMOLED y Panel Táctil (`FT3168`)

### Añadido (Added)
* Módulo `hal_display` (`firmware/hal/hal_display.h` y `firmware/hal/hal_display.cpp`): Driver QSPI de 4 líneas a 40 MHz con DMA para el panel AMOLED `CO5300 / SH8601` ($410 \times 502$), incluyendo el offset de columna de `22` píxeles (`0x16`) extraído del firmware original y control de brillo por software (`0x51`).
* Módulo `hal_touch` (`firmware/hal/hal_touch.h` y `firmware/hal/hal_touch.cpp`): Driver I2C (`0x38`) para el panel táctil `FT3168` sincronizado con el pin de interrupción `GPIO 38` (`WATCH_TP_INT_PIN`).
* Lienzo interactivo de prueba en `firmware/main.cpp`: dibujo táctil en tiempo real sobre la pantalla AMOLED, reporte de coordenadas $(X, Y)$ por `COM5`, cambio de color con botón `BOOT` y ajuste de brillo con botón `PWR`.

---

## [0.2.0] - 2026-11-05 — Fase 1: Entorno Base, Diagnóstico I2C y Gestión de Energía (`AXP2101`)

### Añadido (Added)
* Archivo `platformio.ini` configurado para `ESP32-S3R8` (`qio_opi`, 32 MB Flash, 8 MB Octal PSRAM, USB CDC en `COM5`).
* Módulo `hal_power` (`firmware/hal/hal_power.h` y `firmware/hal/hal_power.cpp`) para control completo del PMIC `AXP2101` (`0x34`) por I2C sin usar `GPIO 35`.
* Programa principal `firmware/main.cpp` con verificación de CPU/Flash/PSRAM, despertar del chip táctil `FT3168` (`GPIO 9`), escáner del bus I2C confirmando los 6 chips en placa (`0x18`, `0x34`, `0x38`, `0x40`, `0x51`, `0x6B`), telemetría de batería en vivo y detección de botones `PWR` y `BOOT`.

---

## [0.1.0] - 2026-11-05 — Inicialización del Proyecto y Diagnóstico de Hardware

### Añadido (Added)
* Creación de la estructura maestra del proyecto en `G:\ITM\reloj-inteligente`.
* Identificación completa por USB (`COM5`) y extracción del log de arranque del reloj de fábrica (`logs/boot_log_fabrica_2026-11-05.txt`).
* Instalación de `esptool v5.6.0` y `pyserial v3.5` en Python 3.14 local.
* Documentación técnica integral en `docs/`:
  * `01-hardware-y-pinout.md`: Especificaciones de CPU, memorias, bus I2C, QSPI AMOLED, audio I2S, MicroSD y advertencia crítica del pin `GPIO 35`.
  * `02-estado-fabrica-y-particiones.md`: Radiografía del firmware de fábrica (`ESP-Brookesia phone demo`) y tabla de particiones original de 32 MB.
  * `03-respaldo-y-recuperacion.md`: Procedimientos de backup completo, restauración 1:1 y desbrickeo por hardware (`BOOT + PWR`).
  * `04-entorno-y-compilacion.md`: Parámetros exactos de compilación para Arduino IDE y PlatformIO.
  * `05-arquitectura-smartwatch.md`: Diseño modular del firmware propio por capas (HAL, Servicios, UI LVGL) y máquina de estados de energía.
  * `06-roadmap-y-fases.md`: Plan de desarrollo incremental de la Fase 0 a la Fase 6.
* Scripts de automatización en PowerShell dentro de `scripts/`:
  * `backup_flash_32mb.ps1`: Respaldo automatizado de los 32 MB de Flash con verificación SHA256.
  * `restore_flash_32mb.ps1`: Restauración segura de cualquier imagen `.bin`.
  * `monitor_serial.ps1`: Monitor serie ligero para leer `COM5` desde terminal.
* Archivo `CONTEXTO_PARA_AGENTE.md` para garantizar continuidad perfecta entre sesiones de trabajo.
* Copia de la foto de la tarjeta de soporte original en `assets/tarjeta_soporte_spotpear.jpg`.
* Respaldo completo de los 32 MB de Flash de fábrica (`backups/factory_backup_32MB.bin`, SHA256 `3883DB5C407AD1ABD9B60942B42ABF30636206DDA3402ACA5D38960B40214D30`) y archivo de verificación `backups/factory_backup_32MB.bin.sha256`.

