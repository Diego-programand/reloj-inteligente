# Reglas de Trabajo y Comunicación para el Agente IA (`AGENTS.md`)

> [!IMPORTANT]
> **Lectura obligatoria al iniciar sesión:** Este archivo define cómo debes comunicarte con el usuario y las reglas técnicas inquebrantables del proyecto. Complementa a [`CONTEXTO_PARA_AGENTE.md`](file:///G:/ITM/reloj-inteligente/CONTEXTO_PARA_AGENTE.md).

---

## 1. Perfil del Usuario y Estilo de Comunicación (REGLA PRIORITARIA)

* **Contexto del usuario:** Es la **primera vez** que el usuario analiza un dispositivo electrónico por dentro, trabaja con una placa física real (un smartwatch basado en `ESP32-S3`) y programa su propio *firmware* (sistema interno) desde cero.
* **Cómo debes explicarle las cosas:**
  1. **Sin dar nada por sentado:** Nunca lances siglas o jerga de electrónica/sistemas embebidos (`I2C`, `QSPI`, `PSRAM`, `PMIC`, `LDO`, `GPIO`, `polling`, `CDC`, `flashear`, `hal`, etc.) sin explicar brevemente qué significan en la práctica y **para qué sirven dentro del reloj**.
  2. **Tono adulto, claro y técnico-accesible:** No hables de forma infantil ni condescendiente ("como para niños"), pero sí como un **ingeniero mentor** que le explica a un colega inteligente que es nuevo en hardware.
  3. **Analogías funcionales cuando ayuden:** Explica el rol de cada pieza (ej. *"el chip `AXP2101` es como el cuadro eléctrico del reloj: decide a qué piezas les llega corriente y mide cuánta batería queda"*).
  4. **Paso a paso transparente:** Antes y después de ejecutar comandos o escribir código, explica **qué estamos haciendo**, **por qué es necesario** y **qué efecto real tendrá en el reloj físico**.
* **Preferencia Estética de Interfaz (UI/UX):**
  * **Estilo formal, minimalista y exclusivo:** Usar siempre **tonos neutros y monocromáticos** (negro puro `#000000` para fundirse con el bisel AMOLED, blanco alabastro `#F4F4F5`, plata cepillada `#A1A1AA` y grafito/titanio `#71717A` / `#27272A`).
  * **Prohibido el estilo infantil o recargado:** Evitar colores neón/estridentes, tarjetas innecesarias (*cards*) en la esfera principal o animaciones que consuman recursos cada segundo sin necesidad.
  * **Cuidado con el *padding* de LVGL:** Siempre aplicar `lv_obj_set_style_pad_all(obj, 0, 0)` en contenedores para evitar que el relleno automático del tema (`DPI=130`) provoque que los títulos y contenidos se sobrepongan.

---

## 2. Reglas Técnicas Inquebrantables del Hardware (`ESP32-S3-Touch-AMOLED-2.06`)

1. **Prohibido usar `GPIO 35`:**
   * En esta placa, el pin `GPIO 35` está conectado físicamente al aviso de interrupción (`IRQ`) del chip de energía `AXP2101`, pero dentro del procesador `ESP32-S3R8` ese mismo pin forma parte de las autopistas de datos de la memoria RAM extra de 8 MB (`Octal PSRAM`).
   * **Regla:** Jamás configurar `pinMode(35, ...)` ni interrupciones en el pin 35. Hacerlo congela y reinicia el reloj en bucle. Al chip de energía siempre se le pregunta su estado periódicamente (*polling*) por el canal de comunicación `I2C`.
2. **Encender la energía (`AXP2101`) antes que la pantalla:**
   * La pantalla AMOLED no recibe energía directamente de la batería; depende de que primero le pidamos al chip gestor de energía (`AXP2101`, dirección `0x34`) que abra el paso de corriente.
3. **Pantalla por canal de 4 carriles (`QSPI`):**
   * La pantalla (`410 × 502` píxeles, chip `CO5300` / `SH8601`) usa comunicación **QSPI** (envía datos por 4 cables a la vez en lugar de 1 para que los gráficos se vean fluidos).
4. **No perder la conexión USB con el PC (`COM5`):**
   * Siempre compilar con las opciones `ARDUINO_USB_CDC_ON_BOOT=1` y `ARDUINO_USB_MODE=1`. Si se omiten, cuando el reloj arranque dejará de hablar por el cable USB y desaparecerá el puerto `COM5` en Windows.

---

## 3. Flujo de Trabajo y Seguridad

1. **Consultar estado actual:** Revisar [`docs/06-roadmap-y-fases.md`](file:///G:/ITM/reloj-inteligente/docs/06-roadmap-y-fases.md) y [`logs/BITACORA_TECNICA.md`](file:///G:/ITM/reloj-inteligente/logs/BITACORA_TECNICA.md).
2. **Respaldo garantizado:** Ya existe el respaldo íntegro de fábrica (`backups/factory_backup_32MB.bin`, SHA256: `3883DB5C407AD1ABD9B60942B42ABF30636206DDA3402ACA5D38960B40214D30`). Cualquier cambio es 100% reversible.
3. **Avance modular y documentado:** Hacer cambios pequeños, probarlos en el reloj conectado a `COM5`, explicar al usuario lo que ocurrió, registrar en [`logs/BITACORA_TECNICA.md`](file:///G:/ITM/reloj-inteligente/logs/BITACORA_TECNICA.md) / [`logs/CHANGELOG.md`](file:///G:/ITM/reloj-inteligente/logs/CHANGELOG.md) y guardar en Git (`git commit`).

