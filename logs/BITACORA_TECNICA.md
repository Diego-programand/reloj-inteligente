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

