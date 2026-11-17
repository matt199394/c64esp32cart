#!/usr/bin/env python3
"""Renderizza le schermate generate dall'ESP32 (menu e sottomenu) in PNG.

I template del menu non sono codice C64: l'ESP32 li costruisce in RAM
(buildMenuScreen, buildNewsSourceScreen, ... in c64esp32Cart.ino) come 40x25
screen code + colori e li invia al C64, che li copia in $0400/$D800.

Questo script riproduce fedelmente layout, testi e colori di quei template
usando il font di sistema rasterizzato a 8x8 (stile "pixel"), cosi' i file
nella documentazione restano coerenti con il firmware.

Uso:
    tools/render_menu.py [cartella_output]

Se hai screenshot reali da un C64 (o da un TV), mettili in docs/screenshots/
con gli stessi nomi: la documentazione li mostra automaticamente.
"""

import os
import sys

from PIL import Image, ImageDraw, ImageFont

# --- palette C64 (Pepto) ---------------------------------------------------
PALETTE = {
    0: (0x00, 0x00, 0x00),      # nero
    1: (0xFF, 0xFF, 0xFF),      # bianco
    2: (0x68, 0x37, 0x2B),      # rosso
    3: (0x70, 0xA4, 0xB2),      # ciano
    4: (0x6F, 0x3D, 0x86),      # viola
    5: (0x58, 0x8D, 0x43),      # verde
    6: (0x35, 0x28, 0x79),      # blu
    7: (0xB8, 0xC7, 0x6F),      # giallo
    8: (0x6F, 0x4F, 0x25),      # arancio
    9: (0x43, 0x39, 0x00),      # marrone
    10: (0x9A, 0x67, 0x59),     # rosso chiaro
    11: (0x44, 0x44, 0x44),     # grigio scuro
    12: (0x6C, 0x6C, 0x6C),     # grigio
    13: (0x9A, 0xD2, 0x84),     # verde chiaro
    14: (0x6C, 0x5E, 0xB5),     # azzurro (bordo standard)
    15: (0x95, 0x95, 0x95),     # grigio chiaro
}

COLS, ROWS = 40, 25
CELL = 8           # cella carattere 8x8 (come il VIC-II)
SCALE = 2          # 2 -> caratteri 16x16 px
BORDER = 28        # bordo C64 in pixel (prima dello scale)

# caratteri del frame generati da drawFrame()
FRAME_H = "\u2500"     # screen code 64: linea orizzontale
FRAME_V = "\u2502"     # screen code 93
FRAME_C = "\u253c"     # screen code 43: incrocio/angolo

_FONT_CANDIDATES = [
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
    "/usr/share/fonts/truetype/freefont/FreeMono.ttf",
]


def load_font():
    for path in _FONT_CANDIDATES:
        if os.path.exists(path):
            return ImageFont.truetype(path, 9)
    return ImageFont.load_default()


FONT = load_font()
_glyph_cache = {}


def glyph(ch):
    """Maschera 8x8 (L) del carattere, in stile pixel."""
    if ch in _glyph_cache:
        return _glyph_cache[ch]
    img = Image.new("L", (CELL, CELL), 0)
    d = ImageDraw.Draw(img)
    try:
        d.text((0, -1), ch, fill=255, font=FONT)
    except Exception:
        pass
    _glyph_cache[ch] = img
    return img


class Screen:
    """40x25 caratteri + colori, come la screen RAM + color RAM del C64."""

    def __init__(self, border=14, background=6):
        self.border = border
        self.background = background
        self.chars = [[" "] * COLS for _ in range(ROWS)]
        self.colors = [[1] * COLS for _ in range(ROWS)]

    def set(self, x, y, ch, color):
        if 0 <= x < COLS and 0 <= y < ROWS:
            self.chars[y][x] = ch
            self.colors[y][x] = color

    def left(self, row, col, text, color):
        for i, ch in enumerate(text):
            self.set(col + i, row, ch, color)

    def center(self, row, text, color):
        self.left(row, max(0, (COLS - len(text)) // 2), text, color)

    def frame(self, color=14):
        for x in range(1, COLS - 1):
            self.set(x, 0, FRAME_H, color)
            self.set(x, ROWS - 1, FRAME_H, color)
        for y in range(1, ROWS - 1):
            self.set(0, y, FRAME_V, color)
            self.set(COLS - 1, y, FRAME_V, color)
        for x, y in ((0, 0), (COLS - 1, 0), (0, ROWS - 1), (COLS - 1, ROWS - 1)):
            self.set(x, y, FRAME_C, color)

    def render(self, path):
        w = (COLS * CELL + 2 * BORDER) * SCALE
        h = (ROWS * CELL + 2 * BORDER) * SCALE
        img = Image.new("RGB", (w, h), PALETTE[self.border])

        area = Image.new("RGB", (COLS * CELL * SCALE, ROWS * CELL * SCALE),
                         PALETTE[self.background])
        draw = ImageDraw.Draw(area)
        for y in range(ROWS):
            for x in range(COLS):
                ch = self.chars[y][x]
                if ch == " ":
                    continue
                color = PALETTE[self.colors[y][x]]
                cx, cy = x * CELL * SCALE, y * CELL * SCALE
                step = SCALE if SCALE > 1 else 1
                if ch in (FRAME_H, FRAME_V, FRAME_C):
                    # linee piene, come i caratteri del frame del VIC-II
                    if ch in (FRAME_H, FRAME_C):
                        draw.rectangle([cx, cy + 3 * SCALE, cx + CELL * SCALE - 1,
                                        cy + 3 * SCALE + step - 1], fill=color)
                    if ch in (FRAME_V, FRAME_C):
                        draw.rectangle([cx + 3 * SCALE, cy, cx + 3 * SCALE + step - 1,
                                        cy + CELL * SCALE - 1], fill=color)
                    continue
                mask = glyph(ch).resize((CELL * SCALE, CELL * SCALE), Image.NEAREST)
                area.paste(color, (cx, cy), mask)
        img.paste(area, (BORDER * SCALE, BORDER * SCALE))
        os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
        img.save(path)
        print("scritto", path)


# --- schermate (specchio di c64esp32Cart.ino) ------------------------------

def menu_main():
    s = Screen()
    s.frame()
    s.center(2, "C64 ESP32 CARTRIDGE", 7)
    s.center(3, "BY M.ANGELINI", 15)
    s.left(5, 8, "MAIN MENU", 3)
    items = ["SD CARD", "SUPERMON", "HELP", "CLOCK / WEATHER", "BASIC",
             "KOALA", "NEWS", "KOALA SLIDESHOW", "PETSCII SLIDESHOW"]
    for i, item in enumerate(items):
        # la riga selezionata lampeggia sul C64 (color wash): qui e' chiara
        color = 13 if i == 0 else 1
        s.left(6 + i, 8, item, color)
    s.left(16, 8, "UP/DOWN SELECT - RETURN ENTER", 15)
    s.left(19, 8, "LOAD PRG/GAMES, SID, KOALA", 3)
    s.left(20, 8, "NEWS + WEATHER FROM INTERNET", 3)
    s.left(21, 8, "PETSCII ART FROM SD CARD", 3)
    return s


def menu_news_source():
    s = Screen()
    s.frame()
    s.center(2, "NEWS SOURCE", 7)
    s.center(4, "SELECT WITH UP/DOWN AND RETURN", 15)
    sources = ["ANSA", "Rai News", "Repubblica", "Il Sole 24 Ore", "Google News",
               "NewsAPI", "BBC World", "Al Jazeera", "SCMP", "CGTN World"]
    for i, name in enumerate(sources):
        s.left(6 + i, 8, name.upper(), 13 if i == 0 else 1)
    s.left(18, 8, "UP/DOWN SELECT - RETURN ENTER", 3)
    return s


def menu_news_background():
    s = Screen()
    s.frame()
    s.center(2, "NEWS BACKGROUND", 7)
    s.center(4, "SELECT WITH UP/DOWN AND RETURN", 15)
    for i, name in enumerate(["PETSCII ART", "KOALA", "CLOCK / WEATHER"]):
        s.left(6 + i, 8, name, 13 if i == 0 else 1)
    s.left(12, 8, "KOALA MUST BE LOADED FROM SD", 3)
    s.left(18, 8, "UP/DOWN SELECT - RETURN ENTER", 3)
    return s


def menu_weather():
    s = Screen()
    s.frame()
    s.center(2, "CLOCK / WEATHER", 7)
    s.left(4, 16, "18:42:07", 7)
    s.center(6, "TUESDAY 17 NOVEMBER 2026", 15)
    s.center(9, "LOCATION: PISA, IT", 3)
    s.center(10, "SCATTERED CLOUDS", 1)
    s.center(12, "TEMP: 14.2 C  MIN 9.8  MAX 16.4", 1)
    s.center(13, "HUMIDITY: 71 %", 1)
    s.center(14, "PRESSURE: 1017 HPA", 1)
    s.center(15, "WIND: 3.2 M/S", 1)
    s.center(16, "CLOUDS: 40 %", 1)
    s.center(20, "SPACE = BACK TO MENU", 15)
    return s


def menu_sid():
    s = Screen()
    s.frame()
    s.center(2, "SID PLAYER", 7)
    info = [
        "NAME: COMMANDO",
        "AUTHOR: ROB HUBBARD",
        "RELEASED: 1985 ELITE",
        "SIZE: 3578 BYTES",
        "TYPE: PSID",
        "LOAD: $4000-$4DF9",
        "INIT: $4000",
        "PLAY: $4003",
        "VIDEO: PAL",
        "SID MODEL: 6581",
    ]
    for i, line in enumerate(info):
        s.left(4 + i, 4, line, 1)
    s.left(19, 6, "F1 PREV - F7 NEXT - SPACE MENU", 3)
    return s


def menu_directory():
    s = Screen()
    s.frame()
    entries = [
        "/..",
        "/C64Music_safe_reloc",
        "/all_gif_koa_split",
        "/petscii-art",
        "/petscii-seq",
        "10_Orbyte.sid",
        "20_Orbyte.sid",
        "espnewsauto.prg",
        "fractal_moon.kla",
        "moon.kla",
    ]
    for i, name in enumerate(entries):
        s.left(1 + i, 2, name[:36], 13 if i == 0 else 1)
    s.left(23, 2, "F1/F7 PAGE - RETURN OPEN - Q QUIT", 15)
    return s


def menu_help():
    s = Screen()
    # helpText dell'ESP32 (comando 63): testo PETSCII con codici colore
    rows = [
        ("+--------------------------------------+", 1),
        ("      C64 ESP32 CARTRIDGE - HELP", 1),
        ("+--------------------------------------+", 1),
        (" SD CARD   LOAD PRG/GAMES, SID, KOALA", 3),
        ("           F1/F7 PAGE, RETURN OPEN", 3),
        ("           Q = QUIT DIRECTORY", 3),
        (" SUPERMON  RAM MONITOR AT $A000", 3),
        (" HELP      THIS PAGE", 3),
        (" CLOCK     TIME AND WEATHER", 3),
        ("           FROM OPENWEATHER", 3),
        (" BASIC     RETURN TO BASIC", 3),
        (" KOALA     SHOW LOADED KOALA", 3),
        (" NEWS      MULTIPLE SOURCES:", 3),
        ("           ANSA/RAI/BBC/ALJAZEERA...", 3),
        (" SLIDESHOW KOALA OR PETSCII ART", 3),
        ("           SPACE EXITS", 3),
        (" SID       LOAD A SID FROM SD FIRST", 1),
        ("           THEN NEWS/SLIDES CAN USE IT", 1),
        (" SD: /PETSCII-ART (.D64)", 1),
        ("     /PETSCII-SEQ (.SEQ)", 1),
        (" UP/DOWN SELECT - RETURN ENTER", 15),
        (" PRESS A KEY TO RETURN TO MENU", 15),
    ]
    for i, (text, color) in enumerate(rows):
        s.left(i, 0, text, color)
    return s


SCREENS = {
    "menu-main.png": menu_main,
    "menu-news-source.png": menu_news_source,
    "menu-news-background.png": menu_news_background,
    "menu-clock-weather.png": menu_weather,
    "menu-sid-player.png": menu_sid,
    "menu-directory.png": menu_directory,
    "menu-help.png": menu_help,
}


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "docs/screenshots"
    for name, builder in SCREENS.items():
        builder().render(os.path.join(outdir, name))


if __name__ == "__main__":
    main()
