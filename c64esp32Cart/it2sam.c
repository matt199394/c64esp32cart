// it2sam.c - conversione testo italiano -> fonemi SAM (non inglesi)
//
// SAM (s-macke) ha solo le tabelle inglesi: qui non si aggiungono fonemi, si
// sceglie per ogni suono italiano il fonema SAM piu' vicino e si scrive la
// stringa che SetInput()/Parser1() si aspettano (nomi di 2 caratteri, cifra di
// stress dopo la vocale, spazio tra parole, terminatore 0x9B).
//
// Regole (volutamente semplici, la lingua non e' il punto di questo test):
//   - vocali: a=AA, e=EY (EH se accentata grave), i=IY, o=OW (AO se grave),
//     u=UW; accenti (anche UTF-8) spostano lo stress;
//   - stress: sillaba accentata, altrimenti la penultima;
//   - c+e/i, ci+v, ce = CH; c+a/o/u, ch = K; g+e/i, gi+v = J; g+a/o/u, gh = G;
//     gn = N Y; gli+v = L Y; sc+e/i = SH; sch = S K; qu = K W;
//     z iniziale = D Z, z interna = T S; h muta; x = K S; j/w = J/W;
//   - doppie ridotte a una (SAM non ha consonanti lunghe);
//   - spazi/punteggiatura -> fonemi di pausa di SAM.
//
// Limite noto: dittonghi e iati sono gestiti con una euristica, quindi lo
// stress puo' cadere sulla sillaba sbagliata; SAM pronuncia comunque in
// "italiano con accento inglese".

#include "it2sam.h"
#include <string.h>

#define MAXTOK 96

typedef struct {
    char ph[MAXTOK][4];          // nome del fonema + eventuale cifra di stress
    unsigned char nuc[MAXTOK];   // indice del nucleo (sillaba), 255 = consonante
    unsigned char isv[MAXTOK];   // 1 = vocale
    int n;
    unsigned char nnuc;
    unsigned char accent_nuc;    // nucleo accentato, 255 = nessuno
    int prev_vowel;
    char prev_vchar;
} ph_t;

static int isvow(char c)
{
    return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u';
}

static void ph_reset(ph_t *p)
{
    p->n = 0;
    p->nnuc = 0;
    p->accent_nuc = 255;
    p->prev_vowel = 0;
    p->prev_vchar = 0;
}

static void put(ph_t *p, const char *s)
{
    if (p->n >= MAXTOK) return;
    p->ph[p->n][0] = s[0];
    p->ph[p->n][1] = s[1] ? s[1] : 0;
    p->ph[p->n][2] = 0;
    p->nuc[p->n] = 255;
    p->isv[p->n] = 0;
    p->n++;
    p->prev_vowel = 0;
}

// accent: 0 = nessuno, 1 = grave (accento aperto), 2 = acuto
static void put_v(ph_t *p, char v, int accent)
{
    const char *name;
    int same;

    if (v == 'a')      name = "AA";
    else if (v == 'e') name = (accent == 1) ? "EH" : "EY";
    else if (v == 'i') name = "IY";
    else if (v == 'o') name = (accent == 1) ? "AO" : "OW";
    else               name = "UW";

    // dittongo: i/u accanto a un'altra vocale restano nello stesso nucleo
    same = p->prev_vowel && (p->prev_vchar == 'i' || p->prev_vchar == 'u' ||
                             v == 'i' || v == 'u');
    if (!same) p->nnuc++;
    if (p->n >= MAXTOK) return;
    put(p, name);
    p->isv[p->n - 1] = 1;
    p->nuc[p->n - 1] = p->nnuc ? (unsigned char)(p->nnuc - 1) : 0;
    p->prev_vowel = 1;
    p->prev_vchar = v;
    if (accent && p->accent_nuc == 255) p->accent_nuc = p->nuc[p->n - 1];
}

// una parola (gia' minuscola, senza accenti) -> token fonemi
static void word_ph(const char *w, const unsigned char *acc, int len, ph_t *p)
{
    int i;

    ph_reset(p);
    for (i = 0; i < len; ) {
        char c  = w[i];
        char c2 = (i + 1 < len) ? w[i + 1] : 0;
        char c3 = (i + 2 < len) ? w[i + 2] : 0;

        if (c == 'c') {
            if (c2 == 'h') {
                put(p, "K"); i += 2;
            } else if (c2 == 'i' && isvow(c3)) {      // ciao, cielo? no: ci+v
                put(p, "CH"); i += 2;
            } else if (c2 == 'i' || c2 == 'e') {      // cinema, cesto
                put(p, "CH"); i += 2; put_v(p, c2, acc[i - 1]);
            } else {                                  // casa, cosa, cura
                put(p, "K"); i++;
            }
        } else if (c == 'g') {
            if (c2 == 'h') {
                put(p, "G"); i += 2;
            } else if (c2 == 'n') {                   // gn
                put(p, "N"); put(p, "Y"); i += 2;
            } else if (c2 == 'l' && c3 == 'i' && (i + 3 < len) && isvow(w[i + 3])) {
                put(p, "L"); put(p, "Y"); i += 3;     // famiglia
            } else if (c2 == 'l' && c3 == 'i') {
                put(p, "L"); put(p, "Y"); i += 2;     // "gli" in fondo: i resta
            } else if (c2 == 'l') {
                put(p, "G"); put(p, "L"); i += 2;     // globo
            } else if (c2 == 'i' && isvow(c3)) {
                put(p, "J"); i += 2;                  // gia', gioco
            } else if (c2 == 'i' || c2 == 'e') {
                put(p, "J"); i += 2; put_v(p, c2, acc[i - 1]); // giro, gelo
            } else {
                put(p, "G"); i++;                     // gatto
            }
        } else if (c == 's') {
            if (c2 == 'c' && c3 == 'h') {             // schema
                put(p, "S"); i++;
            } else if (c2 == 'c' && c3 == 'i' && (i + 3 < len) && isvow(w[i + 3])) {
                put(p, "SH"); i += 3;                 // sciame
            } else if (c2 == 'c' && (c3 == 'e' || c3 == 'i')) {
                put(p, "SH"); i += 3; put_v(p, c3, acc[i - 1]); // scena, sci
            } else if (c2 == 'c') {                   // sca, sco, scu
                put(p, "S"); i++;
            } else {
                put(p, "S"); i++;
            }
        } else if (c == 'q') {
            if (c2 == 'u') { put(p, "K"); put(p, "W"); i += 2; }
            else { put(p, "K"); i++; }
        } else if (c == 'z') {
            if (i == 0) { put(p, "D"); put(p, "Z"); }  // zero
            else { put(p, "T"); put(p, "S"); }         // pizza, grazie
            i++;
        } else if (c == 'h') {                         // muta
            i++;
        } else if (c == 'x') {
            put(p, "K"); put(p, "S"); i++;
        } else if (c == 'j') {
            put(p, "J"); i++;
        } else if (c == 'w') {
            put(p, "W"); i++;
        } else if (c == 'y') {
            put(p, "Y"); i++;
        } else if (c == 'k') {
            put(p, "K"); i++;
        } else if (isvow(c)) {
            put_v(p, c, acc[i]); i++;
        } else if (c >= 'b' && c <= 'v' && !isvow(c)) {
            char t[2];
            t[0] = (char)(c - 'a' + 'A');
            t[1] = 0;
            put(p, t);
            i++;
        } else {
            i++;                                       // cifre, altro: ignorati
        }
    }
}

// stress: sillaba accentata, altrimenti la penultima
static void apply_stress(ph_t *p)
{
    unsigned char target;
    int i;

    if (p->nnuc == 0) return;
    if (p->accent_nuc != 255) target = p->accent_nuc;
    else if (p->nnuc >= 2) target = (unsigned char)(p->nnuc - 2);
    else target = 0;

    for (i = 0; i < p->n; i++) {
        if (p->isv[i] && p->nuc[i] == target) {
            size_t l = strlen(p->ph[i]);
            if (l < 3) {
                p->ph[i][l] = '1';
                p->ph[i][l + 1] = 0;
            }
            return;
        }
    }
}

// UTF-8 / latin-1 accentato -> vocale + tipo di accento
static char acc_map(unsigned char b2, int *accent)
{
    switch (b2) {
    // maiuscole accentate (UTF-8 0xC3 ...)
    case 0x80: *accent = 1; return 'a';
    case 0x81: *accent = 2; return 'a';
    case 0x88: *accent = 1; return 'e';
    case 0x89: *accent = 2; return 'e';
    case 0x8C: *accent = 1; return 'i';
    case 0x8D: *accent = 2; return 'i';
    case 0x92: *accent = 1; return 'o';
    case 0x93: *accent = 2; return 'o';
    case 0x99: *accent = 1; return 'u';
    case 0x9A: *accent = 2; return 'u';
    // minuscole accentate (UTF-8 0xC3 ...)
    case 0xA0: *accent = 1; return 'a';
    case 0xA1: *accent = 2; return 'a';
    case 0xA8: *accent = 1; return 'e';
    case 0xA9: *accent = 2; return 'e';
    case 0xAC: *accent = 1; return 'i';
    case 0xAD: *accent = 2; return 'i';
    case 0xB2: *accent = 1; return 'o';
    case 0xB3: *accent = 2; return 'o';
    case 0xB9: *accent = 1; return 'u';
    case 0xBA: *accent = 2; return 'u';
    default:   *accent = 0; return 0;
    }
}

static int emit(char *out, size_t outmax, int *pos, const char *s)
{
    while (*s) {
        if (*pos >= (int)outmax - 2) return 0;
        out[(*pos)++] = *s++;
    }
    return 1;
}

size_t it_to_sam(const char *text, char *out, size_t outmax)
{
    char w[40];
    unsigned char acc[40];
    int wl = 0, pos = 0, i, wrote = 0;
    ph_t p;

    if (!text || !out || outmax < 8) return 0;

    // Parser2 di sam.c legge phonemeindex[pos-1] quando la frase comincia con
    // una occlusiva (flags&1): con pos=0 leggerebbe phonemeindex[-1]. Una pausa
    // iniziale evita il problema (nel Reciter inglese lo fa la "/H" iniziale).
    out[0] = ' ';
    pos = 1;

    for (i = 0; ; i++) {
        unsigned char b = (unsigned char)text[i];
        char ch = 0, punct = 0;
        int ac = 0;

        if (b == 0) break;
        if (b == 0xC3) {
            unsigned char b2 = (unsigned char)text[i + 1];
            if (b2) {
                ch = acc_map(b2, &ac);
                if (ch) i++;
            }
        } else if (b >= 'a' && b <= 'z') {
            ch = (char)b;
        } else if (b >= 'A' && b <= 'Z') {
            ch = (char)(b - 'A' + 'a');
        } else if (b == '\'') {
            // elisione (l'economia, dell'anno, un'amica) e accenti
            // ascii (a' e' i' o' u'): concatena senza chiudere la parola,
            // cosi' "l'economia" si pronuncia "leconomia".
            continue;
        } else if (b == '.' || b == '?' || b == '!' || b == ',' ||
                   b == ';' || b == ':' || b == '-') {
            punct = (char)b;
        }

        if (ch) {
            if (wl < (int)sizeof(w) - 1) {
                if (wl > 0 && !isvow(ch) && w[wl - 1] == ch) {
                    // doppia consonante: una sola
                    if (ac) acc[wl - 1] = (unsigned char)ac;
                } else {
                    w[wl] = ch;
                    acc[wl] = (unsigned char)ac;
                    wl++;
                }
            }
            continue;
        }

        // fine parola
        if (wl > 0) {
            word_ph(w, acc, wl, &p);
            apply_stress(&p);
            if (p.n > 0) {
                for (int k = 0; k < p.n; k++) emit(out, outmax, &pos, p.ph[k]);
                if (pos < (int)outmax - 2) out[pos++] = ' ';
                wrote++;
            }
            wl = 0;
        }
        if (punct) {
            const char *s = ".";
            if (punct == ',') s = ",";
            else if (punct == '?') s = "?";
            else if (punct == ';' || punct == ':') s = ",";
            else if (punct == '-') s = "-";
            emit(out, outmax, &pos, s);
            if (pos < (int)outmax - 2) out[pos++] = ' ';
            wrote++;
        }
    }

    // parola finale senza punteggiatura
    if (wl > 0) {
        word_ph(w, acc, wl, &p);
        apply_stress(&p);
        if (p.n > 0) {
            for (int k = 0; k < p.n; k++) emit(out, outmax, &pos, p.ph[k]);
            wrote++;
        }
    }

    if (!wrote || pos >= (int)outmax - 1) return 0;
    while (pos > 0 && out[pos - 1] == ' ') pos--;    // niente pausa finale
    out[pos++] = (char)0x9B;    // terminatore atteso da Parser1()
    out[pos] = 0;
    return (size_t)pos;
}
