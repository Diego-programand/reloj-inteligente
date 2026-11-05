# 04. Configuración del Entorno de Desarrollo y Compilación

Para compilar nuestro firmware personalizado sin errores de memoria, reinicios inesperados ni pérdida del puerto USB (`COM5`), el compilador debe recibir la configuración exacta del hardware **`ESP32-S3R8` (8 MB Octal PSRAM + 32 MB Quad SPI Flash)**.

---

## 1. Configuración en Arduino IDE (Instalado en el Sistema)

Ruta del IDE en este equipo: `C:\Users\USER\AppData\Local\Programs\Arduino IDE`

### 1.1. URL del Gestor de Tarjetas (Boards Manager)
En **Archivo > Preferencias > URLs adicionales de gestor de placas**, asegurar que esté presente la URL oficial de Espressif:
```text
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```
Luego, en **Herramientas > Placa > Gestor de placas**, instalar **`esp32` by Espressif Systems** (versión recomendada: `3.0.x` o `2.0.17` según compatibilidad de drivers gráficos).

### 1.2. Parámetros Exactos del Menú `Herramientas` (Tools)

| Parámetro en Arduino IDE | Valor Obligatorio | ¿Por qué es crítico? |
| :--- | :--- | :--- |
| **Board** | `ESP32S3 Dev Module` *(o Waveshare ESP32-S3-Touch-AMOLED-2.06)* | Define la arquitectura Xtensa LX7 Dual-Core |
| **Port** | `COM5` | Puerto USB-Serial/JTAG nativo |
| **USB CDC On Boot** | **`Enabled`** | Si se pone en `Disabled`, `Serial.print()` no funcionará por USB y perderás `COM5` al arrancar |
| **USB Mode** | **`Hardware CDC and JTAG`** | Usa el controlador de hardware integrado del S3 |
| **CPU Frequency** | `240MHz (WiFi)` | Máxima fluidez para renderizar la pantalla AMOLED a 60 FPS |
| **Flash Mode** | **`QIO 80MHz`** | Coincide con la configuración de eFuse y el chip GigaDevice |
| **Flash Size** | **`32MB (256Mb)`** | Capacidad real verificada con `esptool` |
| **PSRAM** | **`OPI PSRAM`** | **CRÍTICO:** La PSRAM de 8 MB del S3R8 es **Octal (`OPI`)**, no Quad (`QSPI`). Si eliges `QSPI PSRAM`, la placa no arrancará |
| **Partition Scheme** | `32M Flash (4.8MB APP/22MB LittleFS)` *(o `Huge APP`)* | Permite almacenar fuentes tipográficas e iconos de alta resolución |
| **Upload Speed** | `921600` | Velocidad óptima de subida por USB nativo |

---

## 2. Configuración en PlatformIO (`platformio.ini` de Referencia)

Si decidimos compilar mediante **PlatformIO** (CLI o VS Code), este es el archivo `platformio.ini` exacto y validado para nuestra placa:

```ini
[env:esp32s3_amoled_206]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino

; Configuración exacta de Memorias (32MB QIO Flash + 8MB Octal PSRAM)
board_build.arduino.memory_type = qio_opi
board_build.flash_mode = qio
board_build.f_flash = 80000000L
board_build.f_cpu = 240000000L
board_upload.flash_size = 32MB
board_upload.maximum_size = 33554432

; Puerto Serie y Velocidades
upload_port = COM5
upload_speed = 921600
monitor_port = COM5
monitor_speed = 115200

; Banderas de Compilación (USB Nativo + PSRAM Habilitada)
build_flags = 
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DCORE_DEBUG_LEVEL=3
```

---

## 3. Librerías Base que Utilizaremos

Para mantener nuestro código limpio, rápido y fácil de mantener, nos apoyaremos en las siguientes librerías probadas:

1. **Gestión de Energía (`AXP2101`) y Sensores (`QMI8658`, `PCF85063`):**
   * **`XPowersLib`** (por Lewis He / `lewisxhe`): La librería estándar de oro para controlar el chip **AXP2101** en ESP32.
   * **`SensorLib`** (por Lewis He / `lewisxhe`): Incluye soporte limpio y directo para el IMU **QMI8658**, el RTC **PCF85063** y controladores táctiles FocalTech (**FT3168**).
2. **Driver de Pantalla QSPI (`CO5300` / `SH8601`):**
   * **`Arduino_GFX`** (por Moon On Our Nation) o el driver QSPI directo de `esp_lcd_sh8601` / `esp_lcd_co5300` proporcionado en el paquete de soporte de Waveshare.
3. **Motor de Interfaz Gráfica (UI):**
   * **`lvgl`** (Light and Versatile Graphics Library, versión `8.3.x` o `9.x`): Configurado con *double buffering* en los **8 MB de Octal PSRAM** para lograr transiciones de pantalla ultra fluidas sin parpadeos.
