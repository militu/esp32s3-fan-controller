/*******************************************************************************
 * Size: 12 px
 * Bpp: 4
 * Opts: --bpp 4 --size 12 --no-compress --font fa-solid-900.ttf --range 62745 --format lvgl -o fa_tower_broadcast_12.c
 ******************************************************************************/

#include "lvgl.h"

#ifndef FA_TOWER_BROADCAST_12
#define FA_TOWER_BROADCAST_12 1
#endif

#if FA_TOWER_BROADCAST_12

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+F519 "" */
    0x25, 0x0, 0x0, 0x0, 0x0, 0x1, 0x60, 0xad,
    0x39, 0x0, 0x0, 0x4, 0x85, 0xf3, 0xe9, 0x9e,
    0x4, 0xeb, 0x6, 0xf3, 0xf6, 0xf8, 0xbc, 0xb,
    0xff, 0x34, 0xf4, 0xf7, 0xe9, 0x9e, 0x4, 0xfc,
    0x6, 0xf3, 0xf6, 0xad, 0x39, 0x0, 0xf8, 0x3,
    0x85, 0xf3, 0x25, 0x0, 0x0, 0xf8, 0x0, 0x1,
    0x50, 0x0, 0x0, 0x0, 0xf8, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0xf8, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0xf8, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0xf8, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0xf7, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x51,
    0x0, 0x0, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 216, .box_w = 14, .box_h = 13, .ofs_x = 0, .ofs_y = -2}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 62745, .range_length = 1, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 4,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t fa_tower_broadcast_12 = {
#else
lv_font_t fa_tower_broadcast_12 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 13,          /*The maximum line height required by the font*/
    .base_line = 2,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if FA_TOWER_BROADCAST_12*/

