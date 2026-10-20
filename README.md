# PhyPlus PHY6222 Smartwatch Custom OS (LP715 / LP719)

[![Architecture](https://img.shields.io/badge/Architecture-ARM%20Cortex--M0-blue.svg)](https://developer.arm.com/Processors/Cortex-M0)
[![SoC](https://img.shields.io/badge/SoC-PhyPlus%20PHY6222QC-green.svg)](http://www.phyplusinc.com/)
[![BLE](https://img.shields.io/badge/Bluetooth-LE%205.2%20%2F%20OTA-orange.svg)]()
[![Display](https://img.shields.io/badge/Display-ST7735S%2080x160%20TFT-red.svg)]()
[![License](https://img.shields.io/badge/License-MIT-brightgreen.svg)](LICENSE)

Открытая кастомная прошивка (Custom OS) и документация по аппаратному реверс-инжинирингу умных часов и фитнес-браслетов на базе микроконтроллера **PhyPlus PHY6222QC** (ARM Cortex-M0).

Разработано для платы **`LP715_MB_V1.3`** (модель **`LP719`**, популярные браслеты Smart Band M4/M5/M6/M7/M8, D20, Y68, 116 Plus).

---

## ⚡ Реализованные режимы и возможности

Прошивка содержит стабильное, верифицированное ядро с 3 экранами UI, интерактивной UART-консолью и BLE-стеком:

1. **Главный циферблат (`MODE_WATCHFACE`)** (`firmware/src/app_os.c`):
   - Системное время (`ЧЧ:ММ` крупно + секунды отдельным полем) и аптайм (`ЧЧ:ММ:СС`).
   - Статус Bluetooth Low Energy (ADV / ONLINE).
   - Заряд аккумулятора: проценты и напряжение в мВ (АЦП AIO9 на `P20`).
   - Драйвер акселерометра `accel.c` присутствует, но в текущей UI-сборке не инициализируется
     (открытый вопрос P1-G в [CODE_REVIEW_RECOMMENDATIONS.md](CODE_REVIEW_RECOMMENDATIONS.md)).

2. **Сканер вибромотора (`MODE_VIBRATOR_TEST`)** (`firmware/src/app_os.c:107-133`, `firmware/src/vibrator.c`):
   - Интерактивный перебор пинов-кандидатов (P03, P00, P14, P02, P15, P18, P20, P23, P26).
     Внимание: `P20` одновременно является входом АЦП батареи — не удерживайте на нём HIGH.
   - Переключение полярности ключа (Active HIGH / Active LOW).
   - Аппаратная защита: выводы `P07` (TM) и `P33` (шлейф ST7735) исключены из сканера на уровне ядра.

3. **Сервисный экран OTA (`MODE_OTA_UPDATE`)** (`firmware/src/app_os.c:135-156`):
   - Подготовка к In-App OTA и перезагрузка в системный загрузчик чипа `slboot`.

4. **Дисплей ST7735S (80x160 IPS SPI)** (`firmware/src/display.c`):
   - Аппаратный SPI 16 МГц.
   - Калиброванное смещение окна (`colstart = 26`, `rowstart = 1`).
   - Включенная инверсия (`INVON 0x21`) для правильной цветопередачи IPS-матрицы.
   - Адресация памяти `MADCTL 0xC8` (вертикальная портретная ориентация).

5. **Интерактивный UART CLI (115200 8N1)** (`firmware/src/uart_cli.c`):
   - Основной инструмент аппаратной отладки через переходник CH340 / FTDI (выводы `P09`/`P10`).
   - Команды: `help`, `status`, `time HH:MM:SS`, `pin <p> <v>`, `read <p>`, `i2c scan`, `i2c r/w`, `ota`, `reboot`.
   - Встроенные guard'ы блокируют попытки перевода `P07` и `P33` в режим вывода HIGH.

6. **Bluetooth Low Energy 5.2**:
   - Имя устройства: `CHASY-STEALTH`.
   - Сервисы: Device Information Service (`0x180A`), Simple Profile (`0xFFF0`), In-App OTA Service
     (`5833ff01-9b8b-5191-6142-22a4536ef123`: Command `…ff02…`, Response `…ff03…`, Data `…ff04…`).

---

## 🎮 Управление (Сенсорная кнопка P11)

Драйвер кнопки (`firmware/src/button.c`) поддерживает 3 типа событий:

| Действие | На главном циферблате | В режиме теста вибро | В режиме OTA |
|---|---|---|---|
| **Одиночный клик** | Тактильный виброотклик (80 мс) | Следующий пин + тестовый импульс | Выход на циферблат |
| **Двойной клик** | Сброс и возврат на циферблат | Смена полярности (HIGH ↔ LOW) | Сброс и возврат на циферблат |
| **Удержание (>1.2 с)** | Вход в экран OTA | Сохранение текущего пина | Перезагрузка в OTA-bootloader |

---

## 🛠️ Аппаратная распиновка

Полная спецификация, активные уровни и источники подтверждения вынесены в канонический документ:  
👉 **[docs/PINMAP.md](docs/PINMAP.md)**

Пятаки платы `LP715_MB_V1.3` вдоль верхнего края:
```text
[GND] [TX] [VCC3V] [GND]  -- [Шлейф дисплея] --  [RX] [HR] [VBUS] [TM] [EN] [BT]
  1     2     3       4                             5    6    7     8    9    10
```
- **UART**: `TX` (P09, pin 8 чипа) -> RX адаптера; `RX` (P10, pin 9 чипа) -> TX адаптера (115200 baud).
- **Вход TM**: пятак 8 (P07 чипа). **3.3V при подаче питания переводит в ROM bootloader `cmd>>:`.** В обычной работе выход строго запрещен!
- **Питание**: `VCC3V` (3.3В) и `GND`.

---

## 🚀 Сборка прошивки

### Требования:
1. Python 3.8+ (`pyserial`, `cryptography` для сборки OTA).
2. Тулчейн `arm-none-eabi-gcc` (Cortex-M0).
3. Vendor SDK PhyPlus в `tools/smartwatch_ref/sdk` (см. инструкцию [docs/SDK_SETUP.md](docs/SDK_SETUP.md)).

### Компиляция:
```bash
python firmware/build.py
```
Скрипт генерирует:
- `firmware/build/custom_firmware.hex` — образ со всеми секциями (Flash XIP + SRAM) для проводной прошивки через UART.
- `firmware/build/custom_firmware_ota.bin` (146 916 байт) — упакованный образ для беспроводной прошивки по BLE.
  Сборка завершается ошибкой, если образ не влезает в 160 КБ staging-области OTA.

*(Генерация 240 МБ плоского `.bin` исключена из-за разрыва адресов XIP Flash и SRAM).*

### Беспроводная прошивка (In-App BLE OTA):
```bash
python scripts/flash/ota_ble_flash.py
```
Прошивальщик находит часы `CHASY-STEALTH`, пишет данные через Write Without Response
(характеристика `…ff04`, свойство `GATT_PROP_WRITE_NO_RSP`), чанками по 230 байт с flow-control
по burst-ACK (`0x87`). Установка прошивки начинается только после того, как часы подтвердили
CRC последнего раздела.

---

## 📜 Лицензия
MIT License. См. файл [LICENSE](LICENSE).
