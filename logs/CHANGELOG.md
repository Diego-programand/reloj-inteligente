# Changelog — Historial de Cambios del Proyecto

Todos los cambios notables en la documentación, herramientas de respaldo y código del firmware de nuestro reloj inteligente se registran en este archivo.

El formato está basado en [Keep a Changelog](https://keepachangelog.com/es-ES/1.0.0/).

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
