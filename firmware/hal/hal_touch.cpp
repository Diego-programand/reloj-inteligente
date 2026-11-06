/**
 * @file hal_touch.cpp
 * @brief Implementación del lector táctil FT3168 por I2C (dirección 0x38) con captura de pulso por interrupción en GPIO 38.
 */

#include "hal_touch.h"
#include "config/pins_config.h"

static TwoWire *touchWire = nullptr;
static bool touchInitialized = false;
static volatile bool touchIrqLatched = false;
static uint16_t lastX = WATCH_LCD_WIDTH / 2;
static uint16_t lastY = WATCH_LCD_HEIGHT / 2;

static void IRAM_ATTR onTouchInterrupt() {
    touchIrqLatched = true;
}

bool hal_touch_init(TwoWire &wirePort) {
    touchWire = &wirePort;

    // 1. Configurar pin de interrupción táctil (GPIO 38) y pin de Reset táctil (GPIO 9)
    pinMode(WATCH_TP_INT_PIN, INPUT_PULLUP);
    pinMode(WATCH_TP_RST_PIN, OUTPUT);

    // 2. Pulso de despertar para el chip FT3168
    digitalWrite(WATCH_TP_RST_PIN, LOW);
    delay(50);
    digitalWrite(WATCH_TP_RST_PIN, HIGH);
    delay(300); // Esperar a que arranque el microcontrolador interno del FT3168

    // 3. Capturar por hardware cualquier bajada del pin INT (GPIO 38) para no perder toques rápidos
    attachInterrupt(digitalPinToInterrupt(WATCH_TP_INT_PIN), onTouchInterrupt, FALLING);

    // 4. Verificar que conteste en la dirección I2C 0x38
    touchWire->beginTransmission(I2C_ADDR_FT3168);
    if (touchWire->endTransmission() == 0) {
        touchInitialized = true;
        return true;
    }

    touchInitialized = false;
    return false;
}

TouchPoint hal_touch_read() {
    TouchPoint pt = {false, lastX, lastY};
    if (!touchInitialized || !touchWire) {
        return pt;
    }

    bool pinLow = (digitalRead(WATCH_TP_INT_PIN) == LOW);
    bool irqSeen = touchIrqLatched;
    if (!pinLow && !irqSeen) {
        return pt;
    }
    touchIrqLatched = false;

    // Leer 5 registros seguidos a partir del registro 0x02 (TD_STATUS, XH, XL, YH, YL)
    touchWire->beginTransmission(I2C_ADDR_FT3168);
    touchWire->write(0x02);
    if (touchWire->endTransmission(true) != 0) {
        if (irqSeen) {
            pt.touched = true;
        }
        return pt;
    }

    uint8_t count = touchWire->requestFrom((uint8_t)I2C_ADDR_FT3168, (uint8_t)5);
    if (count < 5) {
        if (irqSeen) {
            pt.touched = true;
        }
        return pt;
    }

    uint8_t tdStatus = touchWire->read();
    uint8_t xh       = touchWire->read();
    uint8_t xl       = touchWire->read();
    uint8_t yh       = touchWire->read();
    uint8_t yl       = touchWire->read();

    uint8_t numPoints = tdStatus & 0x0F;
    if (numPoints >= 1 && numPoints <= 5) {
        pt.touched = true;
        pt.x = ((uint16_t)(xh & 0x0F) << 8) | xl;
        pt.y = ((uint16_t)(yh & 0x0F) << 8) | yl;

        if (pt.x >= WATCH_LCD_WIDTH)  pt.x = WATCH_LCD_WIDTH - 1;
        if (pt.y >= WATCH_LCD_HEIGHT) pt.y = WATCH_LCD_HEIGHT - 1;
        lastX = pt.x;
        lastY = pt.y;
    } else if (irqSeen) {
        // Si fue un toque ultrarrápido que activó la interrupción GPIO 38 y ya soltó el dedo,
        // igual reportamos el toque para que Tap-to-Wake responda siempre al primer toque.
        pt.touched = true;
    }

    return pt;
}
