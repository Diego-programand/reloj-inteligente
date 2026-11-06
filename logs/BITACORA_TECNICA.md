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

## Entrada #007 — 2026-11-06: Fase 4 Completada — Sensor de Movimiento (`QMI8658`), Gesto de Muñeca y Ahorro Inteligente de Pantalla

* **Fase del Roadmap:** Fase 4 (Completada)
* **Archivos Modificados:** `firmware/hal/hal_imu.h`, `firmware/hal/hal_imu.cpp`, `firmware/services/power_manager.h`, `firmware/services/power_manager.cpp`, `firmware/ui/ui_manager.cpp`, `firmware/main.cpp`, `docs/06-roadmap-y-fases.md`
* **Qué se hizo:**
  1. **Driver directo `hal_imu` para la IMU de 6 ejes `QMI8658` (`0x6B`):**
     * Verificación de identidad (`WHO_AM_I = 0x05`), reset por software (`0x60 = 0xB0`), activación de auto-incremento I2C (`CTRL1 = 0x40`), acelerómetro 3D en escala `±4g @ 125 Hz` (`CTRL2 = 0x16`), giroscopio 3D en escala `±512 dps @ 125 Hz` (`CTRL3 = 0x56`) y filtros digitales paso-bajo (`CTRL5 = 0x11`).
     * **Hallazgo de orientación física del chip en el PCB:** El chip `QMI8658` está soldado en la cara inferior de la placa, por lo que con la pantalla mirando hacia el cielo su eje `Z` crudo mide `-0.93g`. Se invirtió el signo en `hal_imu_read()` (`d.accelZ = -azRaw / 8192.0f`) para que `+1.0g` corresponda siempre a "pantalla mirando hacia arriba".
  2. **Detección de gesto de muñeca (sin contador de pasos, por preferencia de diseño minimalista):**
     * **Encendido al levantar la muñeca (`WRIST_GESTURE_RAISE_TO_VIEW`):** Detecta cuando el reloj pasa de una postura no visible a la postura de lectura (`accelZ >= +0.48g`, cristal mirando hacia el usuario) acompañado de un movimiento reciente de giro o traslación.
     * **Apagado al bajar el brazo (`WRIST_GESTURE_LOWER_ARM`):** Detecta cuando el brazo baja o la muñeca gira hacia afuera (`accelZ < +0.12g` sostenido por 500 ms).
  3. **Gestor de energía de pantalla (`power_manager`):**
     * Apaga el panel AMOLED (`0x28` Display OFF + `0x10` Sleep IN) tras **15 segundos** sin tocar la pantalla o al bajar el brazo (respetando un margen de 4 s tras cualquier toque táctil).
     * Despierta instantáneamente la pantalla al levantar la muñeca o pulsar el botón `PWR`, actualizando primero el buffer de hora en PSRAM para que la pantalla encienda directamente con el minuto actual.
* **Resultado / Verificación en COM5:**
  * Inicialización confirmada: `[EXITO] Sensor QMI8658 activo (Acelerometro 3D + Giroscopio 3D)` y telemetría en vivo de inclinación (`IMU Z`) y velocidad angular (`Giro dps`).
* **Cómo Revertir (Rollback):**
  * `git checkout c6d24f4` para volver al cierre de la Fase 3.

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

