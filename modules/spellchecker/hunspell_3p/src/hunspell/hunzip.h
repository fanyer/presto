/* hunzip: file decompression for sorted dictionaries with optional encryption,
 * algorithm: prefix-suffix encoding and 16-bit Huffman encoding */

#ifndef _HUNZIP_HXX_
#define _HUNZIP_HXX_

#include "modules/spellchecker/hunspell_3p/src/hunspell/hunvisapi.h"

#define BUFSIZE  65536
#define HZIP_EXTENSION ".hz"

#define MSG_OPEN   "error: %s: cannot open\n"
#define MSG_FORMAT "error: %s: not in hzip format\n"
#define MSG_MEMORY "error: %s: missing memory\n"
#define MSG_KEY    "error: %s: missing or bad password\n"

struct bit {
    unsigned char c[2]; // ARRAY OK 2009-03-16 jb
    int v[2];
};

class LIBHUNSPELL_DLL_EXPORTED Hunzip
{

protected:
    char * filename;
    HUNSPELL_FILE * fin;
    int bufsiz, lastbit, inc, inbits, outc;
    struct bit * dec;        // code table
    char in[BUFSIZE];        // ARRAY OK 2009-03-17 jb // input buffer
    char out[BUFSIZE + 1];   // ARRAY OK 2009-04-23 jb // Huffman-decoded buffer
    char line[BUFSIZE + 50]; // ARRAY OK 2009-04-23 jb // decoded line
    int getcode(const char * key);
    int getbuf();
    int fail(const char * err, const char * par);
    
public:   
    Hunzip(const char * filename, const char * key = NULL);
    ~Hunzip();
    const char * getline();
};

#endif
