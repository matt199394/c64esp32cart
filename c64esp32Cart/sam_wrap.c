#include "sam_wrap.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "sam.h"
#include "reciter.h"
#include "it2sam.h"

int debug = 0;                       // richiesto da sam.c / render.c (extern)
int sam_truncated = 0;
extern char input[256];

// Renderizza input[] (gia' in fonemi) e decima 22050 -> 11025 Hz, 4 bit.
static size_t sam_render(uint8_t *out, size_t outmax)
{
    SetInput(input);
    if (!SAMMain()) return 0;

    char *buf = GetBuffer();
    int len = GetBufferLength() / 50;          // campioni a 22050 Hz
    if (!buf) return 0;
    sam_truncated = 0;
    if (len > SAM_BUF_BYTES - 64) { len = SAM_BUF_BYTES - 64; sam_truncated = 1; }

    size_t o = 0;
    for (int k = 0; k + 1 < len && o < outmax; k += 2) {      // 22050 -> 11025 Hz
        unsigned a = (unsigned char)buf[k] >> 4;
        unsigned b = (unsigned char)buf[k + 1] >> 4;
        out[o++] = (uint8_t)((a + b + 1) >> 1);               // media, 0..15
    }
    free(buf);
    return o;
}

size_t sam_say(const char *text, uint8_t *out, size_t outmax)
{
    char tmp[SAM_MAX_CHARS + 4];
    size_t n = 0, i;
    // testo: maiuscolo, solo caratteri stampabili, tagliato a SAM_MAX_CHARS
    for (i = 0; text[i] && n < SAM_MAX_CHARS; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c < 32 || c > 126) c = ' ';
        tmp[n++] = (char)toupper(c);
    }
    while (n && tmp[n - 1] == ' ') n--;
    if (n == 0) return 0;
    tmp[n++] = '[';                  // marcatore di fine per il Reciter
    tmp[n] = 0;

    memset(input, 0, sizeof(input));
    memcpy(input, tmp, n + 1);
    if (!TextToPhonemes((unsigned char *)input)) return 0;

    return sam_render(out, outmax);
}

// Italiano: it2sam.c produce direttamente i fonemi, il Reciter inglese non
// viene chiamato (le sue regole riscriverebbero le lettere).
size_t sam_say_lang(const char *text, uint8_t *out, size_t outmax, int lang)
{
    char ph[IT2SAM_MAX];
    size_t n;

    if (lang != SAM_LANG_IT) return sam_say(text, out, outmax);

    n = it_to_sam(text, ph, sizeof ph);
    if (n == 0) return 0;
    memset(input, 0, sizeof(input));
    memcpy(input, ph, n + 1);
    return sam_render(out, outmax);
}
