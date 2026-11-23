#!/bin/sh
# ============================================================================
#  Verifica che i sorgenti SAM (software commerciale, NON in git) siano a posto
#  per compilare la voce: presenza dei file e le due patch obbligatorie.
#  Vedi SAM_SOURCES.md.
#
#  Uso:  tools/check_sam_sources.sh [cartella_sketch]
#        (default: c64esp32Cart)
# ============================================================================
set -u

DIR=${1:-c64esp32Cart}
FILES="sam.c sam.h render.c render.h reciter.c reciter.h RenderTabs.h ReciterTabs.h SamTabs.h debug.c debug.h"
MISSING=0

echo "cartella: $DIR"
for f in $FILES; do
	if [ -f "$DIR/$f" ]; then
		printf '  ok      %s\n' "$f"
	else
		printf '  MANCA   %s\n' "$f"
		MISSING=$((MISSING + 1))
	fi
done

if [ "$MISSING" -gt 0 ]; then
	echo
	echo "$MISSING file mancanti: copiali come spiegato in SAM_SOURCES.md."
	echo "Nel frattempo il firmware compila lo stesso senza voce"
	echo "(ENABLE_SAM=0 automatico, oppure -DENABLE_SAM=0)."
	exit 1
fi

echo
echo "patch:"
if grep -q "buffer == 0" "$DIR/sam.c"; then
	echo "  ok      sam.c: controllo del malloc in Init()"
else
	echo "  MANCA   sam.c: aggiungere 'if (buffer == 0) { bufferpos = 0; }' dopo il malloc"
fi
if grep -q "bufferpos */ *50 *+ *k *<" "$DIR/render.c"; then
	echo "  ok      render.c: Output8BitAry() resta dentro il buffer"
else
	echo "  MANCA   render.c: Output8BitAry() deve controllare 'bufferpos/50 + k < SAM_BUF_BYTES'"
fi

echo
echo "sorgenti SAM completi: la prossima compilazione include la voce"
echo "(log seriale: 'SAM: voce inclusa (ENABLE_SAM=1)')."
echo "Ricorda: questi file non vanno mai committati (vedi .gitignore)."
