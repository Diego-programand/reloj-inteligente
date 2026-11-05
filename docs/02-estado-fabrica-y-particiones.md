# 02. Estado de Fábrica, Diagnóstico USB y Tabla de Particiones

Este documento preserva la radiografía exacta de cómo llegó el dispositivo de fábrica el **5 de noviembre de 2026**, antes de realizar cualquier modificación.

---

## 1. Identificación en Windows y USB

* **Nombre del dispositivo compuesto:** `USB JTAG/serial debug unit` + `Dispositivo serie USB (COM5)`
* **Hardware ID (VID / PID):** `USB\VID_303A&PID_1001` (Espressif Systems Native USB-Serial/JTAG)
* **Número de Serie USB / MAC:** `B8:1F:3F:D8:2D:24`
* **Ubicación en bus USB local:** `Port_#0010.Hub_#0001`
* **Puerto COM asignado:** **`COM5`** (Velocidad de consola por defecto: `115200` baudios, 8N1).

---

## 2. Información del Firmware Preinstalado de Fábrica

Extraído directamente del arranque del dispositivo:
* **Nombre del proyecto compilado:** `phone_s3_box_3`
* **Descripción de aplicación:** `ESP-Brookesia phone demo` (Interfaz gráfica estilo smartphone sobre LVGL)
* **Versión del firmware de fábrica:** `v0.4.2-92-g5c6be6c-dirty`
* **Fecha de compilación de fábrica:** `Nov 4 2025 14:21:55`
* **Versión de ESP-IDF utilizada:** `v5.5.1-dirty`
* **SHA256 del archivo ELF original:** `b1eb28068...`

### Secuencia de Inicialización Observada en el Firmware Original:
1. Montaje de partición `storage` (SPIFFS): Total `5,775,761` bytes (~5.5 MB), Usado `2,041,634` bytes (~1.95 MB).
2. Inicialización del bus I2C Maestro.
3. Configuración del codec de altavoz **ES8311** en modo Esclavo (`16 bits`, `16000 Hz`, 2 canales).
4. Configuración del ADC de micrófonos **ES7210** en modo Esclavo (`MIC1` y `MIC2` activados, `16 bits`, `16000 Hz`, desmuteado).
5. Arranque de la tarea gráfica **LVGL** y demo **ESP-Brookesia**.
6. Inicialización del bus QSPI y creación del panel con driver **`sh8601` v1.0.2**.
7. Encendido de iluminación de pantalla (`Backlight on`).

---

## 3. Tabla de Particiones Original (Flash de 32 MB)

El bootloader de 2ª etapa reportó la siguiente estructura de particiones en la memoria Flash de 32 MB:

| # | Etiqueta (`Label`) | Uso (`Usage`) | Tipo (`Type`) | Subtipo (`ST`) | Offset (Hex) | Tamaño (Hex) | Tamaño (Legible) |
| :-: | :--- | :--- | :-: | :-: | :--- | :--- | :--- |
| **0** | `nvs` | WiFi data | `01` (Data) | `02` (NVS) | `0x00009000` | `0x00006000` | **24 KB** |
| **1** | `otadata` | OTA data | `01` (Data) | `00` (OTA) | `0x0000F000` | `0x00002000` | **8 KB** |
| **2** | `phy_init` | RF data | `01` (Data) | `01` (PHY) | `0x00011000` | `0x00001000` | **4 KB** |
| **3** | `model` | Unknown data (Modelos IA/Voz) | `01` (Data) | `82` (SPIFFS) | `0x00012000` | `0x000EE000` | **952 KB** (~0.93 MB) |
| **4** | `factory` | Factory App | `00` (App) | `00` (Factory) | `0x00100000` | `0x00900000` | **9,216 KB** (**9.0 MB**) |
| **5** | `ota_0` | OTA App 0 | `00` (App) | `10` (OTA_0) | `0x00A00000` | `0x00600000` | **6,144 KB** (**6.0 MB**) |
| **6** | `ota_1` | OTA App 1 | `00` (App) | `11` (OTA_1) | `0x01000000` | `0x00600000` | **6,144 KB** (**6.0 MB**) |
| **7** | `storage` | SPIFFS Assets | `01` (Data) | `82` (SPIFFS) | `0x01600000` | `0x00600000` | **6,144 KB** (**6.0 MB**) |
| **—** | *(Espacio libre)* | Sin asignar al final | — | — | `0x01C00000` | `0x00400000` | **4,096 KB** (**4.0 MB**) |

> [!NOTE]
> **Observación importante sobre los offsets de arranque:**
> En el chip **ESP32-S3**, el **Bootloader de 2ª etapa** se aloja en el offset **`0x00000000`** (a diferencia del ESP32 clásico donde iba en `0x1000`), y la tabla de particiones se aloja en **`0x00008000`**. Por eso al hacer un volcado completo desde `0x00000000` hasta `0x02000000` (32 MB) se captura absolutamente todo: bootloader, tabla de particiones, modelos de voz, firmware `factory` y archivos SPIFFS.

---

## 4. Recursos Oficiales de Referencia Externa

* **Repositorio Oficial en GitHub (Ejemplos, Esquemáticos y Binarios de Fábrica):**
  * https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06
* **Soporte Técnico del Vendedor (Tarjeta física en [`assets/tarjeta_soporte_spotpear.jpg`](file:///G:/ITM/reloj-inteligente/assets/tarjeta_soporte_spotpear.jpg)):**
  * Web: https://spotpear.com
  * Email Soporte: `techsupport@spotpear.com` | Ventas: `sales@spotpear.com`
* **Log de arranque íntegro guardado en:**
  * [`logs/boot_log_fabrica_2026-11-05.txt`](file:///G:/ITM/reloj-inteligente/logs/boot_log_fabrica_2026-11-05.txt)
