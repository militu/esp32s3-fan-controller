#if 1 /*Set it to "1" to enable content*/

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/

/*Color depth: 1 (1 byte per pixel), 8 (RGB332), 16 (RGB565), 32 (ARGB8888)*/
#define LV_COLOR_DEPTH 16

/*Swap the 2 bytes of RGB565 color. Useful if the display has an 8-bit interface (e.g. SPI)*/
#define LV_COLOR_16_SWAP 0  // Changed to 0 for ILI9341

/*Enable features to draw on transparent background.*/
#define LV_COLOR_SCREEN_TRANSP 0

/*====================
   MEMORY SETTINGS
 *====================*/

/*1: use custom malloc/free, 0: use the built-in `lv_mem_alloc()` and `lv_mem_free()`*/
#define LV_MEM_CUSTOM 0
#if LV_MEM_CUSTOM == 0
    #define LV_MEM_SIZE (48U * 1024U)
#else
    #define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
    #define LV_MEM_CUSTOM_ALLOC   malloc
    #define LV_MEM_CUSTOM_FREE    free
    #define LV_MEM_CUSTOM_REALLOC realloc
#endif
#define LV_MEM_POOL_INCLUDE <esp_heap_caps.h>

/*====================
   HAL SETTINGS
 *====================*/

/*Default display refresh period. LVG will redraw changed areas with this period time*/
#define LV_DISP_DEF_REFR_PERIOD 10      /*[ms]*/

/*Input device read period in milliseconds*/
#define LV_INDEV_DEF_READ_PERIOD 10     /*[ms]*/

/*Use a custom tick source that tells the elapsed time in milliseconds.*/
#define LV_TICK_CUSTOM 1
#if LV_TICK_CUSTOM
    #define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
    #define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())
#endif

/*Default Dot Per Inch. Used to initialize default sizes such as widgets sized, style paddings.*/
#define LV_DPI_DEF 130     /*[px/inch]*/

/*Enable complex draw engine*/
#define LV_DRAW_COMPLEX 1    /* Required for radius, gradients, etc */

/*=====================
*  THEME USAGE
*====================*/

/*A simple, impressive and very complete theme*/
#define LV_USE_THEME_DEFAULT 1
#if LV_USE_THEME_DEFAULT
    /*0: Light mode; 1: Dark mode*/
    #define LV_THEME_DEFAULT_DARK 1
    /*1: Enable grow on press*/
    #define LV_THEME_DEFAULT_GROW 1
#endif

/*====================
 *  FONT USAGE
 *==================*/

/*Enable built-in fonts*/
#define LV_FONT_MONTSERRAT_8 1
#define LV_FONT_MONTSERRAT_10 1
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1

/*Always set a default font*/
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/*==================
 * WIDGET USAGE
 *================*/

#define LV_USE_ARC        1
#define LV_USE_BAR        1
#define LV_USE_BTN        1
#define LV_USE_LABEL      1
#define LV_USE_LINE       1
#define LV_USE_TABLE      1
#define LV_USE_CHART      1

/*==================
 * ANIMATION
 *================*/

#define LV_USE_ANIMATION 1
#define LV_USE_GPU_STM32_DMA2D 0  // Disable unused features
#define LV_USE_GPU_NXP_PXP 0
#define LV_USE_GPU_NXP_VG_LITE 0
#define LV_USE_GPU_SDL 0
#define LV_ARC_INTERPOLATION_STACK_SIZE 4  // Reduce from default

#endif /*LV_CONF_H*/

#endif /*End of "Content enable"*/