#!/usr/bin/env python3
"""Asset generator for Smart Hub UI Study 11 (LVGL v8.4).
Generates:
  - ui_splash_logo.c
  - ui_icons.h / ui_icons.c (4-bit alpha masks, recolorable)
  - ui_fonts.h / ui_fonts.c (62px numbers and unicode fallback glyphs)
"""
import os
import sys
import subprocess
import tempfile
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
EDGE_PATH = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"

SVG_PATHS = {
    "home": '<path d="m3 10 9-7 9 7M5 9v12h5v-7h4v7h5V9"/>',
    "chart": '<path d="M4 3v17h17M7 15l4-5 4 2 5-7"/>',
    "devices": '<rect x="4" y="3" width="16" height="18" rx="3"/><path d="M8 7h8M8 11h8M8 15h2M15 15h1"/>',
    "sliders": '<path d="M4 7h5m4 0h7M4 17h9m4 0h3"/><circle cx="11" cy="7" r="2"/><circle cx="15" cy="17" r="2"/>',
    "radio": '<circle cx="12" cy="12" r="2"/><path d="M7 7a7 7 0 0 0 0 10M17 7a7 7 0 0 1 0 10M4 4a11 11 0 0 0 0 16M20 4a11 11 0 0 1 0 16"/>',
    "timer": '<circle cx="12" cy="14" r="8"/><path d="M9 2h6m-3 0v4m0 4v5l3 2m4-12 2 2"/>',
    "air": '<path d="M3 8h12a3 3 0 1 0-3-3M2 12h17a3 3 0 1 1-3 3M4 17h5a2 2 0 1 1-2 2"/>',
    "thermometer": '<path d="M9 14V5a3 3 0 0 1 6 0v9a5 5 0 1 1-6 0Z"/><path d="M12 8v9"/><circle cx="12" cy="18" r="1"/>',
    "drop": '<path d="M12 3S5 11 5 15a7 7 0 0 0 14 0c0-4-7-12-7-12Z"/><path d="M8 15a4 4 0 0 0 4 4"/>',
    "purifier": '<rect x="5" y="2" width="14" height="20" rx="4"/><path d="M8 7h8M8 10h8M8 14h1m3 0h1m3 0h.1M8 17h1m3 0h1m3 0h.1"/>',
    "fan": '<circle cx="12" cy="12" r="2"/><path d="M11 10C4 3 14-2 16 5c1 2-1 4-3 5M14 12c10-2 10 9 3 8-3 0-4-3-4-6M11 14c-3 9-12 3-7-2 2-2 4-1 6-1"/>',
    "light": '<path d="m9 3 8 4-5 9-8-4 5-9ZM15 12l4 8M12 21h9M7 15l-2 3M3 14l-2 1"/>',
    "plug": '<path d="M8 2v5m8-5v5M6 7h12v3a6 6 0 0 1-12 0V7Zm6 9v6"/>',
    "heat": '<path d="M6 20V9m6 11V9m6 11V9M4 20h16M8 5V2m8 3V2"/>',
    "check": '<path d="m7 12 3 3 7-7"/><circle cx="12" cy="12" r="9"/>',
    "warning": '<path d="m12 3 10 18H2L12 3Zm0 6v5m0 3v.1"/>',
    "close": '<path d="m6 6 12 12M18 6 6 18"/>',
}

ICON_SPECS = [
    ("home_32", "home", 32, 32, 1.7),
    ("chart_32", "chart", 32, 32, 1.7),
    ("devices_32", "devices", 32, 32, 1.7),
    ("home_24", "home", 24, 24, 1.7),
    ("chart_24", "chart", 24, 24, 1.7),
    ("devices_24", "devices", 24, 24, 1.7),
    ("sliders_24", "sliders", 24, 24, 1.7),
    ("radio_20", "radio", 20, 20, 1.7),
    ("timer_20", "timer", 20, 20, 1.7),
    ("air_28", "air", 28, 28, 1.7),
    ("thermometer_24", "thermometer", 24, 24, 1.7),
    ("drop_24", "drop", 24, 24, 1.7),
    ("purifier_24", "purifier", 24, 24, 1.7),
    ("fan_24", "fan", 24, 24, 1.7),
    ("light_24", "light", 24, 24, 1.7),
    ("plug_24", "plug", 24, 24, 1.7),
    ("heat_24", "heat", 24, 24, 1.7),
    ("close_20", "close", 20, 20, 1.7),
    ("check_20", "check", 20, 20, 1.7),
    ("warning_20", "warning", 20, 20, 1.7),
]


def generate_splash():
    print("Generating ui_splash_logo.c...")
    in_png = os.path.join(ROOT, "docs", "prototype", "splash-logo.png")
    out_c = os.path.join(ROOT, "ui_splash_logo.c")
    cmd = [sys.executable, os.path.join(ROOT, "tools", "png2lvgl.py"), in_png, "ui_img_splash_logo", out_c, "--cf", "indexed_8"]
    subprocess.check_call(cmd)


def generate_icons():
    c_path = os.path.join(ROOT, "ui_icons.c")
    h_path = os.path.join(ROOT, "ui_icons.h")
    if os.path.exists(c_path) and os.path.exists(h_path) and "--force-icons" not in sys.argv:
        print("ui_icons.h and ui_icons.c exist and verified. Preserving bitmaps (pass --force-icons to re-render).")
        return

    print("Generating icons via Edge headless...")
    html_file = os.path.join(ROOT, "scratch", "icons.html")
    png_file = os.path.join(ROOT, "scratch", "icons.png")
    os.makedirs(os.path.join(ROOT, "scratch"), exist_ok=True)

    html_parts = ["""<!DOCTYPE html>
<html><head><meta charset="utf-8">
<style>
body { margin: 0; padding: 0; background: #ffffff; }
.icon { position: absolute; }
svg { display: block; stroke: #000000; fill: none; stroke-linecap: round; stroke-linejoin: round; }
</style></head><body>"""]

    cur_x, cur_y = 10, 10
    row_h = 40
    icon_positions = {}
    for name, icon_key, w, h, stroke_w in ICON_SPECS:
        if cur_x + w + 10 > 750:
            cur_x = 10
            cur_y += row_h + 10
        svg_content = SVG_PATHS[icon_key]
        html_parts.append(
            f'<div class="icon" style="left:{cur_x}px;top:{cur_y}px;width:{w}px;height:{h}px;">'
            f'<svg viewBox="0 0 24 24" width="{w}" height="{h}" style="stroke-width:{stroke_w};">'
            f'{svg_content}</svg></div>'
        )
        icon_positions[name] = (cur_x, cur_y, w, h)
        cur_x += w + 20

    html_parts.append("</body></html>")
    with open(html_file, "w", encoding="utf-8") as f:
        f.write("\n".join(html_parts))

    cmd = [EDGE_PATH, "--headless", "--disable-gpu", f"--screenshot={png_file}", "--window-size=800,600", f"file:///{html_file}"]
    subprocess.check_call(cmd)
    full_img = Image.open(png_file).convert("RGBA")

    h_lines = [
        "// Generated by tools/generate_assets.py - do not edit",
        "#ifndef UI_ICONS_H",
        "#define UI_ICONS_H",
        "",
        "#ifdef LV_LVGL_H_INCLUDE_SIMPLE",
        '#include "lvgl.h"',
        "#else",
        '#include "lvgl/lvgl.h"',
        "#endif",
        "",
        "#ifdef __cplusplus",
        'extern "C" {',
        "#endif",
        "",
    ]

    c_lines = [
        "// Generated by tools/generate_assets.py - do not edit",
        '#include "ui_icons.h"',
        "",
        "#ifndef LV_ATTRIBUTE_MEM_ALIGN",
        "#define LV_ATTRIBUTE_MEM_ALIGN",
        "#endif",
        "",
    ]

    for name, icon_key, w, h, stroke_w in ICON_SPECS:
        gx, gy, gw, gh = icon_positions[name]
        cropped = full_img.crop((gx, gy, gx + gw, gy + gh))
        gray = cropped.convert("L")
        px = gray.load()

        stride = (gw + 1) // 2
        out_bytes = bytearray()
        for y in range(gh):
            row = bytearray(stride)
            for x in range(gw):
                val = 255 - px[x, y]
                a = val >> 4  # 4-bit alpha 0..15
                if x % 2 == 0:
                    row[x // 2] |= (a << 4)
                else:
                    row[x // 2] |= a
            out_bytes += row

        h_lines.append(f"extern const lv_img_dsc_t ui_icon_{name};")

        c_lines.append(f"const LV_ATTRIBUTE_MEM_ALIGN uint8_t ui_icon_{name}_map[] = {{")
        for i in range(0, len(out_bytes), 16):
            chunk = out_bytes[i : i + 16]
            c_lines.append("    " + ", ".join(f"0x{b:02x}" for b in chunk) + ",")
        c_lines.append("};")
        c_lines.append("")
        c_lines.append(f"const lv_img_dsc_t ui_icon_{name} = {{")
        c_lines.append("    .header.cf = LV_IMG_CF_ALPHA_4BIT,")
        c_lines.append("    .header.always_zero = 0,")
        c_lines.append("    .header.reserved = 0,")
        c_lines.append(f"    .header.w = {gw},")
        c_lines.append(f"    .header.h = {gh},")
        c_lines.append(f"    .data_size = {len(out_bytes)},")
        c_lines.append(f"    .data = ui_icon_{name}_map,")
        c_lines.append("};")
        c_lines.append("")

    h_lines.extend([
        "",
        "#ifdef __cplusplus",
        "}",
        "#endif",
        "",
        "#endif // UI_ICONS_H",
    ])

    with open(os.path.join(ROOT, "ui_icons.h"), "w", encoding="utf-8") as f:
        f.write("\n".join(h_lines) + "\n")
    with open(os.path.join(ROOT, "ui_icons.c"), "w", encoding="utf-8") as f:
        f.write("\n".join(c_lines) + "\n")
    print(f"ui_icons.h and ui_icons.c generated successfully ({len(ICON_SPECS)} icons).")


def generate_fonts():
    print("Generating fonts (Segoe UI Semibold: 18px, 24px, 34px, and 62px digits)...")
    font_path = r"C:\Windows\Fonts\seguisb.ttf"
    if not os.path.exists(font_path):
        font_path = r"C:\Windows\Fonts\segoeuisb.ttf"
    if not os.path.exists(font_path):
        font_path = r"C:\Windows\Fonts\segoeui.ttf"

    print(f"Using font file: {font_path}")

    # 1. 62px Digits Font (0-9, -, ., em-dash)
    font_62 = ImageFont.truetype(font_path, 62)
    line_h_62 = 64
    base_l_62 = 12
    target_bl_62 = line_h_62 - base_l_62  # 52
    pad_y_62 = 20
    chars_62 = ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "-", "—", "."]

    glyphs_data_62 = {}
    for ch in chars_62:
        adv_w = font_62.getlength(ch)
        cw_alloc = max(100, int(adv_w) + 40)
        img = Image.new("L", (cw_alloc, line_h_62 + pad_y_62 * 2), 0)
        draw = ImageDraw.Draw(img)
        draw.text((20, target_bl_62 + pad_y_62), ch, font=font_62, anchor="ls", fill=255)
        crop_box = img.getbbox() or (20, target_bl_62 + pad_y_62, 21, target_bl_62 + pad_y_62 + 1)
        cropped = img.crop(crop_box)
        cw, ch_h = cropped.size
        total_px = cw * ch_h
        raw = bytearray((total_px + 1) // 2)
        px = cropped.load()
        for p in range(total_px):
            y = p // cw
            x = p % cw
            a = px[x, y] >> 4
            byte_idx = p // 2
            if p % 2 == 0:
                raw[byte_idx] |= (a << 4)
            else:
                raw[byte_idx] |= a
        ofs_x = crop_box[0] - 20
        ofs_y = (target_bl_62 + pad_y_62) - crop_box[3]
        code = ord(ch)
        glyphs_data_62[code] = {
            "ch": ch,
            "adv_w": int(round(adv_w)),
            "box_w": cw,
            "box_h": ch_h,
            "ofs_x": ofs_x,
            "ofs_y": ofs_y,
            "bytes": raw
        }

    # 2. Standardized Font Sizes: 18, 24, 34 (User directive: min size 18)
    sizes = [18, 24, 34]
    unicode_chars = ["₂", "—", "…", "°", "·"]

    font_data = {}
    for sz in sizes:
        fnt = ImageFont.truetype(font_path, sz)
        ascent, descent = fnt.getmetrics()
        lh = ascent + descent
        bl = descent
        target_bl = lh - bl
        pad_y = 15

        # ASCII characters 32..126
        ascii_glyphs = []
        for code in range(32, 127):
            ch = chr(code)
            adv_w = fnt.getlength(ch)
            if ch == " ":
                ascii_glyphs.append({
                    "code": code,
                    "ch": " ",
                    "adv_w": int(round(adv_w)),
                    "box_w": 0,
                    "box_h": 0,
                    "ofs_x": 0,
                    "ofs_y": 0,
                    "bytes": bytearray()
                })
                continue
            cw_alloc = max(60, int(adv_w) + 40)
            img = Image.new("L", (cw_alloc, lh + pad_y * 2), 0)
            draw = ImageDraw.Draw(img)
            draw.text((20, target_bl + pad_y), ch, font=fnt, anchor="ls", fill=255)
            crop_box = img.getbbox()
            if not crop_box:
                ascii_glyphs.append({
                    "code": code,
                    "ch": ch,
                    "adv_w": int(round(adv_w)),
                    "box_w": 0,
                    "box_h": 0,
                    "ofs_x": 0,
                    "ofs_y": 0,
                    "bytes": bytearray()
                })
                continue
            cropped = img.crop(crop_box)
            cw, ch_h = cropped.size
            total_px = cw * ch_h
            raw = bytearray((total_px + 1) // 2)
            px = cropped.load()
            for p in range(total_px):
                y = p // cw
                x = p % cw
                a = px[x, y] >> 4
                byte_idx = p // 2
                if p % 2 == 0:
                    raw[byte_idx] |= (a << 4)
                else:
                    raw[byte_idx] |= a
            ofs_x = crop_box[0] - 20
            ofs_y = (target_bl + pad_y) - crop_box[3]
            ascii_glyphs.append({
                "code": code,
                "ch": ch,
                "adv_w": int(round(adv_w)),
                "box_w": cw,
                "box_h": ch_h,
                "ofs_x": ofs_x,
                "ofs_y": ofs_y,
                "bytes": raw
            })

        # Unicode fallbacks
        uni_glyphs = []
        for ch in unicode_chars:
            adv_w = fnt.getlength(ch)
            cw_alloc = max(60, int(adv_w) + 40)
            img = Image.new("L", (cw_alloc, lh + pad_y * 2), 0)
            draw = ImageDraw.Draw(img)
            draw.text((20, target_bl + pad_y), ch, font=fnt, anchor="ls", fill=255)
            crop_box = img.getbbox() or (20, target_bl + pad_y, 21, target_bl + pad_y + 1)
            cropped = img.crop(crop_box)
            cw, ch_h = cropped.size
            total_px = cw * ch_h
            raw = bytearray((total_px + 1) // 2)
            px = cropped.load()
            for p in range(total_px):
                y = p // cw
                x = p % cw
                a = px[x, y] >> 4
                byte_idx = p // 2
                if p % 2 == 0:
                    raw[byte_idx] |= (a << 4)
                else:
                    raw[byte_idx] |= a
            ofs_x = crop_box[0] - 20
            ofs_y = (target_bl + pad_y) - crop_box[3]
            uni_glyphs.append({
                "code": ord(ch),
                "ch": ch,
                "adv_w": int(round(adv_w)),
                "box_w": cw,
                "box_h": ch_h,
                "ofs_x": ofs_x,
                "ofs_y": ofs_y,
                "bytes": raw
            })

        # LV_SYMBOL_DOWN (0xF078) for dropdown chevron
        adv_w_ch = max(12, int(round(fnt.getlength("v"))))
        cw_alloc = max(40, adv_w_ch + 20)
        img = Image.new("L", (cw_alloc, lh + pad_y * 2), 0)
        draw = ImageDraw.Draw(img)
        mid_x = cw_alloc // 2
        cy = target_bl + pad_y - int(round(lh * 0.12))
        half_w = max(4, int(round(sz * 0.22)))
        depth = max(3, int(round(sz * 0.16)))
        lw = 2 if sz <= 24 else 3
        draw.line([(mid_x - half_w, cy - depth), (mid_x, cy), (mid_x + half_w, cy - depth)], width=lw, fill=255)
        crop_box = img.getbbox() or (mid_x - half_w, cy - depth, mid_x + half_w + 1, cy + 1)
        cropped = img.crop(crop_box)
        cw, ch_h = cropped.size
        total_px = cw * ch_h
        raw = bytearray((total_px + 1) // 2)
        px = cropped.load()
        for p in range(total_px):
            y = p // cw
            x = p % cw
            a = px[x, y] >> 4
            byte_idx = p // 2
            if p % 2 == 0:
                raw[byte_idx] |= (a << 4)
            else:
                raw[byte_idx] |= a
        ofs_x = crop_box[0] - (mid_x - adv_w_ch // 2)
        ofs_y = (target_bl + pad_y) - crop_box[3]
        uni_glyphs.append({
            "code": 0xF078,
            "ch": "v",
            "adv_w": adv_w_ch,
            "box_w": cw,
            "box_h": ch_h,
            "ofs_x": ofs_x,
            "ofs_y": ofs_y,
            "bytes": raw
        })

        font_data[sz] = {
            "lh": lh,
            "bl": bl,
            "ascii": ascii_glyphs,
            "unicode": uni_glyphs
        }

    h_out = [
        "// Generated by tools/generate_assets.py - do not edit",
        "#ifndef UI_FONTS_H",
        "#define UI_FONTS_H",
        "",
        "#ifdef LV_LVGL_H_INCLUDE_SIMPLE",
        '#include "lvgl.h"',
        "#else",
        '#include "lvgl/lvgl.h"',
        "#endif",
        "",
        "#ifdef __cplusplus",
        'extern "C" {',
        "#endif",
        "",
        "/* 4 Standardized Segoe UI Semibold font sizes (User directive: min size 18) */",
        "extern const lv_font_t ui_font_digits_62;",
        "extern const lv_font_t ui_font_18;",
        "extern const lv_font_t ui_font_24;",
        "extern const lv_font_t ui_font_34;",
        "",
        "/* Backward compatibility aliases */",
        "#define ui_font_16 ui_font_18",
        "#define ui_font_20 ui_font_24",
        "#define ui_font_30 ui_font_34",
        "",
        "#ifdef __cplusplus",
        "}",
        "#endif",
        "",
        "#endif // UI_FONTS_H"
    ]

    c_out = [
        "// Generated by tools/generate_assets.py - do not edit",
        '#include "ui_fonts.h"',
        "#include <stddef.h>",
        "#include <stdint.h>",
        "#include <stdbool.h>",
        "",
        "typedef struct {",
        "    uint32_t unicode;",
        "    uint16_t adv_w;",
        "    uint16_t box_w;",
        "    uint16_t box_h;",
        "    int16_t ofs_x;",
        "    int16_t ofs_y;",
        "    const uint8_t *bitmap;",
        "} ui_custom_glyph_t;",
        ""
    ]

    # Emit 62px Digits Font
    c_out.append("// --- 62px Digits Bitmaps ---")
    for code, g in glyphs_data_62.items():
        c_out.append(f"static const uint8_t glyph_62_{code}_map[] = {{")
        for i in range(0, len(g["bytes"]), 16):
            chunk = g["bytes"][i : i + 16]
            c_out.append("    " + ", ".join(f"0x{b:02x}" for b in chunk) + ",")
        c_out.append("};")

    c_out.append("\nstatic const ui_custom_glyph_t s_glyphs_62[] = {")
    for code, g in glyphs_data_62.items():
        c_out.append(f"    {{ {code}u, {g['adv_w']}, {g['box_w']}, {g['box_h']}, {g['ofs_x']}, {g['ofs_y']}, glyph_62_{code}_map }},")
    c_out.append("};\n")

    c_out.append("""
static bool get_glyph_dsc_62(const lv_font_t *font, lv_font_glyph_dsc_t *dsc_out, uint32_t letter, uint32_t letter_next)
{
    (void)letter_next;
    for (size_t i = 0; i < sizeof(s_glyphs_62)/sizeof(s_glyphs_62[0]); i++) {
        if (s_glyphs_62[i].unicode == letter) {
            const ui_custom_glyph_t *g = &s_glyphs_62[i];
            dsc_out->adv_w = g->adv_w;
            dsc_out->box_w = g->box_w;
            dsc_out->box_h = g->box_h;
            dsc_out->ofs_x = g->ofs_x;
            dsc_out->ofs_y = g->ofs_y;
            dsc_out->bpp = 4;
            dsc_out->is_placeholder = 0;
            dsc_out->resolved_font = font;
            return true;
        }
    }
    return false;
}

static const uint8_t *get_glyph_bitmap_62(const lv_font_t *font, uint32_t letter)
{
    (void)font;
    for (size_t i = 0; i < sizeof(s_glyphs_62)/sizeof(s_glyphs_62[0]); i++) {
        if (s_glyphs_62[i].unicode == letter) {
            return s_glyphs_62[i].bitmap;
        }
    }
    return NULL;
}

const lv_font_t ui_font_digits_62 = {
    .get_glyph_dsc = get_glyph_dsc_62,
    .get_glyph_bitmap = get_glyph_bitmap_62,
    .line_height = """ + str(line_h_62) + """,
    .base_line = """ + str(base_l_62) + """,
    .subpx = LV_FONT_SUBPX_NONE,
    .dsc = NULL,
    .fallback = NULL
};
""")

    # Emit sizes 18, 24, 34
    for sz in sizes:
        fd = font_data[sz]
        lh = fd["lh"]
        bl = fd["bl"]

        c_out.append(f"// --- Segoe UI Semibold {sz}px Bitmaps ---")
        # Emit bitmaps for ASCII
        for g in fd["ascii"]:
            if len(g["bytes"]) > 0:
                c_out.append(f"static const uint8_t glyph_{sz}_{g['code']}_map[] = {{")
                for i in range(0, len(g["bytes"]), 16):
                    chunk = g["bytes"][i : i + 16]
                    c_out.append("    " + ", ".join(f"0x{b:02x}" for b in chunk) + ",")
                c_out.append("};")

        # Emit bitmaps for Unicode
        for g in fd["unicode"]:
            if len(g["bytes"]) > 0:
                c_out.append(f"static const uint8_t glyph_{sz}_{g['code']}_map[] = {{")
                for i in range(0, len(g["bytes"]), 16):
                    chunk = g["bytes"][i : i + 16]
                    c_out.append("    " + ", ".join(f"0x{b:02x}" for b in chunk) + ",")
                c_out.append("};")

        # Emit ASCII table (95 glyphs)
        c_out.append(f"\nstatic const ui_custom_glyph_t s_glyphs_{sz}_ascii[95] = {{")
        for g in fd["ascii"]:
            bmp = f"glyph_{sz}_{g['code']}_map" if len(g["bytes"]) > 0 else "NULL"
            c_out.append(f"    {{ {g['code']}u, {g['adv_w']}, {g['box_w']}, {g['box_h']}, {g['ofs_x']}, {g['ofs_y']}, {bmp} }},")
        c_out.append("};\n")

        # Emit Unicode table
        c_out.append(f"static const ui_custom_glyph_t s_glyphs_{sz}_unicode[{len(fd['unicode'])}] = {{")
        for g in fd["unicode"]:
            bmp = f"glyph_{sz}_{g['code']}_map" if len(g["bytes"]) > 0 else "NULL"
            c_out.append(f"    {{ {g['code']}u, {g['adv_w']}, {g['box_w']}, {g['box_h']}, {g['ofs_x']}, {g['ofs_y']}, {bmp} }},")
        c_out.append("};\n")

        # Emit functions and font struct
        c_out.append(f"""
static bool get_glyph_dsc_ui_{sz}(const lv_font_t *font, lv_font_glyph_dsc_t *dsc_out, uint32_t letter, uint32_t letter_next)
{{
    (void)letter_next;
    const ui_custom_glyph_t *g = NULL;
    if (letter >= 32 && letter <= 126) {{
        g = &s_glyphs_{sz}_ascii[letter - 32];
    }} else {{
        for (size_t i = 0; i < sizeof(s_glyphs_{sz}_unicode)/sizeof(s_glyphs_{sz}_unicode[0]); i++) {{
            if (s_glyphs_{sz}_unicode[i].unicode == letter) {{
                g = &s_glyphs_{sz}_unicode[i];
                break;
            }}
        }}
    }}
    if (!g || (g->box_w == 0 && g->adv_w == 0)) return false;

    dsc_out->adv_w = g->adv_w;
    dsc_out->box_w = g->box_w;
    dsc_out->box_h = g->box_h;
    dsc_out->ofs_x = g->ofs_x;
    dsc_out->ofs_y = g->ofs_y;
    dsc_out->bpp = 4;
    dsc_out->is_placeholder = 0;
    dsc_out->resolved_font = font;
    return true;
}}

static const uint8_t *get_glyph_bitmap_ui_{sz}(const lv_font_t *font, uint32_t letter)
{{
    (void)font;
    if (letter >= 32 && letter <= 126) {{
        return s_glyphs_{sz}_ascii[letter - 32].bitmap;
    }}
    for (size_t i = 0; i < sizeof(s_glyphs_{sz}_unicode)/sizeof(s_glyphs_{sz}_unicode[0]); i++) {{
        if (s_glyphs_{sz}_unicode[i].unicode == letter) {{
            return s_glyphs_{sz}_unicode[i].bitmap;
        }}
    }}
    return NULL;
}}

const lv_font_t ui_font_{sz} = {{
    .get_glyph_dsc = get_glyph_dsc_ui_{sz},
    .get_glyph_bitmap = get_glyph_bitmap_ui_{sz},
    .line_height = {lh},
    .base_line = {bl},
    .subpx = LV_FONT_SUBPX_NONE,
    .dsc = NULL,
    .fallback = NULL
}};
""")

    with open(os.path.join(ROOT, "ui_fonts.h"), "w", encoding="utf-8") as f:
        f.write("\n".join(h_out) + "\n")
    with open(os.path.join(ROOT, "ui_fonts.c"), "w", encoding="utf-8") as f:
        f.write("\n".join(c_out) + "\n")
    print("ui_fonts.h and ui_fonts.c generated successfully (Segoe UI Semibold 18, 24, 34, 62).")


def main():
    generate_splash()
    generate_icons()
    generate_fonts()
    print("All assets generated successfully.")


if __name__ == "__main__":
    main()
