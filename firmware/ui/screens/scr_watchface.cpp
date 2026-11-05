/**
 * @file scr_watchface.cpp
 * @brief Implementación visual de la Watchface personalizada en LVGL (410x502).
 *
 * Optimización de recursos y diseño limpio:
 *   - Sin barra circular de segundos (ahorra redibujado de pantalla cada segundo).
 *   - Solo se redibuja el texto de la hora principal (HH:MM) cuando cambia el minuto,
 *     y cada segundo únicamente se actualiza la pequeña etiqueta de segundos (:SS).
 *   - Todos los contenedores usan `pad_all = 0` y coordenadas explícitas con
 *     amplia separación vertical para evitar que títulos y valores se sobrepongan.
 */

#include "scr_watchface.h"

static lv_obj_t *lblTopStatus     = nullptr;
static lv_obj_t *lblTopBatt       = nullptr;
static lv_obj_t *lblDateStr       = nullptr;
static lv_obj_t *lblTimeMain      = nullptr;
static lv_obj_t *lblSecondsAmPm   = nullptr;
static lv_obj_t *lblCardVolt      = nullptr;
static lv_obj_t *lblCardPowerSrc  = nullptr;
static lv_obj_t *lblCardBright    = nullptr;
static lv_obj_t *lblCardBrightSub = nullptr;

static bool use24HourFormat = false; // Por defecto formato 12H con AM/PM (tocar la hora cambia a 24H)
static WatchDateTime lastTime = {2026, 11, 5, 4, 16, 50, 0, true};

// Cache para redibujar la hora/fecha solo cuando realmente cambian (ahorro de CPU/energía)
static int16_t renderedHour   = -1;
static int16_t renderedMinute = -1;
static int16_t renderedDay    = -1;
static bool    renderedFormat = true;

static const char* DIAS_SEMANA[] = {
    "DOMINGO", "LUNES", "MARTES", "MIERCOLES", "JUEVES", "VIERNES", "SABADO"
};

static const char* MESES_ABREV[] = {
    "---", "ENE", "FEB", "MAR", "ABR", "MAY", "JUN",
    "JUL", "AGO", "SEP", "OCT", "NOV", "DIC"
};

/**
 * @brief Al tocar la tarjeta del reloj, alterna entre formato 12H (AM/PM) y 24H.
 */
static void watchface_center_click_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        use24HourFormat = !use24HourFormat;
        scr_watchface_update_time(lastTime);
    }
}

void scr_watchface_build(lv_obj_t *parentTile) {
    // Eliminamos el padding interno por defecto del tile para posicionar con precisión exacta
    lv_obj_set_style_pad_all(parentTile, 0, 0);

    // 1. Barra superior (Y = 16..36): Estado del RTC a la izquierda y Batería a la derecha
    lblTopStatus = lv_label_create(parentTile);
    lv_obj_set_style_text_font(lblTopStatus, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblTopStatus, lv_color_hex(0x00E676), 0);
    lv_label_set_text(lblTopStatus, "RTC ACTIVO");
    lv_obj_align(lblTopStatus, LV_ALIGN_TOP_LEFT, 24, 18);

    lblTopBatt = lv_label_create(parentTile);
    lv_obj_set_style_text_font(lblTopBatt, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lblTopBatt, lv_color_hex(0x00E5FF), 0);
    lv_label_set_text(lblTopBatt, "BAT 100%");
    lv_obj_align(lblTopBatt, LV_ALIGN_TOP_RIGHT, -24, 16);

    // 2. Tarjeta Principal del Reloj (362 x 220 px, en Y = 54..274)
    //    Sin barra circular de segundos para no gastar CPU ni batería redibujando un arco.
    lv_obj_t *cardClock = lv_obj_create(parentTile);
    lv_obj_set_size(cardClock, 362, 220);
    lv_obj_align(cardClock, LV_ALIGN_TOP_MID, 0, 54);
    lv_obj_set_style_pad_all(cardClock, 0, 0); // Cero padding automático: control total
    lv_obj_set_style_bg_color(cardClock, lv_color_hex(0x0D131A), 0);
    lv_obj_set_style_border_color(cardClock, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(cardClock, 2, 0);
    lv_obj_set_style_radius(cardClock, 24, 0);
    lv_obj_clear_flag(cardClock, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(cardClock, watchface_center_click_cb, LV_EVENT_CLICKED, nullptr);

    // 2A. Fecha y Día de la semana (Y = 22 dentro de cardClock)
    lblDateStr = lv_label_create(cardClock);
    lv_obj_set_style_text_font(lblDateStr, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lblDateStr, lv_color_hex(0xFFD54F), 0);
    lv_label_set_text(lblDateStr, "JUEVES, 05 NOV");
    lv_obj_align(lblDateStr, LV_ALIGN_TOP_MID, 0, 22);

    // 2B. Hora y Minutos principales en fuente gigante (48 pt, centrado en Y = 80..132)
    lblTimeMain = lv_label_create(cardClock);
    lv_obj_set_style_text_font(lblTimeMain, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lblTimeMain, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(lblTimeMain, "04:50");
    lv_obj_align(lblTimeMain, LV_ALIGN_CENTER, 0, -2);

    // 2C. Indicador limpio de AM/PM o 24H (sin segundero para ahorrar CPU y pantalla)
    lblSecondsAmPm = lv_label_create(cardClock);
    lv_obj_set_style_text_font(lblSecondsAmPm, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lblSecondsAmPm, lv_color_hex(0x00E5FF), 0);
    lv_label_set_text(lblSecondsAmPm, "PM | 12H");
    lv_obj_align(lblSecondsAmPm, LV_ALIGN_BOTTOM_MID, 0, -22);

    // 3. Dos tarjetas inferiores de resumen (174 x 124 px cada una, en Y = 294..418)
    // --- Tarjeta Izquierda: ENERGIA ---
    lv_obj_t *cardLeft = lv_obj_create(parentTile);
    lv_obj_set_size(cardLeft, 174, 124);
    lv_obj_align(cardLeft, LV_ALIGN_TOP_LEFT, 24, 294);
    lv_obj_set_style_pad_all(cardLeft, 0, 0); // Evita que el padding del tema pise los textos
    lv_obj_set_style_bg_color(cardLeft, lv_color_hex(0x111822), 0);
    lv_obj_set_style_border_color(cardLeft, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(cardLeft, 2, 0);
    lv_obj_set_style_radius(cardLeft, 18, 0);
    lv_obj_clear_flag(cardLeft, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lblLeftTitle = lv_label_create(cardLeft);
    lv_obj_set_style_text_font(lblLeftTitle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblLeftTitle, lv_color_hex(0x90A4AE), 0);
    lv_label_set_text(lblLeftTitle, "ENERGIA");
    lv_obj_align(lblLeftTitle, LV_ALIGN_TOP_MID, 0, 14);

    lblCardVolt = lv_label_create(cardLeft);
    lv_obj_set_style_text_font(lblCardVolt, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lblCardVolt, lv_color_hex(0x00E676), 0);
    lv_label_set_text(lblCardVolt, "4110 mV");
    lv_obj_align(lblCardVolt, LV_ALIGN_TOP_MID, 0, 46);

    lblCardPowerSrc = lv_label_create(cardLeft);
    lv_obj_set_style_text_font(lblCardPowerSrc, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblCardPowerSrc, lv_color_hex(0xB0BEC5), 0);
    lv_label_set_text(lblCardPowerSrc, "USB Conectado");
    lv_obj_align(lblCardPowerSrc, LV_ALIGN_TOP_MID, 0, 88);

    // --- Tarjeta Derecha: PANTALLA ---
    lv_obj_t *cardRight = lv_obj_create(parentTile);
    lv_obj_set_size(cardRight, 174, 124);
    lv_obj_align(cardRight, LV_ALIGN_TOP_RIGHT, -24, 294);
    lv_obj_set_style_pad_all(cardRight, 0, 0); // Evita que el padding del tema pise los textos
    lv_obj_set_style_bg_color(cardRight, lv_color_hex(0x111822), 0);
    lv_obj_set_style_border_color(cardRight, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_width(cardRight, 2, 0);
    lv_obj_set_style_radius(cardRight, 18, 0);
    lv_obj_clear_flag(cardRight, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lblRightTitle = lv_label_create(cardRight);
    lv_obj_set_style_text_font(lblRightTitle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblRightTitle, lv_color_hex(0x90A4AE), 0);
    lv_label_set_text(lblRightTitle, "PANTALLA");
    lv_obj_align(lblRightTitle, LV_ALIGN_TOP_MID, 0, 14);

    lblCardBright = lv_label_create(cardRight);
    lv_obj_set_style_text_font(lblCardBright, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lblCardBright, lv_color_hex(0xFFD54F), 0);
    lv_label_set_text(lblCardBright, "86%");
    lv_obj_align(lblCardBright, LV_ALIGN_TOP_MID, 0, 46);

    lblCardBrightSub = lv_label_create(cardRight);
    lv_obj_set_style_text_font(lblCardBrightSub, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblCardBrightSub, lv_color_hex(0xB0BEC5), 0);
    lv_label_set_text(lblCardBrightSub, "Brillo AMOLED");
    lv_obj_align(lblCardBrightSub, LV_ALIGN_TOP_MID, 0, 88);

    // 4. Guía inferior de navegación por gestos (Y = 460)
    lv_obj_t *lblSwipeHint = lv_label_create(parentTile);
    lv_obj_set_style_text_font(lblSwipeHint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblSwipeHint, lv_color_hex(0x607D8B), 0);
    lv_label_set_text(lblSwipeHint, "Toca la hora (12/24h) | Desliza a Ajustes >>");
    lv_obj_align(lblSwipeHint, LV_ALIGN_BOTTOM_MID, 0, -18);

    // Forzar primer pintado completo
    renderedHour   = -1;
    renderedMinute = -1;
    renderedDay    = -1;
}

void scr_watchface_update_time(const WatchDateTime &dt) {
    lastTime = dt;

    // Si no cambió el día, ni la hora, ni el minuto, ni el formato 12/24H,
    // no redibujamos nada en absoluto (ahorro total de recursos durante 59 de cada 60 seg).
    if (dt.day == renderedDay &&
        dt.hour == renderedHour &&
        dt.minute == renderedMinute &&
        use24HourFormat == renderedFormat) {
        return;
    }

    // Solo redibujar la fecha si cambió el día
    if (lblDateStr && dt.day != renderedDay) {
        renderedDay  = dt.day;
        uint8_t wday = (dt.weekday <= 6) ? dt.weekday : 0;
        uint8_t mon  = (dt.month >= 1 && dt.month <= 12) ? dt.month : 1;
        lv_label_set_text_fmt(lblDateStr, "%s, %02u %s", DIAS_SEMANA[wday], dt.day, MESES_ABREV[mon]);
    }

    renderedHour   = dt.hour;
    renderedMinute = dt.minute;
    renderedFormat = use24HourFormat;

    if (lblTimeMain && lblSecondsAmPm) {
        if (use24HourFormat) {
            lv_label_set_text_fmt(lblTimeMain, "%02u:%02u", dt.hour, dt.minute);
            lv_label_set_text(lblSecondsAmPm, "FORMATO 24H");
        } else {
            bool isPm = (dt.hour >= 12);
            uint8_t h12 = dt.hour % 12;
            if (h12 == 0) h12 = 12;
            lv_label_set_text_fmt(lblTimeMain, "%02u:%02u", h12, dt.minute);
            lv_label_set_text_fmt(lblSecondsAmPm, "%s | 12H", isPm ? "PM" : "AM");
        }
    }
}

void scr_watchface_update_power(const WatchPowerStatus &power, uint8_t brightness) {
    if (lblTopBatt) {
        lv_label_set_text_fmt(lblTopBatt, "BAT %d%%", power.batteryPercent);
    }
    if (lblCardVolt) {
        lv_label_set_text_fmt(lblCardVolt, "%u mV", power.batteryVoltageMv);
    }
    if (lblCardPowerSrc) {
        if (power.isCharging) {
            lv_label_set_text(lblCardPowerSrc, "Cargando...");
        } else if (power.usbConnected) {
            lv_label_set_text(lblCardPowerSrc, "USB Conectado");
        } else {
            lv_label_set_text(lblCardPowerSrc, "En Bateria");
        }
    }
    if (lblCardBright) {
        int pct = (brightness * 100) / 255;
        lv_label_set_text_fmt(lblCardBright, "%d%%", pct);
    }
}
