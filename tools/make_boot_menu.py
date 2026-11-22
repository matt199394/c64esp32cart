#!/usr/bin/env python3
"""Genera la schermata del menu di avvio presente nella EPROM.

Produce `boot_menu.inc` per ACME: 1024 byte di screen code ($0400-$07FF) e
1024 byte di colori ($D800-$DBFF), cioe' il template 40x25 del menu che il
bootstrap mostra appena la C64 parte dalla cartuccia.

Il disegno ricalca i menu generati dall'ESP32 (buildMenuScreen in
c64esp32Cart.ino): cornice doppia azzurra (screen code 64 orizzontale, 93
verticale, 43 angoli), titolo giallo, sottotitolo grigio chiaro, voci bianche
con la riga di dettaglio ciano.

Uso:
    tools/make_boot_menu.py            # scrive boot_menu.inc
"""

import sys

COLS, ROWS = 40, 25
BORDER = 14          # azzurro
BG = 0               # nero

# screen code dei caratteri usati (charset maiuscolo/graphics standard)
MAP = {' ': 32, '@': 0, '[': 27, ']': 29}
for _i in range(26):
    MAP[chr(ord('A') + _i)] = 1 + _i
for _i in range(10):
    MAP[chr(ord('0') + _i)] = 48 + _i
MAP.update({
    '!': 33, '"': 34, '#': 35, '$': 36, '%': 37, '&': 38, "'": 39,
    '(': 40, ')': 41, '*': 42, '+': 43, ',': 44, '-': 45, '.': 46,
    '/': 47, ':': 58, ';': 59, '<': 60, '=': 61, '>': 62, '?': 63,
})

FRAME_H = 64         # linea orizzontale doppia
FRAME_V = 93         # linea verticale
FRAME_C = 43         # angolo/incrocio '+'


class Screen:
    def __init__(self):
        self.scr = [32] * (COLS * ROWS)
        self.col = [BORDER] * (COLS * ROWS)

    def put(self, row, col, text, color=1):
        for i, ch in enumerate(text):
            if col + i >= COLS:
                break
            self.scr[row * COLS + col + i] = MAP[ch.upper()]
            self.col[row * COLS + col + i] = color

    def center(self, row, text, color=1):
        self.put(row, (COLS - len(text)) // 2, text, color)

    def frame(self, color=BORDER):
        for x in range(1, COLS - 1):
            self.put(0, x, ' ', color)
            self.put(ROWS - 1, x, ' ', color)
            self.scr[0 * COLS + x] = FRAME_H
            self.scr[(ROWS - 1) * COLS + x] = FRAME_H
        for y in range(1, ROWS - 1):
            self.scr[y * COLS + 0] = FRAME_V
            self.scr[y * COLS + COLS - 1] = FRAME_V
            self.col[y * COLS + 0] = color
            self.col[y * COLS + COLS - 1] = color
        for x, y in ((0, 0), (COLS - 1, 0), (0, ROWS - 1), (COLS - 1, ROWS - 1)):
            self.scr[y * COLS + x] = FRAME_C
            self.col[y * COLS + x] = color


def build():
    s = Screen()
    s.frame()

    s.center(2, "C64 ESP32 CARTRIDGE", 7)      # giallo
    s.center(3, "BY M.ANGELINI", 15)           # grigio chiaro
    s.center(5, "SCEGLI IL MENU DA CARICARE", 1)

    s.put(7, 5, "1) MENU SENZA SAM", 1)
    s.put(8, 8, "NEWS, KOALA, SID, HELP", 3)   # ciano
    s.put(10, 5, "2) MENU CON SAM", 1)
    s.put(11, 8, "SAM TALK, SAM NEWS", 3)

    s.put(13, 5, "PREMI 1 O 2", 15)

    # riga 15: qui il loader stampa "CARICO ESPNEWSAUTO.PRG" (CHROUT usa il
    # colore che trova in color RAM, quindi la riga deve essere bianca)
    for x in range(1, COLS - 1):
        s.col[15 * COLS + x] = 1

    s.put(20, 5, "EPROM 8K + SD ESP32", 11)
    return s


def emit(f, name, data):
    f.write("%s\n" % name)
    for i in range(0, len(data), 16):
        f.write("        !byte %s\n" % ", ".join("$%02x" % b for b in data[i:i + 16]))


def main():
    s = build()
    scr = s.scr + [32] * (1024 - len(s.scr))
    col = s.col + [BORDER] * (1024 - len(s.col))
    out = sys.argv[1] if len(sys.argv) > 1 else "boot_menu.inc"
    with open(out, "w") as f:
        f.write("; ===========================================================================\n")
        f.write(";  boot_menu.inc - GENERATO da tools/make_boot_menu.py, non modificare a mano\n")
        f.write(";  1024 byte screen code + 1024 byte colori (padding a 1024 per copiare\n")
        f.write(";  pagine intere: $0400-$07FF e $D800-$DBFF sono entrambe aree da 1K).\n")
        f.write("; ===========================================================================\n\n")
        emit(f, "boot_menu_screen", scr)
        f.write("\n")
        emit(f, "boot_menu_colors", col)
    print("%s: %d + %d byte" % (out, len(scr), len(col)))


if __name__ == "__main__":
    main()
