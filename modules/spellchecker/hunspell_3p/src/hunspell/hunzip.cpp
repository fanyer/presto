#include "core/pch.h"
#ifdef SPC_USE_HUNSPELL_ENGINE
#include "modules/spellchecker/hunspell_3p/src/hunspell/hunspell_common.h"

#include "modules/spellchecker/hunspell_3p/src/hunspell/hunzip.h"

#define CODELEN  65536
#define BASEBITREC 5000

#define UNCOMPRESSED '\002'
#define MAGIC "hz0"
#define MAGIC_ENCRYPT "hz1"
#define MAGICLEN (sizeof(MAGIC) - 1)

int Hunzip::fail(const char * err, const char * par) {
    //fprintf(hunspell_stderr, err, par);
    return -1;
}

Hunzip::Hunzip(const char * file, const char * key) {
    bufsiz = 0;
    lastbit = 0;
    inc = 0;
    outc = 0;
    dec = NULL;
    fin = NULL;
    filename = (char *) op_malloc(op_strlen(file) + 1);
    if (filename) op_strcpy(filename, file);
    if (getcode(key) == -1) bufsiz = -1;
    else bufsiz = getbuf();
}

int Hunzip::getcode(const char * key) {
    unsigned char c[2]; // ARRAY OK 2009-02-23 jb
    int i, j, n, p;
    int allocatedbit = BASEBITREC;
    const char * enc = key;

    if (!filename) return -1;

    fin = hunspell_fopen(filename, "rb");
    if (!fin) return -1;

    // read magic number
    if ((hunspell_fread(in, 1, 3, fin) < MAGICLEN)
        || !(op_strncmp(MAGIC, in, MAGICLEN) == 0 ||
                op_strncmp(MAGIC_ENCRYPT, in, MAGICLEN) == 0)) {
            return fail(MSG_FORMAT, filename);
    }

    // check encryption
    if (op_strncmp(MAGIC_ENCRYPT, in, MAGICLEN) == 0) {
        unsigned char cs;
        if (!key) return fail(MSG_KEY, filename);
        if (hunspell_fread(&c, 1, 1, fin) < 1) return fail(MSG_FORMAT, filename);
        for (cs = 0; *enc; enc++) cs ^= *enc;
        if (cs != c[0]) return fail(MSG_KEY, filename);
        enc = key;
    } else key = NULL;

    // read record count
    if (hunspell_fread(&c, 1, 2, fin) < 2) return fail(MSG_FORMAT, filename);

    if (key) {
        c[0] ^= *enc;
        if (*(++enc) == '\0') enc = key;
        c[1] ^= *enc;
    }        
    
    n = ((int) c[0] << 8) + c[1];
    dec = (struct bit *) op_malloc(BASEBITREC * sizeof(struct bit));
    if (!dec) return fail(MSG_MEMORY, filename);
    dec[0].v[0] = 0;
    dec[0].v[1] = 0;

    // read codes
    for (i = 0; i < n; i++) {
        unsigned char l;
        if (hunspell_fread(c, 1, 2, fin) < 2) return fail(MSG_FORMAT, filename);
        if (key) {
            if (*(++enc) == '\0') enc = key;
            c[0] ^= *enc;
            if (*(++enc) == '\0') enc = key;            
            c[1] ^= *enc;
        }        
        if (hunspell_fread(&l, 1, 1, fin) < 1) return fail(MSG_FORMAT, filename);
        if (key) {
            if (*(++enc) == '\0') enc = key;
            l ^= *enc;
        }
        if (hunspell_fread(in, 1, l/8+1, fin) < (size_t) l/8+1) return fail(MSG_FORMAT, filename);
        if (key) for (j = 0; j <= l/8; j++) {
            if (*(++enc) == '\0') enc = key;
            in[j] ^= *enc;
        }
        p = 0;
        for (j = 0; j < l; j++) {
            int b = (in[j/8] & (1 << (7 - (j % 8)))) ? 1 : 0;
            int oldp = p;
            p = dec[p].v[b];
            if (p == 0) {
                lastbit++;
                if (lastbit == allocatedbit) {
                    allocatedbit += BASEBITREC;
                    dec = (struct bit *) op_realloc(dec, allocatedbit * sizeof(struct bit));
                }
                dec[lastbit].v[0] = 0;
                dec[lastbit].v[1] = 0;
                dec[oldp].v[b] = lastbit;
                p = lastbit;
            }
        }
        dec[p].c[0] = c[0];
        dec[p].c[1] = c[1];
    }
    return 0;
}

Hunzip::~Hunzip()
{
    if (dec) op_free(dec);
    if (fin) hunspell_fclose(fin);
    if (filename) op_free(filename);
}

int Hunzip::getbuf() {
    int p = 0;
    int o = 0;
    do {
        if (inc == 0) inbits = hunspell_fread(in, 1, BUFSIZE, fin) * 8;
        for (; inc < inbits; inc++) {
            int b = (in[inc / 8] & (1 << (7 - (inc % 8)))) ? 1 : 0;
            int oldp = p;
            p = dec[p].v[b];
            if (p == 0) {
                if (oldp == lastbit) {
                    hunspell_fclose(fin);
                    fin = NULL;
                    // add last odd byte
                    if (dec[lastbit].c[0]) out[o++]  = dec[lastbit].c[1];
                    return o;
                }
                out[o++] = dec[oldp].c[0];
                if (o == BUFSIZE) return o;
                out[o++] = dec[oldp].c[1];
                if (o == BUFSIZE) return o;
                p = dec[p].v[b];
            }
        }
        inc = 0;
    } while (inbits == BUFSIZE * 8);
    return fail(MSG_FORMAT, filename);
}

const char * Hunzip::getline() {
    char linebuf[BUFSIZE]; // ARRAY OK 2009-02-26 jb
    int l = 0, eol = 0, left = 0, right = 0;
    if (bufsiz == -1) return NULL;
    while (l < bufsiz && !eol) {
        linebuf[l++] = out[outc];
        switch (out[outc]) {
            case '\t': break;
            case 31: { // escape
                if (++outc == bufsiz) {
					if (fin == NULL) {
						outc--;
						break;
					}
                    bufsiz = getbuf();					
                    outc = 0;
                }
                linebuf[l - 1] = out[outc];
                break;
            }
            case ' ': break;
            default: if (((unsigned char) out[outc]) < 47) {
                if (out[outc] > 32) {
                    right = out[outc] - 31;
                    if (++outc == bufsiz) {
						if (fin == NULL) {
							outc--;
							break;
						}
                        bufsiz = getbuf();
                        outc = 0;
                    }
                }
                if (out[outc] == 30) left = 9; else left = out[outc];
                linebuf[l-1] = '\n';
                eol = 1;
            }
        }
        if (++outc == bufsiz) {
            outc = 0;
            bufsiz = fin ? getbuf(): -1;
        }
    }
    if (right) op_strlcpy(linebuf + l - 1, line + op_strlen(line) - right - 1, sizeof(linebuf)-l+1);
    else linebuf[l] = '\0';
    op_strlcpy(line + left, linebuf, sizeof(line)-left);
    return line;
}
#endif //SPC_USE_HUNSPELL_ENGINE
