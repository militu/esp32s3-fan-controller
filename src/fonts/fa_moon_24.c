/*******************************************************************************
 * Size: 24 px
 * Bpp: 4
 * Opts: --bpp 4 --size 24 --no-compress --font fa-solid-900.ttf --range 61830 --format lvgl -o fa_moon_24.c
 ******************************************************************************/

#include "lvgl.h"

#ifndef FA_MOON_24
#define FA_MOON_24 1
#endif

#if FA_MOON_24

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+F186 "" */
    0x0, 0x0, 0x0, 0x3, 0x79, 0xa8, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x6d, 0xff, 0xff, 0xd0,
    0x0, 0x0, 0x0, 0x0, 0x1, 0xcf, 0xff, 0xff,
    0xa0, 0x0, 0x0, 0x0, 0x0, 0x2, 0xef, 0xff,
    0xff, 0x90, 0x0, 0x0, 0x0, 0x0, 0x0, 0xdf,
    0xff, 0xff, 0xc0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x9f, 0xff, 0xff, 0xf3, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x1f, 0xff, 0xff, 0xfd, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x7, 0xff, 0xff, 0xff, 0xa0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0xbf, 0xff, 0xff, 0xf8,
    0x0, 0x0, 0x0, 0x0, 0x0, 0xe, 0xff, 0xff,
    0xff, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0xff,
    0xff, 0xff, 0xfa, 0x0, 0x0, 0x0, 0x0, 0x0,
    0xf, 0xff, 0xff, 0xff, 0xe0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0xdf, 0xff, 0xff, 0xff, 0x50, 0x0,
    0x0, 0x0, 0x0, 0xa, 0xff, 0xff, 0xff, 0xfd,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x4f, 0xff, 0xff,
    0xff, 0xfa, 0x0, 0x0, 0x0, 0x0, 0x0, 0xdf,
    0xff, 0xff, 0xff, 0xfb, 0x10, 0x0, 0x0, 0x0,
    0x4, 0xff, 0xff, 0xff, 0xff, 0xff, 0x82, 0x0,
    0x0, 0x0, 0x9, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xde, 0xc0, 0x0, 0x9, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xf8, 0x0, 0x0, 0x6, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xe6, 0x0, 0x0, 0x0,
    0x1, 0x7d, 0xff, 0xff, 0xfd, 0x71, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x1, 0x34, 0x31, 0x0, 0x0,
    0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 288, .box_w = 19, .box_h = 22, .ofs_x = 0, .ofs_y = -2}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 61830, .range_length = 1, .glyph_id_start = 1,
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
const lv_font_t fa_moon_24 = {
#else
lv_font_t fa_moon_24 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 22,          /*The maximum line height required by the font*/
    .base_line = 2,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -2,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if FA_MOON_24*/

