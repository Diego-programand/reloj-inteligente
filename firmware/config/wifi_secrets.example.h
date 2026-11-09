/**
 * ============================================================================
 * PLANTILLA DE CREDENCIALES WI-FI Y GOOGLE CALENDAR (wifi_secrets.example.h)
 * ============================================================================
 *
 * ¿CÓMO USAR ESTE ARCHIVO?
 * ------------------------
 * 1. Haz una copia de este archivo en esta misma carpeta (`firmware/config/`)
 *    y renómbrala exactamente como:
 *
 *       firmware/config/wifi_secrets.h
 *
 * 2. Reemplaza los valores de ejemplo de abajo con el nombre de tu red Wi-Fi,
 *    tu contraseña y la URL de tu puente de Google Apps Script.
 *    (Consulta `tools/google-apps-script/README_SETUP.md` para el paso a paso).
 *
 * NOTAS TÉCNICAS IMPORTANTES PARA TU RELOJ ESP32-S3:
 * --------------------------------------------------
 * - BANDA WI-FI 2.4 GHz: La radio física integrada en el procesador ESP32-S3
 *   se conecta exclusivamente a redes Wi-Fi de 2.4 GHz (o redes duales 2.4/5 GHz).
 *   No podrá ver redes configuradas únicamente en la banda de 5 GHz.
 *
 * - MODO DEMO AUTOMÁTICO (MOCK DATA): Si aún no creas el archivo `wifi_secrets.h`
 *   (o si mantienes los textos de ejemplo de abajo sin cambiar), el firmware
 *   detectará su ausencia automáticamente en tiempo de compilación mediante
 *   `#if __has_include("config/wifi_secrets.h")` y activará el **Modo Demo
 *   (Mock Data)** sin errores de compilación, cargando una agenda de prueba en
 *   la pantalla AMOLED.
 *
 * - SEGURIDAD GIT: El archivo real `firmware/config/wifi_secrets.h` está
 *   protegido en `.gitignore` para que tus contraseñas jamás se suban al
 *   repositorio.
 */

#pragma once

#ifndef WIFI_SECRETS_H
#define WIFI_SECRETS_H

// Nombre (SSID) de tu red Wi-Fi de 2.4 GHz (o el Hotspot de 2.4 GHz de tu teléfono)
#define WIFI_SSID        "TU_RED_WIFI_2.4GHZ"

// Contraseña de tu red Wi-Fi
#define WIFI_PASSWORD    "TU_PASSWORD_WIFI"

// URL completa del puente Google Apps Script (terminada en /exec?key=TU_TOKEN)
#define GCAL_WEBHOOK_URL "https://script.google.com/macros/s/TU_DEPLOYMENT_ID/exec?key=MI_TOKEN_SUPER_SECRETO_RELOJ_2026"

#endif // WIFI_SECRETS_H
