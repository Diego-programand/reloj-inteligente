/**
 * @file lv_conf.h
 * @brief Configuración del motor gráfico LVGL v8.3 para ESP32-S3-Touch-AMOLED-2.06 (410x502)
 */

#if 1 /* Habilitar contenido de configuración */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   CONFIGURACIÓN DE COLOR
 *====================*/
/* Profundidad de color: 16 bits por píxel (RGB565) */
#define LV_COLOR_DEPTH 16

/* No invertir bytes en LVGL porque hal_display_push_colors ya lo hace por hardware/DMA */
#define LV_COLOR_16_SWAP 0

/*====================
   MEMORIA Y RENDIMIENTO
 *====================*/
/* Usar malloc/free estándar y reservar objetos dinámicos en PSRAM/RAM */
#define LV_MEM_CUSTOM 1
#if LV_MEM_CUSTOM == 1
    #define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
    #define LV_MEM_CUSTOM_ALLOC   malloc
    #define LV_MEM_CUSTOM_FREE    free
    #define LV_MEM_CUSTOM_REALLOC realloc
#endif

/* Periodo de refresco de pantalla: 15 ms (~66 FPS) */
#define LV_DISP_DEF_REFR_PERIOD 15

/* Periodo de lectura del panel táctil: 15 ms */
#define LV_INDEV_DEF_READ_PERIOD 15

/* Usar el reloj de milisegundos de Arduino automáticamente */
#define LV_TICK_CUSTOM 1
#if LV_TICK_CUSTOM
    #define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
    #define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())
#endif

/* Densidad de píxeles de la pantalla AMOLED 2.06" */
#define LV_DPI_DEF 240

/*====================
   TIPOGRAFÍAS (FUENTES)
 *====================*/
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_48 1

#define LV_FONT_DEFAULT &lv_font_montserrat_16

/* Soporte para símbolos UTF-8 (acentos, grados, iconos) */
#define LV_TXT_ENC LV_TXT_ENC_UTF8

/*====================
   WIDGETS Y TEMAS
 *====================*/
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1

#define LV_USE_ARC        1
#define LV_USE_BAR        1
#define LV_USE_BTN        1
#define LV_USE_BTNMATRIX  1
#define LV_USE_CANVAS     1
#define LV_USE_CHECKBOX   1
#define LV_USE_DROPDOWN   1
#define LV_USE_IMG        1
#define LV_USE_LABEL      1
#define LV_USE_LINE       1
#define LV_USE_ROLLER     1
#define LV_USE_SLIDER     1
#define LV_USE_SWITCH     1
#define LV_USE_TEXTAREA   1
#define LV_USE_TABLE      1

#define LV_USE_FLEX       1
#define LV_USE_GRID       1

#endif /*LV_CONF_H*/
#endif /*End of "Content enable"*/

