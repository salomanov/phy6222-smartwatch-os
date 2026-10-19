# -*- coding: utf-8 -*-
"""
LP715 Smartwatch Studio - Графическая среда диагностики, тестирования и управления
MCU: PhyPlus PHY6222QC (32-bit ARM Cortex-M0 @ 16/64MHz, BLE 5.2, 512KB Flash)
Display: ST7735S 0.96" 80x160 IPS SPI
Peripherals: Touch P11, Motor P03, LED P00, Backlight P01, Battery ADC P20, I2C P16/P17
"""

import sys
import os
import time
import queue
import subprocess
import serial
import serial.tools.list_ports
import cv2

try:
    sys.stdout.reconfigure(encoding='utf-8')
    sys.stderr.reconfigure(encoding='utf-8')
except Exception:
    pass

from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout,
    QHBoxLayout, QGridLayout, QLabel, QPushButton, QFrame,
    QSlider, QProgressBar, QComboBox, QTableWidget,
    QTableWidgetItem, QHeaderView, QCheckBox, QTabWidget,
    QTextEdit, QLineEdit, QScrollArea, QGroupBox, QMessageBox
)
from PyQt6.QtCore import Qt, QThread, pyqtSignal, QTimer
from PyQt6.QtGui import QFont, QColor, QImage, QPixmap


# ==============================================================================
# BLE OTA FLASH WORKER THREAD
# ==============================================================================
class OtaFlashWorker(QThread):
    log_line = pyqtSignal(str)
    progress = pyqtSignal(int)
    completed = pyqtSignal(bool, str)

    def run(self):
        script_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'scripts', 'flash', 'ota_ble_flash.py')
        cmd = [sys.executable, script_path]
        self.log_line.emit(f"[*] Запуск процесса BLE OTA: {sys.executable} {script_path}")
        try:
            p = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1,
                encoding='utf-8',
                errors='replace'
            )
            for line in p.stdout:
                clean = line.strip()
                if clean:
                    self.log_line.emit(clean)
                    if "[PROGRESS]" in clean:
                        try:
                            pct_str = clean.split("[PROGRESS]")[1].split("%")[0].strip()
                            self.progress.emit(int(pct_str))
                        except Exception:
                            pass
            p.wait()
            if p.returncode == 0:
                self.completed.emit(True, "Прошивка по воздуху успешно завершена на 100%! Часы перезагружаются.")
            else:
                self.completed.emit(False, f"Процесс завершился с кодом {p.returncode}")
        except Exception as e:
            self.completed.emit(False, f"Исключение при запуске OTA: {e}")


# ==============================================================================
# SERIAL WORKER THREAD
# ==============================================================================
class WatchSerialWorker(QThread):
    connection_changed = pyqtSignal(bool, str)
    telemetry_updated  = pyqtSignal(dict)
    log_received       = pyqtSignal(str)
    i2c_scan_done      = pyqtSignal(list)
    button_state       = pyqtSignal(int)

    def __init__(self, port="COM5", baudrate=115200):
        super().__init__()
        self.port = port
        self.baudrate = baudrate
        self.running = True
        self.paused = False
        self.cmd_queue = queue.Queue()
        self.ser = None
        self.poll_button_active = False
        self.last_user_cmd = 0

    def pause_port(self):
        self.paused = True
        if self.ser and self.ser.is_open:
            try:
                self.ser.close()
            except Exception:
                pass
            self.ser = None

    def resume_port(self):
        self.paused = False

    def send_command(self, cmd_str):
        self.last_user_cmd = time.time()
        self.cmd_queue.put(cmd_str)

    def set_poll_button(self, enabled):
        self.poll_button_active = enabled

    def stop(self):
        self.running = False
        self.wait(1000)

    def run(self):
        last_status_query = time.time()
        last_button_poll = time.time()

        while self.running:
            if self.paused:
                time.sleep(0.3)
                continue

            # 1. Connection check
            if self.ser is None or not self.ser.is_open:
                try:
                    self.ser = serial.Serial(self.port, self.baudrate, timeout=0.1)
                    self.connection_changed.emit(True, f"Подключено к {self.port} (115200 бод)")
                    self.log_received.emit(f"[*] Порт {self.port} успешно открыт.")
                    time.sleep(0.15)
                    self.ser.write(b"\r\n")
                    time.sleep(0.1)
                    self.ser.write(b"ping\r\n")
                    time.sleep(0.2)
                    self.ser.write(b"status\r\n")
                except Exception as e:
                    self.ser = None
                    self.connection_changed.emit(False, f"Поиск часов на {self.port}... ({e})")
                    time.sleep(1.0)
                    continue

            # 2. Process command queue
            try:
                while not self.cmd_queue.empty():
                    cmd = self.cmd_queue.get_nowait()
                    self.ser.write(cmd.encode("utf-8") + b"\r\n")
                    self.log_received.emit(f">>> {cmd}")
                    self.last_user_cmd = time.time()
                    time.sleep(0.1)

                # 3. Read incoming serial responses
                lines = []
                while True:
                    raw_line = self.ser.readline()
                    if not raw_line:
                        break
                    text = raw_line.decode("utf-8", errors="replace").strip()
                    if text and text != "cmd>>":
                        lines.append(text)
                        self.log_received.emit(f"  [CHIP] {text}")

                for text in lines:
                    # Parse status: "FW: v1.5.2-OTA, BLE: ADV, Bat: 73% (DISCHG)"
                    if "FW:" in text and "Bat:" in text:
                        tele = {}
                        parts = [p.strip() for p in text.split(",")]
                        for p in parts:
                            if p.startswith("FW:"):
                                tele['fw'] = p.split(":", 1)[1].strip()
                            elif p.startswith("BLE:"):
                                tele['ble'] = p.split(":", 1)[1].strip()
                            elif p.startswith("Bat:"):
                                sub = p.split(":", 1)[1].strip()
                                tele['bat_str'] = sub
                                try:
                                    tele['bat_pct'] = int(sub.split("%")[0].strip())
                                except Exception:
                                    tele['bat_pct'] = 0
                        self.telemetry_updated.emit(tele)

                    # Parse Pin P11 read: "[OK] Pin P11 = 0"
                    elif "Pin P11 =" in text:
                        try:
                            val = int(text.split("=")[1].strip())
                            self.button_state.emit(val)
                        except Exception:
                            pass

                # 4. Periodic background tasks (only when user has not commanded recently)
                now = time.time()
                user_quiet = now - self.last_user_cmd

                if user_quiet > 0.6 and self.cmd_queue.empty():
                    # Status query every 2.5 seconds
                    if now - last_status_query > 2.5:
                        last_status_query = now
                        self.ser.write(b"status\r\n")
                        time.sleep(0.04)

                    # Live touch button polling if user toggled it (every 300 ms)
                    elif self.poll_button_active and (now - last_button_poll > 0.3):
                        last_button_poll = now
                        self.ser.write(b"read 11\r\n")
                        time.sleep(0.04)

                time.sleep(0.02)

            except Exception as ser_err:
                self.log_received.emit(f"[!] Ошибка порта: {ser_err}")
                if self.ser:
                    try:
                        self.ser.close()
                    except Exception:
                        pass
                self.ser = None
                self.connection_changed.emit(False, f"Обрыв связи: {ser_err}")
                time.sleep(1.0)


# ==============================================================================
# MAIN STUDIO WINDOW
# ==============================================================================
class WatchStudioWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("LP715 Smartwatch Studio — Стенд диагностики и тестирования")
        self.resize(1280, 840)
        self.setMinimumSize(1100, 720)

        self.cap = None
        self.camera_timer = QTimer(self)
        self.camera_timer.timeout.connect(self.update_webcam_frame)

        self.backlight_state = True
        self.init_ui()
        self.apply_theme()

        # Start Serial Worker
        self.worker = WatchSerialWorker(port="COM5")
        self.worker.connection_changed.connect(self.on_connection_changed)
        self.worker.telemetry_updated.connect(self.on_telemetry_updated)
        self.worker.log_received.connect(self.on_log_received)
        self.worker.button_state.connect(self.on_button_state_changed)
        self.worker.start()

    def init_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        main_layout = QVBoxLayout(central)
        main_layout.setContentsMargins(14, 14, 14, 14)
        main_layout.setSpacing(10)

        # ----------------------------------------------------------------------
        # 1. TOP HEADER & TELEMETRY BAR
        # ----------------------------------------------------------------------
        header = QFrame()
        header.setObjectName("headerCard")
        h_layout = QHBoxLayout(header)
        h_layout.setContentsMargins(12, 8, 12, 8)

        self.lbl_conn = QLabel("[*] Поиск часов на COM5...")
        self.lbl_conn.setStyleSheet("color: #f59e0b; font-weight: bold; font-size: 14px;")

        self.lbl_fw = QLabel("ПРОШИВКА: v1.5.3-OTA")
        self.lbl_fw.setStyleSheet("color: #10b981; font-weight: bold; font-size: 14px;")

        self.lbl_ble = QLabel("BLE: ADV")
        self.lbl_ble.setStyleSheet("color: #38bdf8; font-weight: bold; font-size: 14px;")

        self.lbl_bat = QLabel("АКБ: 73% (DISCHG)")
        self.lbl_bat.setStyleSheet("color: #a855f7; font-weight: bold; font-size: 14px;")

        self.btn_ping = QPushButton("PING")
        self.btn_ping.clicked.connect(lambda: self.worker.send_command("ping"))
        self.btn_reboot = QPushButton("REBOOT")
        self.btn_reboot.setObjectName("btnDanger")
        self.btn_reboot.clicked.connect(lambda: self.worker.send_command("reboot"))

        h_layout.addWidget(self.lbl_conn)
        h_layout.addStretch()
        h_layout.addWidget(self.lbl_fw)
        h_layout.addSpacing(20)
        h_layout.addWidget(self.lbl_ble)
        h_layout.addSpacing(20)
        h_layout.addWidget(self.lbl_bat)
        h_layout.addSpacing(20)
        h_layout.addWidget(self.btn_ping)
        h_layout.addWidget(self.btn_reboot)

        main_layout.addWidget(header)

        # ----------------------------------------------------------------------
        # 2. MAIN 3-COLUMN SPLIT
        # ----------------------------------------------------------------------
        body_layout = QHBoxLayout()
        body_layout.setSpacing(12)

        # ======================================================================
        # COLUMN 1: VISUAL WATCH DISPLAY & LIVE WEBCAM (Left, width ~320px)
        # ======================================================================
        col_disp = QVBoxLayout()
        card_disp = QFrame()
        card_disp.setObjectName("card")
        cd_layout = QVBoxLayout(card_disp)

        t_disp = QLabel("ДИСПЛЕЙ ЧАСОВ (ST7735S 80x160)")
        t_disp.setObjectName("cardTitle")
        cd_layout.addWidget(t_disp)

        # Watch bezel frame
        watch_bezel = QFrame()
        watch_bezel.setObjectName("watchBezel")
        wb_layout = QVBoxLayout(watch_bezel)
        wb_layout.setContentsMargins(12, 12, 12, 12)
        wb_layout.setAlignment(Qt.AlignmentFlag.AlignCenter)

        # Screen simulation label
        self.lbl_screen_sim = QLabel()
        self.lbl_screen_sim.setFixedSize(160, 320) # 80x160 scaled 2x
        self.lbl_screen_sim.setStyleSheet("""
            background-color: #000000;
            border: 2px solid #0284c7;
            border-radius: 8px;
        """)
        self.lbl_screen_sim.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.render_simulated_screen()
        wb_layout.addWidget(self.lbl_screen_sim)

        cd_layout.addWidget(watch_bezel)

        # Live Webcam Preview Box
        t_cam = QLabel("ЖИВОЙ КАДР С ВЕБ-КАМЕРЫ:")
        t_cam.setStyleSheet("color: #94a3b8; font-size: 11px; font-weight: bold; margin-top: 6px;")
        cd_layout.addWidget(t_cam)

        self.lbl_cam_preview = QLabel("Камера выключена")
        self.lbl_cam_preview.setFixedSize(240, 160)
        self.lbl_cam_preview.setStyleSheet("background-color: #090d16; border: 1px dashed #334155; border-radius: 6px; color: #64748b;")
        self.lbl_cam_preview.setAlignment(Qt.AlignmentFlag.AlignCenter)
        cd_layout.addWidget(self.lbl_cam_preview, alignment=Qt.AlignmentFlag.AlignCenter)

        btn_cam_row = QHBoxLayout()
        self.btn_cam_toggle = QPushButton("Вкл камеру")
        self.btn_cam_toggle.clicked.connect(self.toggle_webcam)
        self.btn_cam_snap = QPushButton("Снимок")
        self.btn_cam_snap.clicked.connect(self.snap_webcam)
        btn_cam_row.addWidget(self.btn_cam_toggle)
        btn_cam_row.addWidget(self.btn_cam_snap)
        cd_layout.addLayout(btn_cam_row)

        col_disp.addWidget(card_disp)
        body_layout.addLayout(col_disp, 3)

        # ======================================================================
        # COLUMN 2: HARDWARE CONTROLS (Center, width ~420px)
        # ======================================================================
        col_ctrl = QVBoxLayout()

        # Card: Display & Clock
        card_disp_ctrl = QFrame()
        card_disp_ctrl.setObjectName("card")
        cdc_layout = QVBoxLayout(card_disp_ctrl)
        cdc_title = QLabel("УПРАВЛЕНИЕ ЭКРАНОМ И ВРЕМЕНЕМ")
        cdc_title.setObjectName("cardTitle")
        cdc_layout.addWidget(cdc_title)

        row_bkl = QHBoxLayout()
        self.btn_bkl_toggle = QPushButton("Подсветка: ВКЛ (bkl 0)")
        self.btn_bkl_toggle.setObjectName("btnAccent")
        self.btn_bkl_toggle.clicked.connect(self.toggle_backlight)

        self.btn_reinit_disp = QPushButton("Сброс ST7735 (disp)")
        self.btn_reinit_disp.clicked.connect(lambda: self.worker.send_command("disp"))
        row_bkl.addWidget(self.btn_bkl_toggle)
        row_bkl.addWidget(self.btn_reinit_disp)
        cdc_layout.addLayout(row_bkl)

        row_time = QHBoxLayout()
        self.btn_sync_time = QPushButton("Синхронизировать время с ПК")
        self.btn_sync_time.clicked.connect(self.sync_time_pc)
        row_time.addWidget(self.btn_sync_time)
        cdc_layout.addLayout(row_time)

        col_ctrl.addWidget(card_disp_ctrl)

        # Card: Haptics & LED
        card_vib = QFrame()
        card_vib.setObjectName("card")
        cv_layout = QVBoxLayout(card_vib)
        cv_title = QLabel("ВИБРОМОТОР (P03) И СВЕТОДИОД (P00)")
        cv_title.setObjectName("cardTitle")
        cv_layout.addWidget(cv_title)

        row_vib_btns = QHBoxLayout()
        btn_v80 = QPushButton("Тап (80 мс)")
        btn_v80.clicked.connect(lambda: self.worker.send_command("vib 80"))
        btn_v150 = QPushButton("Двойной (150 мс)")
        btn_v150.clicked.connect(lambda: self.worker.send_command("vib 150"))
        btn_v350 = QPushButton("Длинный (350 мс)")
        btn_v350.clicked.connect(lambda: self.worker.send_command("vib 350"))
        row_vib_btns.addWidget(btn_v80)
        row_vib_btns.addWidget(btn_v150)
        row_vib_btns.addWidget(btn_v350)
        cv_layout.addLayout(row_vib_btns)

        row_slider = QHBoxLayout()
        row_slider.addWidget(QLabel("Длительность:"))
        self.slider_vib = QSlider(Qt.Orientation.Horizontal)
        self.slider_vib.setRange(50, 1000)
        self.slider_vib.setValue(120)
        self.lbl_slider_val = QLabel("120 мс")
        self.slider_vib.valueChanged.connect(lambda v: self.lbl_slider_val.setText(f"{v} мс"))
        btn_slider_fire = QPushButton("Тест")
        btn_slider_fire.clicked.connect(lambda: self.worker.send_command(f"vib {self.slider_vib.value()}"))
        row_slider.addWidget(self.slider_vib)
        row_slider.addWidget(self.lbl_slider_val)
        row_slider.addWidget(btn_slider_fire)
        cv_layout.addLayout(row_slider)

        row_led = QHBoxLayout()
        btn_led_on = QPushButton("Зеленый LED ВКЛ (P00=0, P14=1)")
        btn_led_on.clicked.connect(self.turn_on_green_led)
        btn_led_off = QPushButton("Зеленый LED ВЫКЛ (P14=0, P00=1)")
        btn_led_off.clicked.connect(self.turn_off_green_led)
        row_led.addWidget(btn_led_on)
        row_led.addWidget(btn_led_off)
        cv_layout.addLayout(row_led)

        row_pwr = QHBoxLayout()
        btn_p15_on = QPushButton("Питание сенсоров ВКЛ (pin 15 1)")
        btn_p15_on.clicked.connect(lambda: self.worker.send_command("pin 15 1"))
        row_pwr.addWidget(btn_p15_on)
        cv_layout.addLayout(row_pwr)

        col_ctrl.addWidget(card_vib)

        # Card: Touch Sensor & Pins
        card_touch = QFrame()
        card_touch.setObjectName("card")
        ct_layout = QVBoxLayout(card_touch)
        ct_title = QLabel("СЕНСОРНАЯ КНОПКА (P11) И ВХОДЫ")
        ct_title.setObjectName("cardTitle")
        ct_layout.addWidget(ct_title)

        row_touch_status = QHBoxLayout()
        self.lbl_touch_indicator = QLabel("● СЕНСОР: ОТПУЩЕН (P11=0)")
        self.lbl_touch_indicator.setStyleSheet("color: #94a3b8; font-weight: bold; font-size: 13px;")
        self.btn_poll_touch = QPushButton("Вкл живой опрос")
        self.btn_poll_touch.setCheckable(True)
        self.btn_poll_touch.toggled.connect(self.toggle_touch_polling)
        row_touch_status.addWidget(self.lbl_touch_indicator)
        row_touch_status.addStretch()
        row_touch_status.addWidget(self.btn_poll_touch)
        ct_layout.addLayout(row_touch_status)

        # I2C Scanner Block
        row_i2c = QHBoxLayout()
        self.btn_i2c_scan = QPushButton("Сканировать шину I2C (P16/P17)")
        self.btn_i2c_scan.clicked.connect(lambda: self.worker.send_command("i2c scan"))
        row_i2c.addWidget(self.btn_i2c_scan)
        ct_layout.addLayout(row_i2c)

        col_ctrl.addWidget(card_touch)

        # Card: BLE OTA Wireless Firmware Update
        card_ota = QFrame()
        card_ota.setObjectName("card")
        cota_layout = QVBoxLayout(card_ota)
        cota_title = QLabel("ПРОШИВКА ПО ВОЗДУХУ (BLE IN-APP OTA)")
        cota_title.setObjectName("cardTitle")
        cota_layout.addWidget(cota_title)

        lbl_ota_info = QLabel("Файл: firmware/build/custom_firmware_ota.bin (v1.5.3-OTA, 146 KB)")
        lbl_ota_info.setStyleSheet("color: #94a3b8; font-size: 11px;")
        cota_layout.addWidget(lbl_ota_info)

        self.btn_flash_ota = QPushButton("⚡ Прошить часы по воздуху (BLE OTA)")
        self.btn_flash_ota.setStyleSheet("background-color: #0284c7; color: white; font-weight: bold; padding: 8px; border-radius: 6px;")
        self.btn_flash_ota.clicked.connect(self.start_ble_ota)
        cota_layout.addWidget(self.btn_flash_ota)

        self.progress_ota = QProgressBar()
        self.progress_ota.setRange(0, 100)
        self.progress_ota.setValue(0)
        self.progress_ota.setTextVisible(True)
        self.progress_ota.setStyleSheet("QProgressBar { border: 1px solid #334155; border-radius: 4px; text-align: center; background: #0f172a; color: white; font-weight: bold; } QProgressBar::chunk { background-color: #10b981; }")
        self.progress_ota.setVisible(False)
        cota_layout.addWidget(self.progress_ota)

        col_ctrl.addWidget(card_ota)
        body_layout.addLayout(col_ctrl, 4)

        # ======================================================================
        # COLUMN 3: TERMINAL & CHECKLIST REPORT (Right, width ~460px)
        # ======================================================================
        col_right = QVBoxLayout()

        # Tab 1: Live Terminal
        tabs = QTabWidget()

        tab_term = QWidget()
        tt_layout = QVBoxLayout(tab_term)
        tt_layout.setContentsMargins(8, 8, 8, 8)

        self.txt_term = QTextEdit()
        self.txt_term.setReadOnly(True)
        self.txt_term.setObjectName("terminal")
        tt_layout.addWidget(self.txt_term)

        row_send = QHBoxLayout()
        self.txt_cmd_input = QLineEdit()
        self.txt_cmd_input.setPlaceholderText("Команда (help, status, time, read 11, pin 3 0...)")
        self.txt_cmd_input.returnPressed.connect(self.send_custom_command)
        btn_send = QPushButton("Отправить")
        btn_send.clicked.connect(self.send_custom_command)
        btn_clear_term = QPushButton("Очистить")
        btn_clear_term.clicked.connect(self.txt_term.clear)

        row_send.addWidget(self.txt_cmd_input)
        row_send.addWidget(btn_send)
        row_send.addWidget(btn_clear_term)
        tt_layout.addLayout(row_send)

        tabs.addTab(tab_term, "Терминал UART")

        # Tab 2: Hardware Checklist & Report Generator
        tab_check = QWidget()
        tc_layout = QVBoxLayout(tab_check)
        tc_layout.setContentsMargins(8, 8, 8, 8)

        tc_info = QLabel("ОТМЕТЬ РЕАЛЬНОЕ ПОВЕДЕНИЕ ЧАСОВ И СФОРМИРУЙ ОТЧЁТ:")
        tc_info.setStyleSheet("color: #38bdf8; font-weight: bold; font-size: 11px;")
        tc_layout.addWidget(tc_info)

        scroll_check = QScrollArea()
        scroll_check.setWidgetResizable(True)
        scroll_w = QWidget()
        sw_layout = QVBoxLayout(scroll_w)
        sw_layout.setSpacing(6)

        self.cb_screen = QCheckBox("1. Экран светится, Watchface отображается чётко")
        self.cb_screen.setChecked(True)
        self.cb_bkl = QCheckBox("2. Подсветка выключается (bkl 0) и включается (bkl 1)")
        self.cb_bkl.setChecked(True)
        self.cb_time = QCheckBox("3. Время и аптайм идут, синхронизируются с ПК")
        self.cb_time.setChecked(True)
        self.cb_vib = QCheckBox("4. Вибромотор физически отзывается на импульс (vib 80/150)")
        self.cb_touch = QCheckBox("5. Сенсорная кнопка реагирует на касание пальца (P11=1)")
        self.cb_ble = QCheckBox("6. BLE в эфире (имя CHASY-STEALTH, статус ADV/CONN)")
        self.cb_ble.setChecked(True)
        self.cb_ota = QCheckBox("7. Прошивка по воздуху (BLE In-App OTA) шьётся на 100%")
        self.cb_ota.setChecked(True)
        self.cb_accel = QCheckBox("8. Акселерометр обнаружен на шине I2C")

        for cb in [self.cb_screen, self.cb_bkl, self.cb_time, self.cb_vib, self.cb_touch, self.cb_ble, self.cb_ota, self.cb_accel]:
            sw_layout.addWidget(cb)

        scroll_check.setWidget(scroll_w)
        tc_layout.addWidget(scroll_check)

        self.txt_notes = QLineEdit()
        self.txt_notes.setPlaceholderText("Дополнительный комментарий или замечание...")
        tc_layout.addWidget(self.txt_notes)

        btn_gen_report = QPushButton("📋 Скопировать отчёт для LLM / Сохранить")
        btn_gen_report.setObjectName("btnAccent")
        btn_gen_report.clicked.connect(self.generate_report)
        tc_layout.addWidget(btn_gen_report)

        tabs.addTab(tab_check, "Чеклист и Отчёт")

        col_right.addWidget(tabs)
        body_layout.addLayout(col_right, 5)

        main_layout.addLayout(body_layout)

    # --------------------------------------------------------------------------
    # SIMULATED SCREEN RENDERING
    # --------------------------------------------------------------------------
    def render_simulated_screen(self):
        # Create an 80x160 QImage simulating ST7735S display
        img = QImage(80, 160, QImage.Format.Format_RGB32)
        img.fill(QColor(0, 0, 0))

        # Painter
        from PyQt6.QtGui import QPainter, QPen
        p = QPainter(img)

        # Header Navy
        p.fillRect(0, 0, 80, 15, QColor(0, 15, 60))
        p.setPen(QColor(0, 210, 255))
        f_small = QFont("Arial", 6)
        p.setFont(f_small)
        p.drawText(6, 11, "LP715 WATCH")

        # Version line
        p.setPen(QColor(60, 60, 60))
        p.drawLine(2, 18, 78, 18)
        p.setPen(QColor(0, 230, 118))
        p.drawText(6, 28, "v1.5.3-OTA")

        # Dividers
        p.setPen(QColor(60, 60, 60))
        p.drawLine(2, 33, 78, 33)
        p.drawLine(2, 80, 78, 80)

        # Clock
        p.setPen(QColor(0, 210, 255))
        f_clock = QFont("Consolas", 10, QFont.Weight.Bold)
        p.setFont(f_clock)
        now_str = time.strftime("%H:%M:%S")
        p.drawText(4, 60, now_str)

        # Uptime
        p.setFont(f_small)
        p.setPen(QColor(255, 214, 0))
        p.drawText(4, 92, "UPTIME:")
        p.setPen(QColor(255, 255, 255))
        p.drawText(4, 102, "00:15:32")

        # Battery & BLE
        p.setPen(QColor(60, 60, 60))
        p.drawLine(2, 108, 78, 108)
        p.drawLine(2, 138, 78, 138)
        p.setPen(QColor(0, 210, 255))
        p.drawText(4, 150, "HOLD: OTA")

        p.end()

        # Scale 2x for crisp viewing
        pix = QPixmap.fromImage(img).scaled(160, 320, Qt.AspectRatioMode.KeepAspectRatio, Qt.TransformationMode.FastTransformation)
        self.lbl_screen_sim.setPixmap(pix)

    # --------------------------------------------------------------------------
    # WEBCAM HANDLERS
    # --------------------------------------------------------------------------
    def toggle_webcam(self):
        if self.camera_timer.isActive():
            self.camera_timer.stop()
            if self.cap:
                self.cap.release()
                self.cap = None
            self.btn_cam_toggle.setText("Вкл камеру")
            self.lbl_cam_preview.setText("Камера выключена")
            self.lbl_cam_preview.setPixmap(QPixmap())
        else:
            self.cap = cv2.VideoCapture(0)
            if not self.cap.isOpened():
                QMessageBox.warning(self, "Ошибка", "Не удалось открыть веб-камеру (index 0).")
                self.cap = None
                return
            self.btn_cam_toggle.setText("Выкл камеру")
            self.camera_timer.start(50) # 20 FPS

    def update_webcam_frame(self):
        if self.cap and self.cap.isOpened():
            ret, frame = self.cap.read()
            if ret:
                # Resize to preview label
                rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                h, w, ch = rgb.shape
                bytes_per_line = ch * w
                q_img = QImage(rgb.data, w, h, bytes_per_line, QImage.Format.Format_RGB888)
                pix = QPixmap.fromImage(q_img).scaled(240, 160, Qt.AspectRatioMode.KeepAspectRatio)
                self.lbl_cam_preview.setPixmap(pix)

    def snap_webcam(self):
        if self.cap and self.cap.isOpened():
            ret, frame = self.cap.read()
            if ret:
                path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "media", f"studio_snap_{int(time.time())}.jpg")
                os.makedirs(os.path.dirname(path), exist_ok=True)
                cv2.imwrite(path, frame)
                self.on_log_received(f"[+] Снимок сохранен в {path}")
                QMessageBox.information(self, "Снимок сохранен", f"Кадр с камеры сохранен:\n{path}")
        else:
            # One shot capture
            temp_cap = cv2.VideoCapture(0)
            for _ in range(10): temp_cap.read()
            ret, frame = temp_cap.read()
            temp_cap.release()
            if ret:
                path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "media", f"studio_snap_{int(time.time())}.jpg")
                os.makedirs(os.path.dirname(path), exist_ok=True)
                cv2.imwrite(path, frame)
                self.on_log_received(f"[+] Снимок сохранен в {path}")
                QMessageBox.information(self, "Снимок сохранен", f"Кадр сохранен:\n{path}")

    # --------------------------------------------------------------------------
    # HARDWARE ACTIONS
    # --------------------------------------------------------------------------
    def toggle_backlight(self):
        if self.backlight_state:
            self.worker.send_command("bkl 0")
            self.backlight_state = False
            self.btn_bkl_toggle.setText("Подсветка: ВЫКЛ (bkl 1)")
            self.btn_bkl_toggle.setStyleSheet("background-color: #334155; color: #94a3b8;")
        else:
            self.worker.send_command("bkl 1")
            self.backlight_state = True
            self.btn_bkl_toggle.setText("Подсветка: ВКЛ (bkl 0)")
            self.btn_bkl_toggle.setStyleSheet("background-color: #0284c7; color: white;")

    def turn_on_green_led(self):
        # Ground truth from factory dump: P00 is optical sensor power (0=ON), P14 is green LED gate (1=ON)
        self.worker.send_command("pin 0 0")
        self.worker.send_command("pin 14 1")

    def turn_off_green_led(self):
        self.worker.send_command("pin 14 0")
        self.worker.send_command("pin 0 1")

    def sync_time_pc(self):
        now_str = time.strftime("%H:%M:%S")
        self.worker.send_command(f"time {now_str}")
        self.render_simulated_screen()

    def toggle_touch_polling(self, checked):
        self.worker.set_poll_button(checked)
        if checked:
            self.btn_poll_touch.setText("Опрос АКТИВЕН")
            self.btn_poll_touch.setStyleSheet("background-color: #10b981; color: white;")
        else:
            self.btn_poll_touch.setText("Вкл живой опрос")
            self.btn_poll_touch.setStyleSheet("")

    def send_custom_command(self):
        cmd = self.txt_cmd_input.text().strip()
        if cmd:
            self.worker.send_command(cmd)
            self.txt_cmd_input.clear()

    # --------------------------------------------------------------------------
    # WORKER SLOTS
    # --------------------------------------------------------------------------
    def on_connection_changed(self, connected, text):
        if connected:
            self.lbl_conn.setText(f"[●] {text}")
            self.lbl_conn.setStyleSheet("color: #10b981; font-weight: bold; font-size: 14px;")
        else:
            self.lbl_conn.setText(f"[○] {text}")
            self.lbl_conn.setStyleSheet("color: #ef4444; font-weight: bold; font-size: 14px;")

    def on_telemetry_updated(self, d):
        if 'fw' in d:
            self.lbl_fw.setText(f"ПРОШИВКА: {d['fw']}")
        if 'ble' in d:
            self.lbl_ble.setText(f"BLE: {d['ble']}")
        if 'bat_str' in d:
            self.lbl_bat.setText(f"АКБ: {d['bat_str']}")

    def on_button_state_changed(self, val):
        if val == 1:
            self.lbl_touch_indicator.setText("● СЕНСОР: НАЖАТ (P11=1)")
            self.lbl_touch_indicator.setStyleSheet("color: #10b981; font-weight: bold; font-size: 13px;")
            self.cb_touch.setChecked(True)
        else:
            self.lbl_touch_indicator.setText("○ СЕНСОР: ОТПУЩЕН (P11=0)")
            self.lbl_touch_indicator.setStyleSheet("color: #94a3b8; font-weight: bold; font-size: 13px;")

    def on_log_received(self, text):
        self.txt_term.append(text)
        # Auto-scroll
        self.txt_term.verticalScrollBar().setValue(self.txt_term.verticalScrollBar().maximum())

    # --------------------------------------------------------------------------
    # BLE OTA WIRELESS FLASHING
    # --------------------------------------------------------------------------
    def start_ble_ota(self):
        reply = QMessageBox.question(
            self,
            "Прошивка по воздуху",
            "Запустить беспроводную прошивку часов через BLE In-App OTA?\n\n"
            "Порт COM5 будет временно освобождён для предотвращения конфликтов.\n"
            "Убедитесь, что часы не выключены и находятся рядом с ПК.",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        if reply != QMessageBox.StandardButton.Yes:
            return

        self.btn_flash_ota.setEnabled(False)
        self.progress_ota.setVisible(True)
        self.progress_ota.setValue(0)
        self.worker.pause_port()
        self.on_log_received("[*] Порт COM5 временно освобождён для BLE OTA.")

        self.ota_worker = OtaFlashWorker()
        self.ota_worker.log_line.connect(self.on_log_received)
        self.ota_worker.progress.connect(self.on_ota_progress)
        self.ota_worker.completed.connect(self.on_ota_completed)
        self.ota_worker.start()

    def on_ota_progress(self, val):
        self.progress_ota.setValue(val)

    def on_ota_completed(self, success, msg):
        self.btn_flash_ota.setEnabled(True)
        self.worker.resume_port()
        self.on_log_received(f"[*] BLE OTA завершена: {msg}")
        self.on_log_received("[*] Порт COM5 возобновлён.")
        if success:
            self.cb_ota.setChecked(True)
            self.lbl_fw.setText("ПРОШИВКА: v1.5.3-OTA")
            QMessageBox.information(self, "Успех BLE OTA", msg)
        else:
            QMessageBox.warning(self, "BLE OTA", msg)

    # --------------------------------------------------------------------------
    # REPORT GENERATOR
    # --------------------------------------------------------------------------
    def generate_report(self):
        notes = self.txt_notes.text().strip()
        now_date = time.strftime("%Y-%m-%d %H:%M:%S")

        report = f"""# Отчёт о проверке смарт-часов LP715 ({now_date})
**Прошивка**: {self.lbl_fw.text()}
**Батарея**: {self.lbl_bat.text()}
**Статус связи**: {self.lbl_conn.text()}

## Результаты тестирования:
- [{"x" if self.cb_screen.isChecked() else " "}] 1. Экран светится, Watchface отображается чётко
- [{"x" if self.cb_bkl.isChecked() else " "}] 2. Подсветка выключается (bkl 0) и включается (bkl 1)
- [{"x" if self.cb_time.isChecked() else " "}] 3. Время и аптайм идут, синхронизируются с ПК
- [{"x" if self.cb_vib.isChecked() else " "}] 4. Вибромотор физически отзывается на импульс (vib 80/150)
- [{"x" if self.cb_touch.isChecked() else " "}] 5. Сенсорная кнопка реагирует на касание пальца (P11=1)
- [{"x" if self.cb_ble.isChecked() else " "}] 6. BLE в эфире (имя CHASY-STEALTH, статус ADV/CONN)
- [{"x" if self.cb_ota.isChecked() else " "}] 7. Прошивка по воздуху (BLE In-App OTA) шьётся на 100%
- [{"x" if self.cb_accel.isChecked() else " "}] 8. Акселерометр обнаружен на шине I2C

**Комментарий разработчика**:
{notes if notes else "Без дополнительных замечаний."}
"""
        # Copy to clipboard
        clipboard = QApplication.clipboard()
        clipboard.setText(report)

        # Save to file
        report_file = os.path.join(os.path.dirname(os.path.abspath(__file__)), "docs", "HARDWARE_TEST_REPORT.md")
        with open(report_file, "w", encoding="utf-8") as f:
            f.write(report)

        QMessageBox.information(
            self,
            "Отчёт сформирован",
            f"Отчёт скопирован в буфер обмена (Ctrl+V) и сохранён в:\n{report_file}\n\nВы можете сразу вставить его в чат!"
        )

    # --------------------------------------------------------------------------
    # MODERN DARK QSS THEME
    # --------------------------------------------------------------------------
    def apply_theme(self):
        qss = """
        QMainWindow {
            background-color: #0b0f17;
        }
        QWidget {
            color: #e2e8f0;
            font-family: 'Segoe UI', 'Roboto', sans-serif;
            font-size: 12px;
        }
        #headerCard {
            background-color: #111827;
            border: 1px solid #1f2937;
            border-radius: 8px;
        }
        #card {
            background-color: #111827;
            border: 1px solid #1f2937;
            border-radius: 10px;
            padding: 8px;
        }
        #watchBezel {
            background-color: #030712;
            border: 3px solid #1e293b;
            border-radius: 16px;
        }
        #cardTitle {
            color: #38bdf8;
            font-weight: bold;
            font-size: 12px;
            letter-spacing: 0.5px;
            margin-bottom: 4px;
        }
        QPushButton {
            background-color: #1e293b;
            color: #f8fafc;
            border: 1px solid #334155;
            border-radius: 6px;
            padding: 6px 12px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: #334155;
            border-color: #475569;
        }
        QPushButton:pressed {
            background-color: #0f172a;
        }
        #btnAccent {
            background-color: #0284c7;
            border-color: #0369a1;
            color: white;
        }
        #btnAccent:hover {
            background-color: #0369a1;
        }
        #btnDanger {
            background-color: #be123c;
            border-color: #9f1239;
            color: white;
        }
        #btnDanger:hover {
            background-color: #9f1239;
        }
        QLineEdit, QTextEdit {
            background-color: #090d16;
            border: 1px solid #1e293b;
            border-radius: 6px;
            padding: 6px;
            color: #f1f5f9;
        }
        QLineEdit:focus, QTextEdit:focus {
            border-color: #0284c7;
        }
        #terminal {
            font-family: 'Consolas', 'Courier New', monospace;
            font-size: 11px;
            color: #38bdf8;
            background-color: #030712;
        }
        QTabWidget::pane {
            border: 1px solid #1f2937;
            background-color: #111827;
            border-radius: 8px;
        }
        QTabBar::tab {
            background-color: #0f172a;
            color: #94a3b8;
            padding: 8px 16px;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            font-weight: bold;
        }
        QTabBar::tab:selected {
            background-color: #111827;
            color: #38bdf8;
            border-bottom: 2px solid #38bdf8;
        }
        QCheckBox {
            spacing: 8px;
            color: #cbd5e1;
            font-size: 12px;
        }
        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border-radius: 4px;
            border: 1px solid #475569;
            background-color: #0f172a;
        }
        QCheckBox::indicator:checked {
            background-color: #0284c7;
            border-color: #38bdf8;
        }
        QSlider::groove:horizontal {
            height: 6px;
            background: #1e293b;
            border-radius: 3px;
        }
        QSlider::sub-page:horizontal {
            background: #0284c7;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            background: #38bdf8;
            width: 14px;
            margin-top: -4px;
            margin-bottom: -4px;
            border-radius: 7px;
        }
        """
        self.setStyleSheet(qss)


# ==============================================================================
# MAIN ENTRY POINT
# ==============================================================================
if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = WatchStudioWindow()
    window.show()
    sys.exit(app.exec())
