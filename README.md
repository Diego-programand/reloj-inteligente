# Proyecto: Smartwatch Personalizado (ESP32-S3 AMOLED 2.06")

Bienvenido al repositorio central y base de conocimiento para el desarrollo desde cero de nuestro **Reloj Inteligente Personalizado** sobre la placa **Spotpear / Waveshare ESP32-S3-Touch-AMOLED-2.06**.

Este directorio (`G:\ITM\reloj-inteligente`) está diseñado como **fuente única de verdad (Single Source of Truth)** para que cualquier sesión de trabajo —humana o asistida por IA— cuente con especificaciones exactas de hardware, procedimientos de recuperación ante fallos, bitácora de cambios y código modular.

---

## Resumen Rápido del Dispositivo

| Característica | Especificación Verificada |
| :--- | :--- |
| **Modelo de Placa** | `ESP32-S3-Touch-AMOLED-2.06` (Waveshare / Spotpear) |
| **Microcontrolador** | Espressif **ESP32-S3R8** (Dual-Core Xtensa LX7 @ 240 MHz, rev v0.2) |
| **Memoria Flash** | **32 MB** Quad SPI (QIO @ 80 MHz, ID `c8:4019` GigaDevice) |
| **Memoria PSRAM** | **8 MB** Octal PSRAM integrada (`AP_3v3` @ 80 MHz) |
| **Pantalla** | **2.06" AMOLED** ($410 \times 502$ px, 16.7M colores, interfaz QSPI, driver **CO5300 / SH8601**) |
| **Panel Táctil** | Capacitivo **FT3168** por I2C (`0x38`) |
| **Gestión de Energía (PMIC)** | **X-Powers AXP2101** por I2C (`0x34`) + cargador Li-Po 3.7V MX1.25 |
| **Sensores** | IMU 6 ejes **QMI8658** (`0x6B`) + Reloj RTC **PCF85063** (`0x51`) |
| **Audio** | DAC/Altavoz **ES8311** (`0x18`) + ADC 2 Micrófonos **ES7210** (`0x40`) vía I2S |
| **Dirección MAC Nativa** | `B8:1F:3F:D8:2D:24` |
| **Puerto en este PC** | **`COM5`** (`USB\VID_303A&PID_1001` — Puerto físico #10 del Hub raíz) |

---

## Estructura del Directorio

```text
G:\ITM\reloj-inteligente\
├── README.md                              # Este documento (Índice maestro)
├── CONTEXTO_PARA_AGENTE.md                # Instrucciones y contexto para nuevas sesiones de IA
├── docs\                                  # Documentación técnica profunda
│   ├── 01-hardware-y-pinout.md            # Mapa exacto de pines GPIO, bus I2C, QSPI y alertas eléctricas
│   ├── 02-estado-fabrica-y-particiones.md # Tabla de particiones original, firmware de fábrica y diagnóstico USB
│   ├── 03-respaldo-y-recuperacion.md      # Protocolo anti-brick, backup de 32MB y restauración completa
│   ├── 04-entorno-y-compilacion.md        # Configuración exacta para Arduino IDE / PlatformIO y librerías
│   ├── 05-arquitectura-smartwatch.md      # Diseño modular del software propio (HAL, Energía, UI LVGL, Sensores)
│   └── 06-roadmap-y-fases.md              # Hoja de ruta paso a paso con criterios de verificación
├── logs\                                  # Registro histórico y trazabilidad
│   ├── CHANGELOG.md                       # Historial de versiones y cambios aplicados al reloj
│   ├── BITACORA_TECNICA.md                # Diario de ingeniería: qué se probó, errores y cómo se solucionaron
│   └── boot_log_fabrica_2026-11-05.txt    # Log real de arranque extraído por COM5 el día 1
├── backups\                               # Almacén de volcados binarios (.bin) de la memoria Flash
│   └── README_BACKUPS.md                  # Registro de backups e integridad SHA256
├── scripts\                               # Automatización en PowerShell
│   ├── backup_flash_32mb.ps1              # Extrae los 32MB de Flash automáticamente y calcula su SHA256
│   ├── restore_flash_32mb.ps1             # Restaura una imagen .bin en el reloj de forma segura
│   └── monitor_serial.ps1                 # Lector rápido del puerto serie COM5 a 115200 baudios
├── firmware\                              # Código fuente de nuestro reloj inteligente a medida
│   └── README_FIRMWARE.md                 # Organización del código fuente
└── assets\                                # Recursos gráficos y fotos de referencia
    └── tarjeta_soporte_spotpear.jpg       # Tarjeta de soporte técnico recibida en el paquete original
```

---

## Las 4 Reglas de Oro del Proyecto

1. **Nunca flashear sin respaldo previo:** Antes de borrar la Flash por primera vez, ejecutar [`scripts/backup_flash_32mb.ps1`](file:///G:/ITM/reloj-inteligente/scripts/backup_flash_32mb.ps1) para guardar `backups/factory_backup_32MB.bin`.
2. **Prohibido tocar `GPIO 35` en software:** El pin `GPIO 35` está conectado físicamente al pin IRQ del PMIC AXP2101, pero **también es utilizado internamente por la memoria Octal PSRAM (`SPIDQS`)**. Configurar `GPIO 35` como entrada o interrupción provoca un *crash* inmediato del procesador. El AXP2101 se consulta únicamente por I2C (`0x34`).
3. **Secuencia de encendido obligatoria (`AXP2101` primero):** La pantalla AMOLED no enciende sola al alimentar el ESP32-S3; requiere que el código inicialice primero el bus I2C (`SDA=15`, `SCL=14`) y habilite los reguladores LDO del chip **AXP2101**.
4. **Mantener siempre `USB CDC On Boot = Enabled`:** Como nos comunicamos por el puerto USB nativo del ESP32-S3 (`COM5`), compilar con `USB CDC On Boot` desactivado hará que el puerto serie desaparezca al arrancar el firmware (obligando a entrar en modo descarga manual con `BOOT + Reset`).

---

## Navegación Rápida de Documentación

* [Contexto para el Agente IA](file:///G:/ITM/reloj-inteligente/CONTEXTO_PARA_AGENTE.md)
* [01 - Hardware, Sensores y Pinout Completo](file:///G:/ITM/reloj-inteligente/docs/01-hardware-y-pinout.md)
* [02 - Estado de Fábrica y Tabla de Particiones](file:///G:/ITM/reloj-inteligente/docs/02-estado-fabrica-y-particiones.md)
* [03 - Manual de Respaldo, Restauración y Desbrickeo](file:///G:/ITM/reloj-inteligente/docs/03-respaldo-y-recuperacion.md)
* [04 - Configuración del Entorno y Compilación](file:///G:/ITM/reloj-inteligente/docs/04-entorno-y-compilacion.md)
* [05 - Arquitectura de Software del Smartwatch](file:///G:/ITM/reloj-inteligente/docs/05-arquitectura-smartwatch.md)
* [06 - Roadmap y Fases de Desarrollo](file:///G:/ITM/reloj-inteligente/docs/06-roadmap-y-fases.md)
* [Bitácora Técnica](file:///G:/ITM/reloj-inteligente/logs/BITACORA_TECNICA.md) | [Changelog](file:///G:/ITM/reloj-inteligente/logs/CHANGELOG.md)

