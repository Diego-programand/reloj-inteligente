/**
 * @file power_manager.cpp
 * @brief Implementación de la máquina de estados de pantalla y gesto de muñeca.
 */

#include "power_manager.h"
#include "hal/hal_display.h"
#include "hal/hal_rtc.h"
#include "ui/ui_manager.h"

// Tiempo de encendido de pantalla sin tocarla antes de entrar en reposo (15 segundos)
#define SCREEN_TIMEOUT_MS           15000

// Margen tras tocar la pantalla durante el cual no se apaga aunque se incline el reloj
#define TOUCH_GRACE_PERIOD_MS       4000

// Margen tras apagar manualmente con el botón PWR para que soltar el botón no la re-encienda
#define MANUAL_OFF_COOLDOWN_MS      1500

static uint32_t lastActivityMs   = 0;
static uint32_t lastTouchMs      = 0;
static uint32_t manualOffMs      = 0;
static uint32_t lastImuPollMs    = 0;
static bool wristWakeEnabled     = true;

static void wakeScreen(const char *reason) {
    lastActivityMs = millis();
    if (!hal_display_is_on()) {
        // Actualizar la hora en el buffer antes de iluminar los píxeles
        ui_manager_update_time(hal_rtc_get_time());
        ui_manager_loop();
        hal_display_set_power(true);
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
    lastTouchMs    = millis();
    lastImuPollMs  = millis();
}

void power_manager_notify_user_activity() {
    uint32_t now = millis();
    lastActivityMs = now;
    lastTouchMs    = now;
}

void power_manager_toggle_screen_manual() {
    if (hal_display_is_on()) {
        manualOffMs = millis();
        sleepScreen("Boton PWR");
    } else {
        wakeScreen("Boton PWR");
    }
}

void power_manager_set_wrist_wake(bool enabled) {
    wristWakeEnabled = enabled;
}

bool power_manager_is_wrist_wake_enabled() {
    return wristWakeEnabled;
}

void power_manager_update(ImuData *outImu) {
    uint32_t now = millis();

    // 1. Muestrear el sensor de movimiento QMI8658 cada 40 ms (25 Hz)
    if (now - lastImuPollMs >= 40) {
        lastImuPollMs = now;
        WristGesture gesture = hal_imu_poll_gesture(outImu);

        if (wristWakeEnabled) {
            if (!hal_display_is_on()) {
                // Si la pantalla está apagada y pasó el enfriamiento del botón PWR, despertar al levantar muñeca
                if (gesture == WRIST_GESTURE_RAISE_TO_VIEW && (now - manualOffMs >= MANUAL_OFF_COOLDOWN_MS)) {
                    wakeScreen("Gesto de muneca");
                }
            } else {
                // Si la pantalla está encendida y el usuario baja el brazo (y no está tocando la pantalla)
                if (gesture == WRIST_GESTURE_LOWER_ARM && (now - lastTouchMs >= TOUCH_GRACE_PERIOD_MS)) {
                    sleepScreen("Brazo abajo");
                }
            }
        }
    }

    // 2. Apagado automático por inactividad (15 segundos sin actividad)
    if (hal_display_is_on() && (now - lastActivityMs >= SCREEN_TIMEOUT_MS)) {
        sleepScreen("Inactividad 15s");
    }
}
