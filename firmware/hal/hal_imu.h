/**
 * @file hal_imu.h
 * @brief Capa de Abstracción de Hardware (HAL) para el sensor de movimiento QMI8658 (0x6B).
 *
 * ¿QUÉ HACE EL CHIP QMI8658?
 * - Es una IMU (Inertial Measurement Unit o Unidad de Medición Inercial) de 6 ejes:
 *   1. Acelerómetro 3D (X, Y, Z): Mide hacia dónde apunta la gravedad terrestre (en fuerzas 'g').
 *      Nos permite saber la inclinación exacta de tu muñeca (si la pantalla mira hacia tus ojos
 *      o si llevas el brazo caído a un costado).
 *   2. Giroscopio 3D (X, Y, Z): Mide la velocidad de giro (en grados por segundo, 'dps').
 *      Nos permite detectar el gesto de girar la muñeca o levantar el brazo.
 */

#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "config/pins_config.h"

struct ImuData {
    float accelX;      // Aceleración eje X en fuerzas g (-4.0 a +4.0 g)
    float accelY;      // Aceleración eje Y en fuerzas g (-4.0 a +4.0 g)
    float accelZ;      // Aceleración eje Z en fuerzas g (+1.0 g = pantalla mirando arriba)
    float gyroX;       // Velocidad angular X en grados/segundo (dps)
    float gyroY;       // Velocidad angular Y en grados/segundo (dps)
    float gyroZ;       // Velocidad angular Z en grados/segundo (dps)
    float gyroTotal;   // Magnitud total de giro (dps)
    bool  valid;       // true si la lectura I2C fue exitosa
};

enum WristGesture {
    WRIST_GESTURE_NONE = 0,
    WRIST_GESTURE_RAISE_TO_VIEW, // Usuario levantó o giró la muñeca para mirar la hora
    WRIST_GESTURE_LOWER_ARM      // Usuario bajó el brazo / volteó el reloj hacia afuera
};

/**
 * @brief Inicializa el sensor QMI8658 en el bus I2C (0x6B), activando acelerómetro (±4g)
 *        y giroscopio (±512 dps) con filtro de ruido interno.
 * @param wire Instancia de TwoWire ya iniciada en GPIO 15 / GPIO 14.
 * @return true si el chip respondió con WHO_AM_I = 0x05.
 */
bool hal_imu_init(TwoWire &wire = Wire);

/**
 * @brief Lee en una sola ráfaga de 12 bytes los 6 ejes (acelerómetro + giroscopio).
 */
ImuData hal_imu_read();

/**
 * @brief Analiza el movimiento e inclinación actual para detectar si el usuario
 *        acaba de levantar la muñeca para ver la hora o si bajó el brazo.
 *        Debe llamarse periódicamente (ej. cada 40-50 ms).
 */
WristGesture hal_imu_poll_gesture(ImuData *outData = nullptr);
