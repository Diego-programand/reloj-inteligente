# 04. Reproductor Offline y Grabadora Local en Tarjeta MicroSD

Documento de ingeniería y especificación de producto para el aprovechamiento de la ranura **MicroSD / TF** (`GPIO 1, 2, 3`) y el subsistema de audio (**códec `ES8311`** y **micrófonos `ES7210`**) para funcionamiento 100% autónomo sin conexión a internet ni teléfono.

---

## 1. Visión General (Parte Humana / Accesible)

Tu placa cuenta con un lector físico de tarjetas MicroSD integrado. Esto abre dos posibilidades fascinantes que la inmensa mayoría de smartwatches comerciales no permiten:
* **Música y Podcasts Offline en la Muñeca:**
  * Puedes insertar una tarjeta MicroSD (de hasta 32 GB o 64 GB) con tus carpetas de música MP3, audiolibros o podcasts favoritos.
  * El reloj los reproduce directamente por su altavoz integrado sin necesidad de llevar el teléfono móvil ni tener Wi-Fi. Ideal para salir a caminar, relajarte o tener música de fondo en tu escritorio.
* **Grabadora de Voz Offline (Dictáfono de Muñeca):**
  * Si estás en una reunión, en un aula o en un lugar sin cobertura Wi-Fi y tienes una idea, mantienes pulsado el botón de grabar. El reloj guarda el audio en alta calidad (`.wav`) directamente en la tarjeta MicroSD.
  * En cuanto llegas a casa o a la oficina, el reloj detecta el Wi-Fi y sube automáticamente todas tus notas grabadas a tu canal privado de Telegram o a tu Google Drive.

```
       [Tarjeta MicroSD (GPIO 1, 2, 3)]
                     │
         ┌───────────┴───────────┐
         ▼                       ▼
  [Modo Reproductor]       [Modo Grabadora]
   Lee archivos MP3         Graba voz de micrófonos (ES7210)
         │                       │
         ▼                       ▼
  [Altavoz ES8311]         Guarda archivo .WAV en MicroSD
  (Audio amplificado)      (Listo para sincronizar después)
```

---

## 2. Pines de Hardware de la Tarjeta MicroSD

La placa conecta el zócalo MicroSD a los pines nativos del procesador `ESP32-S3`:

| Señal de la Placa | Pin ESP32-S3 | Modo SDMMC (1-bit Nativo) | Modo SPI Estándar |
| :--- | :--- | :--- | :--- |
| **`SD_CMD`** | **`GPIO 1`** | `CMD` (Comandos del bus SD) | `MOSI` (Datos hacia la tarjeta) |
| **`SD_CLK`** | **`GPIO 2`** | `CLK` (Reloj del bus SD hasta 40 MHz) | `SCK` (Reloj SPI) |
| **`SD_D0`** | **`GPIO 3`** | `D0` (Línea de datos bidireccional) | `MISO` (Datos desde la tarjeta) |

> [!TIP]
> **Modo SDMMC 1-bit vs Modo SPI:**
> El hardware del `ESP32-S3` tiene un controlador interno de hardware para tarjetas SD (`SDMMC`). Usar el modo SDMMC de 1 bit en los pines `GPIO 1, 2, 3` ofrece velocidades de lectura/escritura de hasta **`10 a 20 MB/s`** con consumo de CPU casi nulo, lo que permite reproducir música fluida mientras la pantalla AMOLED sigue respondiendo al tacto a 60 FPS sin tirones.

---

## 3. Arquitectura de Software

### 3.1. Librería Recomendada para Reproducción: `ESP32-audioI2S`
La librería estándar más probada y eficiente en la comunidad ESP32 es **`schreibfaul1/ESP32-audioI2S`**:
* Decodifica en tiempo real por software formatos **MP3, AAC, FLAC, WAV, M4A**.
* Entrega los datos directamente al bus I2S del códec **`ES8311`**.
* Controla el volumen por software sin pérdida de calidad y gestiona metadatos ID3 (título, artista).

### 3.2. Fragmento de Inicialización en C++:
```cpp
#include <SD_MMC.h>
#include <Audio.h>

#define SD_CMD_PIN  1
#define SD_CLK_PIN  2
#define SD_D0_PIN   3

#define I2S_MCLK    16
#define I2S_BCLK    41
#define I2S_LRCK    45
#define I2S_DOUT    40

Audio audio;

void setup_offline_audio() {
    // 1. Inicializar bus SDMMC en modo 1-bit
    SD_MMC.setPins(SD_CLK_PIN, SD_CMD_PIN, SD_D0_PIN);
    if (!SD_MMC.begin("/sdcard", true)) { // true = 1-bit mode
        Serial.println("Error montando tarjeta MicroSD");
        return;
    }
    Serial.println("Tarjeta MicroSD montada con exito");

    // 2. Configurar pines I2S hacia el codec ES8311
    audio.setPinout(I2S_BCLK, I2S_LRCK, I2S_DOUT, I2S_MCLK);
    audio.setVolume(12); // Nivel de volumen 0 a 21

    // 3. Reproducir archivo MP3 local
    audio.connecttoFS(SD_MMC, "/musica/pista01.mp3");
}

void loop_audio() {
    audio.loop(); // Procesa los buffers de sonido por DMA sin bloquear la CPU
}
```

---

## 4. Grabación Directa a MicroSD (Dictáfono Autónomo)

Cuando grabas una nota de voz sin internet:
1. El sensor táctil o botón activa la tarea de grabación.
2. El driver `ES7210` lee los micrófonos en fragmentos de 1 KB vía DMA.
3. El procesador vuelca los fragmentos directamente en un archivo abierto `SD_MMC.open("/notas/nota_001.wav", FILE_WRITE)`.
4. Al terminar, se escribe la cabecera WAV final con la duración exacta en segundos.
5. El archivo queda listo para escucharse de inmediato en el altavoz o transferirse al PC o a Telegram.

---

## 5. Presupuesto de Energía en Modo Reproductor

* **Reproduciendo música desde MicroSD hacia el altavoz:**
  * CPU ESP32-S3 decodificando MP3: ~`28 mA`.
  * Códec ES8311 + Amplificador de audio: ~`45 mA` (a volumen medio).
  * Lectura intermitente de la tarjeta MicroSD por ráfagas: ~`10 mA`.
  * **Consumo total estimado:** ~`80 a 85 mA`.
  * Con una batería de `400 mAh`, esto permite **entre 4 y 5 horas continuas de música o audiolibro** directamente en el reloj con la pantalla apagada.

---

## 6. Recursos y Enlaces de Referencia

* **Librería ESP32-audioI2S:** [schreibfaul1/ESP32-audioI2S en GitHub](https://github.com/schreibfaul1/ESP32-audioI2S).
* **Documentación Oficial Espressif SDMMC:** [ESP-IDF SDMMC Host Driver Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/sdmmc_host.html).

