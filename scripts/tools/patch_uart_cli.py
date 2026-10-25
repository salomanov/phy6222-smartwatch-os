import os

with open('firmware/src/uart_cli.c', 'r', encoding='utf-8') as f:
    content = f.read()

extern_target = 'extern void app_os_set_time(uint8_t h, uint8_t m, uint8_t s);'
extern_replace = extern_target + '\nextern void app_os_show_message(const char *text, uint16_t color, uint8_t scale);'
assert extern_target in content, 'Extern target not found!'
content = content.replace(extern_target, extern_replace, 1)

help_target = '  time HH:MM:SS - Set time\\r\\n'
help_replace = '  time HH:MM:SS - Set time\\r\\n  msg [C S] txt - Show message\\r\\n'
assert help_target in content, 'Help target not found!'
content = content.replace(help_target, help_replace, 1)

cmd_target = '    } else if (strcmp(cmd, "ota") == 0) {'
cmd_replace = """    } else if (strncmp(cmd, "msg ", 4) == 0) {
        const char *p = cmd + 4;
        uint16_t col = COLOR_WHITE;
        uint8_t sc = 1;
        if (p[0] && p[1] == ' ' && p[2] && p[3] == ' ') {
            switch (p[0]) {
                case 'G': case 'g': col = COLOR_GREEN; break;
                case 'Y': case 'y': col = COLOR_YELLOW; break;
                case 'C': case 'c': col = COLOR_CYAN; break;
                case 'R': case 'r': col = COLOR_RED; break;
                case 'M': case 'm': col = COLOR_MAGENTA; break;
                case 'B': case 'b': col = COLOR_BLUE; break;
                case 'O': case 'o': col = COLOR_ORANGE; break;
                default: col = COLOR_WHITE; break;
            }
            sc = (p[2] == '2') ? 2 : 1;
            p += 4;
        }
        app_os_show_message(p, col, sc);
        uart_send_str("[OK] Message displayed on screen\\r\\n");
    } else if (strcmp(cmd, "ota") == 0) {"""
assert cmd_target in content, 'Cmd target not found!'
content = content.replace(cmd_target, cmd_replace, 1)

with open('firmware/src/uart_cli.c', 'w', encoding='utf-8', newline='\n') as f:
    f.write(content)

print("Successfully patched firmware/src/uart_cli.c!")
