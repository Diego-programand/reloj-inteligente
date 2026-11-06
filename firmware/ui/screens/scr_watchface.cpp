/**
 * @file scr_watchface.cpp
 * @brief Watchface minimalista, formal y exclusiva (410x502 AMOLED).
 *
 * Filosofía de diseño:
 *   - Fondo negro puro (#000000) para fundirse con el bisel físico de la pantalla AMOLED.
 *   - Sin tarjetas (cards), sin barras de estado, sin colores neón ni elementos superfluos.
 *   - Formato exclusivo de 12 horas con indicador AM/PM.
 *   - Paleta monocromática de alta relojería: blanco alabastro (#F4F4F5), plata cepillada (#A1A1AA)
 *     y titanio oscuro (#71717A / #27272A).
 *   - Eficiencia energética máxima: la pantalla solo se redibuja 1 vez por minuto.
 */

#include "scr_watchface.h"
#include "ui/fonts/font_watch_time_108.h"

static lv_obj_t *lblWeekday  = nullptr;
static lv_obj_t *lblTimeMain = nullptr;
static lv_obj_t *lblAmPm     = nullptr;
static lv_obj_t *lblDateStr  = nullptr;

// Cache para redibujar únicamente cuando cambia el minuto o el día
static int16_t renderedHour   = -1;
static int16_t renderedMinute = -1;
static int16_t renderedDay    = -1;

static const char* DIAS_SEMANA[] = {
    "DOMINGO", "LUNES", "MARTES", "MIERCOLES", "JUEVES", "VIERNES", "SABADO"
};

static const char* MESES_COMPLETOS[] = {
    "---", "ENERO", "FEBRERO", "MARZO", "ABRIL", "MAYO", "JUNIO",
    "JULIO", "AGOSTO", "SEPTIEMBRE", "OCTUBRE", "NOVIEMBRE", "DICIEMBRE"
};

void scr_watchface_build(lv_obj_t *parentTile) {
    lv_obj_set_style_pad_all(parentTile, 0, 0);
    lv_obj_set_style_bg_color(parentTile, lv_color_hex(0x000000), 0);

    // 1. Día de la semana en tamaño grande y legible (24pt, tono titanio #8E8E93)
    lblWeekday = lv_label_create(parentTile);
    lv_obj_set_style_text_font(lblWeekday, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lblWeekday, lv_color_hex(0x8E8E93), 0);
    lv_obj_set_style_text_letter_space(lblWeekday, 5, 0);
    lv_label_set_text(lblWeekday, "VIERNES");
    lv_obj_align(lblWeekday, LV_ALIGN_CENTER, 0, -116);

    // 2. Hora principal gigante en formato 12H (108pt tabular anti-aliased, blanco alabastro #F4F4F5)
    lblTimeMain = lv_label_create(parentTile);
    lv_obj_set_style_text_font(lblTimeMain, &font_watch_time_108, 0);
    lv_obj_set_style_text_color(lblTimeMain, lv_color_hex(0xF4F4F5), 0);
    lv_obj_set_style_text_letter_space(lblTimeMain, 4, 0);
    lv_label_set_text(lblTimeMain, "12:00");
    lv_obj_align(lblTimeMain, LV_ALIGN_CENTER, 0, -18);

    // 3. Indicador AM/PM fijo (28pt, tono plata cepillada #A1A1AA)
    lblAmPm = lv_label_create(parentTile);
    lv_obj_set_style_text_font(lblAmPm, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lblAmPm, lv_color_hex(0xA1A1AA), 0);
    lv_obj_set_style_text_letter_space(lblAmPm, 4, 0);
    lv_label_set_text(lblAmPm, "PM");
    lv_obj_align(lblAmPm, LV_ALIGN_CENTER, 0, 58);

    // 4. Línea divisoria minimalista (64 x 2 píxeles, grafito #27272A)
    lv_obj_t *divider = lv_obj_create(parentTile);
    lv_obj_set_size(divider, 64, 2);
    lv_obj_set_style_pad_all(divider, 0, 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_set_style_bg_color(divider, lv_color_hex(0x27272A), 0);
    lv_obj_align(divider, LV_ALIGN_CENTER, 0, 96);

    // 5. Fecha grande y legible en tono plata cepillada (24pt, #A1A1AA)
    lblDateStr = lv_label_create(parentTile);
    lv_obj_set_style_text_font(lblDateStr, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lblDateStr, lv_color_hex(0xA1A1AA), 0);
    lv_obj_set_style_text_letter_space(lblDateStr, 2, 0);
    lv_label_set_text(lblDateStr, "06 NOVIEMBRE");
    lv_obj_align(lblDateStr, LV_ALIGN_CENTER, 0, 128);

    renderedHour   = -1;
    renderedMinute = -1;
    renderedDay    = -1;
}

void scr_watchface_update_time(const WatchDateTime &dt) {
    // Ahorro total de recursos: si no cambió el minuto ni el día, no se toca la pantalla
    if (dt.day == renderedDay &&
        dt.hour == renderedHour &&
        dt.minute == renderedMinute) {
        return;
    }

    if (dt.day != renderedDay) {
        renderedDay  = dt.day;
        uint8_t wday = (dt.weekday <= 6) ? dt.weekday : 0;
        uint8_t mon  = (dt.month >= 1 && dt.month <= 12) ? dt.month : 1;

        if (lblWeekday) {
            lv_label_set_text(lblWeekday, DIAS_SEMANA[wday]);
        }
        if (lblDateStr) {
            lv_label_set_text_fmt(lblDateStr, "%02u %s", dt.day, MESES_COMPLETOS[mon]);
        }
    }

    renderedHour   = dt.hour;
    renderedMinute = dt.minute;

    if (lblTimeMain && lblAmPm) {
        bool isPm = (dt.hour >= 12);
        uint8_t h12 = dt.hour % 12;
        if (h12 == 0) h12 = 12;
        lv_label_set_text_fmt(lblTimeMain, "%02u:%02u", h12, dt.minute);
        lv_label_set_text(lblAmPm, isPm ? "PM" : "AM");
    }
}

void scr_watchface_update_power(const WatchPowerStatus &power, uint8_t brightness) {
    (void)power;
    (void)brightness;
}
