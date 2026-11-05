/**
 * @file ui_manager.cpp
 * @brief Implementación de LVGL con buffers en Octal PSRAM y pantallas deslizables.
 */

#include "ui_manager.h"
#include <lvgl.h>
#include <esp_heap_caps.h>
#include "config/pins_config.h"
#include "hal/hal_display.h"
#include "hal/hal_touch.h"

// Reservamos buffers de 1/2 pantalla (410 x 251 píxeles = ~205 KB cada uno) en los 8 MB de Octal PSRAM
#define LVGL_BUFFER_LINES       (WATCH_LCD_HEIGHT / 2)
#define LVGL_BUFFER_PIXELS      (WATCH_LCD_WIDTH * LVGL_BUFFER_LINES)

static lv_disp_draw_buf_t drawBuf;
static lv_color_t *buf1 = nullptr;
static lv_color_t *buf2 = nullptr;

// Punteros a los elementos visuales (widgets) para actualizarlos en tiempo real
static lv_obj_t *lblBatteryInfo  = nullptr;
static lv_obj_t *barBattery      = nullptr;
static lv_obj_t *sliderBright    = nullptr;
static lv_obj_t *lblBrightVal    = nullptr;
static lv_obj_t *lblTouchCoords  = nullptr;
static lv_obj_t *lblButtonEvent  = nullptr;
static lv_obj_t *lblTapCounter   = nullptr;
static int tapCount = 0;

/**
 * @brief Puente entre LVGL y nuestra función hal_display_push_colors (QSPI).
 */
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
 * @brief Redondeador obligatorio para pantallas AMOLED CO5300 / SH8601:
 *        El chip físico de la pantalla agrupa los píxeles de 2 en 2 (pares).
 *        Si LVGL intenta actualizar un recuadro con ancho o coordenada impar,
 *        las filas se desfasan diagonalmente y la imagen se ve "rasgada" o deformada.
 *        Esta función fuerza siempre coordenadas iniciales pares (0, 2, 4...)
 *        y coordenadas finales impares (1, 3, 5...) para que ancho y alto sean siempre pares.
 */
static void lvgl_rounder_cb(lv_disp_drv_t *disp_drv, lv_area_t *area) {
    area->x1 = area->x1 & ~0x01; // Redondear hacia abajo a número PAR
    area->y1 = area->y1 & ~0x01; // Redondear hacia abajo a número PAR
    area->x2 = area->x2 | 0x01;  // Redondear hacia arriba a número IMPAR (ancho par)
    area->y2 = area->y2 | 0x01;  // Redondear hacia arriba a número IMPAR (alto par)
}

/**
 * @brief Puente entre LVGL y nuestro lector táctil FT3168 (hal_touch_read).
 */
static void lvgl_touch_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    TouchPoint tp = hal_touch_read();
    if (tp.touched) {
        data->state   = LV_INDEV_STATE_PR;
        data->point.x = tp.x;
        data->point.y = tp.y;

        if (lblTouchCoords) {
            lv_label_set_text_fmt(lblTouchCoords, "Toque activo: X=%3u, Y=%3u", tp.x, tp.y);
        }
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

/**
 * @brief Evento cuando el usuario mueve con el dedo la barra de brillo en pantalla.
 */
static void slider_brightness_event_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t val = lv_slider_get_value(slider);
    if (val < 15) val = 15; // Evitar apagar la pantalla por completo desde el slider táctil
    hal_display_set_brightness((uint8_t)val);

    if (lblBrightVal) {
        int pct = (val * 100) / 255;
        lv_label_set_text_fmt(lblBrightVal, "Brillo AMOLED: %d%% (%ld/255)", pct, (long)val);
    }
}

/**
 * @brief Evento cuando el usuario toca el botón contador en la segunda pantalla.
 */
static void btn_counter_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        tapCount++;
        if (lblTapCounter) {
            lv_label_set_text_fmt(lblTapCounter, "Toques: %d", tapCount);
        }
    }
}

/**
 * @brief Construye las 2 pantallas deslizables (Tileview) de validación de la Fase 2.
 */
static void build_phase2_ui() {
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);

    // Contenedor de mosaicos (Tileview) para deslizar horizontalmente con el dedo
    lv_obj_t *tv = lv_tileview_create(scr);
    lv_obj_set_size(tv, WATCH_LCD_WIDTH, WATCH_LCD_HEIGHT);
    lv_obj_set_style_bg_color(tv, lv_color_hex(0x000000), 0);

    // Crear Pantalla 1 (Columna 0) y Pantalla 2 (Columna 1)
    lv_obj_t *tile1 = lv_tileview_add_tile(tv, 0, 0, LV_DIR_RIGHT);
    lv_obj_t *tile2 = lv_tileview_add_tile(tv, 1, 0, LV_DIR_LEFT);

    // ========================================================================
    // PANTALLA 1: PANEL DE DIAGNÓSTICO Y CONTROL EN VIVO
    // ========================================================================
    lv_obj_t *title = lv_label_create(tile1);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x00E5FF), 0);
    lv_label_set_text(title, "SMARTWATCH S3");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

    lv_obj_t *subtitle = lv_label_create(tile1);
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0x90A4AE), 0);
    lv_label_set_text(subtitle, "Fase 2: LVGL + 8MB PSRAM Activa");
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 48);

    // --- Tarjeta 1: Estado de Batería (AXP2101) ---
    lv_obj_t *cardBatt = lv_obj_create(tile1);
    lv_obj_set_size(cardBatt, 374, 110);
    lv_obj_align(cardBatt, LV_ALIGN_TOP_MID, 0, 78);
    lv_obj_set_style_bg_color(cardBatt, lv_color_hex(0x121820), 0);
    lv_obj_set_style_border_color(cardBatt, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_radius(cardBatt, 18, 0);
    lv_obj_clear_flag(cardBatt, LV_OBJ_FLAG_SCROLLABLE);

    lblBatteryInfo = lv_label_create(cardBatt);
    lv_obj_set_style_text_font(lblBatteryInfo, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lblBatteryInfo, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(lblBatteryInfo, "Bateria: --% (---- mV)");
    lv_obj_align(lblBatteryInfo, LV_ALIGN_TOP_LEFT, 4, 4);

    barBattery = lv_bar_create(cardBatt);
    lv_obj_set_size(barBattery, 336, 22);
    lv_obj_align(barBattery, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_bar_set_range(barBattery, 0, 100);
    lv_bar_set_value(barBattery, 100, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(barBattery, lv_color_hex(0x263238), LV_PART_MAIN);
    lv_obj_set_style_bg_color(barBattery, lv_color_hex(0x00E676), LV_PART_INDICATOR);

    // --- Tarjeta 2: Control Táctil de Brillo AMOLED ---
    lv_obj_t *cardBright = lv_obj_create(tile1);
    lv_obj_set_size(cardBright, 374, 125);
    lv_obj_align(cardBright, LV_ALIGN_TOP_MID, 0, 202);
    lv_obj_set_style_bg_color(cardBright, lv_color_hex(0x121820), 0);
    lv_obj_set_style_border_color(cardBright, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_radius(cardBright, 18, 0);
    lv_obj_clear_flag(cardBright, LV_OBJ_FLAG_SCROLLABLE);

    lblBrightVal = lv_label_create(cardBright);
    lv_obj_set_style_text_font(lblBrightVal, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lblBrightVal, lv_color_hex(0xFFD54F), 0);
    lv_label_set_text(lblBrightVal, "Brillo AMOLED: 86% (220/255)");
    lv_obj_align(lblBrightVal, LV_ALIGN_TOP_LEFT, 4, 2);

    sliderBright = lv_slider_create(cardBright);
    lv_obj_set_size(sliderBright, 326, 26);
    lv_obj_align(sliderBright, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_slider_set_range(sliderBright, 15, 255);
    lv_slider_set_value(sliderBright, hal_display_get_brightness(), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(sliderBright, lv_color_hex(0x00E5FF), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sliderBright, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    lv_obj_add_event_cb(sliderBright, slider_brightness_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    // --- Tarjeta 3: Monitor en Vivo de Toque y Botones ---
    lv_obj_t *cardInput = lv_obj_create(tile1);
    lv_obj_set_size(cardInput, 374, 110);
    lv_obj_align(cardInput, LV_ALIGN_TOP_MID, 0, 340);
    lv_obj_set_style_bg_color(cardInput, lv_color_hex(0x121820), 0);
    lv_obj_set_style_border_color(cardInput, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_radius(cardInput, 18, 0);
    lv_obj_clear_flag(cardInput, LV_OBJ_FLAG_SCROLLABLE);

    lblTouchCoords = lv_label_create(cardInput);
    lv_obj_set_style_text_font(lblTouchCoords, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lblTouchCoords, lv_color_hex(0x80D8FF), 0);
    lv_label_set_text(lblTouchCoords, "Toque activo: X=---, Y=---");
    lv_obj_align(lblTouchCoords, LV_ALIGN_TOP_LEFT, 4, 6);

    lblButtonEvent = lv_label_create(cardInput);
    lv_obj_set_style_text_font(lblButtonEvent, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lblButtonEvent, lv_color_hex(0xB0BEC5), 0);
    lv_label_set_text(lblButtonEvent, "Boton fisico: Ninguno aun");
    lv_obj_align(lblButtonEvent, LV_ALIGN_BOTTOM_LEFT, 4, -6);

    lv_obj_t *lblHint1 = lv_label_create(tile1);
    lv_obj_set_style_text_font(lblHint1, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblHint1, lv_color_hex(0x00E5FF), 0);
    lv_label_set_text(lblHint1, "Desliza hacia la izquierda >>");
    lv_obj_align(lblHint1, LV_ALIGN_BOTTOM_MID, 0, -14);

    // ========================================================================
    // PANTALLA 2: PRUEBA INTERACTIVA DE 60 FPS (AL DESLIZAR A LA IZQUIERDA)
    // ========================================================================
    lv_obj_t *title2 = lv_label_create(tile2);
    lv_obj_set_style_text_font(title2, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title2, lv_color_hex(0x00E676), 0);
    lv_label_set_text(title2, "PANTALLA 2 (SWIPE)");
    lv_obj_align(title2, LV_ALIGN_TOP_MID, 0, 24);

    // Anillo circular interactivo (Arc)
    lv_obj_t *arc = lv_arc_create(tile2);
    lv_obj_set_size(arc, 240, 240);
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_arc_set_value(arc, 75);
    lv_obj_align(arc, LV_ALIGN_CENTER, 0, -25);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x00E5FF), LV_PART_INDICATOR);

    // Botón central dentro del anillo
    lv_obj_t *btn = lv_btn_create(tile2);
    lv_obj_set_size(btn, 160, 64);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, -25);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x0288D1), 0);
    lv_obj_set_style_radius(btn, 32, 0);
    lv_obj_add_event_cb(btn, btn_counter_event_cb, LV_EVENT_CLICKED, nullptr);

    lblTapCounter = lv_label_create(btn);
    lv_obj_set_style_text_font(lblTapCounter, &lv_font_montserrat_20, 0);
    lv_label_set_text(lblTapCounter, "Toques: 0");
    lv_obj_center(lblTapCounter);

    lv_obj_t *lblHint2 = lv_label_create(tile2);
    lv_obj_set_style_text_font(lblHint2, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblHint2, lv_color_hex(0x90A4AE), 0);
    lv_label_set_text(lblHint2, "<< Desliza a la derecha para volver");
    lv_obj_align(lblHint2, LV_ALIGN_BOTTOM_MID, 0, -18);
}

bool ui_manager_init() {
    lv_init();

    // 1. Reservar dos buffers de dibujo grandes dentro de los 8 MB de Octal PSRAM
    buf1 = (lv_color_t *)heap_caps_malloc(LVGL_BUFFER_PIXELS * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    buf2 = (lv_color_t *)heap_caps_malloc(LVGL_BUFFER_PIXELS * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);

    if (!buf1 || !buf2) {
        return false;
    }

    lv_disp_draw_buf_init(&drawBuf, buf1, buf2, LVGL_BUFFER_PIXELS);

    // 2. Registrar el controlador de pantalla (410 x 502) en LVGL
    static lv_disp_drv_t dispDrv;
    lv_disp_drv_init(&dispDrv);
    dispDrv.hor_res    = WATCH_LCD_WIDTH;
    dispDrv.ver_res    = WATCH_LCD_HEIGHT;
    dispDrv.flush_cb   = lvgl_flush_cb;
    dispDrv.rounder_cb = lvgl_rounder_cb; // Garantiza bloques pares sin deformación en CO5300
    dispDrv.draw_buf   = &drawBuf;
    lv_disp_drv_register(&dispDrv);

    // 3. Registrar el panel táctil capacitivo FT3168 en LVGL
    static lv_indev_drv_t indevDrv;
    lv_indev_drv_init(&indevDrv);
    indevDrv.type    = LV_INDEV_TYPE_POINTER;
    indevDrv.read_cb = lvgl_touch_read_cb;
    lv_indev_drv_register(&indevDrv);

    // 4. Construir las pantallas interactivas de la Fase 2
    build_phase2_ui();

    return true;
}

void ui_manager_loop() {
    lv_timer_handler();
}

void ui_manager_update_power(const WatchPowerStatus &status) {
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
        lv_label_set_text_fmt(lblBrightVal, "Brillo AMOLED: %d%% (%u/255)", pct, brightness);
    }
}

