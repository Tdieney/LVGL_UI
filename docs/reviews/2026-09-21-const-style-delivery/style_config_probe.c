#include "../../../ui_theme.h"
#include <stdio.h>
int main(void) {
    int errors=0;
#if LV_USE_ASSERT_STYLE
    printf("STYLE_SENTINEL actual=%08x expected=%08x\n",(unsigned)ui_style_canvas.sentinel,(unsigned)LV_STYLE_SENTINEL_VALUE);
    errors += ui_style_canvas.sentinel != LV_STYLE_SENTINEL_VALUE;
#endif
    lv_color_t expected=lv_color_hex(0xEEECE7);
    lv_color_t actual=ui_style_canvas.v_p.const_props[0].value.color;
    printf("COLOR swap=%d actual=%04x expected=%04x\n",LV_COLOR_16_SWAP,actual.full,expected.full);
    errors += actual.full != expected.full;
    return errors?1:0;
}
