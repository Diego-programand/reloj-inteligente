/**
 * @file ui_manager.cpp
 * @brief Implementación de LVGL con Watchface Principal (Pantalla 1) y Ajustes Simplificados (Pantalla 2).
 */

#include "ui_manager.h"
#include <lvgl.h>
#include <esp_heap_caps.h>
#include <Wire.h>
#include "config/pins_config.h"
#include "hal/hal_display.h"
#include "hal/hal_touch.h"
#include "hal/hal_rtc.h"
#include "services/power_manager.h"
#include "ui/screens/scr_watchface.h"

// Buffer de PANTALLA COMPLETA (502 líneas completas en Octal PSRAM)
// Elimina de raíz cualquier quiebre horizontal al deslizar entre pantallas.
#define LVGL_BUFFER_LINES       WATCH_LCD_HEIGHT
#define LVGL_BUFFER_PIXELS      (WATCH_LCD_WIDTH * LVGL_BUFFER_LINES)

static lv_disp_draw_buf_t drawBuf;
static lv_color_t *buf1 = nullptr;
static lv_color_t *buf2 = nullptr;

// Elementos del Panel de Ajustes (Pantalla 2): Brillo, Volumen y Batería
static lv_obj_t *sliderBright   = nullptr;
static lv_obj_t *lblBrightPct   = nullptr;
static lv_obj_t *sliderVolume   = nullptr;
static lv_obj_t *lblVolumePct   = nullptr;
static lv_obj_t *lblBattPct     = nullptr;
static lv_obj_t *barBattery     = nullptr;

static uint8_t currentVolumePct = 70;
static WatchPowerStatus cachedPower = {true, true, false, 4110, 100, 5000, 4330};

/**
 * @brief Actualiza el registro de volumen digital (0x32) del chip de audio ES8311 (0x18) si está activo.
 */
static void applyHardwareVolume(uint8_t volumePct) {
    if (volumePct > 100) volumePct = 100;
    currentVolumePct = volumePct;

    // Escala de 0..100% a registro DAC ES8311 (0x00 = silencio, 0xBF = 0dB máximo limpio)
    uint8_t regVal = (uint8_t)((volumePct * 0xBF) / 100);
    Wire.beginTransmission(I2C_ADDR_ES8311);
    Wire.write(0x32);
    Wire.write(regVal);
    Wire.endTransmission();
}

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
    if (!hal_display_is_on()) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    TouchPoint tp = hal_touch_read();
    if (tp.touched) {
        power_manager_notify_user_activity();
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

    if (lblBrightPct) {
        int pct = (val * 100) / 255;
        lv_label_set_text_fmt(lblBrightPct, "%d%%", pct);
    }
    scr_watchface_update_power(cachedPower, (uint8_t)val);
}

static void slider_volume_event_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t val = lv_slider_get_value(slider);
    if (val < 0) val = 0;
    if (val > 100) val = 100;

    applyHardwareVolume((uint8_t)val);

    if (lblVolumePct) {
        lv_label_set_text_fmt(lblVolumePct, "%d%%", (int)val);
    }
}

static lv_obj_t *create_clean_card(lv_obj_t *parent, lv_coord_t yOffset, lv_coord_t height) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 366, height);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, yOffset);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x0C0C0E), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x27272A), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

static void style_monochrome_slider(lv_obj_t *slider) {
    lv_obj_set_size(slider, 318, 18);
    lv_obj_align(slider, LV_ALIGN_TOP_MID, 0, 68);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x27272A), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xD4D4D8), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xF4F4F5), LV_PART_KNOB);
}

static void build_ui() {
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);

    // Tileview para deslizar suavemente entre la Watchface (Tile 1) y Ajustes (Tile 2)
    lv_obj_t *tv = lv_tileview_create(scr);
    lv_obj_set_size(tv, WATCH_LCD_WIDTH, WATCH_LCD_HEIGHT);
    lv_obj_set_style_bg_color(tv, lv_color_hex(0x000000), 0);
    lv_obj_set_scrollbar_mode(tv, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *tileWatchface = lv_tileview_add_tile(tv, 0, 0, LV_DIR_RIGHT);
    lv_obj_t *tileSettings  = lv_tileview_add_tile(tv, 1, 0, LV_DIR_LEFT);

    // 1. Construir la Watchface Principal en la Pantalla 1
    scr_watchface_build(tileWatchface);

    // 2. Construir el Panel de Ajustes simplificado en la Pantalla 2
    lv_obj_set_style_pad_all(tileSettings, 0, 0);
    lv_obj_set_style_bg_color(tileSettings, lv_color_hex(0x000000), 0);

    lv_obj_t *title2 = lv_label_create(tileSettings);
    lv_obj_set_style_text_font(title2, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title2, lv_color_hex(0xD4D4D8), 0);
    lv_obj_set_style_text_letter_space(title2, 4, 0);
    lv_label_set_text(title2, "AJUSTES");
    lv_obj_align(title2, LV_ALIGN_TOP_MID, 0, 22);

    // --- Bloque 1: Brillo (Y = 74..192, alto = 118 px) ---
    lv_obj_t *cardBright = create_clean_card(tileSettings, 74, 118);

    lv_obj_t *lblBrightTitle = lv_label_create(cardBright);
    lv_obj_set_style_text_font(lblBrightTitle, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lblBrightTitle, lv_color_hex(0xA1A1AA), 0);
    lv_label_set_text(lblBrightTitle, "Brillo");
    lv_obj_align(lblBrightTitle, LV_ALIGN_TOP_LEFT, 22, 18);

    lblBrightPct = lv_label_create(cardBright);
    lv_obj_set_style_text_font(lblBrightPct, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lblBrightPct, lv_color_hex(0xF4F4F5), 0);
    int initialBrightPct = (hal_display_get_brightness() * 100) / 255;
    lv_label_set_text_fmt(lblBrightPct, "%d%%", initialBrightPct);
    lv_obj_align(lblBrightPct, LV_ALIGN_TOP_RIGHT, -22, 18);

    sliderBright = lv_slider_create(cardBright);
    style_monochrome_slider(sliderBright);
    lv_slider_set_range(sliderBright, 15, 255);
    lv_slider_set_value(sliderBright, hal_display_get_brightness(), LV_ANIM_OFF);
    lv_obj_add_event_cb(sliderBright, slider_brightness_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    // --- Bloque 2: Volumen (Y = 210..328, alto = 118 px) ---
    lv_obj_t *cardVol = create_clean_card(tileSettings, 210, 118);

    lv_obj_t *lblVolTitle = lv_label_create(cardVol);
    lv_obj_set_style_text_font(lblVolTitle, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lblVolTitle, lv_color_hex(0xA1A1AA), 0);
    lv_label_set_text(lblVolTitle, "Volumen");
    lv_obj_align(lblVolTitle, LV_ALIGN_TOP_LEFT, 22, 18);

    lblVolumePct = lv_label_create(cardVol);
    lv_obj_set_style_text_font(lblVolumePct, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lblVolumePct, lv_color_hex(0xF4F4F5), 0);
    lv_label_set_text_fmt(lblVolumePct, "%d%%", currentVolumePct);
    lv_obj_align(lblVolumePct, LV_ALIGN_TOP_RIGHT, -22, 18);

    sliderVolume = lv_slider_create(cardVol);
    style_monochrome_slider(sliderVolume);
    lv_slider_set_range(sliderVolume, 0, 100);
    lv_slider_set_value(sliderVolume, currentVolumePct, LV_ANIM_OFF);
    lv_obj_add_event_cb(sliderVolume, slider_volume_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    // --- Bloque 3: Batería (Y = 346..460, alto = 114 px) ---
    lv_obj_t *cardBatt = create_clean_card(tileSettings, 346, 114);

    lv_obj_t *lblBattTitle = lv_label_create(cardBatt);
    lv_obj_set_style_text_font(lblBattTitle, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lblBattTitle, lv_color_hex(0xA1A1AA), 0);
    lv_label_set_text(lblBattTitle, "Bateria");
    lv_obj_align(lblBattTitle, LV_ALIGN_TOP_LEFT, 22, 18);

    lblBattPct = lv_label_create(cardBatt);
    lv_obj_set_style_text_font(lblBattPct, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lblBattPct, lv_color_hex(0xF4F4F5), 0);
    lv_label_set_text(lblBattPct, "100%");
    lv_obj_align(lblBattPct, LV_ALIGN_TOP_RIGHT, -22, 18);

    barBattery = lv_bar_create(cardBatt);
    lv_obj_set_size(barBattery, 322, 16);
    lv_obj_align(barBattery, LV_ALIGN_TOP_MID, 0, 68);
    lv_bar_set_range(barBattery, 0, 100);
    lv_bar_set_value(barBattery, 100, LV_ANIM_OFF);
    lv_obj_set_style_radius(barBattery, 8, LV_PART_MAIN);
    lv_obj_set_style_radius(barBattery, 8, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(barBattery, lv_color_hex(0x27272A), LV_PART_MAIN);
    lv_obj_set_style_bg_color(barBattery, lv_color_hex(0xD4D4D8), LV_PART_INDICATOR);
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

    if (lblBattPct) {
        lv_label_set_text_fmt(lblBattPct, "%d%%", status.batteryPercent);
    }
    if (barBattery) {
        lv_bar_set_value(barBattery, status.batteryPercent, LV_ANIM_ON);
    }
}

void ui_manager_notify_button(const char *buttonText) {
    (void)buttonText;
}

void ui_manager_sync_brightness(uint8_t brightness) {
    if (sliderBright && brightness >= 15) {
        lv_slider_set_value(sliderBright, brightness, LV_ANIM_ON);
    }
    if (lblBrightPct) {
        int pct = (brightness * 100) / 255;
        lv_label_set_text_fmt(lblBrightPct, "%d%%", pct);
    }
    scr_watchface_update_power(cachedPower, brightness);
}

uint8_t ui_manager_get_volume() {
    return currentVolumePct;
}
