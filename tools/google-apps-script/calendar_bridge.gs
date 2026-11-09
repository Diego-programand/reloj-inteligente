/**
 * ============================================================================
 * PUENTE LIGERO DE GOOGLE CALENDAR PARA ESP32-S3 (calendar_bridge.gs)
 * ============================================================================
 *
 * ¿QUÉ HACE ESTE ARCHIVO Y POR QUÉ LO NECESITAMOS?
 * ------------------------------------------------
 * La API oficial de Google Calendar está pensada para computadores y teléfonos:
 * exige una negociación criptográfica compleja llamada OAuth 2.0 (con tokens
 * que caducan cada hora) y devuelve archivos JSON gigantescos (decenas de KB)
 * llenos de metadatos como colores, enlaces de Meet, historiales y permisos.
 *
 * Para el microcontrolador ESP32-S3 de nuestro reloj inteligente, procesar todo
 * eso significa mantener la antena Wi-Fi encendida más tiempo (gastando batería)
 * y consumir memoria RAM innecesariamente.
 *
 * ANALOGÍA DE HARDWARE:
 * Piensa en este script de Google Apps Script como un "recepcionista privado"
 * que vive gratis dentro de tu propia cuenta de Google:
 *   1. El reloj toca la puerta enviando una llave secreta (?key=...).
 *   2. El recepcionista verifica la llave, abre tu Google Calendar internamente,
 *      filtra solo los próximos 5 eventos de las siguientes 36 horas y recorta
 *      todo el texto sobrante.
 *   3. Le entrega al reloj un resumen ultracompacto (< 500 bytes) listo para
 *      mostrarse en la pantalla AMOLED y almacenarse en la memoria del reloj.
 */

// ============================================================================
// 1. CONFIGURACIÓN DE SEGURIDAD Y LÍMITES
// ============================================================================

/**
 * Token secreto compartido entre este script en la nube y tu archivo local
 * `firmware/config/wifi_secrets.h` en el reloj.
 *
 * IMPORTANTE: Cámbialo por una frase o cadena larga que solo tú conozcas.
 * Cualquier petición que llegue sin `?key=TU_TOKEN` será rechazada al instante.
 */
const SECRET_TOKEN = "MI_TOKEN_SUPER_SECRETO_RELOJ_2026";

/**
 * Ventana de tiempo hacia el futuro que consultará el reloj (en horas).
 * 36 horas permiten ver toda la jornada de hoy y la mañana/tarde de mañana.
 */
const LOOKAHEAD_HOURS = 36;

/**
 * Cantidad máxima de eventos a empaquetar para mantener el JSON por debajo de
 * ~500 bytes y lograr una descarga Wi-Fi instantánea (< 2 segundos).
 */
const MAX_EVENTS = 5;

/**
 * Límites de caracteres por campo para evitar desbordar la pantalla AMOLED
 * (410x502) y proteger el búfer de memoria del microcontrolador.
 */
const MAX_TITLE_LEN = 36;
const MAX_LOC_LEN = 24;

// ============================================================================
// 2. PUNTO DE ENTRADA HTTP GET (Webhook que llama el reloj ESP32-S3)
// ============================================================================

/**
 * Función especial de Google Apps Script que se ejecuta automáticamente cada
 * vez que el reloj (o tu navegador) hace una petición HTTP GET a la URL de la
 * aplicación web.
 *
 * @param {Object} e Objeto de evento inyectado por Google con los parámetros URL.
 * @return {TextOutput} Respuesta en formato JSON estándar (application/json).
 */
function doGet(e) {
  try {
    // ------------------------------------------------------------------------
    // PASO A: Verificación del candado de seguridad (Token Secreto)
    // ------------------------------------------------------------------------
    // Revisamos si la URL incluye `?key=...` y si coincide exactamente con
    // nuestro SECRET_TOKEN. Si no coincide, devolvemos error y no leemos nada.
    const providedKey = (e && e.parameter && e.parameter.key) ? String(e.parameter.key) : "";
    if (providedKey !== SECRET_TOKEN) {
      return buildJsonResponse_({
        status: "error",
        message: "Unauthorized: Invalid or missing key"
      });
    }

    // ------------------------------------------------------------------------
    // PASO B: Cálculo de la ventana de tiempo (Ahora -> Ahora + 36 horas)
    // ------------------------------------------------------------------------
    // En sistemas embebidos trabajamos con "Unix Epoch" (segundos transcurridos
    // desde el 1 de enero de 1970 UTC). Es un número entero de 32/64 bits muy
    // fácil de comparar en C++ sin sufrir con formatos de texto complejos.
    const now = new Date();
    const futureLimit = new Date(now.getTime() + (LOOKAHEAD_HOURS * 60 * 60 * 1000));
    const serverTimeEpoch = Math.floor(now.getTime() / 1000);

    // ------------------------------------------------------------------------
    // PASO C: Consulta al calendario principal de tu cuenta de Google
    // ------------------------------------------------------------------------
    const calendar = CalendarApp.getDefaultCalendar();
    if (!calendar) {
      return buildJsonResponse_({
        status: "error",
        message: "Default calendar not found"
      });
    }

    // Obtenemos todos los eventos dentro de las próximas 36 horas.
    // Google los entrega ordenados cronológicamente por hora de inicio.
    const rawEvents = calendar.getEvents(now, futureLimit);

    // ------------------------------------------------------------------------
    // PASO D: Filtrado y compresión de eventos para el reloj
    // ------------------------------------------------------------------------
    const compactEvents = [];

    for (let i = 0; i < rawEvents.length && compactEvents.length < MAX_EVENTS; i++) {
      const ev = rawEvents[i];

      // Convertimos las fechas de inicio y fin a segundos Unix Epoch.
      const startEpoch = Math.floor(ev.getStartTime().getTime() / 1000);
      const endEpoch = Math.floor(ev.getEndTime().getTime() / 1000);

      // Si un evento ya terminó justo en este segundo, lo ignoramos.
      if (endEpoch <= serverTimeEpoch) {
        continue;
      }

      // Recortamos el ID único a 12 caracteres (quitando el sufijo @google.com)
      // para que el reloj pueda detectar si un evento cambió sin gastar RAM.
      const rawId = String(ev.getId() || "").split("@")[0];
      const shortId = rawId.substring(0, 12);

      // Limpiamos saltos de línea y recortamos título y ubicación.
      const cleanTitle = sanitizeAndTrim_(ev.getTitle() || "Sin título", MAX_TITLE_LEN);
      const cleanLoc = sanitizeAndTrim_(ev.getLocation() || "", MAX_LOC_LEN);

      compactEvents.push({
        id: shortId,
        title: cleanTitle,
        start: startEpoch,
        end: endEpoch,
        loc: cleanLoc,
        all_day: Boolean(ev.isAllDayEvent())
      });
    }

    // ------------------------------------------------------------------------
    // PASO E: Empaquetado final del JSON (< 500 bytes típicos)
    // ------------------------------------------------------------------------
    // Incluimos `server_time` para que el reloj también pueda validar/ajustar
    // su reloj interno (RTC PCF85063) con la hora atómica de los servidores de Google.
    const payload = {
      status: "ok",
      count: compactEvents.length,
      server_time: serverTimeEpoch,
      events: compactEvents
    };

    return buildJsonResponse_(payload);

  } catch (err) {
    // Si algo inesperado falla en la nube, respondemos siempre en formato JSON
    // válido para que el parser del reloj (ArduinoJson) nunca falle al leerlo.
    return buildJsonResponse_({
      status: "error",
      message: String(err && err.message ? err.message : err)
    });
  }
}

// ============================================================================
// 3. FUNCIONES AUXILIARES INTERNAS
// ============================================================================

/**
 * Limpia espacios redundantes o saltos de línea y recorta una cadena de texto
 * a una longitud máxima segura para la pantalla del reloj.
 *
 * @param {string} text Texto original del evento.
 * @param {number} maxLen Cantidad máxima de caracteres permitidos.
 * @return {string} Texto limpio y recortado.
 */
function sanitizeAndTrim_(text, maxLen) {
  const clean = String(text)
    .replace(/[\r\n\t]+/g, " ")
    .replace(/\s+/g, " ")
    .trim();

  if (clean.length <= maxLen) {
    return clean;
  }
  return clean.substring(0, maxLen - 1).trim() + "…";
}

/**
 * Envuelve un objeto JavaScript en una respuesta HTTP con cabecera MIME JSON.
 *
 * @param {Object} data Objeto a serializar como JSON.
 * @return {TextOutput} Objeto TextOutput de Google Apps Script.
 */
function buildJsonResponse_(data) {
  return ContentService
    .createTextOutput(JSON.stringify(data))
    .setMimeType(ContentService.MimeType.JSON);
}

// ============================================================================
// 4. FUNCIÓN DE PRUEBA LOCAL (Para probar desde el editor de Google)
// ============================================================================

/**
 * Ejecuta esta función directamente en el editor de Google Apps Script
 * (seleccionando `testLocalBridge` en la barra superior y pulsando "Ejecutar")
 * para autorizar permisos por primera vez y ver en el registro cómo luce el
 * JSON real que recibirá tu reloj ESP32-S3.
 */
function testLocalBridge() {
  const fakeEvent = {
    parameter: {
      key: SECRET_TOKEN
    }
  };
  const response = doGet(fakeEvent);
  const jsonString = response.getContent();
  Logger.log("Tamaño del JSON generado: " + jsonString.length + " bytes");
  Logger.log("Contenido JSON:\n" + jsonString);
}
