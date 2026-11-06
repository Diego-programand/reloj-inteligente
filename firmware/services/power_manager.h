/**
 * @file power_manager.h
 * @brief Máquina de estados de ahorro de energía y encendido inteligente por movimiento.
 *
 * Funciones principales:
 *   1. Encendido automático al levantar/girar la muñeca (Wake-on-Wrist-Raise con QMI8658).
 *   2. Apagado rápido al bajar el brazo (Sleep-on-Wrist-Drop).
 *   3. Apagado automático por inactividad tras 15 segundos sin tocar la pantalla.
 *   4. Encendido/apagado manual inmediato con el botón físico PWR.
 */

#pragma once

#include <Arduino.h>
#include "hal/hal_imu.h"

/**
 * @brief Inicializa el gestor de energía de pantalla y el temporizador de actividad.
 */
void power_manager_init();

/**
 * @brief Registra actividad del usuario (toque en pantalla o botón) para reiniciar
 *        la cuenta regresiva de apagado automático.
 */
void power_manager_notify_user_activity();

/**
 * @brief Alterna manualmente el encendido/apagado de la pantalla (al pulsar el botón PWR).
 */
void power_manager_toggle_screen_manual();

/**
 * @brief Activa o desactiva el encendido automático al girar la muñeca.
 */
void power_manager_set_wrist_wake(bool enabled);

/**
 * @brief Devuelve true si el encendido por gesto de muñeca está activo.
 */
bool power_manager_is_wrist_wake_enabled();

/**
 * @brief Bucle periódico que consulta la IMU (QMI8658) y gestiona el encendido/apagado
 *        automático de la pantalla AMOLED.
 * @param outImu Puntero opcional para recibir la última lectura del acelerómetro/giroscopio.
 */
void power_manager_update(ImuData *outImu = nullptr);

