/**
 * @file hal_imu.cpp
 * @brief Implementación del driver I2C y detector de gesto de muñeca para QMI8658 (0x6B).
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

// Estado interno del detector de gesto de muñeca
static bool wasInViewingPose = true;
static uint32_t lastMotionMs = 0;
static uint32_t poseEnterMs  = 0;
static uint32_t lowerArmEnterMs = 0;
static float prevAx = 0.0f, prevAy = 0.0f, prevAz = 1.0f;

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
        // Probar dirección alternativa 0x6A por seguridad
        imuAddr = 0x6A;
        if (!readRegs(QMI8658_REG_WHO_AM_I, &who, 1) || who != QMI8658_WHO_AM_I_VAL) {
            imuReady = false;
            return false;
        }
    }

    // 1. Reset por software del chip QMI8658
    writeReg(QMI8658_REG_RESET, 0xB0);
    delay(15);

    // 2. CTRL1 (0x02): Activar auto-incremento de direcciones I2C (ADDR_AI = bit 6) y formato Little-Endian
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
    // Nota de hardware: El chip QMI8658 está soldado en la cara inferior del PCB,
    // por lo que invertimos el signo de Z para que +1.0g equivalga a "pantalla mirando hacia arriba".
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

    uint32_t now = millis();

    // 1. Detectar si hubo movimiento físico reciente (giro de muñeca o cambio de aceleración)
    float deltaA = fabsf(d.accelX - prevAx) + fabsf(d.accelY - prevAy) + fabsf(d.accelZ - prevAz);
    prevAx = d.accelX;
    prevAy = d.accelY;
    prevAz = d.accelZ;

    if (d.gyroTotal > 32.0f || deltaA > 0.18f) {
        lastMotionMs = now;
    }

    // 2. ¿Está la pantalla en postura de lectura?
    //    Con el eje Z corregido (+1.0g = pantalla hacia arriba), cualquier ángulo desde
    //    inclinado hacia tus ojos (az >= 0.35g) hasta horizontal es postura de lectura.
    bool isViewingPose = (d.accelZ >= 0.35f) && (fabsf(d.accelX) < 0.90f) && (fabsf(d.accelY) < 0.90f);

    // 3. ¿Está el reloj volteado hacia abajo / brazo completamente girado hacia afuera?
    //    Solo consideramos brazo bajado cuando el cristal apunta claramente hacia abajo (az < -0.25g).
    bool isArmLowered = (d.accelZ < -0.25f);

    if (!isViewingPose) {
        // Salió de la postura de lectura (ej. bajó el brazo o inclinó el reloj de lado)
        wasInViewingPose = false;
        poseEnterMs = 0;
    } else {
        // Está en postura de lectura:
        // Caso A: Venía de tener el brazo abajo/inclinado (!wasInViewingPose) y lo subió a la vista.
        // Caso B: El reloj estaba quieto sobre la mesa con la pantalla apagada y el usuario lo movió/levantó.
        if (!wasInViewingPose) {
            if (poseEnterMs == 0) {
                poseEnterMs = now;
            } else if ((now - poseEnterMs >= 60) && (now - lastMotionMs <= 1500)) {
                wasInViewingPose = true;
                poseEnterMs = 0;
                return WRIST_GESTURE_RAISE_TO_VIEW;
            }
        } else if (d.gyroTotal > 45.0f || deltaA > 0.25f) {
            return WRIST_GESTURE_RAISE_TO_VIEW;
        }
    }

    // 4. Detección de brazo bajado sostenido (> 500 ms) para apagar la pantalla sin esperar todo el timeout
    if (isArmLowered) {
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
