# 05. Arquitectura de Software de Nuestro Smartwatch

Para que nuestro reloj inteligente sea **rápido, estable, mantenible y nunca se convierta en un archivo gigante imposible de depurar**, diseñaremos el firmware siguiendo una **Arquitectura Modular por Capas**.

Si algún día queremos cambiar el diseño del reloj (Watchface) o agregar una aplicación nueva, solo tocaremos la capa de interfaz (`ui/`) sin riesgo de romper el controlador de batería o la pantalla.

---

## 1. Diagrama de Arquitectura en Capas

```mermaid
flowchart TD
    subgraph UI_Layer ["4. Capa de Interfaz y Apps (LVGL)"]
        Watchface["Watchface Principal (Hora, Fecha, Batería, Pasos)"]
        QuickSettings["Panel Rápido (Brillo, Wi-Fi, No Molestar)"]
        Apps["Mini-Apps (Cronómetro, Clima, Notificaciones, Audio/IA)"]
    end

    subgraph System_Services ["3. Servicios del Sistema (Core OS)"]
        StateMgr["Gestor de Estado y Energía (Active -> Dim -> Sleep)"]
        TimeService["Servicio de Tiempo (Sincronización NTP <-> RTC PCF85063)"]
        HealthService["Servicio de Actividad (Conteo de pasos y gesto de muñeca)"]
        CommService["Servicio de Conectividad (Wi-Fi bajo demanda / BLE)"]
    end

    subgraph HAL_Layer ["2. Capa de Abstracción de Hardware (HAL)"]
        HAL_Power["hal_power: AXP2101 (Voltajes, % Batería, Carga, Botón PWR)"]
        HAL_Display["hal_display: CO5300/SH8601 QSPI + Control de Brillo"]
        HAL_Touch["hal_touch: FT3168 I2C (Lectura de coordenadas y gestos)"]
        HAL_Sensors["hal_sensors: QMI8658 (IMU) + PCF85063 (RTC)"]
        HAL_Audio["hal_audio: ES8311 (Altavoz) + ES7210 (Micrófonos)"]
    end

    subgraph Hardware ["1. Hardware Físico"]
        Board["ESP32-S3R8 (Dual Core 240MHz, 8MB PSRAM, 32MB Flash)"]
    end

    UI_Layer --> System_Services
    System_Services --> HAL_Layer
    HAL_Layer --> Hardware
```

---

## 2. Estructura de Carpetas del Código (`firmware/`)

```text
G:\ITM\reloj-inteligente\firmware\
├── config\
│   └── pins_config.h          # Todas las constantes de pines GPIO y direcciones I2C en un solo lugar
├── hal\                       # Capa de Hardware (Hardware Abstraction Layer)
│   ├── hal_power.h / .cpp     # Inicialización segura del AXP2101 y lectura de batería
│   ├── hal_display.h / .cpp   # Inicialización QSPI AMOLED y ajuste de brillo (0-255)
│   ├── hal_touch.h / .cpp     # Lectura táctil del FT3168
│   ├── hal_rtc.h / .cpp       # Lectura/escritura de hora en el chip PCF85063
│   └── hal_imu.h / .cpp       # Acelerómetro/giroscopio QMI8658 (pasos y giro de muñeca)
├── services\                  # Lógica de negocio del reloj
│   ├── power_manager.h / .cpp # Máquina de estados de energía (encendido, ahorro, reposo)
│   └── time_sync.h / .cpp     # Sincronización de hora exacta por Wi-Fi (NTP) hacia el RTC
├── ui\                        # Interfaz gráfica en LVGL
│   ├── ui_manager.h / .cpp    # Navegación entre pantallas (gestos swipe)
│   ├── screens\
│   │   ├── scr_watchface.cpp  # Pantalla principal del reloj hecha a medida
│   │   ├── scr_settings.cpp   # Ajustes de brillo, batería y sistema
│   │   └── scr_apps.cpp       # Aplicaciones personalizadas
│   └── assets\                # Fuentes tipográficas e iconos personalizados
└── main.ino (o main.cpp)      # Punto de entrada limpio: inicializa HAL, Servicios y bucle UI
```

---

## 3. Gestión Inteligente de Batería (Máquina de Estados)

Un verdadero reloj inteligente debe cuidar su batería y su pantalla AMOLED (evitando el *burn-in* y el consumo innecesario):

```mermaid
stateDiagram-v2
    [*] --> ACTIVO: Arranque / Toque / Giro de muñeca
    ACTIVO --> ATENUADO: Sin interacción por 10 seg
    ATENUADO --> ACTIVO: Toque en pantalla / Botón
    ATENUADO --> REPOSO: Sin interacción por 5 seg adicionales
    REPOSO --> ACTIVO: Botón PWR / Gesto de muñeca / Toque
```

1. **Estado `ACTIVO`:**
   * Brillo de pantalla al nivel elegido por el usuario (ej. 80%).
   * LVGL refrescando a máxima fluidez (60 FPS).
   * CPU a 240 MHz.
2. **Estado `ATENUADO` (Dimmed):**
   * Si pasan 10 segundos sin tocar la pantalla, el brillo baja suavemente al 15%.
3. **Estado `REPOSO` (Screen Off / Light Sleep):**
   * Pasados otros 5 segundos, se apaga el panel AMOLED (`Brightness = 0` / comando `Display Off`).
   * Se reduce la frecuencia de la CPU (por ejemplo a 80 MHz o *Light Sleep*) manteniendo activo el conteo de pasos en el sensor **QMI8658** y la detección del botón `PWR` o toque para despertar en menos de 50 milisegundos.
   * El **Wi-Fi permanece apagado por defecto** y solo se enciende durante unos pocos segundos cuando se necesita sincronizar la hora NTP, consultar el clima o usar funciones de voz/IA.
