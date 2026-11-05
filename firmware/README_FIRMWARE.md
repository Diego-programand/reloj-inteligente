# Directorio de Código Fuente (`firmware/`)

Aquí reside todo el código fuente C/C++ de nuestro reloj inteligente personalizado.

## Estructura

* [`config/pins_config.h`](file:///G:/ITM/reloj-inteligente/firmware/config/pins_config.h): Definición centralizada de todos los pines GPIO, direcciones I2C, resolución de pantalla y constantes de seguridad del hardware.
* `hal/`: Capa de abstracción de hardware (`hal_power`, `hal_display`, `hal_touch`, `hal_rtc`, `hal_imu`, `hal_audio`).
* `services/`: Servicios del reloj (`power_manager`, `time_sync`).
* `ui/`: Pantallas y widgets construidos con **LVGL**.
