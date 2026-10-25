import struct

with open('.backups/factory_dump.bin', 'rb') as f:
    data = f.read()

# 1. Extract proportional 16px ASCII (U+0020..U+007E) and Cyrillic (U+0400..U+045F) from 0x1103F000
FONT_BASE = 0x3F000

def extract_range(start_cp, end_cp, sec_table_off):
    offsets = []
    blob = bytearray()
    for cp in range(start_cp, end_cp + 1):
        idx = cp - (0x0020 if start_cp == 0x0020 else 0x0400)
        entry = struct.unpack_from('<I', data, FONT_BASE + sec_table_off + idx * 4)[0]
        off = entry & 0x3FFFFFF
        if entry == 0xFFFFFFFF or off >= 0x40000:
            offsets.append(0xFFFF)
            continue
        g_off = FONT_BASE + off
        w = data[g_off]
        h = data[g_off + 1]
        if w == 0 or w > 16 or h != 16:
            offsets.append(0xFFFF)
            continue
        n_bytes = (w * h + 7) // 8
        offsets.append(len(blob))
        blob.append(w)
        blob.extend(data[g_off + 2 : g_off + 2 + n_bytes])
    return offsets, bytes(blob)

ascii_offs, ascii_blob = extract_range(0x0020, 0x007E, 0x20)
cyr_offs, cyr_blob     = extract_range(0x0400, 0x045F, 0xFBC)

# 2. Extract RLE (bpp=18) and 1bpp (bpp=1) icons from 0x1105886C asset pack
DESC_BASE = 0x5886C
RLE_BASE  = 0x58DE0
PAL_BASE  = 0x58F24
PIX_BASE  = 0x5A1F0

# Lean 2.5 KB asset pack matching v1.5.14-DEMO size for 100% reliable 1-shot BLE OTA transfer
rle_spec = [
    (37, [0]),       # Sneaker 50x36 (514 B)
]

mono_items = [10, 11] # Alarm (205 B), Trophy (246 B)

pal_blob = bytearray()
rle_data_blob = bytearray()
rle_frame_offs = []
rle_descs = []

for item_id, frames in rle_spec:
    off = DESC_BASE + item_id * 16
    bpp = data[off]
    pal_cnt = data[off + 1]
    w, h, cnt, pal_idx, unk, doff = struct.unpack_from('<HHHHHI', data, off + 2)
    assert bpp == 18
    my_pal_off = len(pal_blob) // 2
    pal_blob.extend(data[PAL_BASE + pal_idx * 2 : PAL_BASE + (pal_idx + pal_cnt) * 2])
    my_frame_idx = len(rle_frame_offs)
    for f in frames:
        if doff & 0x10000000:
            rle_idx = (doff & 0xFFFFFF) + f
            poff = struct.unpack_from('<I', data, RLE_BASE + rle_idx * 4)[0] & 0xFFFFFF
        else:
            poff = doff
        p = PIX_BASE + poff
        n_runs = (data[p] << 8) | data[p + 1]
        chunk_len = 2 + n_runs * 2
        rle_frame_offs.append(len(rle_data_blob))
        rle_data_blob.extend(data[p : p + chunk_len])
    rle_descs.append((item_id, w, h, len(frames), my_pal_off, my_frame_idx))

mono_data_blob = bytearray()
mono_descs = []
for item_id in mono_items:
    off = DESC_BASE + item_id * 16
    bpp = data[off]
    w, h, cnt, pal_idx, unk, doff = struct.unpack_from('<HHHHHI', data, off + 2)
    assert bpp == 1
    stride = (w + 7) // 8
    n_bytes = stride * h
    my_off = len(mono_data_blob)
    mono_data_blob.extend(data[PIX_BASE + doff : PIX_BASE + doff + n_bytes])
    mono_descs.append((item_id, w, h, my_off))

print(f"Font blobs: {len(ascii_blob) + len(cyr_blob)} B")
print(f"RLE palette: {len(pal_blob)} B, RLE data: {len(rle_data_blob)} B, Mono data: {len(mono_data_blob)} B")
print(f"Total asset size: {len(ascii_blob) + len(cyr_blob) + len(pal_blob) + len(rle_data_blob) + len(mono_data_blob)} B")

def fmt_u8(arr):
    lines = []
    for i in range(0, len(arr), 16):
        lines.append("    " + ", ".join(f"0x{b:02X}" for b in arr[i:i+16]) + ",")
    return "\n".join(lines)

def fmt_u16(arr):
    lines = []
    for i in range(0, len(arr), 12):
        lines.append("    " + ", ".join(f"0x{v:04X}" for v in arr[i:i+12]) + ",")
    return "\n".join(lines)

def fmt_u32(arr):
    lines = []
    for i in range(0, len(arr), 8):
        lines.append("    " + ", ".join(f"0x{v:08X}U" for v in arr[i:i+8]) + ",")
    return "\n".join(lines)

pal_u16 = [struct.unpack_from('<H', pal_blob, i)[0] for i in range(0, len(pal_blob), 2)]

h_content = """#ifndef FACTORY_ASSETS_H
#define FACTORY_ASSETS_H

#include <stdint.h>
#include <stdbool.h>

// XIP addresses in factory flash bank 0x11072000 (preserved above OTA staging)
#define FACTORY_SPRITE_BAT_BASE   0x11072000U // 5 battery icons (16x8, stride 26)
#define FACTORY_SPRITE_BLE_ADDR   0x11072082U // 1 BLE icon (16x7, 24 bytes)
#define FACTORY_SPRITE_SMDIG_BASE 0x1107209AU // 14 small 8x10 digits (stride 20)
#define FACTORY_SPRITE_DIV_ADDR   0x110721B2U // 24x2 divider (16 bytes)
#define FACTORY_SPRITE_BGDIG_BASE 0x110721C2U // 11 giant 16x29 digits 0..9 and ':' (stride 68)
#define FACTORY_SPRITE_WKDAY_BASE 0x110724AEU // 7 weekday 32x10 badges MON..SUN (stride 50)
#define FACTORY_BMP_WALLPAPER     0x1107260CU // 80x160 8-bit indexed BMP wallpaper (13876 bytes)

// RLE Color Icon IDs from factory asset pack (0x1105886C)
#define FAC_RLE_STEPS_SHOE     37 // 50x36

// 1bpp Monochrome Icon IDs from factory asset pack (0x1105886C)
#define FAC_MONO_ALARM         10 // 37x41
#define FAC_MONO_TROPHY        11 // 41x41

// Render full-screen 80x160 8bpp BMP wallpaper from 0x1107260C
void factory_draw_bmp_wallpaper(void);

// Render 1bpp factory sprite at (x, y); if over_bmp is true, '0' bits sample the 0x1107260C wallpaper
void factory_draw_sprite_1bpp(uint32_t flash_addr, uint8_t x, uint8_t y, uint16_t fg_color, uint16_t bg_color, bool over_bmp);

// Draw big 16x29 HH:MM time at y position
void factory_draw_big_time(uint8_t hours, uint8_t minutes, uint8_t y, uint16_t fg_color, bool over_bmp);

// Draw small 8x10 numeric string (0-9, '/', ':', '-') at (x, y)
void factory_draw_small_num(uint8_t x, uint8_t y, const char *str, uint16_t fg_color, uint16_t bg_color, bool over_bmp);

// Render proportional 16px UTF-8 text (ASCII + Cyrillic) from factory font 0x1103F000
int16_t factory_draw_utf8(int16_t x, int16_t y, const char *utf8, uint16_t fg_color, uint16_t bg_color);

// Draw RLE color icon from factory asset pack (0x1105886C)
void factory_draw_rle_icon(uint8_t icon_id, uint8_t frame, uint8_t x, uint8_t y);

// Draw 1bpp monochrome icon from factory asset pack (0x1105886C)
void factory_draw_mono_icon(uint8_t icon_id, uint8_t x, uint8_t y, uint16_t fg_color, uint16_t bg_color);

#endif // FACTORY_ASSETS_H
"""

c_content = f"""#include "factory_assets.h"
#include "display.h"
#include "config.h"

// Extracted from PHY_FONTV1.01.03 at 0x1103F000 (ASCII U+0020..U+007E + Cyrillic U+0400..U+045F)
static const uint16_t s_ascii_offs[95] = {{
{fmt_u16(ascii_offs)}
}};

static const uint8_t s_ascii_blob[{len(ascii_blob)}] = {{
{fmt_u8(ascii_blob)}
}};

static const uint16_t s_cyr_offs[96] = {{
{fmt_u16(cyr_offs)}
}};

static const uint8_t s_cyr_blob[{len(cyr_blob)}] = {{
{fmt_u8(cyr_blob)}
}};

// Extracted RLE color assets from 0x1105886C
typedef struct {{
    uint8_t id;
    uint8_t w;
    uint8_t h;
    uint8_t cnt;
    uint16_t pal_off;
    uint16_t frame_idx;
}} fac_rle_desc_t;

typedef struct {{
    uint8_t id;
    uint8_t w;
    uint8_t h;
    uint8_t cnt;
    uint16_t data_off;
}} fac_mono_desc_t;

static const fac_rle_desc_t s_rle_descs[{len(rle_descs)}] = {{
""" + "\n".join(f"    {{{d[0]}, {d[1]}, {d[2]}, {d[3]:2d}, {d[4]:4d}, {d[5]:2d}}}," for d in rle_descs) + f"""
}};

static const uint16_t s_rle_palette[{len(pal_u16)}] = {{
{fmt_u16(pal_u16)}
}};

static const uint32_t s_rle_frame_offs[{len(rle_frame_offs)}] = {{
{fmt_u32(rle_frame_offs)}
}};

static const uint8_t s_rle_data[{len(rle_data_blob)}] = {{
{fmt_u8(rle_data_blob)}
}};

static const fac_mono_desc_t s_mono_descs[{len(mono_descs)}] = {{
""" + "\n".join(f"    {{{d[0]}, {d[1]}, {d[2]}, 1, {d[3]:4d}}}," for d in mono_descs) + f"""
}};

static const uint8_t s_mono_data[{len(mono_data_blob)}] = {{
{fmt_u8(mono_data_blob)}
}};

static uint16_t sample_bmp_pixel(uint8_t x, uint8_t y) {{
    if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) return 0x0000;
    const uint8_t *bmp = (const uint8_t *)FACTORY_BMP_WALLPAPER;
    if (bmp[0] != 'B' || bmp[1] != 'M') return 0x0000;
    const uint8_t *pal = bmp + 0x36;
    const uint8_t *pix = bmp + 0x432;
    uint8_t bmp_row = (SCREEN_HEIGHT - 1) - y;
    uint8_t idx = pix[(uint32_t)bmp_row * SCREEN_WIDTH + x];
    const uint8_t *pe = pal + (uint32_t)idx * 4;
    return ((uint16_t)pe[1] << 8) | pe[2];
}}

void factory_draw_bmp_wallpaper(void) {{
    const uint8_t *bmp = (const uint8_t *)FACTORY_BMP_WALLPAPER;
    if (bmp[0] != 'B' || bmp[1] != 'M') {{
        display_fill_screen(0x0000);
        return;
    }}
    const uint8_t *pal = bmp + 0x36;
    const uint8_t *pix = bmp + 0x432;
    uint8_t row_buf[SCREEN_WIDTH * 2];

    for (uint8_t y = 0; y < SCREEN_HEIGHT; y++) {{
        uint8_t bmp_row = (SCREEN_HEIGHT - 1) - y;
        const uint8_t *src = pix + (uint32_t)bmp_row * SCREEN_WIDTH;
        for (uint8_t x = 0; x < SCREEN_WIDTH; x++) {{
            const uint8_t *pe = pal + (uint32_t)src[x] * 4;
            row_buf[x * 2]     = pe[1];
            row_buf[x * 2 + 1] = pe[2];
        }}
        display_fill_window(0, y, SCREEN_WIDTH - 1, y, (uint16_t *)row_buf, SCREEN_WIDTH);
    }}
}}

void factory_draw_sprite_1bpp(uint32_t flash_addr, uint8_t x, uint8_t y, uint16_t fg_color, uint16_t bg_color, bool over_bmp) {{
    const uint8_t *hdr = (const uint8_t *)flash_addr;
    uint8_t w = hdr[2];
    uint8_t h = hdr[4];
    uint8_t stride = hdr[6];
    if (w == 0 || w > SCREEN_WIDTH || h == 0 || h > SCREEN_HEIGHT) return;

    const uint8_t *bits = hdr + 10;
    uint8_t row_buf[SCREEN_WIDTH * 2];

    for (uint8_t dy = 0; dy < h; dy++) {{
        if (y + dy >= SCREEN_HEIGHT) break;
        const uint8_t *row_bits = bits + (uint32_t)dy * stride;
        for (uint8_t dx = 0; dx < w; dx++) {{
            bool on = (row_bits[dx >> 3] & (0x80U >> (dx & 7))) != 0;
            uint16_t c = on ? fg_color : (over_bmp ? sample_bmp_pixel(x + dx, y + dy) : bg_color);
            row_buf[dx * 2]     = (uint8_t)(c >> 8);
            row_buf[dx * 2 + 1] = (uint8_t)(c & 0xFF);
        }}
        display_fill_window(x, y + dy, x + w - 1, y + dy, (uint16_t *)row_buf, w);
    }}
}}

void factory_draw_big_time(uint8_t hours, uint8_t minutes, uint8_t y, uint16_t fg_color, bool over_bmp) {{
    uint8_t d[5] = {{
        (uint8_t)((hours / 10) % 10),
        (uint8_t)(hours % 10),
        10,
        (uint8_t)((minutes / 10) % 10),
        (uint8_t)(minutes % 10)
    }};
    const uint8_t xs[5] = {{ 2, 18, 34, 46, 62 }};
    for (int i = 0; i < 5; i++) {{
        uint32_t addr = FACTORY_SPRITE_BGDIG_BASE + (uint32_t)d[i] * 68U;
        factory_draw_sprite_1bpp(addr, xs[i], y, fg_color, 0x0000, over_bmp);
    }}
}}

void factory_draw_small_num(uint8_t x, uint8_t y, const char *str, uint16_t fg_color, uint16_t bg_color, bool over_bmp) {{
    uint8_t cur_x = x;
    while (*str && cur_x + 8 <= SCREEN_WIDTH) {{
        char c = *str++;
        int idx = -1;
        if (c >= '0' && c <= '9') idx = c - '0';
        else if (c == '/') idx = 10;
        else if (c == ':') idx = 11;
        else if (c == '-') idx = 12;
        if (idx >= 0) {{
            factory_draw_sprite_1bpp(FACTORY_SPRITE_SMDIG_BASE + (uint32_t)idx * 20U, cur_x, y, fg_color, bg_color, over_bmp);
        }}
        cur_x += 8;
    }}
}}

static uint8_t draw_prop16_glyph(int16_t x, int16_t y, const uint8_t *glyph, uint16_t fg, uint16_t bg) {{
    uint8_t w = glyph ? glyph[0] : 5;
    if (w > 16) w = 16;
    if (x < 0 || x + w > SCREEN_WIDTH || y < 0 || y + 16 > SCREEN_HEIGHT) return w;

    const uint8_t *bits = glyph ? (glyph + 1) : 0;
    uint8_t buf[16 * 16 * 2];
    uint16_t bit_idx = 0;
    uint16_t out = 0;

    for (uint8_t dy = 0; dy < 16; dy++) {{
        for (uint8_t dx = 0; dx < w; dx++) {{
            bool on = false;
            if (bits) {{
                on = ((bits[bit_idx >> 3] >> (bit_idx & 7)) & 1U) != 0;
            }}
            bit_idx++;
            uint16_t c = on ? fg : bg;
            buf[out++] = (uint8_t)(c >> 8);
            buf[out++] = (uint8_t)(c & 0xFF);
        }}
    }}
    display_fill_window((uint8_t)x, (uint8_t)y, (uint8_t)(x + w - 1), (uint8_t)(y + 15), (uint16_t *)buf, (uint32_t)w * 16U);
    return w;
}}

int16_t factory_draw_utf8(int16_t x, int16_t y, const char *utf8, uint16_t fg_color, uint16_t bg_color) {{
    int16_t cur_x = x;
    const uint8_t *s = (const uint8_t *)utf8;
    while (*s) {{
        uint32_t cp = 0;
        if (*s < 0x80) {{
            cp = *s++;
        }} else if ((*s & 0xE0) == 0xC0 && s[1]) {{
            cp = ((uint32_t)(s[0] & 0x1F) << 6) | (uint32_t)(s[1] & 0x3F);
            s += 2;
        }} else if ((*s & 0xF0) == 0xE0 && s[1] && s[2]) {{
            cp = ((uint32_t)(s[0] & 0x0F) << 12) | ((uint32_t)(s[1] & 0x3F) << 6) | (uint32_t)(s[2] & 0x3F);
            s += 3;
        }} else {{
            s++;
            continue;
        }}

        const uint8_t *glyph = 0;
        if (cp >= 0x0020 && cp <= 0x007E) {{
            uint16_t off = s_ascii_offs[cp - 0x0020];
            if (off != 0xFFFF) glyph = &s_ascii_blob[off];
        }} else if (cp >= 0x0400 && cp <= 0x045F) {{
            uint16_t off = s_cyr_offs[cp - 0x0400];
            if (off != 0xFFFF) glyph = &s_cyr_blob[off];
        }}

        uint8_t adv = draw_prop16_glyph(cur_x, y, glyph, fg_color, bg_color);
        cur_x += adv;
        if (cur_x >= SCREEN_WIDTH) break;
    }}
    return cur_x;
}}

void factory_draw_rle_icon(uint8_t icon_id, uint8_t frame, uint8_t x, uint8_t y) {{
    const fac_rle_desc_t *desc = 0;
    for (unsigned i = 0; i < sizeof(s_rle_descs) / sizeof(s_rle_descs[0]); i++) {{
        if (s_rle_descs[i].id == icon_id) {{
            desc = &s_rle_descs[i];
            break;
        }}
    }}
    if (!desc) return;
    if (frame >= desc->cnt) frame = 0;
    uint8_t w = desc->w;
    uint8_t h = desc->h;
    if (x + w > SCREEN_WIDTH || y + h > SCREEN_HEIGHT) return;

    uint32_t doff = s_rle_frame_offs[desc->frame_idx + frame];
    const uint8_t *p = &s_rle_data[doff];
    uint16_t n_runs = ((uint16_t)p[0] << 8) | p[1];
    p += 2;

    const uint16_t *pal = &s_rle_palette[desc->pal_off];
    uint8_t row_buf[SCREEN_WIDTH * 2];
    uint8_t cur_x = 0;
    uint8_t cur_y = 0;

    for (uint16_t r = 0; r < n_runs; r++) {{
        uint8_t rl = p[0];
        uint8_t ci = p[1];
        p += 2;
        uint16_t c = pal[ci];
        uint8_t hi = (uint8_t)(c >> 8);
        uint8_t lo = (uint8_t)(c & 0xFF);
        while (rl-- > 0) {{
            row_buf[cur_x * 2]     = hi;
            row_buf[cur_x * 2 + 1] = lo;
            cur_x++;
            if (cur_x >= w) {{
                display_fill_window(x, y + cur_y, x + w - 1, y + cur_y, (uint16_t *)row_buf, w);
                cur_x = 0;
                cur_y++;
                if (cur_y >= h) return;
            }}
        }}
    }}
}}

void factory_draw_mono_icon(uint8_t icon_id, uint8_t x, uint8_t y, uint16_t fg_color, uint16_t bg_color) {{
    const fac_mono_desc_t *desc = 0;
    for (unsigned i = 0; i < sizeof(s_mono_descs) / sizeof(s_mono_descs[0]); i++) {{
        if (s_mono_descs[i].id == icon_id) {{
            desc = &s_mono_descs[i];
            break;
        }}
    }}
    if (!desc) return;
    uint8_t w = desc->w;
    uint8_t h = desc->h;
    if (x + w > SCREEN_WIDTH || y + h > SCREEN_HEIGHT) return;

    uint8_t stride = (w + 7) >> 3;
    const uint8_t *bits = &s_mono_data[desc->data_off];
    uint8_t row_buf[SCREEN_WIDTH * 2];

    for (uint8_t dy = 0; dy < h; dy++) {{
        const uint8_t *row_bits = bits + (uint32_t)dy * stride;
        for (uint8_t dx = 0; dx < w; dx++) {{
            bool on = ((row_bits[dx >> 3] >> (dx & 7)) & 1U) != 0;
            uint16_t c = on ? fg_color : bg_color;
            row_buf[dx * 2]     = (uint8_t)(c >> 8);
            row_buf[dx * 2 + 1] = (uint8_t)(c & 0xFF);
        }}
        display_fill_window(x, y + dy, x + w - 1, y + dy, (uint16_t *)row_buf, w);
    }}
}}
"""

with open('firmware/src/factory_assets.h', 'w', encoding='utf-8') as f:
    f.write(h_content)

with open('firmware/src/factory_assets.c', 'w', encoding='utf-8') as f:
    f.write(c_content)

print("Generated compact factory_assets.h and factory_assets.c successfully.")
