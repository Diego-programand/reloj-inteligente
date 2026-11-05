/**
 * @file ui_manager.cpp
 * @brief Implementación de LVGL con Watchface Principal (Pantalla 1) y Centro de Control (Pantalla 2).
 */

#include "ui_manager.h"
#include <lvgl.h>
#include <esp_heap_caps.h>
#include "config/pins_config.h"
#include "hal/hal_display.h"
#include "hal/hal_touch.h"
#include "hal/hal_rtc.h"
#include "ui/screens/scr_watchface.h"

#define LVGL_BUFFER_LINES       (WATCH_LCD_HEIGHT / 2)
#define LVGL_BUFFER_PIXELS      (WATCH_LCD_WIDTH * LVGL_BUFFER_LINES)

static lv_disp_draw_buf_t drawBuf;
static lv_color_t *buf1 = nullptr;
static lv_color_t *buf2 = nullptr;

// Elementos del Centro de Control (Pantalla 2)
static lv_obj_t *lblBatteryInfo  = nullptr;
static lv_obj_t *barBattery      = nullptr;
static lv_obj_t *sliderBright    = nullptr;
static lv_obj_t *lblBrightVal    = nullptr;
static lv_obj_t *lblButtonEvent  = nullptr;
static WatchPowerStatus cachedPower = {true, true, false, 4110, 100, 5000, 4330};

static void lvgl_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    hal_display_push_colors(
        (uint16_t)area->x1,
        (uint16_t)area->y1,
        (uint16_t)area->x2,
        (uint16_t)area->y2,
        (const uint16_t *)color_p
    );
    lv_disp_flush_ready(disp);
}

/**
 * @brief Redondeador obligatorio para pantallas AMOLED CO5300 / SH8601 (bloques pares).
 */
static void lvgl_rounder_cb(lv_disp_drv_t *disp_drv, lv_area_t *area) {
    area->x1 = area->x1 & ~0x01;
    area->y1 = area->y1 & ~0x01;
    area->x2 = area->x2 | 0x01;
    area->y2 = area->y2 | 0x01;
}

static void lvgl_touch_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    // Si la pantalla está apagada con el botón PWR, ignorar toques accidentales
    if (!hal_display_is_on()) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    TouchPoint tp = hal_touch_read();
    if (tp.touched) {
        data->state   = LV_INDEV_STATE_PR;
        data->point.x = tp.x;
        data->point.y = tp.y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

static void slider_brightness_event_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t val = lv_slider_get_value(slider);
    if (val < 15) val = 15;
    hal_display_set_brightness((uint8_t)val);

    if (lblBrightVal) {
        int pct = (val * 100) / 255;
        lv_label_set_text_fmt(lblBrightVal, "Brillo de pantalla: %d%%", pct);
    }
    scr_watchface_update_power(cachedPower, (uint8_t)val);
}

/**
 * @brief Botones táctiles en el Centro de Control para ajustar la hora del chip RTC PCF85063.
 */
static void btn_adjust_time_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    intptr_t mode = (intptr_t)lv_event_get_user_data(e);

    WatchDateTime dt = hal_rtc_get_time();
    if (mode == 1) {
        dt.hour = (dt.hour + 1) % 24;
    } else if (mode == 2) {
        dt.minute = (dt.minute + 1) % 60;
        dt.second = 0;
    } else if (mode == 3) {
        dt.minute = (dt.minute == 0) ? 59 : (dt.minute - 1);
        dt.second = 0;
    }
    hal_rtc_set_time(dt);
    scr_watchface_update_time(dt);
}

static void build_ui() {
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);

    // Tileview para deslizar suavemente entre la Watchface (Tile 1) y el Centro de Control (Tile 2)
    lv_obj_t *tv = lv_tileview_create(scr);
    lv_obj_set_size(tv, WATCH_LCD_WIDTH, WATCH_LCD_HEIGHT);
    lv_obj_set_style_bg_color(tv, lv_color_hex(0x000000), 0);
    lv_obj_set_scrollbar_mode(tv, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *tileWatchface = lv_tileview_add_tile(tv, 0, 0, LV_DIR_RIGHT);
    lv_obj_t *tileSettings  = lv_tileview_add_tile(tv, 1, 0, LV_DIR_LEFT);

    // 1. Construir la Watchface Principal en la Pantalla 1
    scr_watchface_build(tileWatchface);

    // 2. Construir el Panel de Ajustes en la Pantalla 2 (estética neutra y sobria)
    lv_obj_set_style_pad_all(tileSettings, 0, 0);
    lv_obj_set_style_bg_color(tileSettings, lv_color_hex(0x000000), 0);

    lv_obj_t *title2 = lv_label_create(tileSettings);
    lv_obj_set_style_text_font(title2, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title2, lv_color_hex(0xA1A1AA), 0);
    lv_obj_set_style_text_letter_space(title2, 4, 0);
    lv_label_set_text(title2, "AJUSTES");
    lv_obj_align(title2, LV_ALIGN_TOP_MID, 0, 22);

    // --- Bloque A: Brillo AMOLED (Y = 60..176, alto = 116 px) ---
    lv_obj_t *cardBright = lv_obj_create(tileSettings);
    lv_obj_set_size(cardBright, 366, 116);
    lv_obj_align(cardBright, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_pad_all(cardBright, 0, 0);
    lv_obj_set_style_bg_color(cardBright, lv_color_hex(0x0C0C0E), 0);
    lv_obj_set_style_border_color(cardBright, lv_color_hex(0x27272A), 0);
    lv_obj_set_style_border_width(cardBright, 1, 0);
    lv_obj_set_style_radius(cardBright, 14, 0);
    lv_obj_clear_flag(cardBright, LV_OBJ_FLAG_SCROLLABLE);

    lblBrightVal = lv_label_create(cardBright);
    lv_obj_set_style_text_font(lblBrightVal, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblBrightVal, lv_color_hex(0xD4D4D8), 0);
    lv_label_set_text(lblBrightVal, "Brillo de pantalla: 86%");
    lv_obj_align(lblBrightVal, LV_ALIGN_TOP_LEFT, 22, 18);

    sliderBright = lv_slider_create(cardBright);
    lv_obj_set_size(sliderBright, 314, 16);
    lv_obj_align(sliderBright, LV_ALIGN_TOP_MID, 0, 66);
    lv_slider_set_range(sliderBright, 15, 255);
    lv_slider_set_value(sliderBright, hal_display_get_brightness(), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(sliderBright, lv_color_hex(0x27272A), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sliderBright, lv_color_hex(0xD4D4D8), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sliderBright, lv_color_hex(0xF4F4F5), LV_PART_KNOB);
    lv_obj_add_event_cb(sliderBright, slider_brightness_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    // --- Bloque B: Ajuste de Hora (Y = 192..320, alto = 128 px) ---
    lv_obj_t *cardRtc = lv_obj_create(tileSettings);
    lv_obj_set_size(cardRtc, 366, 128);
    lv_obj_align(cardRtc, LV_ALIGN_TOP_MID, 0, 192);
    lv_obj_set_style_pad_all(cardRtc, 0, 0);
    lv_obj_set_style_bg_color(cardRtc, lv_color_hex(0x0C0C0E), 0);
    lv_obj_set_style_border_color(cardRtc, lv_color_hex(0x27272A), 0);
    lv_obj_set_style_border_width(cardRtc, 1, 0);
    lv_obj_set_style_radius(cardRtc, 14, 0);
    lv_obj_clear_flag(cardRtc, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lblRtcTitle = lv_label_create(cardRtc);
    lv_obj_set_style_text_font(lblRtcTitle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblRtcTitle, lv_color_hex(0xA1A1AA), 0);
    lv_label_set_text(lblRtcTitle, "Ajuste manual de hora");
    lv_obj_align(lblRtcTitle, LV_ALIGN_TOP_LEFT, 22, 16);

    const char *btnLabels[3] = {"+1 Hora", "+1 Min", "-1 Min"};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *btn = lv_btn_create(cardRtc);
        lv_obj_set_size(btn, 102, 48);
        lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 18 + (i * 114), 56);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x18181B), 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x3F3F46), 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 10, 0);
        lv_obj_add_event_cb(btn, btn_adjust_time_cb, LV_EVENT_CLICKED, (void *)(intptr_t)(i + 1));

        lv_obj_t *lbl = lv_label_create(btn);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xF4F4F5), 0);
        lv_label_set_text(lbl, btnLabels[i]);
        lv_obj_center(lbl);
    }

    // --- Bloque C: Estado de Batería (Y = 336..464, alto = 128 px) ---
    lv_obj_t *cardBatt = lv_obj_create(tileSettings);
    lv_obj_set_size(cardBatt, 366, 128);
    lv_obj_align(cardBatt, LV_ALIGN_TOP_MID, 0, 336);
    lv_obj_set_style_pad_all(cardBatt, 0, 0);
    lv_obj_set_style_bg_color(cardBatt, lv_color_hex(0x0C0C0E), 0);
    lv_obj_set_style_border_color(cardBatt, lv_color_hex(0x27272A), 0);
    lv_obj_set_style_border_width(cardBatt, 1, 0);
    lv_obj_set_style_radius(cardBatt, 14, 0);
    lv_obj_clear_flag(cardBatt, LV_OBJ_FLAG_SCROLLABLE);

    lblBatteryInfo = lv_label_create(cardBatt);
    lv_obj_set_style_text_font(lblBatteryInfo, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblBatteryInfo, lv_color_hex(0xD4D4D8), 0);
    lv_label_set_text(lblBatteryInfo, "Bateria: 100% (4110 mV)");
    lv_obj_align(lblBatteryInfo, LV_ALIGN_TOP_LEFT, 22, 16);

    barBattery = lv_bar_create(cardBatt);
    lv_obj_set_size(barBattery, 322, 12);
    lv_obj_align(barBattery, LV_ALIGN_TOP_MID, 0, 56);
    lv_bar_set_range(barBattery, 0, 100);
    lv_bar_set_value(barBattery, 100, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(barBattery, lv_color_hex(0x27272A), LV_PART_MAIN);
    lv_obj_set_style_bg_color(barBattery, lv_color_hex(0xD4D4D8), LV_PART_INDICATOR);

    lblButtonEvent = lv_label_create(cardBatt);
    lv_obj_set_style_text_font(lblButtonEvent, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblButtonEvent, lv_color_hex(0x71717A), 0);
    lv_label_set_text(lblButtonEvent, "Sistema activo");
    lv_obj_align(lblButtonEvent, LV_ALIGN_TOP_LEFT, 22, 88);
}

bool ui_manager_init() {
    lv_init();

    buf1 = (lv_color_t *)heap_caps_malloc(LVGL_BUFFER_PIXELS * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    buf2 = (lv_color_t *)heap_caps_malloc(LVGL_BUFFER_PIXELS * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);

    if (!buf1 || !buf2) {
        return false;
    }

    lv_disp_draw_buf_init(&drawBuf, buf1, buf2, LVGL_BUFFER_PIXELS);

    static lv_disp_drv_t dispDrv;
    lv_disp_drv_init(&dispDrv);
    dispDrv.hor_res    = WATCH_LCD_WIDTH;
    dispDrv.ver_res    = WATCH_LCD_HEIGHT;
    dispDrv.flush_cb   = lvgl_flush_cb;
    dispDrv.rounder_cb = lvgl_rounder_cb;
    dispDrv.draw_buf   = &drawBuf;
    lv_disp_drv_register(&dispDrv);

    static lv_indev_drv_t indevDrv;
    lv_indev_drv_init(&indevDrv);
    indevDrv.type    = LV_INDEV_TYPE_POINTER;
    indevDrv.read_cb = lvgl_touch_read_cb;
    lv_indev_drv_register(&indevDrv);

    build_ui();
    return true;
}

void ui_manager_loop() {
    lv_timer_handler();
}

void ui_manager_update_time(const WatchDateTime &dt) {
    scr_watchface_update_time(dt);
}

void ui_manager_update_power(const WatchPowerStatus &status) {
    cachedPower = status;
    scr_watchface_update_power(status, hal_display_get_brightness());

    if (lblBatteryInfo) {
        lv_label_set_text_fmt(
            lblBatteryInfo,
            "Bateria: %d%% (%u mV) %s",
            status.batteryPercent,
            status.batteryVoltageMv,
            status.isCharging ? "[CARGANDO]" : "[USB OK]"
        );
    }
    if (barBattery) {
        lv_bar_set_value(barBattery, status.batteryPercent, LV_ANIM_ON);
    }
}

void ui_manager_notify_button(const char *buttonText) {
    if (lblButtonEvent) {
        lv_label_set_text_fmt(lblButtonEvent, "Boton fisico: %s", buttonText);
    }
}

void ui_manager_sync_brightness(uint8_t brightness) {
    if (sliderBright && brightness >= 15) {
        lv_slider_set_value(sliderBright, brightness, LV_ANIM_ON);
    }
    if (lblBrightVal) {
        int pct = (brightness * 100) / 255;
        lv_label_set_text_fmt(lblBrightVal, "Brillo de pantalla: %d%%", pct);
    }
    scr_watchface_update_power(cachedPower, brightness);
}
