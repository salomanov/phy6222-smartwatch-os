import re

with open('firmware/src/app_os.c', 'r', encoding='utf-8') as f:
    text = f.read()

# 1. Page 2: Add row clearing before dynamic text
p2_old = r'''    // Time HH:MM using Giant 16x29 factory digits
    factory_draw_big_time\(s_hours, s_minutes, 26, COLOR_CYAN, false\);

    char buf\[24\];
    snprintf\(buf, sizeof\(buf\), "%05lu [^\"]+", \(unsigned long\)accel_get_steps\(\)\);
    factory_draw_utf8\(4, 114, buf, COLOR_GREEN, COLOR_BLACK\);

    snprintf\(buf, sizeof\(buf\), "%02d:%02d:%02d", s_hours, s_minutes, s_seconds\);
    factory_draw_utf8\(10, 140, buf, COLOR_LIGHTGREY, COLOR_BLACK\);'''

p2_new = '''    // Time HH:MM using Giant 16x29 factory digits (clean row clear prevents ghosts)
    gfx_fill_rect(2, 26, 76, 30, COLOR_BLACK);
    factory_draw_big_time(s_hours, s_minutes, 26, COLOR_CYAN, false);

    char buf[24];
    snprintf(buf, sizeof(buf), "%05lu ШАГ", (unsigned long)accel_get_steps());
    gfx_fill_rect(2, 114, 76, 17, COLOR_BLACK);
    factory_draw_utf8(4, 114, buf, COLOR_GREEN, COLOR_BLACK);

    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", s_hours, s_minutes, s_seconds);
    gfx_fill_rect(2, 140, 76, 17, COLOR_BLACK);
    factory_draw_utf8(10, 140, buf, COLOR_LIGHTGREY, COLOR_BLACK);'''

text, n2 = re.subn(p2_old, p2_new, text)
print(f'Page 2 replaced: {n2}')

# 2. Page 4: Add row clearing before dynamic telemetry lines
p4_old = r'''    char buf\[32\];
    snprintf\(buf, sizeof\(buf\), "[^:]+: %02d:%02d", s_hours, s_minutes\);
    factory_draw_utf8\(2, 22, buf, COLOR_WHITE, COLOR_BLACK\);

    snprintf\(buf, sizeof\(buf\), "[^:]+: %-5lu", \(unsigned long\)accel_get_steps\(\)\);
    factory_draw_utf8\(2, 43, buf, COLOR_GREEN, COLOR_BLACK\);

    snprintf\(buf, sizeof\(buf\), "[^:]+: %-6lu", \(unsigned long\)accel_get_pulses\(\)\);
    factory_draw_utf8\(2, 64, buf, COLOR_CYAN, COLOR_BLACK\);

    snprintf\(buf, sizeof\(buf\), "[^:]+: %3d%%  ", bat_pct\);
    factory_draw_utf8\(2, 85, buf, \(bat_pct < 20\) \? COLOR_RED : COLOR_YELLOW, COLOR_BLACK\);'''

p4_new = '''    char buf[32];
    snprintf(buf, sizeof(buf), "Час: %02d:%02d", s_hours, s_minutes);
    gfx_fill_rect(2, 22, 76, 17, COLOR_BLACK);
    factory_draw_utf8(2, 22, buf, COLOR_WHITE, COLOR_BLACK);

    snprintf(buf, sizeof(buf), "Шаги: %-5lu", (unsigned long)accel_get_steps());
    gfx_fill_rect(2, 43, 76, 17, COLOR_BLACK);
    factory_draw_utf8(2, 43, buf, COLOR_GREEN, COLOR_BLACK);

    snprintf(buf, sizeof(buf), "Имп: %-6lu", (unsigned long)accel_get_pulses());
    gfx_fill_rect(2, 64, 76, 17, COLOR_BLACK);
    factory_draw_utf8(2, 64, buf, COLOR_CYAN, COLOR_BLACK);

    snprintf(buf, sizeof(buf), "Бат: %3d%%", bat_pct);
    gfx_fill_rect(2, 85, 76, 17, COLOR_BLACK);
    factory_draw_utf8(2, 85, buf, (bat_pct < 20) ? COLOR_RED : COLOR_YELLOW, COLOR_BLACK);'''

text, n4 = re.subn(p4_old, p4_new, text)
print(f'Page 4 replaced: {n4}')

# 3. Page 5: Add row clearing before giant digits
p5_old = r'''    uint8_t d0 = \(s_seconds / 10\) % 10;
    uint8_t d1 = s_seconds % 10;
    factory_draw_sprite_1bpp\(FACTORY_SPRITE_BGDIG_BASE \+ 0 \* 68U, 4, 100, COLOR_GREEN, COLOR_BLACK, false\);
    factory_draw_sprite_1bpp\(FACTORY_SPRITE_BGDIG_BASE \+ 7 \* 68U, 22, 100, COLOR_YELLOW, COLOR_BLACK, false\);
    factory_draw_sprite_1bpp\(FACTORY_SPRITE_BGDIG_BASE \+ 10 \* 68U, 38, 100, COLOR_WHITE, COLOR_BLACK, false\);
    factory_draw_sprite_1bpp\(FACTORY_SPRITE_BGDIG_BASE \+ \(uint32_t\)d0 \* 68U, 48, 100, COLOR_CYAN, COLOR_BLACK, false\);
    factory_draw_sprite_1bpp\(FACTORY_SPRITE_BGDIG_BASE \+ \(uint32_t\)d1 \* 68U, 64, 100, COLOR_CYAN, COLOR_BLACK, false\);'''

p5_new = '''    uint8_t d0 = (s_seconds / 10) % 10;
    uint8_t d1 = s_seconds % 10;
    gfx_fill_rect(0, 100, 80, 32, COLOR_BLACK);
    factory_draw_sprite_1bpp(FACTORY_SPRITE_BGDIG_BASE + 0 * 68U, 4, 100, COLOR_GREEN, COLOR_BLACK, false);
    factory_draw_sprite_1bpp(FACTORY_SPRITE_BGDIG_BASE + 7 * 68U, 20, 100, COLOR_YELLOW, COLOR_BLACK, false);
    factory_draw_sprite_1bpp(FACTORY_SPRITE_BGDIG_BASE + 10 * 68U, 36, 100, COLOR_WHITE, COLOR_BLACK, false);
    factory_draw_sprite_1bpp(FACTORY_SPRITE_BGDIG_BASE + (uint32_t)d0 * 68U, 48, 100, COLOR_CYAN, COLOR_BLACK, false);
    factory_draw_sprite_1bpp(FACTORY_SPRITE_BGDIG_BASE + (uint32_t)d1 * 68U, 64, 100, COLOR_CYAN, COLOR_BLACK, false);'''

text, n5 = re.subn(p5_old, p5_new, text)
print(f'Page 5 replaced: {n5}')

# 4. render_watchface: Clear screen on first draw of ANY page
rw_old = r'''static void render_watchface\(void\) \{
    switch \(s_wf_page\) \{'''

rw_new = '''static void render_watchface(void) {
    if (s_diag_first_draw) {
        display_fill_screen(COLOR_BLACK);
    }
    switch (s_wf_page) {'''

text, nr = re.subn(rw_old, rw_new, text)
print(f'render_watchface replaced: {nr}')

with open('firmware/src/app_os.c', 'w', encoding='utf-8') as f:
    f.write(text)

print('app_os.c patched cleanly!')
