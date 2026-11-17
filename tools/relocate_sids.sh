#!/usr/bin/env bash
# Reloca i file SID in aree sicure per la C64 ESP32 Cart.
#
# Uso:
#   tools/relocate_sids.sh <cartella_sorgente> <cartella_destinazione>
#
# Target:
#   $4000-$5FFF -> SID fino a ~8 KB
#   $9000-$BFFF -> SID fino a ~12 KB (RAM sotto BASIC ROM)
#
# Zero page: $80-$F0, per non toccare $FB-$FE usati dallo scroller.
# I file troppo grandi vengono saltati.
#
# ATTENZIONE: sidreloc stampa "Relocating from ... to $xxxx-$yyyy".
# Il range $yyyy e' quello realmente usato dal tune: se supera l'area
# scelta, il SID non e' sicuro con Koala+NEWS.

set -u

SRC="${1:-}"
DST="${2:-}"
SIDRELOC="${SIDRELOC:-/home/matteo/c64/sidreloc-1.0/sidreloc}"

if [ -z "$SRC" ] || [ -z "$DST" ]; then
  echo "Uso: $0 <cartella_sorgente> <cartella_destinazione>"
  exit 1
fi
if [ ! -x "$SIDRELOC" ]; then
  echo "sidreloc non trovato: $SIDRELOC"
  exit 1
fi

mkdir -p "$DST"
shopt -s nullglob

for f in "$SRC"/*.sid "$SRC"/*.SID; do
  base="$(basename "$f")"
  stem="${base%.*}"
  size=$(stat -c%s "$f")
  magic=$(head -c4 "$f")

  if [ "$magic" != "PSID" ] && [ "$magic" != "RSID" ]; then
    echo "SKIP  $base (non e' un file PSID/RSID)"
    continue
  fi

  if [ "$size" -le 8320 ]; then
    page=40
    area=4000
    safe_end=5fff
  elif [ "$size" -le 12416 ]; then
    page=90
    area=9000
    safe_end=bfff
  else
    echo "SKIP  $base (${size} byte: troppo grande per le aree libere)"
    continue
  fi

  out="$DST/${stem}_${area}.sid"
  log=$(mktemp)
  echo "RELOC $base -> $out (target \$$area)"

  "$SIDRELOC" -p "$page" -z 80-f0 -q "$f" "$out" >"$log" 2>&1
  rc=$?
  cat "$log"

  # 0, 32, 64 e 96 sono codici di successo (32/64/96 = warning).
  case "$rc" in
    0|32|64|96)
      dest_end=$(grep -oE 'to \$[0-9a-fA-F]+-\$[0-9a-fA-F]+' "$log" | tail -1 | sed 's/.*-//; s/\$//')
      if [ -n "$dest_end" ]; then
        end_dec=$((16#$dest_end))
        safe_dec=$((16#$safe_end))
        if [ "$end_dec" -gt "$safe_dec" ]; then
          echo "WARN  $base: range fino a \$$dest_end, oltre \$$safe_end!"
          echo "      Non usarlo con Koala+NEWS (o rilocalo in un'area diversa)."
        fi
      fi
      echo "OK    $base"
      ;;
    *)
      echo "FAIL  $base (sidreloc exit $rc)"
      rm -f "$out"
      ;;
  esac
  rm -f "$log"
done
