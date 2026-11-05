# 06. Roadmap y Fases de Desarrollo

Para avanzar con velocidad y **cero errores acumulados**, construiremos el reloj en **6 fases incrementales**. Ninguna fase se da por terminada hasta superar su criterio de validación y quedar registrada en [`logs/BITACORA_TECNICA.md`](file:///G:/ITM/reloj-inteligente/logs/BITACORA_TECNICA.md).

---

## Fase 0: Preparación, Documentación y Respaldo de Fábrica
* [x] Identificar el hardware exacto conectado en `COM5` (`ESP32-S3-Touch-AMOLED-2.06`, 32MB Flash, 8MB Octal PSRAM).
* [x] Extraer el log de arranque original y la tabla de particiones de fábrica.
* [x] Instalar `esptool` v5.6.0 en el entorno de Windows.
* [x] Crear toda la estructura documental y scripts de automatización en `G:\ITM\reloj-inteligente`.
* [x] **Respaldo de fábrica completado:** Ejecutado `.\scripts\backup_flash_32mb.ps1` (`backups\factory_backup_32MB.bin`, `33,554,432` bytes, SHA256: `3883DB5C407AD1ABD9B60942B42ABF30636206DDA3402ACA5D38960B40214D30`).

---

## Fase 1: Entorno Base, Diagnóstico I2C y Gestión de Energía (`AXP2101`)
* **Objetivo:** Compilar y subir nuestro primer programa propio que tome el control seguro de la placa sin depender de nada de fábrica.
* **Tareas:**
  1. Configurar el entorno de compilación (Arduino IDE o PlatformIO) con soporte para `ESP32-S3` (32MB QIO + OPI PSRAM).
  2. Crear un escáner y verificador del bus I2C (`SDA=15`, `SCL=14`) que confirme la presencia de los 6 chips internos (`0x34`, `0x38`, `0x51`, `0x6B`, `0x18`, `0x40`).
  3. Inicializar el chip **AXP2101** (`hal_power`), encender los rieles de alimentación de periféricos y mostrar por el monitor serie (`COM5`) el voltaje real de la batería, el porcentaje de carga y los eventos al presionar el botón `PWR`.
* **Criterio de éxito:** Consola serie en `COM5` reportando los 6 dispositivos I2C, telemetría de batería en vivo y detección limpia del botón `PWR` y `BOOT`.

---

## Fase 2: Encendido de Pantalla AMOLED ($410 \times 502$) y Panel Táctil (`FT3168`)
* **Objetivo:** Dominar la pantalla y el panel táctil con aceleración en PSRAM.
* **Tareas:**
  1. Implementar `hal_display` usando el bus QSPI (`GPIO 4, 5, 6, 7, 11, 12, 8, 13`) con el driver `CO5300 / SH8601`.
  2. Probar el control de brillo por software (de 0% a 100%) y renderizado de colores puros.
  3. Implementar `hal_touch` (`FT3168` en `0x38`, `RST=9`, `INT=38`) y verificar coordenadas $(X, Y)$ en tiempo real al tocar la pantalla.
  4. Integrar el motor gráfico **LVGL** con *buffers* en la Octal PSRAM de 8 MB.
* **Criterio de éxito:** Interfaz LVGL funcionando a 60 FPS en pantalla, respondiendo instantáneamente al dedo y deslizando entre pantallas de prueba.

---

## Fase 3: Reloj en Tiempo Real (`PCF85063`) y Tu Primera Watchface a Medida
* **Objetivo:** Convertir la placa en un reloj funcional y estético diseñado 100% a tu gusto.
* **Tareas:**
  1. Implementar `hal_rtc` para leer y guardar la fecha/hora en el chip **PCF85063** (que mantiene la hora incluso si se reinicia el procesador).
  2. Crear el módulo `time_sync` para conectar unos segundos al Wi-Fi, obtener la hora atómica por servidor NTP (ajustada a tu zona horaria `UTC-5`) y grabarla en el RTC.
  3. Diseñar e implementar **`scr_watchface`**: tu esfera de reloj principal personalizada (hora, segundos, fecha, día de la semana, icono/porcentaje de batería y estado del sistema).
* **Criterio de éxito:** El reloj muestra la hora exacta, conserva la hora al apagarse/reiniciarse y luce tu propio diseño visual en alta resolución.

---

## Fase 4: Movimiento (`QMI8658`), Cuenta Pasos y Ahorro de Batería
* **Objetivo:** Que el reloj funcione en tu muñeca de forma autónoma y eficiente con batería.
* **Tareas:**
  1. Implementar `hal_imu` con el sensor de 6 ejes **QMI8658**.
  2. Configurar el contador de pasos diario y mostrarlo en la Watchface.
  3. Implementar la máquina de estados de energía (`power_manager`): apagado automático de pantalla tras inactividad y **encendido automático al levantar/girar la muñeca** (*Wake on Wrist Tilt*) o pulsar el botón lateral.
* **Criterio de éxito:** La pantalla se apaga sola para ahorrar batería y se enciende al instante cuando levantas el brazo para mirar la hora.

---

## Fase 5: Centro de Control, Menú de Apps y Utilidades Locales
* **Objetivo:** Dotar al reloj de navegación completa por gestos (*Swipe*).
* **Tareas:**
  1. **Deslizar hacia abajo:** Panel de control rápido (barra deslizante de brillo, nivel exacto de batería en mV/%, interruptor de Wi-Fi, modo linterna con pantalla blanca al 100%).
  2. **Deslizar hacia la izquierda/derecha:** Carrusel de utilidades hechas a tu medida (Cronómetro, Temporizador/Alarma con sonido por el altavoz `ES8311`, monitor de actividad/pasos).
  3. Almacenamiento de preferencias (como el nivel de brillo favorito) en memoria no volátil (`Preferences / NVS`).

---

## Fase 6: Funciones Inteligentes Avanzadas (A Elección)
* **Opciones personalizables según lo que quieras agregar:**
  * **Clima en vivo:** Consulta rápida a API meteorológica (Open-Meteo sin necesidad de API Key) al sincronizar por Wi-Fi.
  * **Notificaciones del Teléfono (BLE):** Recibir notificaciones de tu móvil por Bluetooth Low Energy.
  * **Asistente de Voz IA a medida:** Aprovechar los 2 micrófonos (`ES7210`) y el altavoz (`ES8311`) para enviar comandos de voz o notas cuando tú lo decidas.
  * **Reproductor / Galería en MicroSD:** Leer archivos desde la ranura TF (`GPIO 1, 2, 3`).

