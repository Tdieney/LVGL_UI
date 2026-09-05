// Generated placeholder by tools/png2lvgl.py — replace with real asset.
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

const LV_ATTRIBUTE_MEM_ALIGN uint8_t ui_img_topbar_chip_map[] = {
    // 8x8 ALPHA_4BIT = 32 bytes (all transparent placeholder)
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

const lv_img_dsc_t ui_img_topbar_chip = {
    .header.cf = LV_IMG_CF_ALPHA_4BIT,
    .header.always_zero = 0,
    .header.reserved = 0,
    .header.w = 8,
    .header.h = 8,
    .data_size = 32,
    .data = ui_img_topbar_chip_map,
};
