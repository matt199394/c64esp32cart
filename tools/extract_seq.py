#!/usr/bin/env python3
"""
Estrae i file SEQ dai .d64 in una cartella di file .seq numerati.

Uso:
    python3 tools/extract_seq.py <cartella_d64> <cartella_output>

Esempio:
    python3 tools/extract_seq.py /home/matteo/c64/petscii-art ./petscii-seq

I file sono scritti come 0001.seq, 0002.seq, ... e sono ordinabili per nome.
Viene creato anche manifest.txt con l'origine di ogni immagine.
"""

import os
import sys


TRACK_SECTORS = [0] + [21] * 17 + [19] * 7 + [18] * 6 + [17] * 5 + [17] * 5


def sector_offset(track, sector):
    off = 0
    for t in range(1, track):
        off += TRACK_SECTORS[t] * 256
    return off + sector * 256


def read_sector(data, track, sector):
    if track < 1 or track > 40 or sector >= TRACK_SECTORS[track]:
        return None
    off = sector_offset(track, sector)
    if off + 256 > len(data):
        return None
    return data[off:off + 256]


def parse_directory(data):
    entries = []
    track, sector = 18, 1
    seen = set()

    while track != 0 and (track, sector) not in seen:
        seen.add((track, sector))
        sec = read_sector(data, track, sector)
        if sec is None:
            break

        next_track, next_sector = sec[0], sec[1]
        for i in range(8):
            e = sec[2 + i * 32:2 + (i + 1) * 32]
            if len(e) < 32:
                continue
            ftype = e[0]
            if (ftype & 0x0F) != 1:       # SEQ
                continue
            ft, fs = e[1], e[2]
            name = e[3:19].decode("latin1").replace("\xa0", " ").rstrip()
            entries.append((name, ft, fs))

        track, sector = next_track, next_sector

    return entries


def read_seq(data, track, sector):
    out = bytearray()
    seen = set()

    while track != 0:
        if (track, sector) in seen:
            raise ValueError("catena settori ciclica")
        seen.add((track, sector))

        sec = read_sector(data, track, sector)
        if sec is None:
            raise ValueError("settore non valido")

        nt, ns = sec[0], sec[1]
        if nt == 0:
            n = ns if ns >= 2 else 2
            out.extend(sec[2:n])
            break

        out.extend(sec[2:256])
        track, sector = nt, ns

    return bytes(out)


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 1

    src = sys.argv[1]
    dst = sys.argv[2]
    os.makedirs(dst, exist_ok=True)

    manifest = []
    seq_no = 0
    errors = 0

    for name in sorted(os.listdir(src)):
        if not name.lower().endswith(".d64"):
            continue
        path = os.path.join(src, name)
        data = open(path, "rb").read()

        for seq_name, track, sector in parse_directory(data):
            try:
                payload = read_seq(data, track, sector)
            except ValueError as exc:
                print("skip %s / %s: %s" % (name, seq_name, exc))
                errors += 1
                continue

            if not payload:
                continue

            seq_no += 1
            out_name = "%04d.seq" % seq_no
            with open(os.path.join(dst, out_name), "wb") as f:
                f.write(payload)
            manifest.append("%s\t%d\t%s\t%s" %
                            (out_name, len(payload), name, seq_name))

    with open(os.path.join(dst, "manifest.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(manifest) + "\n")

    print("Estratti %d SEQ in %s" % (seq_no, dst))
    print("Errori/skip: %d" % errors)
    return 0


if __name__ == "__main__":
    sys.exit(main())
