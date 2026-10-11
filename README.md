# PhyPlus PHY6222 Smartwatch Custom OS (LP715 / LP719)

[![Architecture](https://img.shields.io/badge/Architecture-ARM%20Cortex--M0-blue.svg)](https://developer.arm.com/Processors/Cortex-M0)
[![SoC](https://img.shields.io/badge/SoC-PhyPlus%20PHY6222QC-green.svg)](http://www.phyplusinc.com/)
[![BLE](https://img.shields.io/badge/Bluetooth-LE%205.2%20%2F%20OTA-orange.svg)]()
[![Display](https://img.shields.io/badge/Display-ST7735S%2080x160%20TFT-red.svg)]()

Полностью открытая кастомная прошивка (Custom OS) и документация по аппаратному реверс-инжинирингу умных часов и фитнес-браслетов на базе микроконтроллера **PhyPlus PHY6222QC** (ARM Cortex-M0).

Проект разработан для платы **`LP715_MB_V1.3`** (заводская модель **`LP719`**, популярные аналоги: Smart Band M4/M5/M6/M7/M8, D20, Y68, 116 Plus).

---

## ⚡ Особенности прошивки
- **Главный циферблат (Watchface)**: вывод системного аптайма, статуса Bluetooth (ADV/CONN), счетчика кликов кнопки и виброотклика.
- **Дисплей ST7735S (80x160)**: собственный откалиброванный драйвер по аппаратному SPI (16 МГц) с правильной ориентацией, цветовой инверсией IPS матрицы и смещением памяти (`colstart=26`, `rowstart=1`).
- **Сенсорная кнопка (P20)**: навигация кликами, фильтрация дребезга, паник-кнопка (быстрый сброс на циферблат).
- **Вибромотор (P14)**: тактильный отклик на действия.
- **Шпаргалка (Stealth Reader)**: постраничный просмотр заметок с русским шрифтом 5x7 UTF-8.
- **Секундомер и фонарик**: встроенные утилиты с переключением экранов.
- **Беспроводная прошивка (OTA)**: встроенный сервис беспроводного обновления `PPlusOTA` с поддержкой перезагрузки в bootloader по клику/удержанию кнопки.
- **Bluetooth Low Energy 5.2**:
  - Имя устройства: `CHASY-STEALTH`
  - Стандартные сервисы: Generic Access, Device Information Service (0x180A), Simple Profile (0xFFF0) для обмена данными, OTA Service (0xFF01/0xFF02).

---

## 🛠️ Аппаратное подключение к UART (Распиновка пятаков платы)

Плата `LP715_MB_V1.3` содержит тестовые площадки вдоль верхнего края:

```text
[GND] [TX] [VCC3V] [GND]  -- [Шлейф дисплея] --  [RX] [HR] [VBUS] [TM] [EN] [BT]
  1     2     3       4                             5    6    7     8    9    10
```

- **Питание**: `VCC3V` (3.3В) и `GND`.
- **UART**: `TX` (Pin 8 чипа, P09) -> к **RX** переходника CH340 / FTDI (115200 baud).
- **UART**: `RX` (Pin 9 чипа, P10) -> к **TX** переходника CH340 / FTDI.
- **Режим прошивки (Bootloader)**: контакт `TM` (Pin 7 чипа) подтянуть к **3.3V**.

Полная спецификация регистров, карты памяти и Flash: [HARDWARE_AND_PLATFORM_SPEC.md](HARDWARE_AND_PLATFORM_SPEC.md).

---

## 🚀 Сборка и прошивка

### Требования:
- Python 3.8+ (`pyserial`)
- Тулчейн `arm-none-eabi-gcc` (Cortex-M0)

### 1. Компиляция:
```bash
python firmware/build.py
```
Скрипт собирает:
- `firmware/build/custom_firmware.hex` — полный бинарник для проводной прошивки через UART.
- `firmware/build/custom_firmware_ota.bin` — сжатый образ для прошивки по воздуху (OTA).

### 2. Прошивка по проводу (UART):
```bash
python flash_firmware.py
```
Подайте питание 3.3В на контакт `VCC3V` при появлении приглашения — прошивальщик автоматически захватит загрузчик `cmd>>:`, сотрет Flash и запишет прошивку блоками по 1024 байта с верификацией.

---

## 📂 Структура репозитория
```text
├── firmware/
│   ├── src/           # Исходный код ядра ОС, дисплея, BLE и приложений
│   ├── build.py       # Скрипт сборки прошивки на базе GCC
│   └── phy6222.ld     # Скрипт линковщика для архитектуры XIP Flash
├── tools/             # Скрипты прошивки, реверс-инжиниринга и SDK PhyPlus
├── graphify-out/      # Построенный граф знаний проекта (10 500+ узлов)
├── HARDWARE_AND_PLATFORM_SPEC.md # Полная спецификация чипа и платы
├── PROJECT_STATE.md   # Долговременная память архитектуры
└── flash_firmware.py  # Автономный прошивальщик с авто-определением порта
```

---

## 📜 Лицензия
MIT / Open Source. Создано для сообщества исследователей встраиваемых систем и микроконтроллеров PhyPlus.
