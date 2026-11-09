/**
 * @file scr_watchface.cpp
 * @brief Watchface minimalista, formal y exclusiva (410x502 AMOLED) con integración
 *        interactiva de Google Calendar y modal emergente ejecutivo.
 *
 * Filosofía de diseño (AGENTS.md):
 *   - Fondo negro puro (#000000) para fundirse con el bisel físico de la pantalla AMOLED.
 *   - Formato exclusivo de 12 horas con indicador AM/PM.
 *   - Paleta monocromática de alta relojería: blanco alabastro (#F4F4F5), plata cepillada (#A1A1AA),
 *     titanio (#71717A) y grafito oscuro (#18181B / #27272A).
 *   - Línea interactiva de agenda bajo la fecha con modal emergente táctil.
 *   - Eficiencia energética máxima (Política de Consumo Cero): la pantalla solo se redibuja
 *     cuando cambia el minuto del RTC o la versión de sincronización del calendario.
 */

#include "scr_watchface.h"
#include <stdio.h>
#include <string.h>
#include "services/calendar_service.h"
#include "services/power_manager.h"
#include "ui/fonts/font_watch_time_108.h"

// Elementos principales de la Watchface
static lv_obj_t *lblWeekday         = nullptr;
static lv_obj_t *lblTimeMain        = nullptr;
static lv_obj_t *lblAmPm            = nullptr;
static lv_obj_t *lblDateStr         = nullptr;
static lv_obj_t *lblCalendarSummary = nullptr;

// Elementos del Modal Emergente de Google Calendar
static lv_obj_t *modalBackdrop      = nullptr;
static lv_obj_t *modalCard          = nullptr;
static lv_obj_t *lblModalHeader     = nullptr;
static lv_obj_t *lblModalTitle      = nullptr;
static lv_obj_t *lblModalTimeRange  = nullptr;
static lv_obj_t *lblModalLocation   = nullptr;
static lv_obj_t *btnCloseModal      = nullptr;

// Caché de estado para redibujar únicamente cuando cambia el minuto, el día o la agenda
static int16_t  renderedHour           = -1;
static int16_t  renderedMinute         = -1;
static int16_t  renderedDay            = -1;
static uint32_t renderedCalSyncVersion = 0xFFFFFFFFUL;

// Nota técnica sobre tipografías LVGL v8.3:
// Las fuentes internas lv_font_montserrat_* incluyen el rango ASCII 0x20..0x7F, 0xB0 (°)
// y 0x2022 (LV_SYMBOL_BULLET = "•"). Usamos LV_SYMBOL_BULLET como punto separador
// para garantizar un renderizado nítido en el panel AMOLED sin glifos faltantes.
#define WATCH_SEP_DOT LV_SYMBOL_BULLET

static const char *DIAS_SEMANA[] = {
    "DOMINGO", "LUNES", "MARTES", "MIERCOLES", "JUEVES", "VIERNES", "SABADO"
};

static const char *MESES_COMPLETOS[] = {
    "---", "ENERO", "FEBRERO", "MARZO", "ABRIL", "MAYO", "JUNIO",
    "JULIO", "AGOSTO", "SEPTIEMBRE", "OCTUBRE", "NOVIEMBRE", "DICIEMBRE"
};

/**
 * @brief Recorta un título largo añadiendo "..." para que la línea resumen
 *        siempre quepa en una sola línea limpia dentro de los 410 px de ancho.
 */
static void truncateSummaryTitle(const char *src, char *dst, size_t dstSize, size_t maxChars) {
    if (!dst || dstSize == 0) {
        return;
    }
    if (!src || src[0] == '\0') {
        strncpy(dst, "Evento", dstSize - 1);
        dst[dstSize - 1] = '\0';
        return;
    }

    size_t len = strlen(src);
    if (len <= maxChars) {
        strncpy(dst, src, dstSize - 1);
        dst[dstSize - 1] = '\0';
        return;
    }

    size_t keep = (maxChars > 3) ? (maxChars - 3) : maxChars;
    if (keep >= dstSize) {
        keep = dstSize - 1;
    }
    memcpy(dst, src, keep);
    dst[keep] = '\0';

    // Eliminar espacio final antes de "..." si quedó justo en el corte
    while (keep > 0 && dst[keep - 1] == ' ') {
        dst[--keep] = '\0';
    }

    strncat(dst, "...", dstSize - strlen(dst) - 1);
}

/**
 * @brief Formatea el tiempo restante en formato corto para la Watchface
 *        (ej. "Ahora", "en 20m", "en 1h 30m", "en 2h").
 */
static void formatRelativeTimeShort(int32_t minsUntil, bool isOngoing, char *outBuf, size_t outSize) {
    if (!outBuf || outSize == 0) {
        return;
    }
    if (isOngoing || minsUntil <= 0) {
        snprintf(outBuf, outSize, "Ahora");
    } else if (minsUntil < 60) {
        snprintf(outBuf, outSize, "en %ldm", (long)minsUntil);
    } else {
        long hours = (long)(minsUntil / 60);
        long mins  = (long)(minsUntil % 60);
        if (mins > 0) {
            snprintf(outBuf, outSize, "en %ldh %ldm", hours, mins);
        } else {
            snprintf(outBuf, outSize, "en %ldh", hours);
        }
    }
}

/**
 * @brief Formatea el tiempo restante en formato descriptivo para el modal emergente
 *        (ej. "En curso ahora", "En 20 min", "En 1 h 30 min").
 */
static void formatRelativeTimeLong(int32_t minsUntil, bool isOngoing, char *outBuf, size_t outSize) {
    if (!outBuf || outSize == 0) {
        return;
    }
    if (isOngoing || minsUntil <= 0) {
        snprintf(outBuf, outSize, "En curso ahora");
    } else if (minsUntil < 60) {
        snprintf(outBuf, outSize, "En %ld min", (long)minsUntil);
    } else {
        long hours = (long)(minsUntil / 60);
        long mins  = (long)(minsUntil % 60);
        if (mins > 0) {
            snprintf(outBuf, outSize, "En %ld h %ld min", hours, mins);
        } else {
            snprintf(outBuf, outSize, "En %ld h", hours);
        }
    }
}

/**
 * @brief Actualiza los textos del modal emergente con el estado actual de la agenda.
 */
static void updateModalContent(const CalendarEvent &ev, bool hasEvent, int32_t minsUntil, bool isOngoing) {
    if (!lblModalHeader || !lblModalTitle || !lblModalTimeRange || !lblModalLocation) {
        return;
    }

    bool isMock = calendar_service_is_using_mock();

    if (!hasEvent) {
        lv_label_set_text(lblModalHeader,
                          isMock ? ("AGENDA SIN EVENTOS " WATCH_SEP_DOT " DEMO") : "AGENDA SIN EVENTOS");
        lv_label_set_text(lblModalTitle, "Sin eventos proximos");
        lv_label_set_text(lblModalTimeRange, "Agenda libre para las proximas horas");
        lv_label_set_text(lblModalLocation, "Ubicacion: ---");
        return;
    }

    if (isOngoing) {
        lv_label_set_text(lblModalHeader,
                          isMock ? ("EVENTO EN CURSO " WATCH_SEP_DOT " DEMO") : "EVENTO EN CURSO");
    } else {
        lv_label_set_text(lblModalHeader,
                          isMock ? ("PROXIMO EVENTO " WATCH_SEP_DOT " DEMO") : "PROXIMO EVENTO");
    }

    lv_label_set_text(lblModalTitle, (ev.title[0] != '\0') ? ev.title : "Evento sin titulo");

    char relBuf[32];
    formatRelativeTimeLong(minsUntil, isOngoing, relBuf, sizeof(relBuf));

    if (ev.allDay) {
        lv_label_set_text_fmt(lblModalTimeRange, "Todo el dia " WATCH_SEP_DOT " %s", relBuf);
    } else {
        uint8_t sh24 = 0, smin = 0;
        uint8_t eh24 = 0, emin = 0;
        calendar_service_unix_to_local_hm(ev.startUnix, sh24, smin);
        calendar_service_unix_to_local_hm(ev.endUnix, eh24, emin);

        char start12h[16];
        calendar_service_format_time_12h(ev.startUnix, start12h, sizeof(start12h));

        lv_label_set_text_fmt(lblModalTimeRange,
                              "%02u:%02u - %02u:%02u (%s)\n%s",
                              sh24, smin, eh24, emin, start12h, relBuf);
    }

    if (ev.location[0] != '\0') {
        lv_label_set_text_fmt(lblModalLocation, "Ubicacion: %s", ev.location);
    } else {
        lv_label_set_text(lblModalLocation, "Sin ubicacion especificada");
    }
}

/**
 * @brief Actualiza la línea resumen bajo la fecha y (si está abierto) el modal emergente.
 */
static void updateCalendarWidgets() {
    CalendarEvent nextEv;
    int32_t minsUntil = -1;
    bool isOngoing    = false;
    bool hasEvent     = calendar_service_get_next_event(nextEv, minsUntil, isOngoing);

    if (lblCalendarSummary) {
        if (!hasEvent) {
            lv_label_set_text(lblCalendarSummary, "Sin eventos proximos");
        } else {
            char shortTitle[24];
            truncateSummaryTitle(nextEv.title, shortTitle, sizeof(shortTitle), 15);

            char relShort[24];
            formatRelativeTimeShort(minsUntil, isOngoing, relShort, sizeof(relShort));

            if (nextEv.allDay) {
                lv_label_set_text_fmt(lblCalendarSummary,
                                      "Hoy " WATCH_SEP_DOT " %s (%s)",
                                      shortTitle, relShort);
            } else {
                uint8_t sh24 = 0, smin = 0;
                calendar_service_unix_to_local_hm(nextEv.startUnix, sh24, smin);
                lv_label_set_text_fmt(lblCalendarSummary,
                                      "%02u:%02u " WATCH_SEP_DOT " %s (%s)",
                                      sh24, smin, shortTitle, relShort);
            }
        }
    }

    if (modalBackdrop && !lv_obj_has_flag(modalBackdrop, LV_OBJ_FLAG_HIDDEN)) {
        updateModalContent(nextEv, hasEvent, minsUntil, isOngoing);
    }
}

/**
 * @brief Callback al tocar la línea interactiva de Google Calendar en la Watchface.
 */
static void calendar_summary_click_cb(lv_event_t *e) {
    (void)e;
    power_manager_notify_user_activity();

    if (!modalBackdrop) {
        return;
    }

    CalendarEvent nextEv;
    int32_t minsUntil = -1;
    bool isOngoing    = false;
    bool hasEvent     = calendar_service_get_next_event(nextEv, minsUntil, isOngoing);

    updateModalContent(nextEv, hasEvent, minsUntil, isOngoing);
    lv_obj_clear_flag(modalBackdrop, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(modalBackdrop);
}

/**
 * @brief Callback para cerrar el modal emergente (al tocar "CERRAR" o el fondo exterior).
 */
static void modal_close_event_cb(lv_event_t *e) {
    lv_obj_t *target = lv_event_get_target(e);
    if (target != modalBackdrop && target != btnCloseModal) {
        return;
    }

    power_manager_notify_user_activity();
    if (modalBackdrop) {
        lv_obj_add_flag(modalBackdrop, LV_OBJ_FLAG_HIDDEN);
    }
}

void scr_watchface_build(lv_obj_t *parentTile) {
    lv_obj_set_style_pad_all(parentTile, 0, 0);
    lv_obj_set_style_bg_color(parentTile, lv_color_hex(0x000000), 0);

    // 1. Día de la semana en tamaño grande y legible (24pt, tono titanio #8E8E93)
    lblWeekday = lv_label_create(parentTile);
    lv_obj_set_style_text_font(lblWeekday, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lblWeekday, lv_color_hex(0x8E8E93), 0);
    lv_obj_set_style_text_letter_space(lblWeekday, 5, 0);
    lv_label_set_text(lblWeekday, "DOMINGO");
    lv_obj_align(lblWeekday, LV_ALIGN_CENTER, 0, -130);

    // 2. Hora principal gigante en formato 12H (108pt tabular anti-aliased, blanco alabastro #F4F4F5)
    lblTimeMain = lv_label_create(parentTile);
    lv_obj_set_style_text_font(lblTimeMain, &font_watch_time_108, 0);
    lv_obj_set_style_text_color(lblTimeMain, lv_color_hex(0xF4F4F5), 0);
    lv_obj_set_style_text_letter_space(lblTimeMain, 4, 0);
    lv_label_set_text(lblTimeMain, "12:00");
    lv_obj_align(lblTimeMain, LV_ALIGN_CENTER, 0, -32);

    // 3. Indicador AM/PM fijo (28pt, tono plata cepillada #A1A1AA)
    lblAmPm = lv_label_create(parentTile);
    lv_obj_set_style_text_font(lblAmPm, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lblAmPm, lv_color_hex(0xA1A1AA), 0);
    lv_obj_set_style_text_letter_space(lblAmPm, 4, 0);
    lv_label_set_text(lblAmPm, "PM");
    lv_obj_align(lblAmPm, LV_ALIGN_CENTER, 0, 46);

    // 4. Línea divisoria minimalista (64 x 2 píxeles, grafito #27272A)
    lv_obj_t *divider = lv_obj_create(parentTile);
    lv_obj_set_size(divider, 64, 2);
    lv_obj_set_style_pad_all(divider, 0, 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_set_style_bg_color(divider, lv_color_hex(0x27272A), 0);
    lv_obj_align(divider, LV_ALIGN_CENTER, 0, 84);

    // 5. Fecha grande y legible en tono plata cepillada (24pt, #A1A1AA)
    lblDateStr = lv_label_create(parentTile);
    lv_obj_set_style_text_font(lblDateStr, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lblDateStr, lv_color_hex(0xA1A1AA), 0);
    lv_obj_set_style_text_letter_space(lblDateStr, 2, 0);
    lv_label_set_text(lblDateStr, "08 NOVIEMBRE");
    lv_obj_align(lblDateStr, LV_ALIGN_CENTER, 0, 118);

    // 6. Línea interactiva de Google Calendar bajo la fecha (18pt, titanio #71717A)
    lblCalendarSummary = lv_label_create(parentTile);
    lv_obj_set_width(lblCalendarSummary, 390);
    lv_label_set_long_mode(lblCalendarSummary, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(lblCalendarSummary, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(lblCalendarSummary, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lblCalendarSummary, lv_color_hex(0x71717A), 0);
    lv_label_set_text(lblCalendarSummary, "Sin eventos proximos");
    lv_obj_align(lblCalendarSummary, LV_ALIGN_CENTER, 0, 172);
    lv_obj_add_flag(lblCalendarSummary, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(lblCalendarSummary, 18);
    lv_obj_add_event_cb(lblCalendarSummary, calendar_summary_click_cb, LV_EVENT_CLICKED, nullptr);

    // 7. Modal Emergente Minimalista (Superposición 410x502 sobre parentTile, oculto al inicio)
    modalBackdrop = lv_obj_create(parentTile);
    lv_obj_set_size(modalBackdrop, 410, 502);
    lv_obj_align(modalBackdrop, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(modalBackdrop, 0, 0);
    lv_obj_set_style_border_width(modalBackdrop, 0, 0);
    lv_obj_set_style_radius(modalBackdrop, 0, 0);
    lv_obj_set_style_bg_color(modalBackdrop, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(modalBackdrop, LV_OPA_90, 0);
    lv_obj_clear_flag(modalBackdrop, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(modalBackdrop, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(modalBackdrop, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(modalBackdrop, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(modalBackdrop, modal_close_event_cb, LV_EVENT_CLICKED, nullptr);

    // Tarjeta ejecutiva del modal (366x284 px, coordenadas pares, fondo grafito #18181B)
    modalCard = lv_obj_create(modalBackdrop);
    lv_obj_set_size(modalCard, 366, 284);
    lv_obj_align(modalCard, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(modalCard, 0, 0);
    lv_obj_set_style_bg_color(modalCard, lv_color_hex(0x18181B), 0);
    lv_obj_set_style_bg_opa(modalCard, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(modalCard, lv_color_hex(0x27272A), 0);
    lv_obj_set_style_border_width(modalCard, 1, 0);
    lv_obj_set_style_radius(modalCard, 16, 0);
    lv_obj_clear_flag(modalCard, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(modalCard, LV_SCROLLBAR_MODE_OFF);

    // Encabezado sutil del modal (14pt, titanio #71717A, letter-space 2)
    lblModalHeader = lv_label_create(modalCard);
    lv_obj_set_style_text_font(lblModalHeader, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblModalHeader, lv_color_hex(0x71717A), 0);
    lv_obj_set_style_text_letter_space(lblModalHeader, 2, 0);
    lv_label_set_text(lblModalHeader, "PROXIMO EVENTO");
    lv_obj_align(lblModalHeader, LV_ALIGN_TOP_MID, 0, 20);

    // Título completo del evento (24pt, blanco alabastro #F4F4F5, wrap en 322 px)
    lblModalTitle = lv_label_create(modalCard);
    lv_obj_set_width(lblModalTitle, 322);
    lv_label_set_long_mode(lblModalTitle, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(lblModalTitle, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(lblModalTitle, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lblModalTitle, lv_color_hex(0xF4F4F5), 0);
    lv_label_set_text(lblModalTitle, "Reunion de Estrategia");
    lv_obj_align(lblModalTitle, LV_ALIGN_TOP_MID, 0, 50);

    // Rango horario y tiempo restante (18pt, plata cepillada #A1A1AA)
    lblModalTimeRange = lv_label_create(modalCard);
    lv_obj_set_width(lblModalTimeRange, 334);
    lv_label_set_long_mode(lblModalTimeRange, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(lblModalTimeRange, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(lblModalTimeRange, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lblModalTimeRange, lv_color_hex(0xA1A1AA), 0);
    lv_label_set_text(lblModalTimeRange, "16:00 - 16:45 (04:00 PM)\nEn 20 min");
    lv_obj_align(lblModalTimeRange, LV_ALIGN_TOP_MID, 0, 120);

    // Ubicación del evento (16pt, titanio #71717A)
    lblModalLocation = lv_label_create(modalCard);
    lv_obj_set_width(lblModalLocation, 322);
    lv_label_set_long_mode(lblModalLocation, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(lblModalLocation, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(lblModalLocation, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lblModalLocation, lv_color_hex(0x71717A), 0);
    lv_label_set_text(lblModalLocation, "Ubicacion: Sala Juntas B");
    lv_obj_align(lblModalLocation, LV_ALIGN_TOP_MID, 0, 182);

    // Botón sutil para cerrar (148x40 px, fondo #27272A, borde #71717A 1px, radio 10px)
    btnCloseModal = lv_btn_create(modalCard);
    lv_obj_set_size(btnCloseModal, 148, 40);
    lv_obj_align(btnCloseModal, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_pad_all(btnCloseModal, 0, 0);
    lv_obj_set_style_bg_color(btnCloseModal, lv_color_hex(0x27272A), 0);
    lv_obj_set_style_bg_opa(btnCloseModal, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btnCloseModal, lv_color_hex(0x71717A), 0);
    lv_obj_set_style_border_width(btnCloseModal, 1, 0);
    lv_obj_set_style_radius(btnCloseModal, 10, 0);
    lv_obj_set_style_shadow_width(btnCloseModal, 0, 0);
    lv_obj_add_event_cb(btnCloseModal, modal_close_event_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *lblCloseBtn = lv_label_create(btnCloseModal);
    lv_obj_set_style_text_font(lblCloseBtn, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lblCloseBtn, lv_color_hex(0xF4F4F5), 0);
    lv_obj_set_style_text_letter_space(lblCloseBtn, 2, 0);
    lv_label_set_text(lblCloseBtn, "CERRAR");
    lv_obj_align(lblCloseBtn, LV_ALIGN_CENTER, 0, 0);

    renderedHour           = -1;
    renderedMinute         = -1;
    renderedDay            = -1;
    renderedCalSyncVersion = 0xFFFFFFFFUL;
}

void scr_watchface_refresh_calendar() {
    renderedCalSyncVersion = calendar_service_get_sync_version();
    updateCalendarWidgets();
}

void scr_watchface_update_time(const WatchDateTime &dt) {
    uint32_t currentCalSyncVer = calendar_service_get_sync_version();

    // Política de Consumo Cero: si no cambió el minuto, el día ni la versión de agenda,
    // retornamos de inmediato sin tocar ningún objeto de LVGL.
    if (dt.day == renderedDay &&
        dt.hour == renderedHour &&
        dt.minute == renderedMinute &&
        currentCalSyncVer == renderedCalSyncVersion) {
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

    renderedHour           = dt.hour;
    renderedMinute         = dt.minute;
    renderedCalSyncVersion = currentCalSyncVer;

    if (lblTimeMain && lblAmPm) {
        bool isPm = (dt.hour >= 12);
        uint8_t h12 = dt.hour % 12;
        if (h12 == 0) h12 = 12;
        lv_label_set_text_fmt(lblTimeMain, "%02u:%02u", h12, dt.minute);
        lv_label_set_text(lblAmPm, isPm ? "PM" : "AM");
    }

    // Actualizar la línea de resumen de Google Calendar (y el modal si está visible)
    updateCalendarWidgets();
}

void scr_watchface_update_power(const WatchPowerStatus &power, uint8_t brightness) {
    (void)power;
    (void)brightness;
}
