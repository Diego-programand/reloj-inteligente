/**
 * @file hal_power.cpp
 * @brief Implementación del controlador de energía AXP2101 por I2C (sin GPIO 35).
 */

#include "hal_power.h"
#include "config/pins_config.h"

#define XPOWERS_CHIP_AXP2101
#include <XPowersLib.h>

static XPowersPMU pmu;
static bool pmuInitialized = false;

bool hal_power_init(TwoWire &wirePort) {
    // 1. Conectar con el chip AXP2101 en la dirección 0x34 usando los pines SDA=15 y SCL=14
    if (!pmu.begin(wirePort, AXP2101_SLAVE_ADDRESS, WATCH_I2C_SDA_PIN, WATCH_I2C_SCL_PIN)) {
        pmuInitialized = false;
        return false;
    }

    // 2. Configurar límites de seguridad de entrada USB (VBUS)
    pmu.setVbusVoltageLimit(XPOWERS_AXP2101_VBUS_VOL_LIM_4V36);
    pmu.setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_900MA);

    // 3. Configurar voltaje mínimo del sistema (3.0 V)
    pmu.setSysPowerDownVoltage(3000);

    // 4. Encender los rieles de alimentación (reguladores LDO) a 3.3V (3300 mV)
    //    Esto alimenta la pantalla AMOLED, el panel táctil, el RTC, el IMU y el audio.
    pmu.setALDO1Voltage(3300);
    pmu.enableALDO1();

    pmu.setALDO2Voltage(3300);
    pmu.enableALDO2();

    pmu.setALDO3Voltage(3300);
    pmu.enableALDO3();

    pmu.setALDO4Voltage(3300);
    pmu.enableALDO4();

    pmu.setBLDO1Voltage(3300);
    pmu.enableBLDO1();

    pmu.setBLDO2Voltage(3300);
    pmu.enableBLDO2();

    pmu.setDLDO1Voltage(3300);
    pmu.enableDLDO1();

    pmu.setDLDO2Voltage(3300);
    pmu.enableDLDO2();

    // 5. Activar los sensores internos de medición (ADC) para leer voltajes y batería
    pmu.enableBattDetection();
    pmu.enableVbusVoltageMeasure();
    pmu.enableBattVoltageMeasure();
    pmu.enableSystemVoltageMeasure();

    // 6. Configurar carga segura de la batería de litio (corriente de 300 mA y tope de 4.2 V)
    pmu.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_300MA);
    pmu.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);

    // 7. Preparar la detección del botón físico PWR por registro I2C (sin usar GPIO 35)
    pmu.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    pmu.clearIrqStatus();
    pmu.enableIRQ(
        XPOWERS_AXP2101_PKEY_SHORT_IRQ |  // Detectar pulsación corta del botón PWR
        XPOWERS_AXP2101_PKEY_LONG_IRQ     // Detectar pulsación larga del botón PWR
    );

    pmuInitialized = true;
    return true;
}

WatchPowerStatus hal_power_get_status() {
    WatchPowerStatus status = {false, false, false, 0, 0, 0, 0};
    if (!pmuInitialized) {
        return status;
    }

    status.batteryConnected = pmu.isBatteryConnect();
    status.usbConnected     = pmu.isVbusIn();
    status.isCharging       = pmu.isCharging();
    status.batteryVoltageMv = status.batteryConnected ? pmu.getBattVoltage() : 0;
    status.batteryPercent   = status.batteryConnected ? pmu.getBatteryPercent() : 0;
    status.vbusVoltageMv    = status.usbConnected ? pmu.getVbusVoltage() : 0;
    status.sysVoltageMv     = pmu.getSystemVoltage();

    return status;
}

PwrButtonEvent hal_power_poll_button() {
    if (!pmuInitialized) {
        return PWR_BTN_NONE;
    }

    // Preguntamos por I2C al registro de estados del AXP2101 (polling seguro)
    pmu.getIrqStatus();

    PwrButtonEvent event = PWR_BTN_NONE;
    if (pmu.isPekeyShortPressIrq()) {
        event = PWR_BTN_SHORT_PRESS;
    } else if (pmu.isPekeyLongPressIrq()) {
        event = PWR_BTN_LONG_PRESS;
    }

    if (event != PWR_BTN_NONE) {
        pmu.clearIrqStatus();
    }

    return event;
}

