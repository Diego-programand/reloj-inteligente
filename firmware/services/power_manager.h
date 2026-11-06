/**
 * @file power_manager.h
 * @brief Gestor de encendido/apagado de pantalla por toque (Tap-to-Wake), botón PWR e inactividad.
 */

#pragma once

#include <Arduino.h>

/**
 * @brief Inicializa los temporizadores de actividad de la pantalla.
 */
void power_manager_init();

/**
 * @brief Registra actividad del usuario (toque en pantalla o botón físico)
 *        para reiniciar el contador de apagado automático.
 */
void power_manager_notify_user_activity();

/**
 * @brief Alterna manualmente el encendido/apagado de la pantalla (al pulsar el botón PWR).
 */
void power_manager_toggle_screen_manual();

/**
 * @brief Ejecuta la vigilancia de toque para despertar (Tap-to-Wake) e inactividad.
 */
void power_manager_update();
