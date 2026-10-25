import os

with open('firmware/src/ble_manager.c', 'r', encoding='utf-8') as f:
    content = f.read()

color_helper = """
static uint16_t parse_color_char(char c) {
    switch (c) {
        case 'W': case 'w': return COLOR_WHITE;
        case 'G': case 'g': return COLOR_GREEN;
        case 'Y': case 'y': return COLOR_YELLOW;
        case 'C': case 'c': return COLOR_CYAN;
        case 'R': case 'r': return COLOR_RED;
        case 'M': case 'm': return COLOR_MAGENTA;
        case 'B': case 'b': return COLOR_BLUE;
        case 'O': case 'o': return COLOR_ORANGE;
        default: return COLOR_WHITE;
    }
}

static char s_ble_msg_accum[160] = {0};
static uint8_t s_ble_msg_len = 0;
static uint16_t s_ble_msg_color = COLOR_WHITE;
static uint8_t s_ble_msg_scale = 1;
"""

msg_handler = """
            // 2. Check BLE Text Messages
            // Case A: Single short packet "MSG:<C>:<S>:<text>" or "MSG:<text>"
            if (val[0] == 'M' && val[1] == 'S' && val[2] == 'G' && val[3] == ':') {
                char text_buf[16] = {0};
                if (val[5] == ':' && val[7] == ':') {
                    // Formatted: MSG:<Color>:<Scale>:<Text>
                    uint16_t col = parse_color_char((char)val[4]);
                    uint8_t sc = (val[6] == '2') ? 2 : 1;
                    memcpy(text_buf, &val[8], 8);
                    text_buf[8] = '\\0';
                    app_os_show_message(text_buf, col, sc);
                    LOG("BLE Message (single formatted): %s\\n", text_buf);
                } else {
                    // Plain: MSG:<Text>
                    memcpy(text_buf, &val[4], 12);
                    text_buf[12] = '\\0';
                    app_os_show_message(text_buf, COLOR_WHITE, 1);
                    LOG("BLE Message (single plain): %s\\n", text_buf);
                }
            }
            // Case B: Streaming Multi-chunk "M+:<C>:<S>:<chunk>" (Start)
            else if (val[0] == 'M' && val[1] == '+' && val[2] == ':') {
                s_ble_msg_color = parse_color_char((char)val[3]);
                s_ble_msg_scale = (val[5] == '2') ? 2 : 1;
                s_ble_msg_len = 0;
                memset(s_ble_msg_accum, 0, sizeof(s_ble_msg_accum));
                for (int i = 7; i < IBEACON_UUID_LEN && val[i] != '\\0'; i++) {
                    if (s_ble_msg_len < sizeof(s_ble_msg_accum) - 1) {
                        s_ble_msg_accum[s_ble_msg_len++] = (char)val[i];
                    }
                }
                s_ble_msg_accum[s_ble_msg_len] = '\\0';
            }
            // Case C: Streaming Multi-chunk "M~:<chunk>" (Continuation)
            else if (val[0] == 'M' && val[1] == '~' && val[2] == ':') {
                for (int i = 3; i < IBEACON_UUID_LEN && val[i] != '\\0'; i++) {
                    if (s_ble_msg_len < sizeof(s_ble_msg_accum) - 1) {
                        s_ble_msg_accum[s_ble_msg_len++] = (char)val[i];
                    }
                }
                s_ble_msg_accum[s_ble_msg_len] = '\\0';
            }
            // Case D: Streaming Multi-chunk "M.:<chunk>" (End & Display)
            else if (val[0] == 'M' && val[1] == '.' && val[2] == ':') {
                for (int i = 3; i < IBEACON_UUID_LEN && val[i] != '\\0'; i++) {
                    if (s_ble_msg_len < sizeof(s_ble_msg_accum) - 1) {
                        s_ble_msg_accum[s_ble_msg_len++] = (char)val[i];
                    }
                }
                s_ble_msg_accum[s_ble_msg_len] = '\\0';
                app_os_show_message(s_ble_msg_accum, s_ble_msg_color, s_ble_msg_scale);
                LOG("BLE Message (multi complete): %s\\n", s_ble_msg_accum);
                s_ble_msg_len = 0;
            }
"""

target1 = "static void watch_SimpleProfileChangeCB(uint8_t paramID) {"
assert target1 in content, "Target 1 not found!"
content = content.replace(target1, color_helper + "\n" + target1, 1)

target2 = '                LOG("BLE Time Synced: %02d:%02d:%02d\\n", h, m, s);\n            }'
assert target2 in content, "Target 2 not found!"
content = content.replace(target2, target2 + "\n" + msg_handler, 1)

with open('firmware/src/ble_manager.c', 'w', encoding='utf-8', newline='\n') as f:
    f.write(content)

print("Successfully patched firmware/src/ble_manager.c!")
