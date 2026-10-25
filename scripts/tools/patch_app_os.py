import os

with open('firmware/src/app_os.c', 'r', encoding='utf-8') as f:
    content = f.read()

notif_code = """
static char s_notification_text[160] = {0};
static uint16_t s_notification_color = COLOR_WHITE;
static uint8_t s_notification_scale = 1;

void app_os_show_message(const char *text, uint16_t color, uint8_t scale) {
    if (!text) return;
    strncpy(s_notification_text, text, sizeof(s_notification_text) - 1);
    s_notification_text[sizeof(s_notification_text) - 1] = '\\0';
    s_notification_color = color ? color : COLOR_WHITE;
    s_notification_scale = (scale == 2) ? 2 : 1;
    s_current_mode = MODE_NOTIFICATION;
    s_diag_first_draw = true;
    s_needs_redraw = true;
    vibrator_pulse(120);
}

void app_os_dismiss_message(void) {
    if (s_current_mode == MODE_NOTIFICATION) {
        s_current_mode = MODE_WATCHFACE;
        s_diag_first_draw = true;
        s_needs_redraw = true;
    }
}

static void render_notification(void) {
    display_fill_screen(COLOR_BLACK);

    // Header banner
    gfx_fill_rect(0, 0, 80, 15, COLOR_NAVY);
    gfx_draw_string_utf8(6, 4, "СООБЩЕНИЕ", COLOR_YELLOW, COLOR_NAVY, 1);
    gfx_draw_fast_h_line(2, 17, 76, COLOR_DARKGREY);

    // Body with automatic word wrapping
    gfx_draw_string_utf8(4, 22, s_notification_text, s_notification_color, COLOR_BLACK, s_notification_scale);

    // Footer
    gfx_draw_fast_h_line(2, 144, 76, COLOR_DARKGREY);
    gfx_draw_string_utf8(8, 148, "КЛИК: ВЫХОД", COLOR_CYAN, COLOR_BLACK, 1);
}
"""

target = '    gfx_draw_string(4, 134, "HOLD: FLASH", COLOR_CYAN, COLOR_BLACK, 1);\n}'
replacement = target + "\n" + notif_code

assert target in content, 'Target not found!'
content = content.replace(target, replacement, 1)

# Now handle button click in MODE_NOTIFICATION
btn_target = '    if (evt == BTN_EVENT_CLICK) {\n        s_click_count++;\n        s_vib_count++;\n'
btn_replace = btn_target + """        if (s_current_mode == MODE_NOTIFICATION) {
            app_os_dismiss_message();
            vibrator_pulse(50);
            return;
        }
"""
assert btn_target in content, 'Button target not found!'
content = content.replace(btn_target, btn_replace, 1)

# Now handle render switch
sw_target = """            case MODE_OTA_UPDATE:
                render_ota_update();
                break;"""
sw_replace = sw_target + """
            case MODE_NOTIFICATION:
                render_notification();
                break;"""
assert sw_target in content, 'Switch target not found!'
content = content.replace(sw_target, sw_replace, 1)

with open('firmware/src/app_os.c', 'w', encoding='utf-8', newline='\n') as f:
    f.write(content)
print('Successfully updated firmware/src/app_os.c!')
