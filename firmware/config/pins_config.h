/**
 * @file pins_config.h
 * @brief Mapa oficial de pines y direcciones I2C para ESP32-S3-Touch-AMOLED-2.06
 * 
 * PLACA: Waveshare / Spotpear ESP32-S3-Touch-AMOLED-2.06
 * MCU:   ESP32-S3R8 (Dual-Core 240MHz, 8MB Octal PSRAM, 32MB Quad SPI Flash)
 * 
 * ADVERTENCIA CRÍTICA DE HARDWARE:
 * NUNCA configurar GPIO 35 como pin de entrada/salida ni interrupción.
 * Aunque en el esquemático está conectado a AXP2101_IRQ, el GPIO 35 es usado
 * internamente por la memoria Octal PSRAM (SPIDQS). Tocarlo causa crash inmediato.
 */

#pragma once

// ============================================================================
// 1. DIMENSIONES DE LA PANTALLA AMOLED (CO5300 / SH8601)
// ============================================================================
#define WATCH_LCD_WIDTH             410
#define WATCH_LCD_HEIGHT            502

// ============================================================================
// 2. BUS I2C PRINCIPAL (Compartido por PMIC, Touch, RTC, IMU y Audio)
// ============================================================================
#define WATCH_I2C_SDA_PIN           15
#define WATCH_I2C_SCL_PIN           14
#define WATCH_I2C_FREQ_HZ           400000

// Direcciones I2C (7-bit)
#define I2C_ADDR_AXP2101            0x34  // PMIC Gestión de energía y batería
#define I2C_ADDR_FT3168             0x38  // Panel táctil capacitivo
#define I2C_ADDR_PCF85063           0x51  // Reloj en Tiempo Real (RTC)
#define I2C_ADDR_QMI8658            0x6B  // IMU 6 ejes (alternativa: 0x6A)
#define I2C_ADDR_ES8311             0x18  // DAC Altavoz
#define I2C_ADDR_ES7210             0x40  // ADC 2 Micrófonos

// ============================================================================
// 3. PANTALLA AMOLED QSPI (Driver CO5300 / SH8601)
// ============================================================================
#define WATCH_LCD_CS_PIN            12
#define WATCH_LCD_SCK_PIN           11
#define WATCH_LCD_D0_PIN            4
#define WATCH_LCD_D1_PIN            5
#define WATCH_LCD_D2_PIN            6
#define WATCH_LCD_D3_PIN            7
#define WATCH_LCD_RST_PIN           8
#define WATCH_LCD_TE_EN_PIN         13

// ============================================================================
// 4. PANEL TÁCTIL CAPACITIVO (FT3168)
// ============================================================================
#define WATCH_TP_RST_PIN            9
#define WATCH_TP_INT_PIN            38

// ============================================================================
// 5. SUBSISTEMA DE AUDIO I2S (ES8311 + ES7210)
// ============================================================================
#define WATCH_I2S_MCLK_PIN          16
#define WATCH_I2S_BCLK_PIN          41
#define WATCH_I2S_LRCK_PIN          45
#define WATCH_I2S_DOUT_PIN          40
#define WATCH_I2S_DIN_PIN           42
#define WATCH_PA_CTRL_PIN           46    // Habilitación de amplificador de altavoz

// ============================================================================
// 6. RANURA MICROSD / TF CARD (SDMMC 1-bit / SPI)
// ============================================================================
#define WATCH_SD_CMD_MOSI_PIN       1
#define WATCH_SD_CLK_SCK_PIN        2
#define WATCH_SD_D0_MISO_PIN        3

// ============================================================================
// 7. BOTONES FÍSICOS Y PINES RESTRINGIDOS
// ============================================================================
#define WATCH_BTN_BOOT_PIN          0     // Activo en LOW
// Nota: El botón PWR se lee vía I2C a través del registro de eventos del AXP2101.
// PROHIBIDO USAR GPIO 35 (Conflicto con Octal PSRAM SPIDQS).
