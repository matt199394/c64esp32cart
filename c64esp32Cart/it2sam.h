#ifndef IT2SAM_H
#define IT2SAM_H

#include <stddef.h>

// Converte testo italiano nella stringa di fonemi che si aspetta sam.c
// (nomi di 2 caratteri, cifra di stress dopo la vocale accentata, '/'
// non usato, spazio tra parole, terminatore 0x9B).
//
// Ritorna la lunghezza della stringa scritta in out, terminatore 0x9B
// compreso (piu' il NUL), oppure 0 se il testo non produce fonemi.
// out deve essere grande almeno IT2SAM_MAX byte.
#define IT2SAM_MAX 160

size_t it_to_sam(const char *text, char *out, size_t outmax);

#endif
