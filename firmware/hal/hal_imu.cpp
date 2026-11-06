/**
 * @file hal_imu.cpp
 * @brief Implementación del driver I2C y detector sensible de gesto/movimiento para QMI8658 (0x6B).
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
#define QMI8658_REG_AX_L            0x35
#define QMI8658_REG_RESET           0x60

#define QMI8658_WHO_AM_I_VAL        0x05

static TwoWire *imuWire = nullptr;
static uint8_t imuAddr = I2C_ADDR_QMI8658;
static bool imuReady = false;

// Estado interno del detector de gesto de muñeca y movimiento
static bool wasInViewingPose = true;
static uint32_t lowerArmEnterMs = 0;

// Vector de gravedad filtrado (promedio lento) para detectar cualquier inclinación o levantamiento suave
static float baselineAx = 0.0f;
static float baselineAy = 0.0f;
static float baselineAz = 1.0f;
static bool  baselineInit = false;

static bool writeReg(uint8_t reg, uint8_t val) {
    if (!imuWire) return false;
    imuWire->beginTransmission(imuAddr);
    imuWire->write(reg);
    imuWire->write(val);
    return (imuWire->endTransmission() == 0);
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

    // 1. Reset por software del chip QMI8658
    writeReg(QMI8658_REG_RESET, 0xB0);
    delay(30);

    // 2. CTRL1 (0x02): Auto-incremento de direcciones I2C (ADDR_AI = bit 6) y formato Little-Endian
    writeReg(QMI8658_REG_CTRL1, 0x40);

    // 3. CTRL2 (0x03): Acelerómetro en rango ±4g (0x10) y frecuencia 125 Hz (0x06) -> 0x16
    writeReg(QMI8658_REG_CTRL2, 0x16);

    // 4. CTRL3 (0x04): Giroscopio en rango ±512 dps (0x50) y frecuencia 125 Hz (0x06) -> 0x56
    writeReg(QMI8658_REG_CTRL3, 0x56);

    // 5. CTRL5 (0x06): Activar filtro paso-bajo interno en acelerómetro y giroscopio
    writeReg(QMI8658_REG_CTRL5, 0x11);

    // 6. CTRL7 (0x08): Encender acelerómetro (bit 0) y giroscopio (bit 1) -> 0x03
    writeReg(QMI8658_REG_CTRL7, 0x03);
    delay(20);

    imuReady = true;
    return true;
}

ImuData hal_imu_read() {
    ImuData d = {0, 0, 0, 0, 0, 0, 0, false};
    if (!imuReady) return d;

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

    // Escala ±4g -> 8192 LSB por cada 1g.
    // El chip QMI8658 está soldado en la cara inferior del PCB, por lo que invertimos Z
    // para que +1.0g equivalga a "pantalla mirando hacia arriba".
    d.accelX =  (float)axRaw / 8192.0f;
    d.accelY =  (float)ayRaw / 8192.0f;
    d.accelZ = -(float)azRaw / 8192.0f;

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

    if (!baselineInit) {
        baselineAx = d.accelX;
        baselineAy = d.accelY;
        baselineAz = d.accelZ;
        baselineInit = true;
    }

    // 1. Calcular cuánto se ha inclinado o movido el reloj respecto a su postura de reposo reciente
    float tiltDelta = fabsf(d.accelX - baselineAx) +
                      fabsf(d.accelY - baselineAy) +
                      fabsf(d.accelZ - baselineAz);

    // Actualizar suavemente la línea base de reposo (filtro exponencial lento)
    baselineAx = baselineAx * 0.88f + d.accelX * 0.12f;
    baselineAy = baselineAy * 0.88f + d.accelY * 0.12f;
    baselineAz = baselineAz * 0.88f + d.accelZ * 0.12f;

    // 2. ¿Está la pantalla orientada hacia el usuario? (desde ligeramente inclinada az >= 0.15g hasta plana +1.0g)
    bool isViewingPose = (d.accelZ >= 0.15f);

    // 3. ¿Está el reloj claramente volteado hacia abajo? (az < -0.30g)
    bool isFaceDown = (d.accelZ < -0.30f);

    if (!isViewingPose) {
        wasInViewingPose = false;
    } else {
        // Despertar si:
        // A) El reloj venía de estar inclinado afuera/abajo (!wasInViewingPose) y entró en postura de vista, O
        // B) El usuario giró o levantó el reloj suavemente (giro >= 14 dps o cambio de inclinación >= 0.14g)
        if (!wasInViewingPose || d.gyroTotal >= 14.0f || tiltDelta >= 0.14f) {
            wasInViewingPose = true;
            // Sincronizar baseline para no disparar en bucle mientras ya está quieto
            baselineAx = d.accelX;
            baselineAy = d.accelY;
            baselineAz = d.accelZ;
            return WRIST_GESTURE_RAISE_TO_VIEW;
        }
    }

    // 4. Si se voltea el reloj hacia abajo durante 500 ms, apagar pantalla
    uint32_t now = millis();
    if (isFaceDown) {
        if (lowerArmEnterMs == 0) {
            lowerArmEnterMs = now;
        } else if (now - lowerArmEnterMs >= 500) {
            lowerArmEnterMs = 0;
            wasInViewingPose = false;
            return WRIST_GESTURE_LOWER_ARM;
        }
    } else {
        lowerArmEnterMs = 0;
    }

    return WRIST_GESTURE_NONE;
}
