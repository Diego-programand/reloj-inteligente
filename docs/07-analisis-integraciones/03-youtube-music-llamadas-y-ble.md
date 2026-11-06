# 03. Control Multimedia (YouTube Music), Llamadas y Notificaciones vía Bluetooth BLE

Documento de ingeniería y especificación de producto para la comunicación entre el teléfono móvil (Android/iOS) y el reloj **ESP32-S3-Touch-AMOLED-2.06** mediante **Bluetooth Low Energy (BLE)**.

---

## 1. Visión General (Parte Humana / Accesible)

El objetivo es convertir tu reloj en el mando a distancia perfecto mientras escuchas música con auriculares o trabajas en tu escritorio:
* **Control de YouTube Music:** Mientras la música suena en tu teléfono, en la pantalla AMOLED de tu reloj aparece el **nombre de la canción**, el **artista** y controles táctiles sobrios (Play/Pausa, Pasar canción, Canción anterior y volumen).
* **Gestión de Llamadas Telefónicas:** Si alguien te llama, tu reloj vibra y muestra en grande el nombre de la persona (`"Mamá"`, `"Jefe"`, etc.) con dos botones táctiles:
  1. `[Silenciar]`: Apaga el sonido del teléfono al instante sin colgar.
  2. `[Rechazar con mensaje]`: Cuelga la llamada y le pide a tu teléfono que envíe automáticamente un mensaje de texto o Telegram: *"Estoy ocupado, te escribo en breve"*.
* **Notificaciones de Mensajería:** Mensajes de Telegram y WhatsApp se muestran como tarjetas discretas que desaparecen solas tras 5 segundos.

```
+------------------------------------------+
|  16:15                            98%    |
|                                          |
|            YOUTUBE MUSIC                 |
|                                          |
|        Starboy (feat. Daft Punk)         |
|        The Weeknd                        |
|                                          |
|       [ |<< ]     [ >|| ]     [ >>| ]    |
|                                          |
|       - - - - - - - - ● - - - -          |
|       Volumen: 65%                       |
+------------------------------------------+
```

---

## 2. Aclaración Técnica Fundamental: ¿Por qué BLE y no Bluetooth Clásico?

Para entender qué se puede y qué no se puede hacer con las llamadas telefónicas:
* **El procesador `ESP32-S3` solo cuenta con Bluetooth Low Energy (BLE 5.0):** El Bluetooth clásico (el protocolo antiguo de 1998 que usan los manos libres de los coches y auriculares analógicos para transmitir voz por la red telefónica tradicional) no está presente en el silicio del ESP32-S3.
* **Lo que SÍ podemos hacer al 100%:**
  * Recibir el aviso inmediato de llamada entrante con el nombre del contacto (*Caller ID*).
  * Controlar la llamada desde la muñeca: descolgar en el móvil, silenciar el timbre o colgar.
  * Disparar mensajes predeterminados de rechazo por SMS, WhatsApp o Telegram.
  * Y para hablar voz por el reloj: usar el canal **Wi-Fi** (notas de voz, bots de Telegram o intercomunicador VoIP).

---

## 3. Análisis Comparativo de Opciones de Conexión

Para enlazar el reloj con tu teléfono móvil existen 3 opciones principales:

| Criterio | Opción A: Modo Mando Directo (BLE HID Consumer) | Opción B: App Libre `Chronos` (Recomendada) | Opción C: Protocolo Estándar `Gadgetbridge` |
| :--- | :--- | :--- | :--- |
| **¿Requiere instalar app en el móvil?** | **NO.** El teléfono lo empareja en Ajustes como un teclado/mando Bluetooth multimedia. | **SÍ.** Se instala la app gratuita de código abierto *Chronos* (de Felix Biego) desde Google Play Store. | **SÍ.** Se instala *Gadgetbridge* (app de F-Droid líder en privacidad). |
| **Control de YouTube Music** | **Básico:** Play, Pausa, Siguiente, Anterior, Volumen. **No muestra título ni artista.** | **Completo:** Muestra **Título de la canción**, **Artista**, estado de reproducción y control táctil. | **Completo:** Muestra título, artista y controles multimedia. |
| **Identificador de Llamadas** | **No disponible:** Android no comparte eventos de llamadas con teclados HID. | **Disponible:** Muestra el nombre del contacto y permite silenciar/colgar con un toque. | **Disponible:** Alerta de llamadas, SMS y notificaciones de aplicaciones seleccionadas. |
| **Notificaciones de Telegram / WhatsApp** | **No disponible.** | **Disponible:** Transmite el texto del mensaje y el remitente al reloj por BLE. | **Disponible:** Notificaciones filtrables por aplicación con respuestas rápidas. |
| **Consumo de Batería en el Reloj** | Mínimo (~0.5 mA en reposo BLE). | Mínimo (~0.6 mA con enlace NimBLE activo). | Mínimo (~0.6 mA con enlace BLE). |

> [!TIP]
> **Recomendación de Ingeniería:**
> La **Opción B (`Chronos`)** es la más balanceada: la librería para ESP32 (`chronos-esp32`) ya está escrita, usa la pila ultraligera **NimBLE** (que ahorra memoria RAM frente a la pila Bluedroid nativa) y la aplicación en Android está probada y funciona sin configuraciones complejas.

---

## 4. Arquitectura de Implementación con `Chronos-ESP32`

### 4.1. Flujo de Comunicación BLE:
```
┌────────────────────────────────┐                 ┌───────────────────────────────┐
│     TELÉFONO ANDROID           │                 │      RELOJ INTELIGENTE        │
│  - App Chronos en segundo plano│                 │  - ESP32-S3 (Pila NimBLE)     │
│  - YouTube Music reproduciendo │                 │  - hal_ble / ble_service      │
└───────────────┬────────────────┘                 └───────────────┬───────────────┘
                │                                                  │
                │        [BLE Notification Service]                │
                ├─────────────────────────────────────────────────►│
                │  Metadata: "Starboy" / "The Weeknd"              │ Muestra canción en AMOLED
                │                                                  │
                │        [Toque táctil: Botón Siguiente]           │
                │◄─────────────────────────────────────────────────┤ Envía comando 0x03 (NEXT)
                │                                                  │
                │        [Evento de Llamada Entrante]              │
                ├─────────────────────────────────────────────────►│
                │  Caller ID: "Carlos Trabajo"                     │ Pantalla vibra y muestra modal
                │                                                  │
                │        [Toque táctil: Silenciar]                 │
                │◄─────────────────────────────────────────────────┤ Envía comando 0x07 (MUTE)
```

### 4.2. Estructura de Software en el Firmware:

```
firmware/
├── hal/
│   ├── hal_ble.h           // Inicialización de radio BLE con NimBLE-Arduino
│   └── hal_ble.cpp
├── services/
│   ├── ble_service.h       // Callbacks de Chronos: música, llamadas, notificaciones
│   └── ble_service.cpp
└── ui/
    └── screens/
        ├── scr_music.h     // Pantalla táctil de control multimedia LVGL
        ├── scr_music.cpp
        └── scr_call_alert.cpp // Diálogo flotante modal de llamada entrante
```

### 4.3. Fragmento de Integración en C++:
```cpp
#include <ChronosESP32.h>

ChronosESP32 watch("Smartwatch-S3");

void onMusicChanged(String title, String artist, String album) {
    // Actualizar las etiquetas de texto de LVGL en la pantalla AMOLED
    ui_music_update_track(title.c_str(), artist.c_str());
}

void onCallReceived(String caller, String number) {
    // Abrir ventana modal de llamada entrante sobre cualquier pantalla activa
    ui_show_call_overlay(caller.c_str(), number.c_str());
}

void onNotificationReceived(Notification notification) {
    // Mostrar burbuja discreta con el mensaje de Telegram / WhatsApp
    ui_show_notification(notification.app.c_str(), notification.message.c_str());
}

void hal_ble_setup() {
    watch.setMusicCallback(onMusicChanged);
    watch.setCallCallback(onCallReceived);
    watch.setNotificationCallback(onNotificationReceived);
    watch.begin();
}
```

---

## 5. Presupuesto de Memoria y Rendimiento

* **Pila Bluetooth NimBLE:** Ocupa apenas **`35 KB`** de memoria interna (frente a los más de 120 KB que requiere la pila clásica Bluedroid de ESP-IDF), dejando intacta la memoria para el refresco a 60 FPS de LVGL.
* **Consumo de Energía:**
  * En modo de publicidad (*Advertising*): ~`1.2 mA`.
  * Enlace activo con el teléfono en reposo: ~`0.5 mA` gracias a los intervalos de conexión de baja energía (*BLE Connection Interval* ajustado a 250 ms).
  * El impacto en la batería es imperceptible en el día a día.

---

## 6. Recursos, Librerías y Enlaces de Referencia

* **Librería ESP32 Chronos:** [fbiego/chronos-esp32 en GitHub](https://github.com/fbiego/chronos-esp32).
* **Aplicación Móvil Chronos:** [Chronos en Google Play Store](https://play.google.com/store/apps/details?id=com.fbiego.chronos).
* **Pila NimBLE para ESP32:** [h2zero/NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) (la implementación BLE más eficiente y rápida para microcontroladores Espressif).
* **Librería BLE HID alternativa (sin app):** [T-vK/ESP32-BLE-Keyboard](https://github.com/T-vK/ESP32-BLE-Keyboard).

