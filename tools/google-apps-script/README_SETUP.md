# Guía Paso a Paso: Puente de Google Calendar para el Reloj (`ESP32-S3`)

Esta guía explica cómo conectar tu cuenta de **Google Calendar** con tu reloj inteligente **ESP32-S3-Touch-AMOLED-2.06** de forma segura, gratuita y optimizada para no agotar la batería.

---

## 1. ¿Por qué usamos un puente en Google Apps Script? (La analogía del Recepcionista)

Si intentáramos que el reloj se conecte directamente a la API oficial de Google Calendar, tendríamos dos problemas graves de hardware:

1. **El guardián pesado (OAuth 2.0):** Google exige un protocolo de seguridad llamado *OAuth 2.0*, pensado para celulares y PCs con navegador web. Requiere renovar llaves criptográficas grandes cada hora y almacenar certificados pesados.
2. **Exceso de equipaje (Megabytes de datos):** Cuando pides tus eventos a la API estándar de Google, el servidor responde con un archivo gigantesco lleno de información que el reloj no necesita (enlaces de videollamadas, correos de todos los invitados, colores HTML, historial de creación, etc.).

### La solución: Tu recepcionista privado en la nube
**Google Apps Script** es una herramienta oficial y gratuita de tu propia cuenta de Google. Nuestro archivo [`calendar_bridge.gs`](calendar_bridge.gs) actúa como un **recepcionista privado**:

* Vive dentro de tu cuenta de Google (por lo que ya tiene permiso directo para leer tu calendario sin pedir contraseñas cada hora).
* Cuando el reloj enciende su antena Wi-Fi por **2 segundos**, toca la puerta del recepcionista mostrándole una contraseña secreta (`?key=...`).
* El recepcionista lee tu agenda de las próximas **36 horas**, toma como máximo los **5 eventos más próximos**, recorta todo lo innecesario y le entrega al reloj un paquete minúsculo de **~450 bytes** (menos de medio kilobyte) que incluye además la hora exacta del servidor (`server_time`) para mantener el reloj en hora.
* El reloj guarda ese resumen en su memoria y **apaga el Wi-Fi de inmediato** para ahorrar batería.

---

## 2. Configurar el Puente en tu Cuenta de Google (Paso a Paso)

Sigue estos pasos desde tu computador. Solo necesitas hacerlo **una vez** (toma unos 5 minutos):

### Paso 1: Crear el proyecto en Google Apps Script
1. Abre tu navegador e ingresa a **[https://script.google.com](https://script.google.com)** con la cuenta de Google donde tienes tu calendario.
2. Haz clic en el botón **Nuevo proyecto** (*New project*) arriba a la izquierda.
3. Arriba, donde dice *"Proyecto sin título"*, haz clic y cámbiale el nombre a: `Reloj ESP32 Calendar Bridge`.

### Paso 2: Pegar el código del puente
1. En el editor central verás un archivo llamado `Código.gs` con una función vacía `function myFunction() {}`. **Borra todo ese contenido**.
2. Abre el archivo [`tools/google-apps-script/calendar_bridge.gs`](calendar_bridge.gs) de este repositorio, copia todo su contenido y pégalo en el editor de Google Apps Script.

### Paso 3: Elegir tu Token Secreto (`SECRET_TOKEN`)
1. Busca la línea 35 del código que acabas de pegar:
   ```javascript
   const SECRET_TOKEN = "MI_TOKEN_SUPER_SECRETO_RELOJ_2026";
   ```
2. Cambia `"MI_TOKEN_SUPER_SECRETO_RELOJ_2026"` por una contraseña larga inventada por ti (sin espacios ni símbolos raros como `&` o `#`; usa letras, números y guiones bajos, por ejemplo: `"RelojS3_Juan_ClavePrivada_9876"`).
3. Pulsa el ícono del disquete **Guardar proyecto** (o `Ctrl + S`).

### Paso 4: Probar y autorizar el acceso a tu Calendario
1. En la barra superior del editor, junto al botón **Ejecutar**, abre el menú desplegable (que suele decir `doGet`) y cámbialo a **`testLocalBridge`**.
2. Haz clic en **Ejecutar**.
3. Google mostrará una ventana emergente diciendo *"Se necesita autorización"*. Esto es normal porque el script que acabas de crear en tu cuenta pide permiso para leer tu propio calendario:
   * Haz clic en **Revisar permisos**.
   * Elige tu cuenta de Google.
   * Si aparece la pantalla *"Google no ha verificado esta aplicación"* (aparece siempre para scripts personales creados por uno mismo), haz clic abajo a la izquierda en **Configuración avanzada** (*Advanced*) y luego en **Ir a Reloj ESP32 Calendar Bridge (no seguro)**.
   * Haz clic en **Permitir** (*Allow*).
4. En la parte inferior ("Registro de ejecución") verás que imprime el tamaño del JSON (~200 a 450 bytes) y tus eventos próximos reales en formato JSON.

### Paso 5: Publicar como Aplicación Web (Webhook)
1. Arriba a la derecha, haz clic en el botón azul **Implementar** (*Deploy*) > **Nueva implementación** (*New deployment*).
2. Junto a *"Selecciona el tipo"*, haz clic en el ícono de engranaje ⚙️ y selecciona **Aplicación web** (*Web app*).
3. Configura los tres campos exactamente así:
   * **Descripción:** `Puente Calendario v1`
   * **Ejecutar como (*Execute as*):** Selecciona **`Yo (tu_correo@gmail.com)`**. *(Esto hace que el script lea tu calendario usando tu identidad interna).*
   * **Quién tiene acceso (*Who has access*):** Selecciona **`Cualquier persona` (*Anyone*)**.
     > **¿Por qué "Cualquier persona"?** Porque el chip `ESP32-S3` del reloj no tiene un navegador Chrome para iniciar sesión interactivamente con cookies de Google. Al seleccionar *"Cualquier persona"*, permitimos que el reloj llegue a la puerta del script, pero **nuestro código adentro bloquea a cualquiera que no envíe tu `SECRET_TOKEN` exacto en `?key=...`**.
4. Haz clic en **Implementar** (*Deploy*).
5. Copia la **URL de la aplicación web** que te entrega Google. Tendrá este formato:
   ```text
   https://script.google.com/macros/s/AKfycbx.../exec
   ```

### Paso 6: Verificar en tu navegador
1. A la URL que copiaste, agrégale al final `?key=` seguido de tu token secreto:
   ```text
   https://script.google.com/macros/s/AKfycbx.../exec?key=MI_TOKEN_SUPER_SECRETO_RELOJ_2026
   ```
2. Pega esa dirección completa en una pestaña de tu navegador. Deberías ver una respuesta como esta:
   ```json
   {
     "status": "ok",
     "count": 2,
     "server_time": 1775678400,
     "events": [
       {
         "id": "4k8m2n9p1q2r",
         "title": "Reunión de Arquitectura",
         "start": 1775682000,
         "end": 1775685600,
         "loc": "Sala B / Meet",
         "all_day": false
       }
     ]
   }
   ```
3. Si pruebas cambiando una letra del `key` en la URL, verás que responde `{"status":"error","message":"Unauthorized: Invalid or missing key"}`. ¡Tu puente ya está blindado y funcionando!

---

## 3. Configurar tus Credenciales en el Reloj (`wifi_secrets.h`)

Por seguridad profesional, **nunca** escribimos contraseñas de Wi-Fi ni tokens secretos directamente en los archivos del código fuente que se suben a GitHub/Git.

### Cómo activar tus datos reales en el reloj:
1. Ve a la carpeta `firmware/config/` de este proyecto.
2. Verás el archivo plantilla [`firmware/config/wifi_secrets.example.h`](../../firmware/config/wifi_secrets.example.h).
3. Haz una **copia** de ese archivo en la misma carpeta y llámala exactamente:
   ```text
   firmware/config/wifi_secrets.h
   ```
4. Abre tu nuevo `firmware/config/wifi_secrets.h` y rellena tus 3 datos reales:
   * `WIFI_SSID`: El nombre de tu red Wi-Fi de **2.4 GHz** (recuerda que la antena física del chip `ESP32-S3` opera en la banda de 2.4 GHz, no en redes exclusivas de 5 GHz).
   * `WIFI_PASSWORD`: La contraseña de tu Wi-Fi.
   * `GCAL_WEBHOOK_URL`: La URL completa de tu script con `?key=TU_TOKEN` al final.

### Protección automática y Modo Demo (Mock Data)
* **Protección Git (`.gitignore`):** El archivo `firmware/config/wifi_secrets.h` está incluido en la lista negra de `.gitignore`. Así puedes hacer `git commit` con total tranquilidad sin riesgo de filtrar tu clave Wi-Fi ni tu token de calendario.
* **Compilación sin errores (Modo Demo automático):** Si todavía no has creado `firmware/config/wifi_secrets.h` (o si dejas los valores de ejemplo por defecto), el firmware del reloj lo detecta automáticamente al compilar mediante la directiva `#if __has_include("config/wifi_secrets.h")`. En ese caso, en lugar de fallar, el reloj activa automáticamente el **Modo Demo (Mock Data)** mostrando eventos de demostración para que puedas probar la interfaz táctil AMOLED desde el primer minuto.
