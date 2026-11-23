#ifndef SAM_WRAP_H
#define SAM_WRAP_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

// SAM scrive nel buffer a 22050 Hz, 8 bit. Il buffer e' piccolo per stare
// nella RAM dell'ESP32: 3 s di parlato + margine (SAM scrive 5 byte in avanti).
#ifndef SAM_BUF_SECONDS
#define SAM_BUF_SECONDS 2
#endif
#define SAM_BUF_BYTES   (22050 * SAM_BUF_SECONDS + 64)
// lunghezza massima del testo per frase (vedi sam_say)
#define SAM_MAX_CHARS   16

// Converte testo inglese in campioni a 11025 Hz, 4 bit (valori 0..15, uno per byte).
// Ritorna il numero di campioni scritti in out (0 se errore/testo vuoto).
// out deve avere almeno SAM_BUF_SECONDS*11025+32 byte.
size_t sam_say(const char *text, uint8_t *out, size_t outmax);

// Come sam_say, ma sceglie la lingua: SAM_LANG_EN (Reciter inglese) oppure
// SAM_LANG_IT (it2sam.c: regole italiane -> fonemi SAM).
#define SAM_LANG_EN 0
#define SAM_LANG_IT 1
size_t sam_say_lang(const char *text, uint8_t *out, size_t outmax, int lang);
// 1 se l ultima frase e stata troncata perche piu lunga del buffer
extern int sam_truncated;

#ifdef __cplusplus
}
#endif
#endif
