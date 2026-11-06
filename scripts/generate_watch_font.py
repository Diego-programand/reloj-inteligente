"""
Genera una fuente nativa LVGL v8 (4-bpp anti-aliased) de gran tamaño (108 px)
para los dígitos de la hora principal del reloj inteligente (0-9, ':', espacio).
"""
from PIL import Image, ImageDraw, ImageFont
import os

FONT_PATH = r"C:\Windows\Fonts\segoeuib.ttf"
FONT_SIZE = 108
OUT_C = r"G:\ITM\reloj-inteligente\firmware\ui\fonts\font_watch_time_108.c"
OUT_H = r"G:\ITM\reloj-inteligente\firmware\ui\fonts\font_watch_time_108.h"

os.makedirs(os.path.dirname(OUT_C), exist_ok=True)
font = ImageFont.truetype(FONT_PATH, FONT_SIZE)

# Rango ASCII continuo desde ' ' (32) hasta ':' (58)
START_CODE = 32
END_CODE   = 58

ascent, descent = font.getmetrics()
line_height = ascent + descent
base_line   = descent

bitmap_bytes = []
glyph_dscs   = []

# Glyph 0 reservado por LVGL
glyph_dscs.append({
    "bitmap_index": 0,
    "adv_w": 0,
    "box_w": 0,
    "box_h": 0,
    "ofs_x": 0,
    "ofs_y": 0,
})

for code in range(START_CODE, END_CODE + 1):
    ch = chr(code)
    adv_px = int(round(font.getlength(ch)))

    if ch == ' ' or ch not in "0123456789:":
        glyph_dscs.append({
            "bitmap_index": len(bitmap_bytes),
            "adv_w": adv_px * 16,
            "box_w": 0,
            "box_h": 0,
            "ofs_x": 0,
            "ofs_y": 0,
            "comment": f"U+{code:04X} '{ch}'"
        })
        continue

    # Renderizar en un lienzo amplio con origen en (20, 20) y línea base en y = 20 + ascent
    canvas_w = max(adv_px + 60, 160)
    canvas_h = line_height + 60
    orig_x = 20
    orig_y = 20
    baseline_y = orig_y + ascent

    img = Image.new("L", (canvas_w, canvas_h), 0)
    draw = ImageDraw.Draw(img)
    draw.text((orig_x, orig_y), ch, fill=255, font=font)

    bbox = img.getbbox()
    if not bbox:
        glyph_dscs.append({
            "bitmap_index": len(bitmap_bytes),
            "adv_w": adv_px * 16,
            "box_w": 0,
            "box_h": 0,
            "ofs_x": 0,
            "ofs_y": 0,
            "comment": f"U+{code:04X} '{ch}'"
        })
        continue

    x0, y0, x1, y1 = bbox
    box_w = x1 - x0
    box_h = y1 - y0
    ofs_x = x0 - orig_x
    ofs_y = baseline_y - y1

    crop = img.crop(bbox)
    pixels = list(crop.getdata())

    # Empaquetar a 4 bits por píxel (0..15)
    nibbles = [p >> 4 for p in pixels]
    if len(nibbles) % 2 == 1:
        nibbles.append(0)

    b_idx = len(bitmap_bytes)
    for i in range(0, len(nibbles), 2):
        bitmap_bytes.append((nibbles[i] << 4) | nibbles[i + 1])

    glyph_dscs.append({
        "bitmap_index": b_idx,
        "adv_w": adv_px * 16,
        "box_w": box_w,
        "box_h": box_h,
        "ofs_x": ofs_x,
        "ofs_y": ofs_y,
        "comment": f"U+{code:04X} '{ch}' ({box_w}x{box_h})"
    })

with open(OUT_H, "w", encoding="utf-8") as fh:
    fh.write("""#pragma once
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_font_t font_watch_time_108;

#ifdef __cplusplus
}
#endif
""")

with open(OUT_C, "w", encoding="utf-8") as fc:
    fc.write('#include "font_watch_time_108.h"\n\n')
    fc.write("static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {\n    ")
    for idx, b in enumerate(bitmap_bytes):
        fc.write(f"0x{b:02x}, ")
        if (idx + 1) % 16 == 0:
            fc.write("\n    ")
    fc.write("\n};\n\n")

    fc.write("static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {\n")
    for idx, g in enumerate(glyph_dscs):
        cmt = f" /* {g.get('comment', 'Reserved')} */"
        fc.write(
            f"    {{.bitmap_index = {g['bitmap_index']}, .adv_w = {g['adv_w']}, "
            f".box_w = {g['box_w']}, .box_h = {g['box_h']}, "
            f".ofs_x = {g['ofs_x']}, .ofs_y = {g['ofs_y']}}},{cmt}\n"
        )
    fc.write("};\n\n")

    count = END_CODE - START_CODE + 1
    fc.write(f"""static const lv_font_fmt_txt_cmap_t cmaps[] = {{
    {{
        .range_start = {START_CODE}, .range_length = {count}, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0,
        .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }}
}};

static lv_font_fmt_txt_glyph_cache_t cache;
static const lv_font_fmt_txt_dsc_t font_dsc = {{
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 4,
    .kern_classes = 0,
    .bitmap_format = 0,
    .cache = &cache
}};

const lv_font_t font_watch_time_108 = {{
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .line_height = {line_height},
    .base_line = {base_line},
    .subpx = LV_FONT_SUBPX_NONE,
    .underline_position = 0,
    .underline_thickness = 0,
    .dsc = &font_dsc
}};
""")

print(f"Generada font_watch_time_108: {len(bitmap_bytes)} bytes, line_height={line_height}")
