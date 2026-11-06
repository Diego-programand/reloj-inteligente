/**
 * @file power_manager.cpp
 * @brief Implementación de encendido por toque (Tap-to-Wake), botón PWR y apagado por inactividad.
 */

#include "power_manager.h"
#include "hal/hal_display.h"
#include "hal/hal_touch.h"
#include "hal/hal_rtc.h"
#include "ui/ui_manager.h"
#include <lvgl.h>

// Tiempo de encendido de pantalla sin actividad antes de apagarse sola (20 segundos)
#define SCREEN_TIMEOUT_MS           20000

// Margen tras apagar manualmente con el botón PWR para evitar rebotes
#define MANUAL_OFF_COOLDOWN_MS      400

static uint32_t lastActivityMs = 0;
static uint32_t manualOffMs    = 0;

static void wakeScreen(const char *reason) {
    lastActivityMs = millis();
    if (!hal_display_is_on()) {
        hal_display_set_power(true);
        ui_manager_update_time(hal_rtc_get_time());
        lv_obj_invalidate(lv_scr_act());
        ui_manager_loop();
        ui_manager_notify_button(reason);
        Serial.printf(">>> [ENERGIA] Pantalla ENCENDIDA (%s) <<<\n", reason);
    }
}

static void sleepScreen(const char *reason) {
    if (hal_display_is_on()) {
        hal_display_set_power(false);
        Serial.printf(">>> [ENERGIA] Pantalla APAGADA (%s) <<<\n", reason);
    }
}

void power_manager_init() {
    lastActivityMs = millis();
}

void power_manager_notify_user_activity() {
    lastActivityMs = millis();
}

void power_manager_toggle_screen_manual() {
    if (hal_display_is_on()) {
        manualOffMs = millis();
        sleepScreen("Boton PWR");
    } else {
        wakeScreen("Boton PWR");
    }
}

void power_manager_update() {
    uint32_t now = millis();

    // 1. Si la pantalla está APAGADA, encender al tocar cualquier parte del cristal (Tap-to-Wake)
    if (!hal_display_is_on()) {
        TouchPoint tp = hal_touch_read();
        if (tp.touched && (now - manualOffMs >= MANUAL_OFF_COOLDOWN_MS)) {
            wakeScreen("Toque en pantalla");
        }
        return;
    }

    // 2. Si la pantalla está ENCENDIDA, apagar tras 20 segundos sin tocarla
    if (now - lastActivityMs >= SCREEN_TIMEOUT_MS) {
        sleepScreen("Inactividad 20s");
    }
}
