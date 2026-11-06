/**
 * @file hal_imu.cpp
 * @brief Implementación del driver I2C y detector de movimiento para QMI8658 (0x6B).
 */

#include "hal_imu.h"
#include <math.h>

// Registros internos del chip QMI8658
#define QMI8658_REG_WHO_AM_I        0x00
#define QMI8658_REG_CTRL1           0x02
#define QMI8658_REG_CTRL2           0x03
#define QMI8658_REG_CTRL3           0x04
#define QMI8658_REG_CTRL5           0x06
#define QMI8658_REG_CTRL7           0x08
#define QMI8658_REG_STATUS0         0x2E
#define QMI8658_REG_TIMESTAMP_L     0x30
#define QMI8658_REG_AX_L            0x35

#define QMI8658_WHO_AM_I_VAL        0x05

static TwoWire *imuWire = nullptr;
static uint8_t imuAddr = I2C_ADDR_QMI8658;
static bool imuReady = false;

// Estado interno del detector de movimiento
static uint32_t lowerArmEnterMs = 0;
static float prevAx = 0.0f;
static float prevAy = 0.0f;
static float prevAz = 1.0f;
static float slowAx = 0.0f;
static float slowAy = 0.0f;
static float slowAz = 1.0f;
static bool  filterInit = false;

static bool writeReg(uint8_t reg, uint8_t val) {
    if (!imuWire) return false;
    imuWire->beginTransmission(imuAddr);
    imuWire->write(reg);
    imuWire->write(val);
    return (imuWire->endTransmission(true) == 0);
}

static bool readRegs(uint8_t startReg, uint8_t *buf, size_t len) {
    if (!imuWire) return false;
    imuWire->beginTransmission(imuAddr);
    imuWire->write(startReg);
    if (imuWire->endTransmission(false) != 0) {
        return false;
    }
    size_t readCount = imuWire->requestFrom(imuAddr, (uint8_t)len);
    if (readCount != len) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        buf[i] = imuWire->read();
    }
    return true;
}

bool hal_imu_init(TwoWire &wire) {
    imuWire = &wire;
    imuAddr = I2C_ADDR_QMI8658;

    uint8_t who = 0;
    if (!readRegs(QMI8658_REG_WHO_AM_I, &who, 1) || who != QMI8658_WHO_AM_I_VAL) {
        imuAddr = 0x6A;
        if (!readRegs(QMI8658_REG_WHO_AM_I, &who, 1) || who != QMI8658_WHO_AM_I_VAL) {
            imuReady = false;
            return false;
        }
    }

    // Secuencia exacta verificada en el firmware de fábrica de la placa:
    // 1. Apagar sensores brevemente antes de configurar
    writeReg(QMI8658_REG_CTRL7, 0x00);
    delay(10);

    // 2. CTRL1 (0x02) = 0x40: Auto-incremento de registro (ADDR_AI=1), Little-Endian, oscilador interno activo
    writeReg(QMI8658_REG_CTRL1, 0x40);

    // 3. CTRL2 (0x03) = 0x23: Acelerómetro ±8g (4096 LSB/g), ODR 1000 Hz
    writeReg(QMI8658_REG_CTRL2, 0x23);

    // 4. CTRL3 (0x04) = 0x53: Giroscopio ±512 dps (64 LSB/dps), ODR 1000 Hz
    writeReg(QMI8658_REG_CTRL3, 0x53);

    // 5. CTRL5 (0x06) = 0x11: Filtros paso-bajo activos
    writeReg(QMI8658_REG_CTRL5, 0x11);

    // 6. CTRL7 (0x08) = 0x03: Activar Acelerómetro (bit 0) y Giroscopio (bit 1) en flujo continuo
    writeReg(QMI8658_REG_CTRL7, 0x03);
    delay(20);

    imuReady = true;
    return true;
}

ImuData hal_imu_read() {
    ImuData d = {0, 0, 0, 0, 0, 0, 0, false};
    if (!imuReady) return d;

    // Leer STATUS0 y TIMESTAMP (0x2E..0x32) para desbloquear el latch de datos nuevos del QMI8658
    uint8_t statusAndTs[5] = {0};
    readRegs(QMI8658_REG_STATUS0, statusAndTs, 5);

    // Leer los 12 bytes de Acelerómetro (X,Y,Z) y Giroscopio (X,Y,Z) desde 0x35
    uint8_t raw[12] = {0};
    if (!readRegs(QMI8658_REG_AX_L, raw, 12)) {
        return d;
    }

    int16_t axRaw = (int16_t)((uint16_t)raw[0]  | ((uint16_t)raw[1]  << 8));
    int16_t ayRaw = (int16_t)((uint16_t)raw[2]  | ((uint16_t)raw[3]  << 8));
    int16_t azRaw = (int16_t)((uint16_t)raw[4]  | ((uint16_t)raw[5]  << 8));
    int16_t gxRaw = (int16_t)((uint16_t)raw[6]  | ((uint16_t)raw[7]  << 8));
    int16_t gyRaw = (int16_t)((uint16_t)raw[8]  | ((uint16_t)raw[9]  << 8));
    int16_t gzRaw = (int16_t)((uint16_t)raw[10] | ((uint16_t)raw[11] << 8));

    // Escala ±8g -> 4096 LSB por cada 1g (invertimos Z por montaje en cara inferior del PCB)
    d.accelX =  (float)axRaw / 4096.0f;
    d.accelY =  (float)ayRaw / 4096.0f;
    d.accelZ = -(float)azRaw / 4096.0f;

    // Escala ±512 dps -> 64 LSB por cada 1 grado/seg
    d.gyroX = (float)gxRaw / 64.0f;
    d.gyroY = (float)gyRaw / 64.0f;
    d.gyroZ = (float)gzRaw / 64.0f;

    d.gyroTotal = sqrtf(d.gyroX * d.gyroX + d.gyroY * d.gyroY + d.gyroZ * d.gyroZ);
    d.valid = true;
    return d;
}

WristGesture hal_imu_poll_gesture(ImuData *outData) {
    ImuData d = hal_imu_read();
    if (outData) {
        *outData = d;
    }
    if (!d.valid) return WRIST_GESTURE_NONE;

    if (!filterInit) {
        prevAx = slowAx = d.accelX;
        prevAy = slowAy = d.accelY;
        prevAz = slowAz = d.accelZ;
        filterInit = true;
        return WRIST_GESTURE_NONE;
    }

    // Cambio rápido (muestra a muestra, 40 ms) y cambio suave (respecto a postura lenta de 1 segundo)
    float stepDelta = fabsf(d.accelX - prevAx) +
                      fabsf(d.accelY - prevAy) +
                      fabsf(d.accelZ - prevAz);

    float slowDelta = fabsf(d.accelX - slowAx) +
                      fabsf(d.accelY - slowAy) +
                      fabsf(d.accelZ - slowAz);

    prevAx = d.accelX;
    prevAy = d.accelY;
    prevAz = d.accelZ;

    // Filtro lento (95% memoria / 5% nuevo) para que un movimiento lento acumule diferencia fácilmente
    slowAx = slowAx * 0.95f + d.accelX * 0.05f;
    slowAy = slowAy * 0.95f + d.accelY * 0.05f;
    slowAz = slowAz * 0.95f + d.accelZ * 0.05f;

    // ¿Está el reloj completamente boca abajo? (az < -0.55g)
    bool isFaceDown = (d.accelZ < -0.55f);

    // Si NO está boca abajo, despertar con cualquier movimiento o giro natural del reloj:
    // - Giro >= 8 dps, o
    // - Cambio entre muestras >= 0.06g, o
    // - Cambio de inclinación acumulado >= 0.10g
    if (!isFaceDown) {
        if (d.gyroTotal >= 8.0f || stepDelta >= 0.06f || slowDelta >= 0.10f) {
            slowAx = d.accelX;
            slowAy = d.accelY;
            slowAz = d.accelZ;
            lowerArmEnterMs = 0;
            return WRIST_GESTURE_RAISE_TO_VIEW;
        }
    }

    // Si se coloca boca abajo durante 600 ms, apagar pantalla
    uint32_t now = millis();
    if (isFaceDown) {
        if (lowerArmEnterMs == 0) {
            lowerArmEnterMs = now;
        } else if (now - lowerArmEnterMs >= 600) {
            lowerArmEnterMs = 0;
            return WRIST_GESTURE_LOWER_ARM;
        }
    } else {
        lowerArmEnterMs = 0;
    }

    return WRIST_GESTURE_NONE;
}
