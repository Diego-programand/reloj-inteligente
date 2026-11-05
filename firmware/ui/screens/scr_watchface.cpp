/**
 * @file scr_watchface.cpp
 * @brief Implementación visual de la Watchface personalizada en LVGL (410x502).
 */

#include "scr_watchface.h"

static lv_obj_t *lblTopStatus    = nullptr;
static lv_obj_t *lblTopBatt      = nullptr;
static lv_obj_t *arcSeconds      = nullptr;
static lv_obj_t *lblDateStr      = nullptr;
static lv_obj_t *lblTimeMain     = nullptr;
static lv_obj_t *lblSecondsAmPm  = nullptr;
static lv_obj_t *lblCardVolt     = nullptr;
static lv_obj_t *lblCardBright   = nullptr;

static bool use24HourFormat = false; // Por defecto formato 12H con AM/PM (tocar la hora cambia a 24H)
static WatchDateTime lastTime = {2026, 11, 5, 4, 16, 50, 0, true};

static const char* DIAS_SEMANA[] = {
    "DOMINGO", "LUNES", "MARTES", "MIERCOLES", "JUEVES", "VIERNES", "SABADO"
};

static const char* MESES_ABREV[] = {
    "---", "ENE", "FEB", "MAR", "ABR", "MAY", "JUN",
    "JUL", "AGO", "SEP", "OCT", "NOV", "DIC"
};

/**
 * @brief Al tocar el centro de la esfera del reloj, alterna entre formato 12H (AM/PM) y 24H.
 */
static void watchface_center_click_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        use24HourFormat = !use24HourFormat;
        scr_watchface_update_time(lastTime);
    }
}

void scr_watchface_build(lv_obj_t *parentTile) {
    // 1. Barra superior: Estado del RTC a la izquierda y Batería a la derecha
    lblTopStatus = lv_label_create(parentTile);
    lv_obj_set_style_text_font(lblTopStatus, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblTopStatus, lv_color_hex(0x00E676), 0);
    lv_label_set_text(lblTopStatus, "RTC ACTIVO");
    lv_obj_align(lblTopStatus, LV_ALIGN_TOP_LEFT, 24, 16);

    lblTopBatt = lv_label_create(parentTile);
    lv_obj_set_style_text_font(lblTopBatt, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lblTopBatt, lv_color_hex(0x00E5FF), 0);
    lv_label_set_text(lblTopBatt, "BAT 100%");
    lv_obj_align(lblTopBatt, LV_ALIGN_TOP_RIGHT, -24, 14);

    // 2. Anillo circular central de segundos (304 x 304 px, números pares para CO5300)
    arcSeconds = lv_arc_create(parentTile);
    lv_obj_set_size(arcSeconds, 304, 304);
    lv_arc_set_rotation(arcSeconds, 270); // Empezar arriba del todo (las 12 en punto)
    lv_arc_set_bg_angles(arcSeconds, 0, 360);
    lv_arc_set_range(arcSeconds, 0, 59);
    lv_arc_set_value(arcSeconds, 0);
    lv_obj_remove_style(arcSeconds, nullptr, LV_PART_KNOB);   // Sin bolita para que luzca como reloj puro
    lv_obj_clear_flag(arcSeconds, LV_OBJ_FLAG_CLICKABLE);     // No arrastrable con el dedo
    lv_obj_set_style_arc_width(arcSeconds, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arcSeconds, lv_color_hex(0x16202C), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arcSeconds, 12, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arcSeconds, lv_color_hex(0x00E5FF), LV_PART_INDICATOR);
    lv_obj_align(arcSeconds, LV_ALIGN_TOP_MID, 0, 48);

    // Contenedor invisible clicable en el centro del dial para cambiar entre 12H y 24H
    lv_obj_t *centerBox = lv_obj_create(parentTile);
    lv_obj_set_size(centerBox, 250, 180);
    lv_obj_align(centerBox, LV_ALIGN_TOP_MID, 0, 110);
    lv_obj_set_style_bg_opa(centerBox, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(centerBox, 0, 0);
    lv_obj_clear_flag(centerBox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(centerBox, watchface_center_click_cb, LV_EVENT_CLICKED, nullptr);

    // 3. Fecha y Día de la semana (arriba de la hora)
    lblDateStr = lv_label_create(centerBox);
    lv_obj_set_style_text_font(lblDateStr, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lblDateStr, lv_color_hex(0xFFD54F), 0);
    lv_label_set_text(lblDateStr, "JUEVES, 05 NOV");
    lv_obj_align(lblDateStr, LV_ALIGN_TOP_MID, 0, 6);

    // 4. Hora y Minutos principales en fuente grande (48 pt)
    lblTimeMain = lv_label_create(centerBox);
    lv_obj_set_style_text_font(lblTimeMain, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lblTimeMain, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(lblTimeMain, "04:50");
    lv_obj_align(lblTimeMain, LV_ALIGN_CENTER, 0, -4);

    // 5. Segundos y formato AM/PM o 24H (debajo de la hora)
    lblSecondsAmPm = lv_label_create(centerBox);
    lv_obj_set_style_text_font(lblSecondsAmPm, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lblSecondsAmPm, lv_color_hex(0x00E5FF), 0);
    lv_label_set_text(lblSecondsAmPm, ":00 PM");
    lv_obj_align(lblSecondsAmPm, LV_ALIGN_BOTTOM_MID, 0, -4);

    // 6. Dos tarjetas inferiores de resumen ("Complicaciones" del reloj)
    lv_obj_t *cardLeft = lv_obj_create(parentTile);
    lv_obj_set_size(cardLeft, 178, 86);
    lv_obj_align(cardLeft, LV_ALIGN_BOTTOM_LEFT, 18, -36);
    lv_obj_set_style_bg_color(cardLeft, lv_color_hex(0x111822), 0);
    lv_obj_set_style_border_color(cardLeft, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_radius(cardLeft, 16, 0);
    lv_obj_clear_flag(cardLeft, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lblLeftTitle = lv_label_create(cardLeft);
    lv_obj_set_style_text_font(lblLeftTitle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblLeftTitle, lv_color_hex(0x90A4AE), 0);
    lv_label_set_text(lblLeftTitle, "ENERGIA");
    lv_obj_align(lblLeftTitle, LV_ALIGN_TOP_MID, 0, -2);

    lblCardVolt = lv_label_create(cardLeft);
    lv_obj_set_style_text_font(lblCardVolt, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lblCardVolt, lv_color_hex(0x00E676), 0);
    lv_label_set_text(lblCardVolt, "4110 mV");
    lv_obj_align(lblCardVolt, LV_ALIGN_BOTTOM_MID, 0, 2);

    lv_obj_t *cardRight = lv_obj_create(parentTile);
    lv_obj_set_size(cardRight, 178, 86);
    lv_obj_align(cardRight, LV_ALIGN_BOTTOM_RIGHT, -18, -36);
    lv_obj_set_style_bg_color(cardRight, lv_color_hex(0x111822), 0);
    lv_obj_set_style_border_color(cardRight, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_radius(cardRight, 16, 0);
    lv_obj_clear_flag(cardRight, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lblRightTitle = lv_label_create(cardRight);
    lv_obj_set_style_text_font(lblRightTitle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblRightTitle, lv_color_hex(0x90A4AE), 0);
    lv_label_set_text(lblRightTitle, "PANTALLA");
    lv_obj_align(lblRightTitle, LV_ALIGN_TOP_MID, 0, -2);

    lblCardBright = lv_label_create(cardRight);
    lv_obj_set_style_text_font(lblCardBright, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lblCardBright, lv_color_hex(0xFFD54F), 0);
    lv_label_set_text(lblCardBright, "Brillo 86%");
    lv_obj_align(lblCardBright, LV_ALIGN_BOTTOM_MID, 0, 2);

    // 7. Guía inferior de navegación por gestos
    lv_obj_t *lblSwipeHint = lv_label_create(parentTile);
    lv_obj_set_style_text_font(lblSwipeHint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblSwipeHint, lv_color_hex(0x607D8B), 0);
    lv_label_set_text(lblSwipeHint, "Toca la hora (12/24h) | Desliza a Ajustes >>");
    lv_obj_align(lblSwipeHint, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void scr_watchface_update_time(const WatchDateTime &dt) {
    lastTime = dt;

    if (arcSeconds) {
        lv_arc_set_value(arcSeconds, dt.second);
    }

    if (lblDateStr) {
        uint8_t wday = (dt.weekday <= 6) ? dt.weekday : 0;
        uint8_t mon  = (dt.month >= 1 && dt.month <= 12) ? dt.month : 1;
        lv_label_set_text_fmt(lblDateStr, "%s, %02u %s", DIAS_SEMANA[wday], dt.day, MESES_ABREV[mon]);
    }

    if (lblTimeMain && lblSecondsAmPm) {
        if (use24HourFormat) {
            lv_label_set_text_fmt(lblTimeMain, "%02u:%02u", dt.hour, dt.minute);
            lv_label_set_text_fmt(lblSecondsAmPm, ":%02u 24H", dt.second);
        } else {
            bool isPm = (dt.hour >= 12);
            uint8_t h12 = dt.hour % 12;
            if (h12 == 0) h12 = 12;
            lv_label_set_text_fmt(lblTimeMain, "%02u:%02u", h12, dt.minute);
            lv_label_set_text_fmt(lblSecondsAmPm, ":%02u %s", dt.second, isPm ? "PM" : "AM");
        }
    }
}

void scr_watchface_update_power(const WatchPowerStatus &power, uint8_t brightness) {
    if (lblTopBatt) {
        lv_label_set_text_fmt(lblTopBatt, "BAT %d%%", power.batteryPercent);
    }
    if (lblCardVolt) {
        lv_label_set_text_fmt(
            lblCardVolt,
            "%u mV %s",
            power.batteryVoltageMv,
            power.usbConnected ? "USB" : "BAT"
        );
    }
    if (lblCardBright) {
        int pct = (brightness * 100) / 255;
        lv_label_set_text_fmt(lblCardBright, "Brillo %d%%", pct);
    }
}

