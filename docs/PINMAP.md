# Единая карта аппаратных выводов (PINMAP)

**Плата**: `LP715_MB_V1.3` (SoC PhyPlus PHY6222QC, ARM Cortex-M0)  
**Статус**: Единый источник истины по всем выводам (Single Source of Truth). Любые изменения в коде должны быть согласованы с этой таблицей.

---

## 1. Сводная таблица GPIO

| GPIO пин | Функция / Назначение | Направление / Активный уровень | Где определено в коде | Статус подтверждения |
|---|---|---|---|---|
| **P00** | Питание пульсометра (HR Power Switch) | Output, Active LOW (0 = включен) | `firmware/src/config.h:19`, Factory Dump `0x1102B130` | **Подтверждено дизассемблером factory_dump.bin** |
| **P01** | Подсветка дисплея (BKL) | Output, Active LOW (0 = горит) | `firmware/src/config.h:11`, `firmware/src/display.c:28` | **Подтверждено** (дисплей светится) |
| **P02** | Аналоговый IO / Служебный | Output / Disabled Analog | Factory Dump `0x11020F70` | Служебный |
| **P03** | Вибромотор (Vibrator) | Output, Active HIGH (1 = вибро) | `firmware/src/config.h:17-18`, `firmware/src/vibrator.c:37` | **Подтверждено физически пользователем** |
| **P07** | **Аппаратный вход TM (Test Mode)** | **Input, pull-down. ЗАПРЕЩЕН ВЫХОД!** | `HARDWARE_AND_PLATFORM_SPEC.md:40`, `firmware/src/vibrator.c:9`, `firmware/src/uart_cli.c:189` | **Подтверждено аппаратно** (пятак TM, 3.3V = ROM bootloader) |
| **P09** | UART TX (отладка/прошивка) | Output (115200 8N1) -> RX CH340 | `firmware/src/config.h:23`, `firmware/src/uart_cli.c:31` | **Подтверждено** (CLI работает) |
| **P10** | UART RX (отладка/прошивка) | Input (115200 8N1) <- TX CH340 | `firmware/src/config.h:24`, `firmware/src/uart_cli.c:32` | **Подтверждено** (CLI работает) |
| **P11** | Сенсорная кнопка (Touch) | Input, pull-down (1 = касание) | `firmware/src/config.h:16`, `firmware/src/button.c:7` | **Подтверждено физически пользователем** |
| **P14** | **Зеленый светодиод пульсометра (Green LED)**| Output, Active HIGH (1 = горит) | Factory Dump `0x11028048`, `0x110281E6` | **Подтверждено дизассемблером factory_dump.bin** |
| **P15** | **Питание/сброс сенсоров (Sensor Power/RST)**| Output, Active HIGH (1 = включен) | Factory Dump `0x11020FFC`, `0x11021008` | **Подтверждено дизассемблером factory_dump.bin** |
| **P16** | **Акселерометр I2C SCL (Clock)** | Open-Drain / Strong Pull-Up | Factory Dump `0x11037FA0`, `0x1103137C` | **Подтверждено дизассемблером factory_dump.bin** |
| **P17** | **Акселерометр I2C SDA (Data)** — фактически используется кодом | Open-Drain / Strong Pull-Up | `firmware/src/config.h` (`ACCEL_SDA_PIN = GPIO_P17`), `firmware/src/accel.c:10,152` | **Не подтверждено на железе**: ROM-анализ указывает на P19, код использует P17. Требуется проверка `i2c scan` |
| **P19** | Кандидат I2C SDA по ROM-анализу (в коде не используется) | — | Factory Dump `0x11037FA8`, `0x11031384` | **Требует проверки** на железе |
| **P23** | Кандидат GPIO | Output | `firmware/src/vibrator.c:29` | Тестовый кандидат |
| **P24** | ST7735S Display RST | Output (Reset) | `firmware/src/config.h:9`, `firmware/src/display.c:19` | **Подтверждено** (экран инициализируется) |
| **P25** | ST7735S Display DC | Output (Data/Command) | `firmware/src/config.h:8`, `firmware/src/display.c:18` | **Подтверждено** (экран инициализируется) |
| **P26** | Кандидат GPIO | Output | `firmware/src/vibrator.c:30` | Тестовый кандидат |
| **P31** | ST7735S Display CS | Output, Active LOW | `firmware/src/config.h:10`, `firmware/src/display.c:20` | **Подтверждено** (экран инициализируется) |
| **P32** | ST7735S Display MOSI | Output (SPI Data Out) | `firmware/src/config.h:13`, `firmware/src/display.c:22` | **Подтверждено** (экран инициализируется) |
| **P33** | **Шлейф дисплея ST7735S** | **НЕ ТРОГАТЬ! ЗАПРЕЩЕН ВЫХОД!** | `ERRORS_AND_FAILURES.md (1.1)`, `firmware/src/vibrator.c:9`, `firmware/src/uart_cli.c:191` | **Подтверждено** (любой импульс гасит дисплей) |
| **P34** | ST7735S Display SCLK | Output (SPI Clock 16 МГц) | `firmware/src/config.h:12`, `firmware/src/display.c:21` | **Подтверждено** (экран инициализируется) |

---

## 2. Неподтвержденная периферия

- **Пульсометр (PPG)**: В ранних черновых заметках упоминались P07/P11, но `P07` является входом TM, а `P11` — сенсорной кнопкой. На данной плате отдельный датчик пульса **не подтвержден** и в текущей прошивке не опрашивается.
- **Активный уровень вибромотора**: В текущей сборке используется `GPIO_P03` с активным уровнем HIGH (`config.h:17-18`). В `vibrator.c` предусмотрен сканер `MODE_VIBRATOR_TEST` для поиска альтернативных ключей (P14 и др.).

---

## 3. Критические аппаратные запреты (Hardware Guards)

1. **Запрет на `P07`**: Пин подключен к пятаку `TM`. Подача логической единицы переводит чип в режим UART ROM bootloader, прерывая исполнение приложения. Защищено программными guard'ами в `vibrator.c` и `uart_cli.c`.
2. **Запрет на `P33`**: Пин соединен с внутренними цепями дисплея ST7735S. Любая генерация импульсов на P33 приводит к гашению и рассинхронизации экрана. Защищено программными guard'ами в `vibrator.c` и `uart_cli.c`.
3. **I2C шина**: Только выводы `P16` (SCL) и `P17` (SDA). Использование других пинов для I2C датчиков запрещено.
