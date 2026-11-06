# 02. Asistente Personal de IA en la Muñeca: Telegram y Voz Bidireccional

Documento de ingeniería y especificación de producto para habilitar el subsistema de audio digital (**micrófonos `ES7210`**, **altavoz `ES8311`**) y la integración con **bots de Telegram** y **Google Gemini** en el reloj **ESP32-S3-Touch-AMOLED-2.06**.

---

## 1. Visión General (Parte Humana / Accesible)

Este reloj físico tiene algo que casi ningún reloj de bricolaje posee: **dos micrófonos digitales de alta fidelidad** y **un altavoz integrado con su propio chip amplificador**.

Imagina este flujo de trabajo diario:
1. **Pulsar y Hablar:** Mantienes pulsado el botón lateral de tu reloj (o tocas un icono minimalista de micrófono en la pantalla).
2. **Tu orden de voz:** Dices con naturalidad: *"Pregúntale a Gemini qué citas tengo hoy y mándame un resumen a Telegram"*, o simplemente dictas una nota: *"Acuérdate de comprar café"*.
3. **Respuesta instantánea:**
   * **En pantalla:** En menos de 2 segundos, la pantalla AMOLED muestra la respuesta formateada con tipografía nítida en blanco alabastro sobre negro puro.
   * **En altavoz (opcional):** Si estás caminando o con las manos ocupadas, el altavoz del reloj te responde con voz clara y concisa generada por IA.

```
       [Mantener pulsado Botón BOOT]
                    │
                    ▼
     [Micrófonos Digitales ES7210]
        Capturan tu voz (16 kHz)
                    │
                    ▼
      [Envío por Wi-Fi al Agente]
     (Telegram Bot API / Google Gemini)
                    │
                    ▼
       ┌────────────┴────────────┐
       ▼                         ▼
 [Pantalla AMOLED]        [Altavoz ES8311]
  Muestra el texto       Reproduce la voz
```

---

## 2. Hardware de Audio Involucrado (Pines y Chips Físicos)

A diferencia de placas básicas que usan señales analógicas ruidosas, este reloj utiliza dos circuitos integrados profesionales que se comunican por el bus digital **I2S** (*Inter-IC Sound*):

| Componente | Chip | Bus de Control | Bus de Audio Digital | Función en el Reloj |
| :--- | :--- | :--- | :--- | :--- |
| **Entrada de Audio (2 Micrófonos)** | **ES7210** | I2C (`0x40`) | `I2S_DIN` (`GPIO 42`) | Convierte la voz analógica de los 2 micrófonos en audio digital cristalino de 16 bits sin ruidos. |
| **Salida de Audio (Altavoz)** | **ES8311** | I2C (`0x18`) | `I2S_DOUT` (`GPIO 40`) | Decodifica el audio digital hacia una señal de sonido analógica. |
| **Amplificador de Potencia** | Amplificador Mono | Digital On/Off | **`PA_CTRL` (`GPIO 46`)** | Pin de control que enciende o apaga el amplificador del altavoz para evitar ruidos de fondo cuando no suena nada. |
| **Líneas de Reloj Compartidas** | — | — | **`MCLK = GPIO 16`**<br>**`BCLK = GPIO 41`**<br>**`LRCK = GPIO 45`** | Relojes de sincronización digital para que los micrófonos y el altavoz hablen el mismo idioma que el procesador `ESP32-S3`. |

> [!TIP]
> **La gran ventaja de nuestros 8 MB de Octal PSRAM:**
> En microcontroladores comunes, grabar 5 segundos de audio llena toda la memoria RAM interna y el reloj se reinicia por falta de memoria.
> En nuestro reloj disponemos de **8,192 KB de memoria Octal PSRAM**. Un búfer de 6 segundos de audio en calidad voz (16 kHz, 16 bits mono) ocupa exactamente **`192 KB`**, es decir, **menos del 2.5% de nuestra PSRAM**.

---

## 3. Análisis Técnico de Arquitecturas para la IA

### Comparativa de Métodos para Conectar el Reloj con la IA:

| Criterio | Método A: Directo a Telegram Bot API (`sendVoice`) | Método B: Servidor Puente / Webhook + Gemini (Recomendado) | Método C: Reconocimiento Offline en el Reloj (`ESP-SR`) |
| :--- | :--- | :--- | :--- |
| **¿Cómo opera?** | El ESP32 comprime el audio en formato `.ogg` (Opus) y hace una petición HTTPS POST directa a Telegram. | El ESP32 sube el audio sin comprimir (WAV) a un webhook personal ultra rápido (ej. Cloudflare Worker o script Python), que llama a **Google Gemini** y envía el resultado a Telegram. | El procesador `ESP32-S3` ejecuta un modelo neuronal local para reconocer palabras clave fijas (ej. *"Encender luz"*, *"Hora"*). |
| **Capacidad de Comprensión** | Depende de si un bot externo lee el mensaje de voz y lo responde. | **Máxima:** Inteligencia artificial generativa completa (Google Gemini 1.5 Flash), entiende contexto, resume y razona. | **Limitada:** Solo entiende 20 a 50 comandos fijos predefinidos en inglés/chino; no conversa ni razona. |
| **Latencia / Tiempo de respuesta** | Media: 3 a 5 segundos. | **Rápida:** 1.5 a 2.5 segundos. | **Instantánea:** ~200 ms (pero sin inteligencia contextual). |
| **Carga en el reloj** | Media-alta (requiere compresión Opus en software). | **Mínima:** El reloj solo graba y envía; el servidor hace todo el trabajo pesado. | **Muy alta:** Consume mucha CPU y batería continuamente. |

---

## 4. Especificación de la Arquitectura Recomendada (Método B: Reloj -> Webhook -> Gemini -> Telegram)

### 4.1. Flujo de Datos Paso a Paso:
```
[Usuario habla] 
      │ 
      ▼
1. Driver I2S graba audio PCM (16 kHz, 16-bit, 1 canal) en búfer de PSRAM.
      │
      ▼
2. Al soltar el botón, se añade una cabecera WAV estándar de 44 bytes al inicio del búfer.
      │
      ▼
3. El reloj envía un POST HTTP en streaming al Webhook del usuario (con cifrado SSL).
      │
      ▼
4. El Webhook:
   a) Transcribe el audio con Whisper o directamente con la API multimodal de Gemini Flash.
   b) Gemini genera la respuesta ejecutiva.
   c) Envía la copia del mensaje a tu chat de Telegram.
   d) Retorna al reloj un JSON ligero:
      {
        "text": "Tienes 2 reuniones hoy. La primera es a las 16:00.",
        "tts_available": true
      }
      │
      ▼
5. El reloj renderiza el texto en la pantalla AMOLED y emite un tono sutil de confirmación.
```

### 4.2. Inicialización del Hardware de Audio en C++:

#### Activación del Amplificador (`PA_CTRL`):
```cpp
#define WATCH_PA_CTRL_PIN 46

void audio_enable_speaker(bool enable) {
    pinMode(WATCH_PA_CTRL_PIN, OUTPUT);
    digitalWrite(WATCH_PA_CTRL_PIN, enable ? HIGH : LOW);
}
```

#### Configuración del bus I2S compartido (`firmware/hal/hal_audio.cpp`):
```cpp
#include <driver/i2s.h>

#define I2S_NUM          I2S_NUM_0
#define I2S_MCLK_PIN     16
#define I2S_BCLK_PIN     41
#define I2S_LRCK_PIN     45
#define I2S_DOUT_PIN     40 // Hacia ES8311 (Altavoz)
#define I2S_DIN_PIN      42 // Desde ES7210 (Micrófonos)

void hal_audio_init() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX),
        .sample_rate = 16000, // Calidad óptima para voz y modelos de IA
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 512,
        .use_apll = true,
        .tx_desc_auto_clear = true
    };

    i2s_pin_config_t pin_config = {
        .mck_io_num = I2S_MCLK_PIN,
        .bck_io_num = I2S_BCLK_PIN,
        .ws_io_num = I2S_LRCK_PIN,
        .data_out_num = I2S_DOUT_PIN,
        .data_in_num = I2S_DIN_PIN
    };

    i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM, &pin_config);
}
```

---

## 5. Casos de Uso Concretos Diseñados para Ti

1. **"Modo Dictado Rápido":**
   * Presionas el botón lateral, dices: *"Comprar café y revisar informe del lunes"*.
   * El bot de Telegram lo anota inmediatamente en tu chat guardado de Telegram y el reloj confirma con una vibración corta y la frase: `✓ Guardado en Telegram`.
2. **"Consulta Rápida a la IA":**
   * Pregunta: *"¿A cuántos kilómetros está Madrid de Valencia?"* o *"Resume la regla 80/20"*.
   * Respuesta en pantalla: En 2 segundos aparece la respuesta condensada en 3 líneas de texto blanco legible sobre el fondo negro del reloj.
3. **"Audio Memo Offline":**
   * Si estás fuera de cobertura Wi-Fi, el audio se almacena temporalmente en la tarjeta MicroSD del reloj y se sube automáticamente a Telegram en cuanto el reloj se conecte a la red de tu casa o al punto de acceso de tu móvil.

---

## 6. Recursos, Librerías y Enlaces de Referencia

* **Driver Oficial Waveshare para ES7210 / ES8311:** Repositorio oficial [waveshareteam/ESP32-S3-Touch-AMOLED-2.06](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06) (ejemplos `05_Spec_Analyzer` y `08_ES8311`).
* **Librería de Audio para ESP32:** `pschatzmann/arduino-audio-tools` (potente conjunto de herramientas para manipulación de streams I2S y codificación en ESP32).
* **Telegram Bot API Reference:** [Telegram Bots Voice & Audio Methods](https://core.telegram.org/bots/api#sendvoice).
* **Google Gemini API Documentation:** [Gemini REST API Multimodal Audio Guide](https://ai.google.dev/docs).

