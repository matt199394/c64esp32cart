#!/usr/bin/env bash
# Scarica una pagina Televideo RAI e la converte in Koala (.koa) per C64.
# Uso: tools/televideo_fetch.sh <pagina> [cartella_output]
set -euo pipefail

PAGE="${1:-100}"
OUTDIR="${2:-/home/matteo/c64/televideo_koa}"
BASE="https://www.rainews.it/tlvd/images/nazionale/16_9_page-${PAGE}.png"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$OUTDIR"

echo "Scarico pagina $PAGE: $BASE"
curl -L --max-time 30 -f -s "$BASE" -o "$TMP/page.png"

identify "$TMP/page.png" >/dev/null
convert "$TMP/page.png" -gravity center -crop 640x400+0+0 +repage \
  -filter point -resize 320x200! "$TMP/base.png"

# Prima prova a 8 colori; se png2prg trova celle con troppi colori, usa 4.
for COLORS in 8 4; do
  convert "$TMP/base.png" -dither None -colors "$COLORS" "$TMP/q$COLORS.png"
  if png2prg -m koala -targetdir "$TMP" "$TMP/q$COLORS.png" >"$TMP/png2prg.log" 2>&1; then
    mv "$TMP/q$COLORS.koa" "$OUTDIR/tlvd_${PAGE}.koa"
    echo "OK: $OUTDIR/tlvd_${PAGE}.koa ($COLORS colori)"
    exit 0
  fi
  echo "  $COLORS colori non bastano, riprovo..."
done

echo "ERRORE: conversione fallita per pagina $PAGE"
cat "$TMP/png2prg.log" >&2
exit 1
