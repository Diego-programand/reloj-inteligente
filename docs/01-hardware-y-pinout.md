# 01. Especificaciones de Hardware y Pinout Completo

Documento de referencia técnica para la placa **Waveshare / Spotpear ESP32-S3-Touch-AMOLED-2.06**. Todos los datos aquí listados han sido verificados contra el hardware físico conectado en `COM5` y los esquemáticos oficiales del fabricante.

---

## 1. Núcleo y Memorias (SoC)

* **SoC:** Espressif **ESP32-S3R8** (encapsulado QFN56, revisión de silicio `v0.2`).
* **CPU:** Procesador Dual-Core Xtensa® 32-bit LX7, frecuencia ajustable hasta **240 MHz** + Coprocesador de ultra bajo consumo (ULP-RISC-V / LP Core).
* **SRAM Interna:** 512 KB SRAM + 384 KB ROM + 16 KB SRAM en RTC.
* **PSRAM Integrada:** **8 MB (64 Mbit) Octal PSRAM** (`AP_3v3`, vendor `0x0d`, velocidad **80 MHz**).
  * *Nota:* Ocupa internamente los pines de interfaz Octal SPI (`GPIO 33` a `GPIO 37`).
* **Memoria Flash Externa:** **32 MB (256 Mbit) Quad SPI Flash** (fabricante GigaDevice `0xC8`, ID `0x4019`, modo **QIO @ 80 MHz**, voltaje 3.3V por eFuse).
* **Conectividad Inalámbrica:**
  * **Wi-Fi:** 2.4 GHz (802.11 b/g/n) con antena integrada en placa.
  * **Bluetooth:** Bluetooth 5.0 (BLE + Mesh).
  * **Dirección MAC de fábrica:** `B8:1F:3F:D8:2D:24`.

---

## 2. Mapa Completo de Pines (GPIO Pinout)

### 2.1. Bus I2C Principal (Compartido por todos los sensores y PMIC)
Todos los circuitos integrados de control comparten el mismo bus I2C:

| Señal | Pin ESP32-S3 | Función / Notas |
| :--- | :--- | :--- |
| **`I2C_SDA`** | **`GPIO 15`** | Línea de datos I2C (requiere pull-up interno/externo, velocidad típica 400 kHz) |
| **`I2C_SCL`** | **`GPIO 14`** | Línea de reloj I2C |

#### Mapa de Direcciones I2C (7-bit) en el Bus (`GPIO 15` / `GPIO 14`):
| Dirección Hex | Chip | Componente / Función |
| :--- | :--- | :--- |
| **`0x34`** | **AXP2101** | PMIC (Gestión de batería Li-Po, carga, botón PWR y rieles de voltaje LDO) |
| **`0x38`** | **FT3168** | Controlador de panel táctil capacitivo |
| **`0x51`** | **PCF85063** | Reloj en Tiempo Real (RTC) con calendario y alarmas |
| **`0x6B`** *(o `0x6A`)* | **QMI8658** | Sensor IMU de 6 ejes (Acelerómetro 3D + Giroscopio 3D) |
| **`0x18`** | **ES8311** | Codec de Audio Mono DAC (Salida hacia el altavoz integrado) |
| **`0x40`** | **ES7210** | ADC de Audio de 4 canales (Entrada de los 2 micrófonos digitales) |

---

### 2.2. Pantalla AMOLED 2.06" (Interfaz QSPI)
* **Panel:** AMOLED de 2.06 pulgadas, forma rectangular con esquinas redondeadas (estilo Apple Watch).
* **Resolución:** **$410 \times 502$ píxeles**, 16.7 millones de colores (RGB565 en LVGL).
* **Controlador IC:** **CO5300** (compatible a nivel de comandos QSPI con el driver **SH8601**).

| Señal | Pin ESP32-S3 | Descripción |
| :--- | :--- | :--- |
| **`LCD_CS`** | **`GPIO 12`** | Chip Select de la pantalla QSPI |
| **`LCD_SCK`** | **`GPIO 11`** | Reloj SPI/QSPI (`SCL`) |
| **`LCD_SIO0` (`D0`)** | **`GPIO 4`** | Línea de datos QSPI 0 |
| **`LCD_SIO1` (`D1`)** | **`GPIO 5`** | Línea de datos QSPI 1 |
| **`LCD_SIO2` (`D2`)** | **`GPIO 6`** | Línea de datos QSPI 2 |
| **`LCD_SIO3` (`D3`)** | **`GPIO 7`** | Línea de datos QSPI 3 |
| **`LCD_RST`** | **`GPIO 8`** | Reset físico del controlador de pantalla (activo en bajo) |
| **`LCD_TE` / `LCD_EN`** | **`GPIO 13`** | Tearing Effect / Habilitación de pantalla |

> [!TIP]
> **Control de Brillo en AMOLED:** A diferencia de las pantallas LCD TFT tradicionales que usan un pin PWM para un LED de retroiluminación (*backlight*), en las pantallas **AMOLED (CO5300/SH8601)** cada píxel emite su propia luz. El brillo se ajusta enviando el comando DCS `0x51` (*Write Display Brightness*, valor de `0x00` a `0xFF`) directamente por el bus QSPI.

---

### 2.3. Panel Táctil Capacitivo (`FT3168`)

| Señal | Pin ESP32-S3 | Descripción |
| :--- | :--- | :--- |
| **`TP_SDA`** | **`GPIO 15`** | Datos I2C (bus compartido, dirección `0x38`) |
| **`TP_SCL`** | **`GPIO 14`** | Reloj I2C (bus compartido) |
| **`TP_RST`** | **`GPIO 9`** | Reset físico del controlador táctil |
| **`TP_INT`** | **`GPIO 38`** | Interrupción táctil (indica cuando hay un toque activo en pantalla) |

---

### 2.4. Subsistema de Audio (`ES8311` + `ES7210` — Bus I2S)

| Señal | Pin ESP32-S3 | Descripción |
| :--- | :--- | :--- |
| **`I2S_MCLK`** | **`GPIO 16`** | Master Clock del bus I2S |
| **`I2S_BCLK`** | **`GPIO 41`** | Bit Clock del bus I2S |
| **`I2S_LRCK` (`WS`)** | **`GPIO 45`** | Word Select / Left-Right Clock |
| **`I2S_DOUT` (`DSDIN`)** | **`GPIO 40`** | Salida de datos de audio digital del ESP32 hacia el DAC `ES8311` |
| **`I2S_DIN` (`ASDOUT`)** | **`GPIO 42`** | Entrada de datos de los micrófonos desde el ADC `ES7210` hacia el ESP32 |
| **`PA_CTRL`** | **`GPIO 46`** | Habilitación del amplificador de potencia del altavoz (*Power Amplifier Enable*) |

---

### 2.5. Lector de Tarjetas MicroSD / TF (`SDMMC` / `SPI`)

| Señal | Pin ESP32-S3 | Modo SDMMC (1-bit) | Modo SPI |
| :--- | :--- | :--- | :--- |
| **`SD_CMD` / `MOSI`** | **`GPIO 1`** | `CMD` | `MOSI` |
| **`SD_CLK` / `SCK`** | **`GPIO 2`** | `CLK` | `SCK` |
| **`SD_D0` / `MISO`** | **`GPIO 3`** | `D0` | `MISO` |

---

### 2.6. Botones Físicos y Puerto USB

| Componente | Pin / Conexión | Descripción |
| :--- | :--- | :--- |
| **Botón `BOOT`** | **`GPIO 0`** | Botón lateral (activo en `LOW`). Sirve para entrar en modo bootloader y como botón programable por el usuario en el reloj. |
| **Botón `PWR`** | **Pin `PWRON` del `AXP2101`** | Botón lateral de encendido/apagado. Conectado al PMIC AXP2101. Pulsación larga apaga/enciende; pulsación corta se detecta leyendo el registro de eventos del AXP2101 por I2C. |
| **`USB D-`** | **`GPIO 19`** | Línea diferencial USB negativa nativa |
| **`USB D+`** | **`GPIO 20`** | Línea diferencial USB positiva nativa |

---

## 3. Advertencias Críticas de Hardware

> [!CAUTION]
> **1. Peligro de Cuelgue en `GPIO 35` (Conflicto PSRAM vs AXP2101 IRQ):**
> En la placa física, la salida de interrupción del chip AXP2101 (`PMU_IRQ`) está unida a `GPIO 35`. Sin embargo, el módulo `ESP32-S3R8` utiliza `GPIO 35` como línea `SPIDQS` para su memoria **Octal PSRAM de 8 MB**.
> * **Nunca** llames a `pinMode(35, INPUT)` ni configures interrupciones GPIO en el pin 35.
> * Lee siempre los eventos del botón PWR y estado de batería mediante consultas periódicas (*polling* cada 100–500 ms) al AXP2101 por I2C (`0x34`).

> [!WARNING]
> **2. Gestión de Energía del AXP2101:**
> No configures tensiones superiores a **3.3V** en los reguladores `ALDO1`–`ALDO4` o `BLDO1`–`BLDO2` del AXP2101, ya que alimentan directamente la lógica de 3.3V y 1.8V de los sensores y la pantalla AMOLED.

