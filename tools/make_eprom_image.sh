#!/bin/sh
# ============================================================================
#  Crea l'immagine 64K per la 27C512 a partire da un binario da 8K.
#
#  Uso:
#      tools/make_eprom_image.sh [bank|all] [input.bin] [output.bin]
#
#  Default:
#      bank      = 0        (banco selezionato dai jumper = offset 0x0000)
#      input.bin = c64esp8kBOOT.bin
#      output.bin= W27C512_<nome input>.bin
#
#  Il banco N viene scritto a offset N*8192, il resto del file viene
#  riempito con 0xFF (stato vergine della EEPROM).
#  Con [bank] = all gli 8K vengono replicati in tutti e 8 i banchi: cosi' il
#  contenuto e' valido qualunque sia la posizione dei jumper JA13/JA14/JA15
#  (senza jumper A13/A14/A15 sono tirate a +5V -> banco 7).
# ============================================================================
set -eu

BANK=${1:-0}
IN=${2:-c64esp8kBOOT.bin}
OUT=${3:-}

if [ -z "$OUT" ]; then
	OUT="W27C512_$(basename "$IN" .bin).bin"
fi

if [ ! -f "$IN" ]; then
	echo "errore: $IN non trovato" >&2
	exit 1
fi

SIZE=$(wc -c < "$IN")
if [ "$SIZE" -ne 8192 ]; then
	echo "errore: $IN e' $SIZE byte, attesi 8192" >&2
	exit 1
fi

# immagine 64K vergine
dd if=/dev/zero bs=65536 count=1 2>/dev/null | tr '\000' '\377' > "$OUT"

if [ "$BANK" = "all" ]; then
	for b in 0 1 2 3 4 5 6 7; do
		dd if="$IN" of="$OUT" bs=8192 seek="$b" conv=notrunc 2>/dev/null
	done
	echo "input : $IN ($SIZE byte)"
	echo "output: $OUT ($(wc -c < "$OUT") byte), replicato nei banchi 0-7"
	sha256sum "$OUT"
	exit 0
fi

case "$BANK" in
	''|*[!0-9]*) echo "errore: bank non numerico ($BANK)" >&2; exit 1 ;;
esac
if [ "$BANK" -gt 7 ]; then
	echo "errore: bank $BANK fuori range (0..7)" >&2
	exit 1
fi

OFF=$((BANK * 8192))

# inserisce gli 8K nel banco scelto
dd if="$IN" of="$OUT" bs=8192 seek="$BANK" conv=notrunc 2>/dev/null

echo "input : $IN ($SIZE byte)"
echo "output: $OUT ($(wc -c < "$OUT") byte), banco $BANK -> offset 0x$(printf %04X "$OFF")"
sha256sum "$OUT"
