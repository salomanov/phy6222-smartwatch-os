# Настройка вендорского SDK (tools/smartwatch_ref)

Для компиляции кастомной прошивки требуется вендорский SDK PhyPlus PHY6222QC (драйверы Flash, GPIO, SPI, I2C, BLE-стек OSAL и структуры ROM-загрузчика).

---

## 1. Расположение SDK в проекте

SDK должен находиться по пути:
```text
tools/smartwatch_ref/sdk/
```

В рабочей копии репозитория папка `tools/smartwatch_ref` привязана как gitlink (коммит `02cc0c961d926506ee577454d7bc1286bdb3d803`).

Ключевые директории SDK:
- `tools/smartwatch_ref/sdk/components/` — стек BLE, OSAL, драйверы периферии, профили OTA
- `tools/smartwatch_ref/sdk/misc/` — CMSIS заголовки Cortex-M0 и заголовочные файлы чипа PhyPlus
- `tools/smartwatch_ref/phy62x2_ota.py` — утилита упаковки OTA-образов

---

## 2. Установка в чистом клоне

Если после клонирования репозитория папка `tools/smartwatch_ref/sdk` пуста:

1. Если настроен доступ к сабмодулю:
   ```bash
   git submodule update --init --recursive
   ```
2. Либо скопируйте SDK PhyPlus PHY6222 (версия 3.1.x / коммит `02cc0c961d926506ee577454d7bc1286bdb3d803`) в каталог `tools/smartwatch_ref/`.

---

## 3. Тулчейн компилятора

Для сборки необходим `arm-none-eabi-gcc` версии 10+:
- Windows: xPack GNU Arm Embedded Toolchain (`arm-none-eabi-gcc.exe`)
- Путь к компилятору настраивается в переменной `GCC_BIN` файла `firmware/build.py` (или добавляется в системный `PATH`).

После размещения SDK запустите тестовую сборку:
```bash
python firmware/build.py
```
Ожидаемый результат: `[+] BUILD SUCCESS!`
