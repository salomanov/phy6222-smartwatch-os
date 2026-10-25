# -*- coding: utf-8 -*-
"""
LP715 Smartwatch BLE Pager & Live Display Simulator
Interactive text composer and live display preview with direct BLE dispatch.
"""

import sys
import os
import tempfile
import subprocess
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QLabel, QPushButton, QTextEdit, QRadioButton, QButtonGroup,
    QFrame, QMessageBox, QGroupBox
)
from PyQt6.QtCore import Qt, QThread, pyqtSignal
from PyQt6.QtGui import QPainter, QColor, QFont, QPen, QBrush

try:
    sys.stdout.reconfigure(encoding='utf-8')
    sys.stderr.reconfigure(encoding='utf-8')
except Exception:
    pass

# Display geometry
DISP_W = 80
DISP_H = 160
SCALE_FACTOR = 3  # Preview size 240x480

# Physical limits of smartwatch screen
MAX_LINES_SCALE1 = 11  # 5x7 font (12 chars/line)
MAX_CHARS_SCALE1 = 12
MAX_LINES_SCALE2 = 6   # 10x14 font (6 chars/line)
MAX_CHARS_SCALE2 = 6

COLORS = [
    ("Белый", "White", "#FFFFFF", QColor(255, 255, 255)),
    ("Зеленый", "Green", "#2ECC71", QColor(46, 204, 113)),
    ("Желтый", "Yellow", "#F1C40F", QColor(241, 196, 15)),
    ("Голубой", "Cyan", "#00FFFF", QColor(0, 255, 255)),
    ("Красный", "Red", "#E74C3C", QColor(231, 76, 60)),
    ("Розовый", "Magenta", "#E056FD", QColor(224, 86, 253)),
    ("Синий", "Blue", "#3498DB", QColor(52, 152, 219)),
    ("Оранжевый", "Orange", "#E67E22", QColor(230, 126, 34)),
]


# ==============================================================================
# BLE SENDER THREAD (Uses temp file to prevent Windows CLI quote & newline issues)
# ==============================================================================
class BleSenderThread(QThread):
    finished_signal = pyqtSignal(bool, str)

    def __init__(self, text, color_name, scale):
        super().__init__()
        self.text = text
        self.color_name = color_name
        self.scale = scale

    def run(self):
        base_dir = os.path.dirname(os.path.abspath(__file__))
        ps1_script = os.path.join(base_dir, "scripts", "tools", "ble_send_text.ps1")
        
        # Write payload to a UTF-8 temporary file
        tmp_path = os.path.join(tempfile.gettempdir(), "lp715_pager_msg.txt")
        try:
            with open(tmp_path, "w", encoding="utf-8") as f:
                f.write(self.text)
        except Exception as e:
            self.finished_signal.emit(False, f"Ошибка записи temp-файла: {e}")
            return

        cmd = [
            "powershell.exe",
            "-ExecutionPolicy", "Bypass",
            "-File", ps1_script,
            "-TextFile", tmp_path,
            "-Color", self.color_name,
            "-Scale", str(self.scale)
        ]
        try:
            res = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="ignore", timeout=30)
            if res.returncode == 0 and "SUCCESS!" in res.stdout:
                self.finished_signal.emit(True, "Сообщение доставлено на часы!")
            else:
                err_msg = res.stderr.strip() or res.stdout.strip() or f"Код возврата {res.returncode}"
                # Filter useful part of error
                lines = [l for l in err_msg.splitlines() if "[!]" in l or "Error" in l or "found" in l]
                summary = " | ".join(lines) if lines else err_msg[:120]
                self.finished_signal.emit(False, summary)
        except subprocess.TimeoutExpired:
            self.finished_signal.emit(False, "Таймаут подключения BLE (часы не ответили за 30 сек)")
        except Exception as e:
            self.finished_signal.emit(False, f"Сбой: {e}")
        finally:
            try:
                if os.path.exists(tmp_path):
                    os.remove(tmp_path)
            except Exception:
                pass


# ==============================================================================
# VIRTUAL WATCH DISPLAY WIDGET
# ==============================================================================
class WatchDisplaySimulator(QWidget):
    def __init__(self):
        super().__init__()
        self.setFixedSize(DISP_W * SCALE_FACTOR + 40, DISP_H * SCALE_FACTOR + 40)
        self.text = "Привет, мир!"
        self.text_color = QColor(46, 204, 113)  # Green
        self.scale = 1
        self.overflow = False
        self.visible_lines = []
        self.hidden_lines = []

    def update_content(self, text, qcolor, scale):
        self.text = text
        self.text_color = qcolor
        self.scale = scale
        max_chars = MAX_CHARS_SCALE1 if scale == 1 else MAX_CHARS_SCALE2
        max_lines = MAX_LINES_SCALE1 if scale == 1 else MAX_LINES_SCALE2
        all_lines = self.wrap_text(text, max_chars)
        self.visible_lines = all_lines[:max_lines]
        self.hidden_lines = all_lines[max_lines:]
        self.overflow = len(self.hidden_lines) > 0
        self.update()

    def wrap_text(self, text, max_chars):
        lines = []
        # Support manual newlines (\n) first
        paragraphs = text.split('\n')
        for para in paragraphs:
            para = para.strip()
            if not para:
                lines.append("")
                continue
            words = para.split(' ')
            cur_line = ""
            for word in words:
                if not word:
                    continue
                if not cur_line:
                    if len(word) > max_chars:
                        for i in range(0, len(word), max_chars):
                            lines.append(word[i:i+max_chars])
                    else:
                        cur_line = word
                else:
                    if len(cur_line) + 1 + len(word) <= max_chars:
                        cur_line += " " + word
                    else:
                        lines.append(cur_line)
                        if len(word) > max_chars:
                            for i in range(0, len(word), max_chars):
                                lines.append(word[i:i+max_chars])
                            cur_line = ""
                        else:
                            cur_line = word
            if cur_line:
                lines.append(cur_line)
        return lines

    def paintEvent(self, event):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing, True)

        # 1. Watch Body / Bezel
        body_rect = self.rect().adjusted(6, 6, -6, -6)
        p.setBrush(QBrush(QColor(25, 27, 34)))
        p.setPen(QPen(QColor(55, 60, 75), 3))
        p.drawRoundedRect(body_rect, 24, 24)

        # 2. Display Glass (80x160 scaled 3x -> 240x480)
        disp_x = 20
        disp_y = 20
        disp_w = DISP_W * SCALE_FACTOR
        disp_h = DISP_H * SCALE_FACTOR

        p.setBrush(QBrush(QColor(0, 0, 0)))
        p.setPen(QPen(QColor(10, 10, 10), 1))
        p.drawRect(disp_x, disp_y, disp_w, disp_h)

        # 3. Header Bar ("СООБЩЕНИЕ" on Navy Blue)
        hdr_h = 18 * SCALE_FACTOR
        p.fillRect(disp_x, disp_y, disp_w, hdr_h, QColor(0, 32, 96))
        
        p.setPen(QColor(255, 255, 255))
        f_hdr = QFont("Consolas", int(7 * SCALE_FACTOR * 0.8), QFont.Weight.Bold)
        p.setFont(f_hdr)
        p.drawText(disp_x, disp_y, disp_w, hdr_h, Qt.AlignmentFlag.AlignCenter, "СООБЩЕНИЕ")

        # 4. Footer Bar ("КЛИК: ВЫХОД" on Dark Gray)
        ftr_h = 16 * SCALE_FACTOR
        ftr_y = disp_y + disp_h - ftr_h
        p.fillRect(disp_x, ftr_y, disp_w, ftr_h, QColor(25, 25, 30))
        
        p.setPen(QColor(160, 160, 160))
        f_ftr = QFont("Consolas", int(6 * SCALE_FACTOR * 0.8))
        p.setFont(f_ftr)
        p.drawText(disp_x, ftr_y, disp_w, ftr_h, Qt.AlignmentFlag.AlignCenter, "КЛИК: ВЫХОД")

        # 5. Message Body
        line_height = 10 * SCALE_FACTOR if self.scale == 1 else 19 * SCALE_FACTOR
        font_size = int((6 if self.scale == 1 else 13) * SCALE_FACTOR * 0.82)
        
        f_body = QFont("Consolas", font_size, QFont.Weight.Bold if self.scale == 2 else QFont.Weight.Normal)
        p.setFont(f_body)
        p.setPen(self.text_color)

        start_y = disp_y + hdr_h + int(5 * SCALE_FACTOR)

        for i, line in enumerate(self.visible_lines):
            y_pos = start_y + (i * line_height)
            p.drawText(disp_x + 8, y_pos + font_size + 2, line)

        # 6. Overflow badge if text exceeded screen height
        if self.overflow:
            p.setBrush(QBrush(QColor(180, 40, 40, 220)))
            p.setPen(Qt.PenStyle.NoPen)
            badge_h = 18 * SCALE_FACTOR
            badge_y = ftr_y - badge_h - 4
            p.drawRoundedRect(disp_x + 8, badge_y, disp_w - 16, badge_h, 6, 6)
            
            p.setPen(QColor(255, 255, 255))
            f_warn = QFont("Segoe UI", 9, QFont.Weight.Bold)
            p.setFont(f_warn)
            p.drawText(disp_x + 8, badge_y, disp_w - 16, badge_h, Qt.AlignmentFlag.AlignCenter,
                       f"⚠️ +{len(self.hidden_lines)} строк не влезло")


# ==============================================================================
# MAIN WINDOW
# ==============================================================================
class WatchPagerWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("LP715 Smartwatch - BLE Пейджер и Live-Экран")
        self.setMinimumSize(860, 620)
        self.setStyleSheet("""
            QMainWindow { background-color: #1A1C23; }
            QWidget { color: #E0E0E0; font-family: 'Segoe UI', Arial, sans-serif; font-size: 13px; }
            QGroupBox { border: 1px solid #333846; border-radius: 8px; margin-top: 14px; padding-top: 14px; font-weight: bold; }
            QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 12px; color: #5dade2; }
            QTextEdit { background-color: #12141A; border: 1px solid #333846; border-radius: 6px; padding: 8px; font-size: 15px; color: #FFFFFF; }
            QPushButton { background-color: #242836; border: 1px solid #3A4155; border-radius: 6px; padding: 8px 14px; font-weight: bold; }
            QPushButton:hover { background-color: #2F3649; border-color: #5dade2; }
            QPushButton#btnSend { background-color: #27AE60; border: none; font-size: 15px; padding: 12px; color: white; border-radius: 6px; }
            QPushButton#btnSend:hover { background-color: #2ECC71; }
            QPushButton#btnSend:disabled { background-color: #555; color: #888; }
            QRadioButton { spacing: 8px; font-weight: normal; }
            QRadioButton::indicator { width: 16px; height: 16px; }
        """)

        self.cur_color_name = "Green"
        self.cur_qcolor = QColor(46, 204, 113)
        self.cur_scale = 1
        self.sender_thread = None

        self.init_ui()

    def init_ui(self):
        main_widget = QWidget()
        self.setCentralWidget(main_widget)
        main_layout = QHBoxLayout(main_widget)
        main_layout.setContentsMargins(20, 20, 20, 20)
        main_layout.setSpacing(24)

        # -------------------------------------------------------------
        # LEFT: Control Panel
        # -------------------------------------------------------------
        ctrl_layout = QVBoxLayout()
        ctrl_layout.setSpacing(12)

        lbl_title = QLabel("💬 Отправка сообщений на часы по BLE")
        lbl_title.setStyleSheet("font-size: 18px; font-weight: bold; color: #5dade2;")
        ctrl_layout.addWidget(lbl_title)

        # Text Input Group
        grp_text = QGroupBox("Текст сообщения")
        grp_text_layout = QVBoxLayout(grp_text)
        
        self.txt_input = QTextEdit()
        self.txt_input.setPlaceholderText("Введите текст для отображения на экране часов...")
        self.txt_input.setText("Привет, мир!")
        self.txt_input.textChanged.connect(self.on_content_changed)
        grp_text_layout.addWidget(self.txt_input)

        # Stats & Overflow Warning Label
        self.lbl_stats = QLabel("Символов: 12 | Строк: 1/11 | Лимит в строке: 12")
        self.lbl_stats.setStyleSheet("color: #888; font-size: 12px;")
        grp_text_layout.addWidget(self.lbl_stats)

        # Quick Templates
        tpl_layout = QHBoxLayout()
        templates = ["Привет!", "ТЕСТ", "Чай готов!", "Звонок в дверь", "Код: 5824"]
        for tpl in templates:
            btn = QPushButton(tpl)
            btn.setStyleSheet("font-size: 11px; padding: 4px 8px;")
            btn.clicked.connect(lambda ch, t=tpl: self.set_template(t))
            tpl_layout.addWidget(btn)
        grp_text_layout.addLayout(tpl_layout)

        ctrl_layout.addWidget(grp_text)

        # Color Selection Group
        grp_color = QGroupBox("Цвет текста на экране")
        grp_color_layout = QHBoxLayout(grp_color)
        self.color_btn_group = QButtonGroup(self)

        for i, (name_ru, name_en, hex_code, qcol) in enumerate(COLORS):
            btn = QPushButton(name_ru)
            btn.setCheckable(True)
            btn.setStyleSheet(f"""
                QPushButton {{
                    border: 2px solid #333846;
                    border-radius: 6px;
                    padding: 6px;
                    font-weight: bold;
                    color: {hex_code};
                }}
                QPushButton:checked {{
                    border: 2px solid {hex_code};
                    background-color: #222634;
                }}
            """)
            if name_en == "Green":
                btn.setChecked(True)
            btn.clicked.connect(lambda ch, ne=name_en, qc=qcol: self.on_color_selected(ne, qc))
            self.color_btn_group.addButton(btn, i)
            grp_color_layout.addWidget(btn)

        ctrl_layout.addWidget(grp_color)

        # Font Scale Selection Group
        grp_scale = QGroupBox("Размер шрифта")
        grp_scale_layout = QHBoxLayout(grp_scale)

        self.rb_scale1 = QRadioButton("Scale 1 (Компактный: до 12 симв/строка, до 11 строк)")
        self.rb_scale1.setChecked(True)
        self.rb_scale1.toggled.connect(self.on_scale_toggled)
        grp_scale_layout.addWidget(self.rb_scale1)

        self.rb_scale2 = QRadioButton("Scale 2 (Крупный: до 6 симв/строка, до 6 строк)")
        self.rb_scale2.toggled.connect(self.on_scale_toggled)
        grp_scale_layout.addWidget(self.rb_scale2)

        ctrl_layout.addWidget(grp_scale)

        # Send Action Button
        self.btn_send = QPushButton("🚀 Отправить на часы (BLE)")
        self.btn_send.setObjectName("btnSend")
        self.btn_send.clicked.connect(self.send_to_watch)
        ctrl_layout.addWidget(self.btn_send)

        # Status text
        self.lbl_status = QLabel("Готов к отправке на CHASY-STEALTH (62:62:15:07:07:15)")
        self.lbl_status.setStyleSheet("color: #7F8C8D; font-size: 12px;")
        self.lbl_status.setAlignment(Qt.AlignmentFlag.AlignCenter)
        ctrl_layout.addWidget(self.lbl_status)

        ctrl_layout.addStretch()
        main_layout.addLayout(ctrl_layout, stretch=3)

        # -------------------------------------------------------------
        # RIGHT: Live Display Simulator
        # -------------------------------------------------------------
        preview_box = QVBoxLayout()
        preview_box.setAlignment(Qt.AlignmentFlag.AlignCenter)
        
        lbl_sim_title = QLabel("📱 Экран часов LP715 (80×160 IPS)")
        lbl_sim_title.setStyleSheet("font-weight: bold; color: #5dade2; margin-bottom: 8px;")
        preview_box.addWidget(lbl_sim_title, alignment=Qt.AlignmentFlag.AlignCenter)

        self.sim = WatchDisplaySimulator()
        preview_box.addWidget(self.sim, alignment=Qt.AlignmentFlag.AlignCenter)

        lbl_sim_hint = QLabel("Точная эмуляция переноса слов и шрифтов")
        lbl_sim_hint.setStyleSheet("color: #666; font-size: 11px; margin-top: 6px;")
        preview_box.addWidget(lbl_sim_hint, alignment=Qt.AlignmentFlag.AlignCenter)

        main_layout.addLayout(preview_box, stretch=2)

        self.on_content_changed()

    def set_template(self, text):
        self.txt_input.setText(text)

    def on_color_selected(self, color_name, qcol):
        self.cur_color_name = color_name
        self.cur_qcolor = qcol
        self.update_preview()

    def on_scale_toggled(self):
        self.cur_scale = 2 if self.rb_scale2.isChecked() else 1
        self.update_preview()

    def on_content_changed(self):
        self.update_preview()

    def update_preview(self):
        text = self.txt_input.toPlainText().strip() or " "
        self.sim.update_content(text, self.cur_qcolor, self.cur_scale)

        max_chars = MAX_CHARS_SCALE1 if self.cur_scale == 1 else MAX_CHARS_SCALE2
        max_lines = MAX_LINES_SCALE1 if self.cur_scale == 1 else MAX_LINES_SCALE2
        all_lines = self.sim.wrap_text(text, max_chars)
        line_count = len(all_lines)
        utf8_len = len(text.encode('utf-8'))

        if line_count > max_lines:
            overflow_lines = line_count - max_lines
            self.lbl_stats.setText(
                f"⚠️ ПЕРЕПОЛНЕНИЕ: {line_count}/{max_lines} строк (+{overflow_lines} не влезет). Символов: {len(text)}."
            )
            self.lbl_stats.setStyleSheet("color: #E74C3C; font-weight: bold; font-size: 12px;")
        else:
            self.lbl_stats.setText(
                f"Строк: {line_count}/{max_lines} | Символов: {len(text)} | UTF-8: {utf8_len} байт (лимит строки: {max_chars} симв.)"
            )
            self.lbl_stats.setStyleSheet("color: #2ECC71; font-size: 12px;")

    def send_to_watch(self):
        raw_text = self.txt_input.toPlainText().strip()
        if not raw_text:
            QMessageBox.warning(self, "Внимание", "Пожалуйста, введите текст сообщения перед отправкой.")
            return

        max_chars = MAX_CHARS_SCALE1 if self.cur_scale == 1 else MAX_CHARS_SCALE2
        max_lines = MAX_LINES_SCALE1 if self.cur_scale == 1 else MAX_LINES_SCALE2
        all_lines = self.sim.wrap_text(raw_text, max_chars)

        # If text exceeds display capacity, gracefully trim to visible screen lines
        text_to_send = raw_text
        if len(all_lines) > max_lines:
            trimmed_lines = all_lines[:max_lines]
            text_to_send = " ".join(trimmed_lines)

        self.btn_send.setEnabled(False)
        self.btn_send.setText("⏳ Отправка по BLE...")
        self.lbl_status.setText("Подключение к часам по BLE...")
        self.lbl_status.setStyleSheet("color: #F39C12; font-weight: bold;")

        self.sender_thread = BleSenderThread(text_to_send, self.cur_color_name, self.cur_scale)
        self.sender_thread.finished_signal.connect(self.on_send_finished)
        self.sender_thread.start()

    def on_send_finished(self, success, message):
        self.btn_send.setEnabled(True)
        self.btn_send.setText("🚀 Отправить на часы (BLE)")
        if success:
            self.lbl_status.setText("✅ " + message)
            self.lbl_status.setStyleSheet("color: #2ECC71; font-weight: bold;")
        else:
            self.lbl_status.setText("❌ " + message)
            self.lbl_status.setStyleSheet("color: #E74C3C; font-weight: bold;")


def main():
    app = QApplication(sys.argv)
    window = WatchPagerWindow()
    window.show()
    sys.exit(app.exec())


if __name__ == '__main__':
    main()
