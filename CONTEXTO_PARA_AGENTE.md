# Contexto Maestro para Sesiones de IA (Antigravity)

> [!IMPORTANT]
> **Instrucción para el Agente IA:** Lee este documento antes de ejecutar comandos o escribir código en este proyecto. Contiene todo el contexto técnico descubierto en la primera sesión de diagnóstico con el hardware físico conectado.

---

## 1. Objetivo y Perfil del Usuario
El usuario compró un reloj de desarrollo **ESP32-S3-Touch-AMOLED-2.06** (distribuido por Spotpear / fabricado por Waveshare) en AliExpress.
* **No desea conservar la interfaz de fábrica** (`ESP-Brookesia phone demo`).
* **Objetivo principal:** Programar desde cero su propio sistema operativo / firmware de reloj inteligente hecho 100% a su medida, con control total sobre la estética, funciones, sensores y consumo de batería.
* **Filosofía de trabajo:** Orden estricto, modularidad, documentación continua en `logs/BITACORA_TECNICA.md` y `logs/CHANGELOG.md`, y capacidad de revertir cualquier cambio en segundos si algo falla.
* **REGLA DE COMUNICACIÓN (Ver [`AGENTS.md`](file:///G:/ITM/reloj-inteligente/AGENTS.md)):** Es la primera vez que el usuario trabaja con una placa física y programa hardware desde cero. **Nunca des por sentado términos técnicos ni siglas** (`I2C`, `GPIO`, `PSRAM`, `PMIC`, `polling`, `HAL`, `CDC`, etc.). Explícalos siempre con claridad, naturalidad y rigor accesible (sin tono infantil, sino como un mentor técnico claro) a medida que avanzamos.

---

## 2. Estado del Entorno en este PC (Windows)
* **Directorio de trabajo (Workspace):** `G:\ITM\reloj-inteligente`
* **Puerto serie del reloj:** **`COM5`** (`USB\VID_303A&PID_1001\B8:1F:3F:D8:2D:24`, ubicado en `Port_#0010.Hub_#0001`).
* **Herramientas ya instaladas en el sistema:**
  * **Python:** `Python 3.14.6` (`C:\Program Files\Python314\python.exe`)
  * **esptool:** `v5.6.0` instalado vía pip (`python -m esptool`)
  * **pyserial:** `v3.5` instalado en Python
  * **Git:** `git version 2.55.0.windows.3`
  * **Arduino IDE:** Instalado en `C:\Users\USER\AppData\Local\Programs\Arduino IDE` (configuración en `C:\Users\USER\AppData\Local\Arduino15` y librerías en `C:\Users\USER\Documents\Arduino\libraries`).
  * **Visual Studio:** Instalado en `C:\Program Files\Microsoft Visual Studio`.

---

## 3. Reglas Técnicas Inquebrantables del Hardware (`ESP32-S3-Touch-AMOLED-2.06`)

1. **Conflicto mortal en `GPIO 35`:**
   * En el diseño electrónico de esta placa, la línea `IRQ` del chip de energía **AXP2101** está conectada a `GPIO 35`.
   * Sin embargo, en el chip `ESP32-S3R8`, los pines `GPIO 33` a `GPIO 37` son utilizados internamente por el bus **Octal PSRAM (8 MB)** (`SPIDQS`).
   * **REGLA:** Jamás configurar `pinMode(35, ...)` ni asociar interrupciones a `GPIO 35`. Hacerlo corrompe el bus PSRAM y causa un *Guru Meditation Error / Reboot Loop* instantáneo. El estado del AXP2101 se lee siempre por *polling* vía I2C (`0x34`).
2. **Encendido de Periféricos vía PMIC (`AXP2101`):**
   * La pantalla AMOLED y varios periféricos dependen de los reguladores LDO del **AXP2101** (en el bus I2C `SDA = GPIO 15`, `SCL = GPIO 14`).
   * Si no se inicializa primero el AXP2101, el display no responderá ni encenderá.
3. **Pantalla QSPI (`CO5300` / compatible `SH8601`):**
   * Resolución nativa: **$410 \times 502$ píxeles**.
   * No usa SPI normal de 1 bit, sino **QSPI (Quad SPI)** de 4 líneas de datos (`GPIO 4, 5, 6, 7`), reloj en `GPIO 11`, CS en `GPIO 12`, RST en `GPIO 8` y TE/EN en `GPIO 13`.
   * En el firmware original (log en [`logs/boot_log_fabrica_2026-11-05.txt`](file:///G:/ITM/reloj-inteligente/logs/boot_log_fabrica_2026-11-05.txt)), el driver se inicializa con el componente `sh8601` v1.0.2 / `co5300`.
4. **Preservar Comunicación USB (`COM5`):**
   * El reloj se conecta directamente por los pines USB nativos del ESP32-S3 (`GPIO 19` D-, `GPIO 20` D+) usando el periférico interno `USB-Serial/JTAG`.
   * Siempre compilar con `ARDUINO_USB_CDC_ON_BOOT=1` y `ARDUINO_USB_MODE=1` (Hardware CDC and JTAG) para que el puerto `COM5` siga activo mientras corre nuestro firmware.

---

## 4. Protocolo de Trabajo en cada Sesión

Cada vez que hagamos un cambio en el proyecto, sigue este ciclo:
1. **Consultar:** Revisar [`docs/06-roadmap-y-fases.md`](file:///G:/ITM/reloj-inteligente/docs/06-roadmap-y-fases.md) y [`logs/BITACORA_TECNICA.md`](file:///G:/ITM/reloj-inteligente/logs/BITACORA_TECNICA.md) para ver en qué fase estamos.
2. **Respaldar / Versionar:** Asegurar que el estado previo esté guardado en Git (`git commit`) y que el backup de fábrica exista en `backups/`.
3. **Implementar y Compilar:** Hacer cambios pequeños, aislados y verificables en `firmware/`.
4. **Probar y Monitorear:** Flashear al puerto `COM5` y verificar la salida serie con [`scripts/monitor_serial.ps1`](file:///G:/ITM/reloj-inteligente/scripts/monitor_serial.ps1).
5. **Documentar:** Registrar lo realizado en [`logs/BITACORA_TECNICA.md`](file:///G:/ITM/reloj-inteligente/logs/BITACORA_TECNICA.md) y actualizar [`logs/CHANGELOG.md`](file:///G:/ITM/reloj-inteligente/logs/CHANGELOG.md).

